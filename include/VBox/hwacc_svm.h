/** @file
 * SVM Structures and Definitions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_svm_h
#define ___VBox_svm_h

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/cpum.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

/** @defgroup grp_svm   svm Types and Definitions
 * @ingroup grp_hwaccm
 * @{
 */


/** @name SVM Basic Exit Reasons.
 * @{
 */
/** Invalid guest state in VMCB. */
#define SVM_EXIT_INVALID                -1
/** Read from CR0-CR15. */
#define SVM_EXIT_READ_CR0               0x0
#define SVM_EXIT_READ_CR1               0x1
#define SVM_EXIT_READ_CR2               0x2
#define SVM_EXIT_READ_CR3               0x3
#define SVM_EXIT_READ_CR4               0x4
#define SVM_EXIT_READ_CR5               0x5
#define SVM_EXIT_READ_CR6               0x6
#define SVM_EXIT_READ_CR7               0x7
#define SVM_EXIT_READ_CR8               0x8
#define SVM_EXIT_READ_CR9               0x9
#define SVM_EXIT_READ_CR10              0xA
#define SVM_EXIT_READ_CR11              0xB
#define SVM_EXIT_READ_CR12              0xC
#define SVM_EXIT_READ_CR13              0xD
#define SVM_EXIT_READ_CR14              0xE
#define SVM_EXIT_READ_CR15              0xF
/** Writes to CR0-CR15. */
#define SVM_EXIT_WRITE_CR0              0x10
#define SVM_EXIT_WRITE_CR1              0x11
#define SVM_EXIT_WRITE_CR2              0x12
#define SVM_EXIT_WRITE_CR3              0x13
#define SVM_EXIT_WRITE_CR4              0x14
#define SVM_EXIT_WRITE_CR5              0x15
#define SVM_EXIT_WRITE_CR6              0x16
#define SVM_EXIT_WRITE_CR7              0x17
#define SVM_EXIT_WRITE_CR8              0x18
#define SVM_EXIT_WRITE_CR9              0x19
#define SVM_EXIT_WRITE_CR10             0x1A
#define SVM_EXIT_WRITE_CR11             0x1B
#define SVM_EXIT_WRITE_CR12             0x1C
#define SVM_EXIT_WRITE_CR13             0x1D
#define SVM_EXIT_WRITE_CR14             0x1E
#define SVM_EXIT_WRITE_CR15             0x1F
/** Read from DR0-DR15. */
#define SVM_EXIT_READ_DR0               0x20
#define SVM_EXIT_READ_DR1               0x21
#define SVM_EXIT_READ_DR2               0x22
#define SVM_EXIT_READ_DR3               0x23
#define SVM_EXIT_READ_DR4               0x24
#define SVM_EXIT_READ_DR5               0x25
#define SVM_EXIT_READ_DR6               0x26
#define SVM_EXIT_READ_DR7               0x27
#define SVM_EXIT_READ_DR8               0x28
#define SVM_EXIT_READ_DR9               0x29
#define SVM_EXIT_READ_DR10              0x2A
#define SVM_EXIT_READ_DR11              0x2B
#define SVM_EXIT_READ_DR12              0x2C
#define SVM_EXIT_READ_DR13              0x2D
#define SVM_EXIT_READ_DR14              0x2E
#define SVM_EXIT_READ_DR15              0x2F
/** Writes to DR0-DR15. */
#define SVM_EXIT_WRITE_DR0              0x30
#define SVM_EXIT_WRITE_DR1              0x31
#define SVM_EXIT_WRITE_DR2              0x32
#define SVM_EXIT_WRITE_DR3              0x33
#define SVM_EXIT_WRITE_DR4              0x34
#define SVM_EXIT_WRITE_DR5              0x35
#define SVM_EXIT_WRITE_DR6              0x36
#define SVM_EXIT_WRITE_DR7              0x37
#define SVM_EXIT_WRITE_DR8              0x38
#define SVM_EXIT_WRITE_DR9              0x39
#define SVM_EXIT_WRITE_DR10             0x3A
#define SVM_EXIT_WRITE_DR11             0x3B
#define SVM_EXIT_WRITE_DR12             0x3C
#define SVM_EXIT_WRITE_DR13             0x3D
#define SVM_EXIT_WRITE_DR14             0x3E
#define SVM_EXIT_WRITE_DR15             0x3F
/* Exception 0-31. */
#define SVM_EXIT_EXCEPTION_0            0x40
#define SVM_EXIT_EXCEPTION_1            0x41
#define SVM_EXIT_EXCEPTION_2            0x42
#define SVM_EXIT_EXCEPTION_3            0x43
#define SVM_EXIT_EXCEPTION_4            0x44
#define SVM_EXIT_EXCEPTION_5            0x45
#define SVM_EXIT_EXCEPTION_6            0x46
#define SVM_EXIT_EXCEPTION_7            0x47
#define SVM_EXIT_EXCEPTION_8            0x48
#define SVM_EXIT_EXCEPTION_9            0x49
#define SVM_EXIT_EXCEPTION_A            0x4A
#define SVM_EXIT_EXCEPTION_B            0x4B
#define SVM_EXIT_EXCEPTION_C            0x4C
#define SVM_EXIT_EXCEPTION_D            0x4D
#define SVM_EXIT_EXCEPTION_E            0x4E
#define SVM_EXIT_EXCEPTION_F            0x4F
#define SVM_EXIT_EXCEPTION_10           0x50
#define SVM_EXIT_EXCEPTION_11           0x51
#define SVM_EXIT_EXCEPTION_12           0x52
#define SVM_EXIT_EXCEPTION_13           0x53
#define SVM_EXIT_EXCEPTION_14           0x54
#define SVM_EXIT_EXCEPTION_15           0x55
#define SVM_EXIT_EXCEPTION_16           0x56
#define SVM_EXIT_EXCEPTION_17           0x57
#define SVM_EXIT_EXCEPTION_18           0x58
#define SVM_EXIT_EXCEPTION_19           0x59
#define SVM_EXIT_EXCEPTION_1A           0x5A
#define SVM_EXIT_EXCEPTION_1B           0x5B
#define SVM_EXIT_EXCEPTION_1C           0x5C
#define SVM_EXIT_EXCEPTION_1D           0x5D
#define SVM_EXIT_EXCEPTION_1E           0x5E
#define SVM_EXIT_EXCEPTION_1F           0x5F
/** Physical maskable interrupt. */
#define SVM_EXIT_INTR                   0x60
/** Non-maskable interrupt. */
#define SVM_EXIT_NMI                    0x61
/** System Management interrupt. */
#define SVM_EXIT_SMI                    0x62
/** Physical INIT signal. */
#define SVM_EXIT_INIT                   0x63
/** Virtual interrupt. */
#define SVM_EXIT_VINTR                  0x64
/** Write to CR0 that changed any bits other than CR0.TS or CR0.MP. */
#define SVM_EXIT_CR0_SEL_WRITE          0x65
/** IDTR read. */
#define SVM_EXIT_IDTR_READ              0x66
/** GDTR read. */
#define SVM_EXIT_GDTR_READ              0x67
/** LDTR read. */
#define SVM_EXIT_LDTR_READ              0x68
/** TR read. */
#define SVM_EXIT_TR_READ                0x69
/** IDTR write. */
#define SVM_EXIT_IDTR_WRITE             0x6A
/** GDTR write. */
#define SVM_EXIT_GDTR_WRITE             0x6B
/** LDTR write. */
#define SVM_EXIT_LDTR_WRITE             0x6C
/** TR write. */
#define SVM_EXIT_TR_WRITE               0x6D
/** RDTSC instruction. */
#define SVM_EXIT_RDTSC                  0x6E
/** RDPMC instruction. */
#define SVM_EXIT_RDPMC                  0x6F
/** PUSHF instruction. */
#define SVM_EXIT_PUSHF                  0x70
/** POPF instruction. */
#define SVM_EXIT_POPF                   0x71
/** CPUID instruction. */
#define SVM_EXIT_CPUID                  0x72
/** RSM instruction. */
#define SVM_EXIT_RSM                    0x73
/** IRET instruction. */
#define SVM_EXIT_IRET                   0x74
/** software interrupt (INTn instructions). */
#define SVM_EXIT_SWINT                  0x75
/** INVD instruction. */
#define SVM_EXIT_INVD                   0x76
/** PAUSE instruction. */
#define SVM_EXIT_PAUSE                  0x77
/** HLT instruction. */
#define SVM_EXIT_HLT                    0x78
/** INVLPG instructions. */
#define SVM_EXIT_INVLPG                 0x79
/** INVLPGA instruction. */
#define SVM_EXIT_INVLPGA                0x7A
/** IN or OUT accessing protected port (the EXITINFO1 field provides more information). */
#define SVM_EXIT_IOIO                   0x7B
/** RDMSR or WRMSR access to protected MSR. */
#define SVM_EXIT_MSR                    0x7C
/** task switch. */
#define SVM_EXIT_TASK_SWITCH            0x7D
/** FP legacy handling enabled, and processor is frozen in an x87/mmx instruction waiting for an interrupt. */
#define SVM_EXIT_FERR_FREEZE            0x7E
/** Shutdown. */
#define SVM_EXIT_SHUTDOWN               0x7F
/** VMRUN instruction. */
#define SVM_EXIT_VMRUN                  0x80
/** VMMCALL instruction. */
#define SVM_EXIT_VMMCALL                0x81
/** VMLOAD instruction. */
#define SVM_EXIT_VMLOAD                 0x82
/** VMSAVE instruction. */
#define SVM_EXIT_VMSAVE                 0x83
/** STGI instruction. */
#define SVM_EXIT_STGI                   0x84
/** CLGI instruction. */
#define SVM_EXIT_CLGI                   0x85
/** SKINIT instruction. */
#define SVM_EXIT_SKINIT                 0x86
/** RDTSCP instruction. */
#define SVM_EXIT_RDTSCP                 0x87
/** ICEBP instruction. */
#define SVM_EXIT_ICEBP                  0x88
/** WBINVD instruction. */
#define SVM_INVD                        0x89
/** Nested paging: host-level page fault occurred (EXITINFO1 contains fault errorcode; EXITINFO2 contains the guest physical address causing the fault.). */
#define SVM_EXIT_NPF                    0x400

/** @} */


/** @name SVM_VMCB.ctrl.u32InterceptCtrl1
 * @{
 */
/* 0 Intercept INTR (physical maskable interrupt) */
#define SVM_CTRL1_INTERCEPT_INTR              RT_BIT(0)
/* 1 Intercept NMI */
#define SVM_CTRL1_INTERCEPT_NMI               RT_BIT(1)
/* 2 Intercept SMI */
#define SVM_CTRL1_INTERCEPT_SMI               RT_BIT(2)
/* 3 Intercept INIT */
#define SVM_CTRL1_INTERCEPT_INIT              RT_BIT(3)
/* 4 Intercept VINTR (virtual maskable interrupt) */
#define SVM_CTRL1_INTERCEPT_VINTR             RT_BIT(4)
/* 5 Intercept CR0 writes that change bits other than CR0.TS or CR0.MP */
#define SVM_CTRL1_INTERCEPT_CR0               RT_BIT(5)
/* 6 Intercept reads of IDTR */
#define SVM_CTRL1_INTERCEPT_IDTR_READS        RT_BIT(6)
/* 7 Intercept reads of GDTR */
#define SVM_CTRL1_INTERCEPT_GDTR_READS        RT_BIT(7)
/* 8 Intercept reads of LDTR */
#define SVM_CTRL1_INTERCEPT_LDTR_READS        RT_BIT(8)
/* 9 Intercept reads of TR */
#define SVM_CTRL1_INTERCEPT_TR_READS          RT_BIT(9)
/* 10 Intercept writes of IDTR */
#define SVM_CTRL1_INTERCEPT_IDTR_WRITES       RT_BIT(10)
/* 11 Intercept writes of GDTR */
#define SVM_CTRL1_INTERCEPT_GDTR_WRITES       RT_BIT(11)
/* 12 Intercept writes of LDTR */
#define SVM_CTRL1_INTERCEPT_LDTR_WRITES       RT_BIT(12)
/* 13 Intercept writes of TR */
#define SVM_CTRL1_INTERCEPT_TR_WRITES         RT_BIT(13)
/* 14 Intercept RDTSC instruction */
#define SVM_CTRL1_INTERCEPT_RDTSC             RT_BIT(14)
/* 15 Intercept RDPMC instruction */
#define SVM_CTRL1_INTERCEPT_RDPMC             RT_BIT(15)
/* 16 Intercept PUSHF instruction */
#define SVM_CTRL1_INTERCEPT_PUSHF             RT_BIT(16)
/* 17 Intercept POPF instruction */
#define SVM_CTRL1_INTERCEPT_POPF              RT_BIT(17)
/* 18 Intercept CPUID instruction */
#define SVM_CTRL1_INTERCEPT_CPUID             RT_BIT(18)
/* 19 Intercept RSM instruction */
#define SVM_CTRL1_INTERCEPT_RSM               RT_BIT(19)
/* 20 Intercept IRET instruction */
#define SVM_CTRL1_INTERCEPT_IRET              RT_BIT(20)
/* 21 Intercept INTn instruction */
#define SVM_CTRL1_INTERCEPT_INTN              RT_BIT(21)
/* 22 Intercept INVD instruction */
#define SVM_CTRL1_INTERCEPT_INVD              RT_BIT(22)
/* 23 Intercept PAUSE instruction */
#define SVM_CTRL1_INTERCEPT_PAUSE             RT_BIT(23)
/* 24 Intercept HLT instruction */
#define SVM_CTRL1_INTERCEPT_HLT               RT_BIT(24)
/* 25 Intercept INVLPG instruction */
#define SVM_CTRL1_INTERCEPT_INVLPG            RT_BIT(25)
/* 26 Intercept INVLPGA instruction */
#define SVM_CTRL1_INTERCEPT_INVLPGA           RT_BIT(26)
/* 27 IOIO_PROT Intercept IN/OUT accesses to selected ports. */
#define SVM_CTRL1_INTERCEPT_INOUT_BITMAP      RT_BIT(27)
/* 28 MSR_PROT Intercept RDMSR or WRMSR accesses to selected MSRs. */
#define SVM_CTRL1_INTERCEPT_MSR_SHADOW        RT_BIT(28)
/* 29 Intercept task switches. */
#define SVM_CTRL1_INTERCEPT_TASK_SWITCH       RT_BIT(29)
/* 30 FERR_FREEZE: intercept processor "freezing" during legacy FERR handling. */
#define SVM_CTRL1_INTERCEPT_FERR_FREEZE       RT_BIT(30)
/* 31 Intercept shutdown events. */
#define SVM_CTRL1_INTERCEPT_SHUTDOWN          RT_BIT(31)
/** @} */


/** @name SVM_VMCB.ctrl.u32InterceptCtrl2
 * @{
 */
/* 0 Intercept VMRUN instruction */
#define SVM_CTRL2_INTERCEPT_VMRUN             RT_BIT(0)
/* 1 Intercept VMMCALL instruction */
#define SVM_CTRL2_INTERCEPT_VMMCALL           RT_BIT(1)
/* 2 Intercept VMLOAD instruction */
#define SVM_CTRL2_INTERCEPT_VMLOAD            RT_BIT(2)
/* 3 Intercept VMSAVE instruction */
#define SVM_CTRL2_INTERCEPT_VMSAVE            RT_BIT(3)
/* 4 Intercept STGI instruction */
#define SVM_CTRL2_INTERCEPT_STGI              RT_BIT(4)
/* 5 Intercept CLGI instruction */
#define SVM_CTRL2_INTERCEPT_CLGI              RT_BIT(5)
/* 6 Intercept SKINIT instruction */
#define SVM_CTRL2_INTERCEPT_SKINIT            RT_BIT(6)
/* 7 Intercept RDTSCP instruction */
#define SVM_CTRL2_INTERCEPT_RDTSCP            RT_BIT(7)
/* 8 Intercept ICEBP instruction */
#define SVM_CTRL2_INTERCEPT_ICEBP             RT_BIT(8)
/* 9 Intercept WBINVD instruction */
#define SVM_CTRL2_INTERCEPT_WBINVD            RT_BIT(9)
/** @} */

/** @name SVM_VMCB.ctrl.u64NestedPaging
 * @{
 */
#define SVM_NESTED_PAGING_ENABLE                RT_BIT(0)
/** @} */

/** @name SVM_VMCB.ctrl.u64IntShadow
 * @{
 */
#define SVM_INTERRUPT_SHADOW_ACTIVE             RT_BIT(0)
/** @} */


/** @name SVM_INTCTRL.u3Type
 * @{
 */
/** External or virtual interrupt. */
#define SVM_EVENT_EXTERNAL_IRQ                  0
/** Non-maskable interrupt. */
#define SVM_EVENT_NMI                           1
/** Exception; fault or trap. */
#define SVM_EVENT_EXCEPTION                     3
/** Software interrupt. */
#define SVM_EVENT_SOFTWARE_INT                  4
/** @} */




/**
 * SVM Selector type; includes hidden parts
 */
#pragma pack(1)
typedef struct
{
    uint16_t    u16Sel;
    uint16_t    u16Attr;
    uint32_t    u32Limit;
    uint64_t    u64Base;        /* Only lower 32 bits are implemented for CS, DS, ES & SS. */
} SVMSEL;
#pragma pack()

/**
 * SVM GDTR/IDTR type
 */
#pragma pack(1)
typedef struct
{
    uint16_t    u16Reserved1;
    uint16_t    u16Reserved2;
    uint32_t    u32Limit;       /* Only lower 16 bits are implemented. */
    uint64_t    u64Base;
} SVMGDTR;
#pragma pack()

typedef SVMGDTR SVMIDTR;

/**
 * SVM Event injection structure
 */
#pragma pack(1)
typedef union
{
    struct
    {
        uint32_t    u8Vector            : 8;
        uint32_t    u3Type              : 3;
        uint32_t    u1ErrorCodeValid    : 1;
        uint32_t    u19Reserved         : 19;
        uint32_t    u1Valid             : 1;
        uint32_t    u32ErrorCode        : 32;    
    } n;
    uint64_t    au64[1];
} SVM_EVENT;
#pragma pack()


/**
 * SVM Interrupt control structure
 */
#pragma pack(1)
typedef union
{
    struct
    {
        uint32_t    u8VTPR              : 8;
        uint32_t    u1VIrqValid         : 1;
        uint32_t    u7Reserved          : 7;
        uint32_t    u4VIrqPriority      : 4;
        uint32_t    u1IgnoreTPR         : 1;
        uint32_t    u3Reserved          : 3;
        uint32_t    u1VIrqMasking       : 1;
        uint32_t    u7Reserved2         : 7;
        uint32_t    u8VIrqVector        : 8;
        uint32_t    u24Reserved         : 24;
    } n;
    uint64_t    au64[1];
} SVM_INTCTRL;
#pragma pack()


/**
 * SVM TLB control structure
 */
#pragma pack(1)
typedef union
{
    struct
    {
        uint32_t    u32ASID             : 32;
        uint32_t    u1TLBFlush          : 1;
        uint32_t    u7Reserved          : 7;
        uint32_t    u24Reserved         : 24;
    } n;
    uint64_t    au64[1];
} SVM_TLBCTRL;
#pragma pack()


/**
 * SVM IOIO exit structure
 */
#pragma pack(1)
typedef union
{
    struct
    {
        uint32_t    u1Type              : 1;        /* 0 = out, 1 = in */
        uint32_t    u1Reserved          : 1;
        uint32_t    u1STR               : 1;
        uint32_t    u1REP               : 1;
        uint32_t    u1OP8               : 1;
        uint32_t    u1OP16              : 1;
        uint32_t    u1OP32              : 1;
        uint32_t    u1ADDR16            : 1;
        uint32_t    u1ADDR32            : 1;
        uint32_t    u1ADDR64            : 1;
        uint32_t    u6Reserved          : 6;
        uint32_t    u16Port             : 16;
    } n;
    uint32_t    au32[1];
} SVM_IOIO_EXIT;
#pragma pack()


/**
 * SVM VM Control Block. (VMCB)
 */
#pragma pack(1)
typedef struct _SVM_VMCB
{
    /** Control Area. */
    struct
    {
        /** Offset 0x00 - Intercept reads of CR0-15. */
        uint16_t    u16InterceptRdCRx;
        /** Offset 0x02 - Intercept writes to CR0-15. */
        uint16_t    u16InterceptWrCRx;
        /** Offset 0x04 - Intercept reads of DR0-15. */
        uint16_t    u16InterceptRdDRx;
        /** Offset 0x06 - Intercept writes to DR0-15. */
        uint16_t    u16InterceptWrDRx;
        /** Offset 0x08 - Intercept exception vectors 0-31. */
        uint32_t    u32InterceptException;
        /** Offset 0x0C - Intercept control field 1. */
        uint32_t    u32InterceptCtrl1;
        /** Offset 0x0C - Intercept control field 2. */
        uint32_t    u32InterceptCtrl2;
        /** Offset 0x14-0x3F - Reserved. */
        uint8_t     u8Reserved[0x40-0x14];
        /** Offset 0x40 - Physical address of IOPM. */
        uint64_t    u64IOPMPhysAddr;
        /** Offset 0x48 - Physical address of MSRPM. */
        uint64_t    u64MSRPMPhysAddr;
        /** Offset 0x50 - TSC Offset. */
        uint64_t    u64TSCOffset;
        /** Offset 0x58 - TLB control field. */
        SVM_TLBCTRL TLBCtrl;
        /** Offset 0x60 - Interrupt control field. */
        SVM_INTCTRL IntCtrl;
        /** Offset 0x68 - Interrupt shadow. */
        uint64_t    u64IntShadow;
        /** Offset 0x70 - Exit code. */
        uint64_t    u64ExitCode;
        /** Offset 0x78 - Exit info 1. */
        uint64_t    u64ExitInfo1;
        /** Offset 0x80 - Exit info 2. */
        uint64_t    u64ExitInfo2;
        /** Offset 0x88 - Exit Interrupt info. */
        SVM_EVENT   ExitIntInfo;
        /** Offset 0x90 - Nested Paging. */
        uint64_t    u64NestedPaging;
        /** Offset 0x98-0xA7 - Reserved. */
        uint8_t     u8Reserved2[0xA8-0x98];
        /** Offset 0xA8 - Event injection. */
        SVM_EVENT   EventInject;
        /** Offset 0xB0 - Host CR3 for nested paging. */
        uint64_t    u64HostCR3;
        /** Offset 0xB8 - LBR Virtualization. */
        uint64_t    u64LBRVirt;
    } ctrl;

    /** Offset 0xC0-0x3FF - Reserved. */
    uint8_t     u8Reserved3[0x400-0xC0];

    /* State Save Area. Starts at offset 0x400. */
    struct
    {
        /** Offset 0x400 - Guest ES register + hidden parts. */
        SVMSEL      ES;
        /** Offset 0x410 - Guest CS register + hidden parts. */
        SVMSEL      CS;
        /** Offset 0x420 - Guest SS register + hidden parts. */
        SVMSEL      SS;
        /** Offset 0x430 - Guest DS register + hidden parts. */
        SVMSEL      DS;
        /** Offset 0x440 - Guest FS register + hidden parts. */
        SVMSEL      FS;
        /** Offset 0x450 - Guest GS register + hidden parts. */
        SVMSEL      GS;
        /** Offset 0x460 - Guest GDTR register. */
        SVMGDTR     GDTR;
        /** Offset 0x470 - Guest LDTR register + hidden parts. */
        SVMSEL      LDTR;
        /** Offset 0x480 - Guest IDTR register. */
        SVMIDTR     IDTR;
        /** Offset 0x490 - Guest TR register + hidden parts. */
        SVMSEL      TR;
        /** Offset 0x4A0-0x4CA - Reserved. */
        uint8_t     u8Reserved4[0x4CB-0x4A0];
        /** Offset 0x4CB - CPL. */
        uint8_t     u8CPL;
        /** Offset 0x4CC-0x4CF - Reserved. */
        uint8_t     u8Reserved5[0x4D0-0x4CC];
        /** Offset 0x4D0 - EFER. */
        uint64_t    u64EFER;
        /** Offset 0x4D8-0x547 - Reserved. */
        uint8_t     u8Reserved6[0x548-0x4D8];
        /** Offset 0x548 - CR4. */
        uint64_t    u64CR4;
        /** Offset 0x550 - CR3. */
        uint64_t    u64CR3;
        /** Offset 0x558 - CR0. */
        uint64_t    u64CR0;
        /** Offset 0x560 - DR7. */
        uint64_t    u64DR7;
        /** Offset 0x568 - DR6. */
        uint64_t    u64DR6;
        /** Offset 0x570 - RFLAGS. */
        uint64_t    u64RFlags;
        /** Offset 0x578 - RIP. */
        uint64_t    u64RIP;
        /** Offset 0x580-0x5D7 - Reserved. */
        uint8_t     u8Reserved7[0x5D8-0x580];
        /** Offset 0x5D8 - RSP. */
        uint64_t    u64RSP;
        /** Offset 0x5E0-0x5F7 - Reserved. */
        uint8_t     u8Reserved8[0x5F8-0x5E0];
        /** Offset 0x5F8 - RAX. */
        uint64_t    u64RAX;
        /** Offset 0x600 - STAR. */
        uint64_t    u64STAR;
        /** Offset 0x608 - LSTAR. */
        uint64_t    u64LSTAR;
        /** Offset 0x610 - CSTAR. */
        uint64_t    u64CSTAR;
        /** Offset 0x618 - SFMASK. */
        uint64_t    u64SFMASK;
        /** Offset 0x620 - KernelGSBase. */
        uint64_t    u64KernelGSBase;
        /** Offset 0x628 - SYSENTER_CS. */
        uint64_t    u64SysEnterCS;
        /** Offset 0x630 - SYSENTER_ESP. */
        uint64_t    u64SysEnterESP;
        /** Offset 0x638 - SYSENTER_EIP. */
        uint64_t    u64SysEnterEIP;
        /** Offset 0x640 - CR2. */
        uint64_t    u64CR2;
        /** Offset 0x648-0x667 - Reserved. */
        uint8_t     u8Reserved9[0x668-0x648];
        /** Offset 0x668 - G_PAT. */
        uint64_t    u64GPAT;
        /** Offset 0x670 - DBGCTL. */
        uint64_t    u64DBGCTL;
        /** Offset 0x678 - BR_FROM. */
        uint64_t    u64BR_FROM;
        /** Offset 0x680 - BR_TO. */
        uint64_t    u64BR_TO;
        /** Offset 0x688 - LASTEXCPFROM. */
        uint64_t    u64LASTEXCPFROM;
        /** Offset 0x690 - LASTEXCPTO. */
        uint64_t    u64LASTEXCPTO;
    } guest;

    /** Offset 0x698-0xFFF- Reserved. */
    uint8_t     u8Reserved10[0x1000-0x698];
} SVM_VMCB;
#pragma pack()


/**
 * Prepares for and executes VMRUN
 *
 * @returns VBox status code
 * @param   pVMCBHostPhys  Physical address of host VMCB
 * @param   pVMCBPhys      Physical address of the VMCB
 * @param   pCtx           Guest context
 */
DECLASM(int) SVMVMRun(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx);


/**
 * Executes INVLPGA
 *
 * @param   pPageGC     Virtual page to invalidate
 * @param   uASID       Tagged TLB id
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) SVMInvlpgA(RTGCPTR pPageGC, uint32_t uASID);
#else
DECLINLINE(void) SVMInvlpgA(RTGCPTR pPageGC, uint32_t uASID)
{
# if RT_INLINE_ASM_GNU_STYLE
    AssertFailed();
# else
    __asm
    {
#   ifdef RT_ARCH_AMD64
        mov     rax, pPageGC
#   else
        mov     eax, pPageGC
#   endif
        push    ecx
        mov     ecx, uASID
        _emit   0x0F
        _emit   0x01
        _emit   0xDF   /* invlpga rAX, ECX */

        pop     ecx
    }
# endif
}
#endif



/** @} */

#endif

