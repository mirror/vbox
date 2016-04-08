/** @file
 * APIC - Advanced Programmable Interrupt Controller.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifndef ___VBox_vmm_apic_h
#define ___VBox_vmm_apic_h

#include <VBox/vmm/pdmins.h>
#include <VBox/vmm/pdmdev.h>

/** @defgroup grp_apic   The local APIC VMM API
 * @ingroup grp_vmm
 * @{
 */

/** The APIC hardware version we are emulating. */
#define XAPIC_HARDWARE_VERSION               XAPIC_HARDWARE_VERSION_P4

/** Gets the APIC base physical address. */
#define MSR_APICBASE_GET_PHYSADDR(a)         ((a) & PAGE_BASE_GC_MASK)
/** Gets the APIC mode. */
#define MSR_APICBASE_GET_MODE(a)             (((a) >> 10) & UINT64_C(3))
/** The APIC global enable bit. */
#define MSR_APICBASE_XAPIC_ENABLE_BIT        RT_BIT_64(11)
/** The x2APIC global enable bit. */
#define MSR_APICBASE_X2APIC_ENABLE_BIT       RT_BIT_64(10)
/** The APIC bootstrap processor bit. */
#define MSR_APICBASE_BOOTSTRAP_CPU_BIT       RT_BIT_64(8)
/** The default APIC base address. */
#define XAPIC_APICBASE_PHYSADDR              (UINT64_C(0xfee00000) << PAGE_SHIFT)
/** The APIC base MSR - Is the APIC enabled?  */
#define MSR_APICBASE_IS_ENABLED(a_Msr)       RT_BOOL((a_Msr) & MSR_APICBASE_XAPIC_ENABLE_BIT)

/** Offset of APIC ID Register. */
#define XAPIC_OFF_ID                         0x020
/** Offset of APIC Version Register. */
#define XAPIC_OFF_VERSION                    0x030
/** Offset of Task Priority Register. */
#define XAPIC_OFF_TPR                        0x080
/** Offset of Arbitrartion Priority register. */
#define XAPIC_OFF_APR                        0x090
/** Offset of Processor Priority register. */
#define XAPIC_OFF_PPR                        0x0A0
/** Offset of End Of Interrupt register. */
#define XAPIC_OFF_EOI                        0x0B0
/** Offset of Remote Read Register. */
#define XAPIC_OFF_RRD                        0x0C0
/** Offset of Logical Destination Register. */
#define XAPIC_OFF_LDR                        0x0D0
/** Offset of Destination Format Register. */
#define XAPIC_OFF_DFR                        0x0E0
/** Offset of Spurious Interrupt Vector Register. */
#define XAPIC_OFF_SVR                        0x0F0
/** Offset of In-service Register (bits 31:0). */
#define XAPIC_OFF_ISR0                       0x100
/** Offset of In-service Register (bits 63:32). */
#define XAPIC_OFF_ISR1                       0x110
/** Offset of In-service Register (bits 95:64). */
#define XAPIC_OFF_ISR2                       0x120
/** Offset of In-service Register (bits 127:96). */
#define XAPIC_OFF_ISR3                       0x130
/** Offset of In-service Register (bits 159:128). */
#define XAPIC_OFF_ISR4                       0x140
/** Offset of In-service Register (bits 191:160). */
#define XAPIC_OFF_ISR5                       0x150
/** Offset of In-service Register (bits 223:192). */
#define XAPIC_OFF_ISR6                       0x160
/** Offset of In-service Register (bits 255:224). */
#define XAPIC_OFF_ISR7                       0x170
/** Offset of Trigger Mode Register (bits 31:0). */
#define XAPIC_OFF_TMR0                       0x180
/** Offset of Trigger Mode Register (bits 63:32). */
#define XAPIC_OFF_TMR1                       0x190
/** Offset of Trigger Mode Register (bits 95:64). */
#define XAPIC_OFF_TMR2                       0x1A0
/** Offset of Trigger Mode Register (bits 127:96). */
#define XAPIC_OFF_TMR3                       0x1B0
/** Offset of Trigger Mode Register (bits 159:128). */
#define XAPIC_OFF_TMR4                       0x1C0
/** Offset of Trigger Mode Register (bits 191:160). */
#define XAPIC_OFF_TMR5                       0x1D0
/** Offset of Trigger Mode Register (bits 223:192). */
#define XAPIC_OFF_TMR6                       0x1E0
/** Offset of Trigger Mode Register (bits 255:224). */
#define XAPIC_OFF_TMR7                       0x1F0
/** Offset of Interrupt Request Register (bits 31:0). */
#define XAPIC_OFF_IRR0                       0x200
/** Offset of Interrupt Request Register (bits 63:32). */
#define XAPIC_OFF_IRR1                       0x210
/** Offset of Interrupt Request Register (bits 95:64). */
#define XAPIC_OFF_IRR2                       0x220
/** Offset of Interrupt Request Register (bits 127:96). */
#define XAPIC_OFF_IRR3                       0x230
/** Offset of Interrupt Request Register (bits 159:128). */
#define XAPIC_OFF_IRR4                       0x240
/** Offset of Interrupt Request Register (bits 191:160). */
#define XAPIC_OFF_IRR5                       0x250
/** Offset of Interrupt Request Register (bits 223:192). */
#define XAPIC_OFF_IRR6                       0x260
/** Offset of Interrupt Request Register (bits 255:224). */
#define XAPIC_OFF_IRR7                       0x270
/** Offset of Error Status Register. */
#define XAPIC_OFF_ESR                        0x280
/** Offset of LVT CMCI Register. */
#define XAPIC_OFF_LVT_CMCI                   0x2F0
/** Offset of Interrupt Command Register - Lo. */
#define XAPIC_OFF_ICR_LO                     0x300
/** Offset of Interrupt Command Register - Hi. */
#define XAPIC_OFF_ICR_HI                     0x310
/** Offset of LVT Timer Register. */
#define XAPIC_OFF_LVT_TIMER                  0x320
/** Offset of LVT Thermal Sensor Register. */
#define XAPIC_OFF_LVT_THERMAL                0x330
/** Offset of LVT Performance Counter Register. */
#define XAPIC_OFF_LVT_PERF                   0x340
/** Offset of LVT LINT0 Register. */
#define XAPIC_OFF_LVT_LINT0                  0x350
/** Offset of LVT LINT1 Register. */
#define XAPIC_OFF_LVT_LINT1                  0x360
/** Offset of LVT Error Register . */
#define XAPIC_OFF_LVT_ERROR                  0x370
/** Offset of Timer Initial Count Register. */
#define XAPIC_OFF_TIMER_ICR                  0x380
/** Offset of Timer Current Count Register. */
#define XAPIC_OFF_TIMER_CCR                  0x390
/** Offset of Timer Divide Configuration Register. */
#define XAPIC_OFF_TIMER_DCR                  0x3E0
/** Offset of Self-IPI Register (x2APIC only). */
#define X2APIC_OFF_SELF_IPI                  0x3F0

/** Offset of LVT range start. */
#define XAPIC_OFF_LVT_START                  XAPIC_OFF_LVT_TIMER
/** Offset of LVT range end (inclusive).  */
#define XAPIC_OFF_LVT_END                    XAPIC_OFF_LVT_ERROR
/** Offset of LVT extended range start. */
#define XAPIC_OFF_LVT_EXT_START              XAPIC_OFF_LVT_CMCI
/** Offset of LVT extended range end (inclusive). */
#define XAPIC_OFF_LVT_EXT_END                XAPIC_OFF_LVT_CMCI


/**
 * The xAPIC sparse 256-bit register.
 */
typedef union XAPIC256BITREG
{
    /** The sparse-bitmap view. */
    struct
    {
        uint32_t    u32Reg;
        uint32_t    uReserved0[3];
    } u[8];
    /** The 32-bit view. */
    uint32_t        au32[32];
} XAPIC256BITREG;
/** Pointer to an xAPIC sparse bitmap register. */
typedef XAPIC256BITREG *PXAPIC256BITREG;
/** Pointer to a const xAPIC sparse bitmap register. */
typedef XAPIC256BITREG const *PCXAPIC256BITREG;
AssertCompileSize(XAPIC256BITREG, 128);

/**
 * The xAPIC memory layout as per Intel/AMD specs.
 */
typedef struct XAPICPAGE
{
    /* 0x00 - Reserved. */
    uint32_t                    uReserved0[8];
    /* 0x20 - APIC ID. */
    struct
    {
        uint8_t                 u8Reserved0[3];
        uint8_t                 u8ApicId;
        uint32_t                u32Reserved0[3];
    } id;
    /* 0x30 - APIC version register. */
    union
    {
        struct
        {
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
            uint8_t             u8Version;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
            uint8_t             uReserved0;
            uint8_t             u8MaxLvtEntry;
            uint8_t             fEoiBroadcastSupression : 1;
            uint8_t             u7Reserved1   : 7;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Version;
            uint32_t            u32Reserved0[3];
        } all;
    } version;
    /* 0x40 - Reserved. */
    uint32_t                    uReserved1[16];
    /* 0x80 - Task Priority Register (TPR). */
    struct
    {
        uint8_t                 u8Tpr;
        uint8_t                 u8Reserved0[3];
        uint32_t                u32Reserved0[3];
    } tpr;
    /* 0x90 - Arbitration Priority Register (APR). */
    struct
    {
        uint8_t                 u8Apr;
        uint8_t                 u8Reserved0[3];
        uint32_t                u32Reserved0[3];
    } apr;
    /* 0xA0 - Processor Priority Register (PPR). */
    struct
    {
        uint8_t                 u8Ppr;
        uint8_t                 u8Reserved0[3];
        uint32_t                u32Reserved0[3];
    } ppr;
    /* 0xB0 - End Of Interrupt Register (EOI). */
    struct
    {
        uint32_t                u32Eoi;
        uint32_t                u32Reserved0[3];
    } eoi;
    /* 0xC0 - Remote Read Register (RRD). */
    struct
    {
        uint32_t                u32Rrd;
        uint32_t                u32Reserved0[3];
    } rrd;
    /* 0xD0 - Logical Destination Register (LDR). */
    union
    {
        struct
        {
            uint32_t            u24Reserved0    : 24;
            uint32_t            u8LogicalApicId : 8;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Ldr;
            uint32_t            u32Reserved0[3];
        } all;
    } ldr;
    /* 0xE0 - Destination Format Register (DFR). */
    union
    {
        struct
        {
            uint32_t            u28ReservedMb1 : 28;    /* MB1 */
            uint32_t            u4Model        : 4;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Dfr;
            uint32_t            u32Reserved0[3];
        } all;
    } dfr;
    /* 0xF0 - Spurious-Interrupt Vector Register (SVR). */
    union
    {
        struct
        {
            uint32_t            u8SpuriousVector     : 8;
            uint32_t            fApicSoftwareEnable  : 1;
            uint32_t            u3Reserved0          : 3;
            uint32_t            fSupressEoiBroadcast : 1;
            uint32_t            u19Reserved1         : 19;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Svr;
            uint32_t            u32Reserved0[3];
        } all;
    } svr;
    /* 0x100 - In-service Register (ISR). */
    XAPIC256BITREG              isr;
    /* 0x180 - Trigger Mode Register (TMR). */
    XAPIC256BITREG              tmr;
    /* 0x200 - Interrupt Request Register (IRR). */
    XAPIC256BITREG              irr;
    /* 0x280 - Error Status Register (ESR). */
    union
    {
        struct
        {
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
            uint32_t            u4Reserved0        : 4;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
            uint32_t            fRedirectableIpi   : 1;
            uint32_t            fSendIllegalVector : 1;
            uint32_t            fRcvdIllegalVector : 1;
            uint32_t            fIllegalRegAddr    : 1;
            uint32_t            u24Reserved1       : 24;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Errors;
            uint32_t            u32Reserved0[3];
        } all;
    } esr;
    /* 0x290 - Reserved. */
    uint32_t                    uReserved2[28];
    /* 0x300 - Interrupt Command Register (ICR) - Low. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1DestMode       : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            fReserved0       : 1;
            uint32_t            u1Level          : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u2Reserved1      : 2;
            uint32_t            u2DestShorthand  : 2;
            uint32_t            u12Reserved2     : 12;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32IcrLo;
            uint32_t            u32Reserved0[3];
        } all;
    } icr_lo;
    /* 0x310 - Interrupt Comannd Register (ICR) - High. */
    union
    {
        struct
        {
            uint32_t            u24Reserved0 : 24;
            uint32_t            u8Dest       : 8;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32IcrHi;
            uint32_t            u32Reserved0[3];
        } all;
    } icr_hi;
    /* 0x320 - Local Vector Table (LVT) Timer Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u4Reserved0      : 4;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u2TimerMode      : 2;
            uint32_t            u13Reserved2     : 13;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtTimer;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_timer;
    /* 0x330 - Local Vector Table (LVT) Thermal Sensor Register. */
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtThermal;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_thermal;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
    /* 0x340 - Local Vector Table (LVT) Performance Monitor Counter (PMC) Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtPerf;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_perf;
    /* 0x350 - Local Vector Table (LVT) LINT0 Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u1IntrPolarity   : 1;
            uint32_t            u1RemoteIrr      : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtLint0;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_lint0;
    /* 0x360 - Local Vector Table (LVT) LINT1 Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u1IntrPolarity   : 1;
            uint32_t            u1RemoteIrr      : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtLint1;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_lint1;
    /* 0x370 - Local Vector Table (LVT) Error Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u4Reserved0      : 4;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtError;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_error;
    /* 0x380 - Timer Initial Counter Register. */
    struct
    {
        uint32_t                u32InitialCount;
        uint32_t                u32Reserved0[3];
    } timer_icr;
    /* 0x390 - Timer Current Counter Register. */
    struct
    {
        uint32_t                u32CurrentCount;
        uint32_t                u32Reserved0[3];
    } timer_ccr;
    /* 0x3A0 - Reserved. */
    uint32_t                    u32Reserved3[16];
    /* 0x3E0 - Timer Divide Configuration Register. */
    union
    {
        struct
        {
            uint32_t            u2DivideValue0 : 2;
            uint32_t            u1Reserved0    : 1;
            uint32_t            u1DivideValue1 : 1;
            uint32_t            u28Reserved1   : 28;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32DivideValue;
            uint32_t            u32Reserved0[3];
        } all;
    } timer_dcr;
    /* 0x3F0 - Reserved. */
    uint8_t                     u8Reserved0[3088];
} XAPICPAGE;
/** Pointer to a XAPICPAGE struct. */
typedef volatile XAPICPAGE *PXAPICPAGE;
/** Pointer to a const XAPICPAGE struct. */
typedef const volatile XAPICPAGE *PCXAPICPAGE;
AssertCompileSize(XAPICPAGE, 4096);
AssertCompileMemberOffset(XAPICPAGE, id,          XAPIC_OFF_ID);
AssertCompileMemberOffset(XAPICPAGE, version,     XAPIC_OFF_VERSION);
AssertCompileMemberOffset(XAPICPAGE, tpr,         XAPIC_OFF_TPR);
AssertCompileMemberOffset(XAPICPAGE, apr,         XAPIC_OFF_APR);
AssertCompileMemberOffset(XAPICPAGE, ppr,         XAPIC_OFF_PPR);
AssertCompileMemberOffset(XAPICPAGE, eoi,         XAPIC_OFF_EOI);
AssertCompileMemberOffset(XAPICPAGE, rrd,         XAPIC_OFF_RRD);
AssertCompileMemberOffset(XAPICPAGE, ldr,         XAPIC_OFF_LDR);
AssertCompileMemberOffset(XAPICPAGE, dfr,         XAPIC_OFF_DFR);
AssertCompileMemberOffset(XAPICPAGE, svr,         XAPIC_OFF_SVR);
AssertCompileMemberOffset(XAPICPAGE, isr,         XAPIC_OFF_ISR0);
AssertCompileMemberOffset(XAPICPAGE, tmr,         XAPIC_OFF_TMR0);
AssertCompileMemberOffset(XAPICPAGE, irr,         XAPIC_OFF_IRR0);
AssertCompileMemberOffset(XAPICPAGE, esr,         XAPIC_OFF_ESR);
AssertCompileMemberOffset(XAPICPAGE, icr_lo,      XAPIC_OFF_ICR_LO);
AssertCompileMemberOffset(XAPICPAGE, icr_hi,      XAPIC_OFF_ICR_HI);
AssertCompileMemberOffset(XAPICPAGE, lvt_timer,   XAPIC_OFF_LVT_TIMER);
AssertCompileMemberOffset(XAPICPAGE, lvt_thermal, XAPIC_OFF_LVT_THERMAL);
AssertCompileMemberOffset(XAPICPAGE, lvt_perf,    XAPIC_OFF_LVT_PERF);
AssertCompileMemberOffset(XAPICPAGE, lvt_lint0,   XAPIC_OFF_LVT_LINT0);
AssertCompileMemberOffset(XAPICPAGE, lvt_lint1,   XAPIC_OFF_LVT_LINT1);
AssertCompileMemberOffset(XAPICPAGE, lvt_error,   XAPIC_OFF_LVT_ERROR);
AssertCompileMemberOffset(XAPICPAGE, timer_icr,   XAPIC_OFF_TIMER_ICR);
AssertCompileMemberOffset(XAPICPAGE, timer_ccr,   XAPIC_OFF_TIMER_CCR);
AssertCompileMemberOffset(XAPICPAGE, timer_dcr,   XAPIC_OFF_TIMER_DCR);

/**
 * The x2APIC memory layout as per Intel/AMD specs.
 */
typedef struct X2APICPAGE
{
    /* 0x00 - Reserved. */
    uint32_t                    uReserved0[8];
    /* 0x20 - APIC ID. */
    struct
    {
        uint32_t                u32ApicId;
        uint32_t                u32Reserved0[3];
    } id;
    /* 0x30 - APIC version register. */
    union
    {
        struct
        {
            uint8_t             u8Version;
            uint8_t             u8Reserved0;
            uint8_t             u8MaxLvtEntry;
            uint8_t             fEoiBroadcastSupression : 1;
            uint8_t             u7Reserved1   : 7;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Version;
            uint32_t            u32Reserved2[3];
        } all;
    } version;
    /* 0x40 - Reserved. */
    uint32_t                    uReserved1[16];
    /* 0x80 - Task Priority Register (TPR). */
    struct
    {
        uint8_t                 u8Tpr;
        uint8_t                 u8Reserved0[3];
        uint32_t                u32Reserved0[3];
    } tpr;
    /* 0x90 - Reserved. */
    uint32_t                    uReserved2[4];
    /* 0xA0 - Processor Priority Register (PPR). */
    struct
    {
        uint8_t                 u8Ppr;
        uint8_t                 u8Reserved0[3];
        uint32_t                u32Reserved0[3];
    } ppr;
    /* 0xB0 - End Of Interrupt Register (EOI). */
    struct
    {
        uint32_t                u32Eoi;
        uint32_t                u32Reserved0[3];
    } eoi;
    /* 0xC0 - Remote Read Register (RRD). */
    struct
    {
        uint32_t                u32Rrd;
        uint32_t                u32Reserved0[3];
    } rrd;
    /* 0xD0 - Logical Destination Register (LDR). */
    struct
    {
        uint32_t                u32LogicalApicId;
        uint32_t                u32Reserved1[3];
    } ldr;
    /* 0xE0 - Reserved. */
    uint32_t                    uReserved3[4];
    /* 0xF0 - Spurious-Interrupt Vector Register (SVR). */
    union
    {
        struct
        {
            uint32_t            u8SpuriousVector     : 8;
            uint32_t            fApicSoftwareEnable  : 1;
            uint32_t            u3Reserved0          : 3;
            uint32_t            fSupressEoiBroadcast : 1;
            uint32_t            u19Reserved1         : 19;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32Svr;
            uint32_t            uReserved0[3];
        } all;
    } svr;
    /* 0x100 - In-service Register (ISR). */
    XAPIC256BITREG              isr;
    /* 0x180 - Trigger Mode Register (TMR). */
    XAPIC256BITREG              tmr;
    /* 0x200 - Interrupt Request Register (IRR). */
    XAPIC256BITREG              irr;
    /* 0x280 - Error Status Register (ESR). */
    union
    {
        struct
        {
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
            uint32_t            u4Reserved0        : 4;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
            uint32_t            fRedirectableIpi   : 1;
            uint32_t            fSendIllegalVector : 1;
            uint32_t            fRcvdIllegalVector : 1;
            uint32_t            fIllegalRegAddr    : 1;
            uint32_t            u24Reserved1       : 24;
            uint32_t            uReserved0[3];
        } u;
        struct
        {
            uint32_t            u32Errors;
            uint32_t            u32Reserved0[3];
        } all;
    } esr;
    /* 0x290 - Reserved. */
    uint32_t                    uReserved4[28];
    /* 0x300 - Interrupt Command Register (ICR) - Low. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1DestMode       : 1;
            uint32_t            u2Reserved0      : 2;
            uint32_t            u1Level          : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u2Reserved1      : 2;
            uint32_t            u2DestShorthand  : 2;
            uint32_t            u12Reserved2     : 12;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32IcrLo;
            uint32_t            u32Reserved3[3];
        } all;
    } icr_lo;
    /* 0x310 - Interrupt Comannd Register (ICR) - High. */
    struct
    {
        uint32_t                u32IcrHi;
        uint32_t                uReserved1[3];
    } icr_hi;
    /* 0x320 - Local Vector Table (LVT) Timer Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u4Reserved0      : 4;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u2TimerMode      : 2;
            uint32_t            u13Reserved2     : 13;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtTimer;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_timer;
    /* 0x330 - Local Vector Table (LVT) Thermal Sensor Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtThermal;
            uint32_t            uReserved0[3];
        } all;
    } lvt_thermal;
    /* 0x340 - Local Vector Table (LVT) Performance Monitor Counter (PMC) Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtPerf;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_perf;
    /* 0x350 - Local Vector Table (LVT) LINT0 Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u1IntrPolarity   : 1;
            uint32_t            u1RemoteIrr      : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtLint0;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_lint0;
    /* 0x360 - Local Vector Table (LVT) LINT1 Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u3DeliveryMode   : 3;
            uint32_t            u1Reserved0      : 1;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u1IntrPolarity   : 1;
            uint32_t            u1RemoteIrr      : 1;
            uint32_t            u1TriggerMode    : 1;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtLint1;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_lint1;
    /* 0x370 - Local Vector Table (LVT) Error Register. */
    union
    {
        struct
        {
            uint32_t            u8Vector         : 8;
            uint32_t            u4Reserved0      : 4;
            uint32_t            u1DeliveryStatus : 1;
            uint32_t            u3Reserved1      : 3;
            uint32_t            u1Mask           : 1;
            uint32_t            u15Reserved2     : 15;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32LvtError;
            uint32_t            u32Reserved0[3];
        } all;
    } lvt_error;
    /* 0x380 - Timer Initial Counter Register. */
    struct
    {
        uint32_t                u32InitialCount;
        uint32_t                u32Reserved0[3];
    } timer_icr;
    /* 0x390 - Timer Current Counter Register. */
    struct
    {
        uint32_t                u32CurrentCount;
        uint32_t                u32Reserved0[3];
    } timer_ccr;
    /* 0x3A0 - Reserved. */
    uint32_t                    uReserved5[16];
    /* 0x3E0 - Timer Divide Configuration Register. */
    union
    {
        struct
        {
            uint32_t            u2DivideValue0 : 2;
            uint32_t            u1Reserved0    : 1;
            uint32_t            u1DivideValue1 : 1;
            uint32_t            u28Reserved1   : 28;
            uint32_t            u32Reserved0[3];
        } u;
        struct
        {
            uint32_t            u32DivideValue;
            uint32_t            u32Reserved0[3];
        } all;
    } timer_dcr;
    /* 0x3F0 - Self IPI Register. */
    struct
    {
        uint32_t                u8Vector     : 8;
        uint32_t                u24Reserved0 : 24;
        uint32_t                u32Reserved0[3];
    } self_ipi;
    /* 0x400 - Reserved. */
    uint8_t                     u8Reserved0[3072];
} X2APICPAGE;
/** Pointer to a X2APICPAGE struct. */
typedef volatile X2APICPAGE *PX2APICPAGE;
/** Pointer to a const X2APICPAGE struct. */
typedef const volatile X2APICPAGE *PCX2APICPAGE;
//AssertCompileSize(X2APICPAGE, 4096);
AssertCompileMemberOffset(X2APICPAGE, id,          XAPIC_OFF_ID);
AssertCompileMemberOffset(X2APICPAGE, version,     XAPIC_OFF_VERSION);
AssertCompileMemberOffset(X2APICPAGE, tpr,         XAPIC_OFF_TPR);
AssertCompileMemberOffset(X2APICPAGE, ppr,         XAPIC_OFF_PPR);
AssertCompileMemberOffset(X2APICPAGE, eoi,         XAPIC_OFF_EOI);
AssertCompileMemberOffset(X2APICPAGE, rrd,         XAPIC_OFF_RRD);
AssertCompileMemberOffset(X2APICPAGE, ldr,         XAPIC_OFF_LDR);
AssertCompileMemberOffset(X2APICPAGE, svr,         XAPIC_OFF_SVR);
AssertCompileMemberOffset(X2APICPAGE, isr,         XAPIC_OFF_ISR0);
AssertCompileMemberOffset(X2APICPAGE, tmr,         XAPIC_OFF_TMR0);
AssertCompileMemberOffset(X2APICPAGE, irr,         XAPIC_OFF_IRR0);
AssertCompileMemberOffset(X2APICPAGE, esr,         XAPIC_OFF_ESR);
AssertCompileMemberOffset(X2APICPAGE, icr_lo,      XAPIC_OFF_ICR_LO);
AssertCompileMemberOffset(X2APICPAGE, icr_hi,      XAPIC_OFF_ICR_HI);
AssertCompileMemberOffset(X2APICPAGE, lvt_timer,   XAPIC_OFF_LVT_TIMER);
AssertCompileMemberOffset(X2APICPAGE, lvt_thermal, XAPIC_OFF_LVT_THERMAL);
AssertCompileMemberOffset(X2APICPAGE, lvt_perf,    XAPIC_OFF_LVT_PERF);
AssertCompileMemberOffset(X2APICPAGE, lvt_lint0,   XAPIC_OFF_LVT_LINT0);
AssertCompileMemberOffset(X2APICPAGE, lvt_lint1,   XAPIC_OFF_LVT_LINT1);
AssertCompileMemberOffset(X2APICPAGE, lvt_error,   XAPIC_OFF_LVT_ERROR);
AssertCompileMemberOffset(X2APICPAGE, timer_icr,   XAPIC_OFF_TIMER_ICR);
AssertCompileMemberOffset(X2APICPAGE, timer_ccr,   XAPIC_OFF_TIMER_CCR);
AssertCompileMemberOffset(X2APICPAGE, timer_dcr,   XAPIC_OFF_TIMER_DCR);
AssertCompileMemberOffset(X2APICPAGE, self_ipi,    X2APIC_OFF_SELF_IPI);

/** The offset (in bits) of the posted-interrupt bitmap's outstanding
 *  notification bit. */
#define XAPIC_PIB_NOTIFICATION_BIT             UINT32_C(256)

/**
 * APIC Pending Interrupt Bitmap (PIB).
 * The layout is critical as it mimics VT-x's Posted Interrupt Bitmap!
 */
typedef struct APICPIB
{
    uint32_t volatile aVectorBitmap[8];
    uint32_t volatile fOutstandingNotification;
    uint8_t           au8Reserved[28];
} APICPIB;
AssertCompileMemberOffset(APICPIB, fOutstandingNotification, XAPIC_PIB_NOTIFICATION_BIT / 8);
AssertCompileSize(APICPIB, 64);
/** Pointer to a pending interrupt bitmap. */
typedef APICPIB *PAPICPIB;
/** Pointer to a const pending interrupt bitmap. */
typedef const APICPIB *PCAPICPIB;

RT_C_DECLS_BEGIN

#ifdef IN_RING3
/** @defgroup grp_apic_r3  The APIC Host Context Ring-3 API
 * @{
 */
VMMR3_INT_DECL(void)        APICR3InitIpi(PVMCPU pVCpu);
VMMR3_INT_DECL(void)        APICR3Reset(PVMCPU pVCpu);
/** @} */
#endif /* IN_RING3 */

#ifdef IN_RING0
/** @defgroup grp_apic_r0  The APIC Host Context Ring-0 API
 * @{
 */
VMMR0_INT_DECL(int)         APICR0InitVM(PVM pVM);
VMMR0_INT_DECL(int)         APICR0TermVM(PVM pVM);
/** @} */
#endif /* IN_RING0 */

VMMDECL(bool)               APICQueueInterruptToService(PVMCPU pVCpu, uint8_t u8PendingIntr);
VMMDECL(void)               APICDequeueInterruptFromService(PVMCPU pVCpu, uint8_t u8PendingIntr);
VMMDECL(void)               APICUpdatePendingInterrupts(PVMCPU pVCpu);
VMMDECL(bool)               APICGetHighestPendingInterrupt(PVMCPU pVCpu, uint8_t *pu8PendingIntr);

RT_C_DECLS_END

extern const PDMDEVREG      g_DeviceAPIC;
/** @} */

#endif

