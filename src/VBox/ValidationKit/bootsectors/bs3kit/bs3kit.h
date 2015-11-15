/* $Id$ */
/** @file
 * BS3Kit - structures, symbols, macros and stuff.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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

#ifndef ___bs3kit_h
#define ___bs3kit_h

#ifndef DOXYGEN_RUNNING
# define IN_RING0
#endif
#include <iprt/cdefs.h>
#include <iprt/types.h>
#ifndef DOXYGEN_RUNNING
# undef  IN_RING0
#endif
#include <iprt/x86.h>


/** @defgroup grp_bs3kit     BS3Kit
 * @{ */


/** @name BS3_ADDR_XXX - Static Memory Allocation
 * @{ */
/** The flat load address for the code after the bootsector. */
#define BS3_ADDR_LOAD           0x10000
/** Where we save the boot registers during init.
 * Located right before the code. */
#define BS3_ADDR_REG_SAVE       (BS3_ADDR_LOAD - sizeof(BS3REGS) - 8)
/** Where the stack starts (initial RSP value).
 * Located 16 bytes (assumed by boot sector) before the saved registers.
 * SS.BASE=0. The size is a little short of 32KB  */
#define BS3_ADDR_STACK          (BS3_ADDR_REG_SAVE - 16)
/** The ring-0 stack (8KB) for ring transitions. */
#define BS3_ADDR_STACK_R0       0x06000
/** The ring-1 stack (8KB) for ring transitions. */
#define BS3_ADDR_STACK_R1       0x04000
/** The ring-2 stack (8KB) for ring transitions. */
#define BS3_ADDR_STACK_R2       0x02000
/** IST1 ring-0 stack for long mode (4KB), used for double faults elsewhere. */
#define BS3_ADDR_STACK_R0_IST1  0x09000
/** IST2 ring-0 stack for long mode (3KB), used for spare 0 stack elsewhere. */
#define BS3_ADDR_STACK_R0_IST2  0x08000
/** IST3 ring-0 stack for long mode (1KB). */
#define BS3_ADDR_STACK_R0_IST3  0x07400
/** IST4 ring-0 stack for long mode (1KB), used for spare 1 stack elsewhere. */
#define BS3_ADDR_STACK_R0_IST4  0x07000
/** IST5 ring-0 stack for long mode (1KB). */
#define BS3_ADDR_STACK_R0_IST5  0x06c00
/** IST6 ring-0 stack for long mode (1KB). */
#define BS3_ADDR_STACK_R0_IST6  0x06800
/** IST7 ring-0 stack for long mode (1KB). */
#define BS3_ADDR_STACK_R0_IST7  0x06400

/** The base address of the BS3TEXT16 segment (same as BS3_LOAD_ADDR).
 * @sa BS3_SEL_TEXT16 */
#define BS3_ADDR_BS3TEXT16      0x10000
/** The base address of the BS3SYSTEM16 segment.
 * @sa BS3_SEL_SYSTEM16 */
#define BS3_ADDR_BS3SYSTEM16    0x20000
/** The base address of the BS3DATA16 segment.
 * @sa BS3_SEL_DATA16 */
#define BS3_ADDR_BS3DATA16      0x27000
/** @} */

/** @name BS3_SEL_XXX - GDT selector assignments.
 *
 * The real mode segment numbers for BS16TEXT, BS16DATA and BS16SYSTEM are
 * present in the GDT, this allows the 16-bit C/C++ and assembly code to
 * continue using the real mode segment values in ring-0 protected mode.
 *
 * The three segments have fixed locations:
 * | segment     | flat address | real mode segment |
 * | ----------- | ------------ | ----------------- |
 * | BS3TEXT16   |   0x00010000 |             1000h |
 * | BS3SYSTEM16 |   0x00020000 |             2000h |
 * | BS3DATA16   |   0x00027000 |             2700h |
 *
 * This means that we've got a lot of GDT space to play around with.
 *
 * @{ */
#define BS3_SEL_LDT                 0x0010 /**< The LDT selector for Bs3Ldt. */
#define BS3_SEL_TSS16               0x0020 /**< The 16-bit TSS selector. */
#define BS3_SEL_TSS16_DF            0x0028 /**< The 16-bit TSS selector for double faults. */
#define BS3_SEL_TSS16_SPARE0        0x0030 /**< The 16-bit TSS selector for testing. */
#define BS3_SEL_TSS16_SPARE1        0x0038 /**< The 16-bit TSS selector for testing. */
#define BS3_SEL_TSS32               0x0040 /**< The 32-bit TSS selector. */
#define BS3_SEL_TSS32_DF            0x0048 /**< The 32-bit TSS selector for double faults. */
#define BS3_SEL_TSS32_SPARE0        0x0050 /**< The 32-bit TSS selector for testing. */
#define BS3_SEL_TSS32_SPARE1        0x0058 /**< The 32-bit TSS selector for testing. */
#define BS3_SEL_TSS32_IOBP_IRB      0x0060 /**< The 32-bit TSS selector with I/O permission and interrupt redirection bitmaps. */
#define BS3_SEL_TSS32_IRB           0x0068 /**< The 32-bit TSS selector with only interrupt redirection bitmap (IOPB stripped by limit). */
#define BS3_SEL_TSS64               0x0070 /**< The 64-bit TSS selector. */
#define BS3_SEL_TSS64_SPARE0        0x0080 /**< The 64-bit TSS selector. */
#define BS3_SEL_TSS64_SPARE1        0x0090 /**< The 64-bit TSS selector. */
#define BS3_SEL_TSS64_IOBP          0x00a0 /**< The 64-bit TSS selector. */

#define BS3_SEL_VMMDEV_MMIO16       0x00f8 /**< Selector for accessing the VMMDev MMIO segment at 0100000h from 16-bit code. */

#define BS3_SEL_R0_FIRST            0x0100 /**< The first selector in the ring-0 block. */
#define BS3_SEL_R0_CS16             0x0100 /**< ring-0: 16-bit code selector,  base 0x10000. */
#define BS3_SEL_R0_DS16             0x0108 /**< ring-0: 16-bit data selector,  base 0x23000. */
#define BS3_SEL_R0_SS16             0x0110 /**< ring-0: 16-bit stack selector, base 0x00000. */
#define BS3_SEL_R0_CS32             0x0118 /**< ring-0: 32-bit flat code selector. */
#define BS3_SEL_R0_DS32             0x0120 /**< ring-0: 32-bit flat data selector. */
#define BS3_SEL_R0_SS32             0x0128 /**< ring-0: 32-bit flat stack selector. */
#define BS3_SEL_R0_CS64             0x0130 /**< ring-0: 64-bit flat code selector. */
#define BS3_SEL_R0_DS64             0x0138 /**< ring-0: 64-bit flat data & stack selector. */
#define BS3_SEL_R0_CS16_EO          0x0140 /**< ring-0: 16-bit execute-only code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R0_CS16_CNF         0x0148 /**< ring-0: 16-bit conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R0_CS16_CNF_EO      0x0150 /**< ring-0: 16-bit execute-only conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R0_CS32_EO          0x0158 /**< ring-0: 32-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R0_CS32_CNF         0x0160 /**< ring-0: 32-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R0_CS32_CNF_EO      0x0168 /**< ring-0: 32-bit execute-only conforming code selector, not accessed, flat. */
#define BS3_SEL_R0_CS64_EO          0x0170 /**< ring-0: 64-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R0_CS64_CNF         0x0178 /**< ring-0: 64-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R0_CS64_CNF_EO      0x0180 /**< ring-0: 64-bit execute-only conforming code selector, not accessed, flat. */

#define BS3_SEL_R1_FIRST            0x0200 /**< The first selector in the ring-1 block. */
#define BS3_SEL_R1_CS16             0x0200 /**< ring-1: 16-bit code selector,  base 0x10000. */
#define BS3_SEL_R1_DS16             0x0208 /**< ring-1: 16-bit data selector,  base 0x23000. */
#define BS3_SEL_R1_SS16             0x0210 /**< ring-1: 16-bit stack selector, base 0x00000. */
#define BS3_SEL_R1_CS32             0x0218 /**< ring-1: 32-bit flat code selector. */
#define BS3_SEL_R1_DS32             0x0220 /**< ring-1: 32-bit flat data selector. */
#define BS3_SEL_R1_SS32             0x0228 /**< ring-1: 32-bit flat stack selector. */
#define BS3_SEL_R1_CS64             0x0230 /**< ring-1: 64-bit flat code selector. */
#define BS3_SEL_R1_DS64             0x0238 /**< ring-1: 64-bit flat data & stack selector. */
#define BS3_SEL_R1_CS16_EO          0x0240 /**< ring-1: 16-bit execute-only code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R1_CS16_CNF         0x0248 /**< ring-1: 16-bit conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R1_CS16_CNF_EO      0x0250 /**< ring-1: 16-bit execute-only conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R1_CS32_EO          0x0258 /**< ring-1: 32-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R1_CS32_CNF         0x0260 /**< ring-1: 32-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R1_CS32_CNF_EO      0x0268 /**< ring-1: 32-bit execute-only conforming code selector, not accessed, flat. */
#define BS3_SEL_R1_CS64_EO          0x0270 /**< ring-1: 64-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R1_CS64_CNF         0x0278 /**< ring-1: 64-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R1_CS64_CNF_EO      0x0280 /**< ring-1: 64-bit execute-only conforming code selector, not accessed, flat. */

#define BS3_SEL_R2_FIRST            0x0300 /**< The first selector in the ring-2 block. */
#define BS3_SEL_R2_CS16             0x0300 /**< ring-2: 16-bit code selector,  base 0x10000. */
#define BS3_SEL_R2_DS16             0x0308 /**< ring-2: 16-bit data selector,  base 0x23000. */
#define BS3_SEL_R2_SS16             0x0310 /**< ring-2: 16-bit stack selector, base 0x00000. */
#define BS3_SEL_R2_CS32             0x0318 /**< ring-2: 32-bit flat code selector. */
#define BS3_SEL_R2_DS32             0x0320 /**< ring-2: 32-bit flat data selector. */
#define BS3_SEL_R2_SS32             0x0328 /**< ring-2: 32-bit flat stack selector. */
#define BS3_SEL_R2_CS64             0x0330 /**< ring-2: 64-bit flat code selector. */
#define BS3_SEL_R2_DS64             0x0338 /**< ring-2: 64-bit flat data & stack selector. */
#define BS3_SEL_R2_CS16_EO          0x0340 /**< ring-2: 16-bit execute-only code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R2_CS16_CNF         0x0348 /**< ring-2: 16-bit conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R2_CS16_CNF_EO      0x0350 /**< ring-2: 16-bit execute-only conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R2_CS32_EO          0x0358 /**< ring-2: 32-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R2_CS32_CNF         0x0360 /**< ring-2: 32-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R2_CS32_CNF_EO      0x0368 /**< ring-2: 32-bit execute-only conforming code selector, not accessed, flat. */
#define BS3_SEL_R2_CS64_EO          0x0370 /**< ring-2: 64-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R2_CS64_CNF         0x0378 /**< ring-2: 64-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R2_CS64_CNF_EO      0x0380 /**< ring-2: 64-bit execute-only conforming code selector, not accessed, flat. */

#define BS3_SEL_R3_FIRST            0x0400 /**< The first selector in the ring-3 block. */
#define BS3_SEL_R3_CS16             0x0400 /**< ring-3: 16-bit code selector,  base 0x10000. */
#define BS3_SEL_R3_DS16             0x0408 /**< ring-3: 16-bit data selector,  base 0x23000. */
#define BS3_SEL_R3_SS16             0x0410 /**< ring-3: 16-bit stack selector, base 0x00000. */
#define BS3_SEL_R3_CS32             0x0418 /**< ring-3: 32-bit flat code selector. */
#define BS3_SEL_R3_DS32             0x0420 /**< ring-3: 32-bit flat data selector. */
#define BS3_SEL_R3_SS32             0x0428 /**< ring-3: 32-bit flat stack selector. */
#define BS3_SEL_R3_CS64             0x0430 /**< ring-3: 64-bit flat code selector. */
#define BS3_SEL_R3_DS64             0x0438 /**< ring-3: 64-bit flat data & stack selector. */
#define BS3_SEL_R3_CS16_EO          0x0440 /**< ring-3: 16-bit execute-only code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R3_CS16_CNF         0x0448 /**< ring-3: 16-bit conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R3_CS16_CNF_EO      0x0450 /**< ring-3: 16-bit execute-only conforming code selector, not accessed, 0xfffe limit, CS16 base. */
#define BS3_SEL_R3_CS32_EO          0x0458 /**< ring-3: 32-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R3_CS32_CNF         0x0460 /**< ring-3: 32-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R3_CS32_CNF_EO      0x0468 /**< ring-3: 32-bit execute-only conforming code selector, not accessed, flat. */
#define BS3_SEL_R3_CS64_EO          0x0470 /**< ring-3: 64-bit execute-only code selector, not accessed, flat. */
#define BS3_SEL_R3_CS64_CNF         0x0478 /**< ring-3: 64-bit conforming code selector, not accessed, flat. */
#define BS3_SEL_R3_CS64_CNF_EO      0x0480 /**< ring-3: 64-bit execute-only conforming code selector, not accessed, flat. */

#define BS3_SEL_SPARE_FIRST         0x0500 /**< The first selector in the spare block */
#define BS3_SEL_SPARE_00            0x0500 /**< Spare selector number 00h. */
#define BS3_SEL_SPARE_01            0x0508 /**< Spare selector number 01h. */
#define BS3_SEL_SPARE_02            0x0510 /**< Spare selector number 02h. */
#define BS3_SEL_SPARE_03            0x0518 /**< Spare selector number 03h. */
#define BS3_SEL_SPARE_04            0x0520 /**< Spare selector number 04h. */
#define BS3_SEL_SPARE_05            0x0528 /**< Spare selector number 05h. */
#define BS3_SEL_SPARE_06            0x0530 /**< Spare selector number 06h. */
#define BS3_SEL_SPARE_07            0x0538 /**< Spare selector number 07h. */
#define BS3_SEL_SPARE_08            0x0540 /**< Spare selector number 08h. */
#define BS3_SEL_SPARE_09            0x0548 /**< Spare selector number 09h. */
#define BS3_SEL_SPARE_0a            0x0550 /**< Spare selector number 0ah. */
#define BS3_SEL_SPARE_0b            0x0558 /**< Spare selector number 0bh. */
#define BS3_SEL_SPARE_0c            0x0560 /**< Spare selector number 0ch. */
#define BS3_SEL_SPARE_0d            0x0568 /**< Spare selector number 0dh. */
#define BS3_SEL_SPARE_0e            0x0570 /**< Spare selector number 0eh. */
#define BS3_SEL_SPARE_0f            0x0578 /**< Spare selector number 0fh. */
#define BS3_SEL_SPARE_10            0x0580 /**< Spare selector number 10h. */
#define BS3_SEL_SPARE_11            0x0588 /**< Spare selector number 11h. */
#define BS3_SEL_SPARE_12            0x0590 /**< Spare selector number 12h. */
#define BS3_SEL_SPARE_13            0x0598 /**< Spare selector number 13h. */
#define BS3_SEL_SPARE_14            0x05a0 /**< Spare selector number 14h. */
#define BS3_SEL_SPARE_15            0x05a8 /**< Spare selector number 15h. */
#define BS3_SEL_SPARE_16            0x05b0 /**< Spare selector number 16h. */
#define BS3_SEL_SPARE_17            0x05b8 /**< Spare selector number 17h. */
#define BS3_SEL_SPARE_18            0x05c0 /**< Spare selector number 18h. */
#define BS3_SEL_SPARE_19            0x05c8 /**< Spare selector number 19h. */
#define BS3_SEL_SPARE_1a            0x05d0 /**< Spare selector number 1ah. */
#define BS3_SEL_SPARE_1b            0x05d8 /**< Spare selector number 1bh. */
#define BS3_SEL_SPARE_1c            0x05e0 /**< Spare selector number 1ch. */
#define BS3_SEL_SPARE_1d            0x05e8 /**< Spare selector number 1dh. */
#define BS3_SEL_SPARE_1e            0x05f0 /**< Spare selector number 1eh. */
#define BS3_SEL_SPARE_1f            0x05f8 /**< Spare selector number 1fh. */

#define BS3_SEL_TILED               0x0600 /**< 16-bit data tiling: First - base=0x00000000, limit=64KB. */
#define BS3_SEL_TILED_LAST          0x0df8 /**< 16-bit data tiling: Last  - base=0x00ff0000, limit=64KB. */
#define BS3_SEL_TILED_AREA_SIZE     0x001000000 /**< 16-bit data tiling: Size of addressable area, in bytes. (16 MB) */

#define BS3_SEL_FREE_PART1          0x0e00 /**< Free selector space - part \#1. */
#define BS3_SEL_FREE_PART1_LAST     0x0ff8 /**< Free selector space - part \#1, last entry. */

#define BS3_SEL_TEXT16              0x1000 /**< The BS3TEXT16 selector. */

#define BS3_SEL_FREE_PART2          0x1008 /**< Free selector space - part \#2. */
#define BS3_SEL_FREE_PART2_LAST     0x1ff8 /**< Free selector space - part \#2, last entry. */

#define BS3_SEL_SYSTEM16            0x2000 /**< The BS3SYSTEM16 selector. */

#define BS3_SEL_FREE_PART3          0x2008 /**< Free selector space - part \#3. */
#define BS3_SEL_FREE_PART3_LAST     0x26f8 /**< Free selector space - part \#3, last entry. */

#define BS3_SEL_DATA16              0x2700 /**< The BS3DATA16 selector. */

#define BS3_SEL_GDT_LIMIT           0x2707 /**< The GDT limit. */
/** @} */


/** @def BS3_FAR
 * For inidicating far pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_NEAR
 * For inidicating near pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
#ifdef M_I86
# define BS3_FAR            __far
# define BS3_NEAR           __near
#else
# define BS3_FAR
# define BS3_NEAR
#endif

#if ARCH_BITS == 16 || defined(DOXYGEN_RUNNING)
/** @def BS3_FP_SEG
 * Get the selector (segment) part of a far pointer.
 * @returns selector.
 * @param   a_pv        Far pointer.
 */
# define BS3_FP_SEG(a_pv)            ((uint16_t)(void __seg *)(void BS3_FAR *)(a_pv))
/** @def BS3_FP_OFF
 * Get the segment offset part of a far pointer.
 * @returns offset.
 * @param   a_pv        Far pointer.
 */
# define BS3_FP_OFF(a_pv)            ((uint16_t)(void __near *)(a_pv))
/** @def BS3_FP_MAKE
 * Create a far pointer.
 * @returns selector.
 * @param   a_pv        Far pointer.
 */
# define BS3_FP_MAKE(a_uSeg, a_off)  ((void __seg *)(a_uSeg) + (void __near *)(a_off))
#endif

/** @def BS3_CALL
 * The calling convension used by BS3 functions.  */
#if ARCH_BITS != 64
# define BS3_CALL           __cdecl
#elif !defined(_MSC_VER)
# define BS3_CALL           __attribute__((__ms_abi__))
#else
# define BS3_CALL
#endif

/** @def IN_BS3KIT
 * Indicates that we're in the same link job as the BS3Kit code. */
#ifdef DOXYGEN_RUNNING
# define IN_BS3KIT
#endif

/** @def BS3_DECL
 * Declares a BS3Kit function.
 * @param a_Type        The return type. */
#ifdef IN_BS3KIT
# define BS3_DECL(a_Type)   DECLEXPORT(a_Type) BS3_CALL
#else
# define BS3_DECL(a_Type)   DECLIMPORT(a_Type) BS3_CALL
#endif

/**
 * Constructs a common name.
 *
 * Example: BS3_CMN_NM(Bs3Shutdown)
 *
 * @param   a_Name      The name of the function or global variable.
 */
#define BS3_CMN_NM(a_Name)  RT_CONCAT3(a_Name,_c,ARCH_BITS)

/**
 * Constructs a data name.
 *
 * Example: BS3_DATA_NM(Bs3Gdt)
 *
 * @param   a_Name      The name of the global variable.
 */
#if ARCH_BITS == 64
# define BS3_DATA_NM(a_Name)  RT_CONCAT(_,a_Name)
#else
# define BS3_DATA_NM(a_Name)  a_Name
#endif

/**
 * Template for createing a pointer union type.
 * @param   a_BaseName      The base type name.
 * @param   a_Modifier      The type modifier.
 */
#define BS3_PTR_UNION_TEMPLATE(a_BaseName, a_Modifiers) \
    typedef union a_BaseName \
    { \
        /** Pointer into the void. */ \
        a_Modifiers void BS3_FAR                  *pv; \
        /** As a signed integer. */ \
        intptr_t                                   i; \
        /** As an unsigned integer. */ \
        uintptr_t                                  u; \
        /** Pointer to char value. */ \
        a_Modifiers char BS3_FAR                   *pch; \
        /** Pointer to char value. */ \
        a_Modifiers unsigned char BS3_FAR          *puch; \
        /** Pointer to a int value. */ \
        a_Modifiers int BS3_FAR                    *pi; \
        /** Pointer to a unsigned int value. */ \
        a_Modifiers unsigned int BS3_FAR           *pu; \
        /** Pointer to a long value. */ \
        a_Modifiers long BS3_FAR                   *pl; \
        /** Pointer to a long value. */ \
        a_Modifiers unsigned long BS3_FAR          *pul; \
        /** Pointer to a memory size value. */ \
        a_Modifiers size_t BS3_FAR                 *pcb; \
        /** Pointer to a byte value. */ \
        a_Modifiers uint8_t BS3_FAR                *pb; \
        /** Pointer to a 8-bit unsigned value. */ \
        a_Modifiers uint8_t BS3_FAR                *pu8; \
        /** Pointer to a 16-bit unsigned value. */ \
        a_Modifiers uint16_t BS3_FAR               *pu16; \
        /** Pointer to a 32-bit unsigned value. */ \
        a_Modifiers uint32_t BS3_FAR               *pu32; \
        /** Pointer to a 64-bit unsigned value. */ \
        a_Modifiers uint64_t BS3_FAR               *pu64; \
        /** Pointer to a UTF-16 character. */ \
        a_Modifiers RTUTF16 BS3_FAR                *pwc; \
        /** Pointer to a UUID character. */ \
        a_Modifiers RTUUID BS3_FAR                 *pUuid; \
    } a_BaseName; \
    /** Pointer to a pointer union. */ \
    typedef a_BaseName *RT_CONCAT(P,a_BaseName)
BS3_PTR_UNION_TEMPLATE(BS3PTRUNION, RT_NOTHING);
BS3_PTR_UNION_TEMPLATE(BS3CPTRUNION, const);
BS3_PTR_UNION_TEMPLATE(BS3VPTRUNION, volatile);
BS3_PTR_UNION_TEMPLATE(BS3CVPTRUNION, const volatile);


/** @defgroup grp_bs3kit_system System structures
 * @{ */
/** The GDT, indexed by BS3_SEL_XXX shifted by 3. */
extern X86DESC BS3_DATA_NM(Bs3Gdt)[(BS3_SEL_GDT_LIMIT + 1) / 8];

extern X86DESC64 BS3_DATA_NM(Bs3Gdt_Ldt);                   /**< @see BS3_SEL_LDT */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss16);                  /**< @see BS3_SEL_TSS16  */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss16DoubleFault);       /**< @see BS3_SEL_TSS16_DF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss16Spare0);            /**< @see BS3_SEL_TSS16_SPARE0 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss16Spare1);            /**< @see BS3_SEL_TSS16_SPARE1 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32);                  /**< @see BS3_SEL_TSS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32DoubleFault);       /**< @see BS3_SEL_TSS32_DF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32Spare0);            /**< @see BS3_SEL_TSS32_SPARE0 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32Spare1);            /**< @see BS3_SEL_TSS32_SPARE1 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32IobpIntRedirBm);    /**< @see BS3_SEL_TSS32_IOBP_IRB */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss32IntRedirBm);        /**< @see BS3_SEL_TSS32_IRB */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss64);                  /**< @see BS3_SEL_TSS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss64Spare0);            /**< @see BS3_SEL_TSS64_SPARE0 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss64Spare1);            /**< @see BS3_SEL_TSS64_SPARE1 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_Tss64Iobp);              /**< @see BS3_SEL_TSS64_IOBP */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_MMIO16);              /**< @see BS3_SEL_VMMDEV_MMIO16 */

extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_First);               /**< @see BS3_SEL_R0_FIRST */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS16);                /**< @see BS3_SEL_R0_CS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_DS16);                /**< @see BS3_SEL_R0_DS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_SS16);                /**< @see BS3_SEL_R0_SS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS32);                /**< @see BS3_SEL_R0_CS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_DS32);                /**< @see BS3_SEL_R0_DS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_SS32);                /**< @see BS3_SEL_R0_SS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS64);                /**< @see BS3_SEL_R0_CS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_DS64);                /**< @see BS3_SEL_R0_DS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS16_EO);             /**< @see BS3_SEL_R0_CS16_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS16_CNF);            /**< @see BS3_SEL_R0_CS16_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS16_CND_EO);         /**< @see BS3_SEL_R0_CS16_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS32_EO);             /**< @see BS3_SEL_R0_CS32_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS32_CNF);            /**< @see BS3_SEL_R0_CS32_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS32_CNF_EO);         /**< @see BS3_SEL_R0_CS32_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS64_EO);             /**< @see BS3_SEL_R0_CS64_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS64_CNF);            /**< @see BS3_SEL_R0_CS64_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R0_CS64_CNF_EO);         /**< @see BS3_SEL_R0_CS64_CNF_EO */

extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_First);               /**< @see BS3_SEL_R1_FIRST */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS16);                /**< @see BS3_SEL_R1_CS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_DS16);                /**< @see BS3_SEL_R1_DS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_SS16);                /**< @see BS3_SEL_R1_SS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS32);                /**< @see BS3_SEL_R1_CS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_DS32);                /**< @see BS3_SEL_R1_DS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_SS32);                /**< @see BS3_SEL_R1_SS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS64);                /**< @see BS3_SEL_R1_CS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_DS64);                /**< @see BS3_SEL_R1_DS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS16_EO);             /**< @see BS3_SEL_R1_CS16_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS16_CNF);            /**< @see BS3_SEL_R1_CS16_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS16_CND_EO);         /**< @see BS3_SEL_R1_CS16_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS32_EO);             /**< @see BS3_SEL_R1_CS32_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS32_CNF);            /**< @see BS3_SEL_R1_CS32_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS32_CNF_EO);         /**< @see BS3_SEL_R1_CS32_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS64_EO);             /**< @see BS3_SEL_R1_CS64_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS64_CNF);            /**< @see BS3_SEL_R1_CS64_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R1_CS64_CNF_EO);         /**< @see BS3_SEL_R1_CS64_CNF_EO */

extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_First);               /**< @see BS3_SEL_R2_FIRST */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS16);                /**< @see BS3_SEL_R2_CS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_DS16);                /**< @see BS3_SEL_R2_DS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_SS16);                /**< @see BS3_SEL_R2_SS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS32);                /**< @see BS3_SEL_R2_CS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_DS32);                /**< @see BS3_SEL_R2_DS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_SS32);                /**< @see BS3_SEL_R2_SS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS64);                /**< @see BS3_SEL_R2_CS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_DS64);                /**< @see BS3_SEL_R2_DS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS16_EO);             /**< @see BS3_SEL_R2_CS16_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS16_CNF);            /**< @see BS3_SEL_R2_CS16_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS16_CND_EO);         /**< @see BS3_SEL_R2_CS16_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS32_EO);             /**< @see BS3_SEL_R2_CS32_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS32_CNF);            /**< @see BS3_SEL_R2_CS32_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS32_CNF_EO);         /**< @see BS3_SEL_R2_CS32_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS64_EO);             /**< @see BS3_SEL_R2_CS64_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS64_CNF);            /**< @see BS3_SEL_R2_CS64_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R2_CS64_CNF_EO);         /**< @see BS3_SEL_R2_CS64_CNF_EO */

extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_First);               /**< @see BS3_SEL_R3_FIRST */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS16);                /**< @see BS3_SEL_R3_CS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_DS16);                /**< @see BS3_SEL_R3_DS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_SS16);                /**< @see BS3_SEL_R3_SS16 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS32);                /**< @see BS3_SEL_R3_CS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_DS32);                /**< @see BS3_SEL_R3_DS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_SS32);                /**< @see BS3_SEL_R3_SS32 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS64);                /**< @see BS3_SEL_R3_CS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_DS64);                /**< @see BS3_SEL_R3_DS64 */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS16_EO);             /**< @see BS3_SEL_R3_CS16_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS16_CNF);            /**< @see BS3_SEL_R3_CS16_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS16_CND_EO);         /**< @see BS3_SEL_R3_CS16_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS32_EO);             /**< @see BS3_SEL_R3_CS32_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS32_CNF);            /**< @see BS3_SEL_R3_CS32_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS32_CNF_EO);         /**< @see BS3_SEL_R3_CS32_CNF_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS64_EO);             /**< @see BS3_SEL_R3_CS64_EO */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS64_CNF);            /**< @see BS3_SEL_R3_CS64_CNF */
extern X86DESC BS3_DATA_NM(Bs3Gdte_R3_CS64_CNF_EO);         /**< @see BS3_SEL_R3_CS64_CNF_EO */

extern X86DESC BS3_DATA_NM(Bs3GdteSpare00); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_00 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare01); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_01 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare02); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_02 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare03); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_03 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare04); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_04 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare05); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_05 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare06); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_06 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare07); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_07 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare08); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_08 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare09); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_09 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0a); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0a */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0b); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0b */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0c); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0c */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0d); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0d */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0e); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0e */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare0f); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0f */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare10); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_10 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare11); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_11 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare12); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_12 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare13); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_13 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare14); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_14 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare15); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_15 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare16); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_16 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare17); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_17 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare18); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_18 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare19); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_19 */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1a); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1a */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1b); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1b */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1c); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1c */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1d); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1d */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1e); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1e */
extern X86DESC BS3_DATA_NM(Bs3GdteSpare1f); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1f */

/** GDTs setting up the tiled 16-bit access to the first 16 MBs of memory.
 * @see BS3_SEL_TILED, BS3_SEL_TILED_LAST, BS3_SEL_TILED_AREA_SIZE */
extern X86DESC BS3_DATA_NM(Bs3GdteTiled)[256];
/** Free GDTes, part \#1. */
extern X86DESC BS3_DATA_NM(Bs3GdteFreePart1)[64];
/** The BS3CODE16 GDT entry. @see BS3_SEL_TEXT16   */
extern X86DESC BS3_DATA_NM(Bs3Gdte_CODE16);
/** Free GDTes, part \#2. */
extern X86DESC BS3_DATA_NM(Bs3GdteFreePart2)[511];
/** The BS3SYSTEM16 GDT entry. */
extern X86DESC BS3_DATA_NM(Bs3Gdte_SYSTEM16);
/** Free GDTes, part \#3. */
extern X86DESC BS3_DATA_NM(Bs3GdteFreePart3)[223];
/** The BS3DATA16 GDT entry. */
extern X86DESC BS3_DATA_NM(Bs3Gdte_DATA16);
/** The end of the GDT (exclusive). */
extern X86DESC BS3_DATA_NM(Bs3GdtEnd);

/** The default 16-bit TSS. */
extern X86TSS16  BS3_DATA_NM(Bs3Tss16);
extern X86TSS16  BS3_DATA_NM(Bs3Tss16DoubleFault);
extern X86TSS16  BS3_DATA_NM(Bs3Tss16Spare0);
extern X86TSS16  BS3_DATA_NM(Bs3Tss16Spare1);
/** The default 32-bit TSS. */
extern X86TSS32  BS3_DATA_NM(Bs3Tss32);
extern X86TSS32  BS3_DATA_NM(Bs3Tss32DoubleFault);
extern X86TSS32  BS3_DATA_NM(Bs3Tss32Spare0);
extern X86TSS32  BS3_DATA_NM(Bs3Tss32Spare1);
/** The default 64-bit TSS. */
extern X86TSS64  BS3_DATA_NM(Bs3Tss64);
extern X86TSS64  BS3_DATA_NM(Bs3Tss64Spare0);
extern X86TSS64  BS3_DATA_NM(Bs3Tss64Spare1);
extern X86TSS64  BS3_DATA_NM(Bs3Tss64WithIopb);
extern X86TSS32  BS3_DATA_NM(Bs3Tss32WithIopb);
/** Interrupt redirection bitmap used by Bs3Tss32WithIopb. */
extern uint8_t   BS3_DATA_NM(Bs3SharedIntRedirBm)[32];
/** I/O permission bitmap used by Bs3Tss32WithIopb and Bs3Tss64WithIopb. */
extern uint8_t   BS3_DATA_NM(Bs3SharedIobp)[8192+2];
/** End of the I/O permission bitmap (exclusive). */
extern uint8_t   BS3_DATA_NM(Bs3SharedIobpEnd);
/** 16-bit IDT. */
extern X86DESC   BS3_DATA_NM(Bs3Idt16)[256];
/** 32-bit IDT. */
extern X86DESC   BS3_DATA_NM(Bs3Idt32)[256];
/** 64-bit IDT. */
extern X86DESC64 BS3_DATA_NM(Bs3Idt64)[256];
/** Structure for the LIDT instruction for loading the 16-bit IDT. */
extern X86XDTR64 BS3_DATA_NM(Bs3Lidt_Idt16);
/** Structure for the LIDT instruction for loading the 32-bit IDT. */
extern X86XDTR64 BS3_DATA_NM(Bs3Lidt_Idt32);
/** Structure for the LIDT instruction for loading the 64-bit IDT. */
extern X86XDTR64 BS3_DATA_NM(Bs3Lidt_Idt64);
/** Structure for the LIDT instruction for loading the real mode interrupt
 *  vector table.. */
extern X86XDTR64 BS3_DATA_NM(Bs3Lidt_Ivt);
/** Structure for the LGDT instruction for loading the GDT. */
extern X86XDTR64 BS3_DATA_NM(Bs3Lgdt_Gdt);
/** The LDT (all entries are empty, fill in for testing). */
extern X86DESC   BS3_DATA_NM(Bs3Ldt)[118];
/** The end of the LDT (exclusive).   */
extern X86DESC   BS3_DATA_NM(Bs3LdtEnd);




/** @defgroup grp_bs3kit_cmn    Common Functions and Data
 *
 * The common functions comes in three variations: 16-bit, 32-bit and 64-bit.
 * Templated code uses the #BS3_CMN_NM macro to mangle the name according to the
 * desired
 *
 * @{
 */

/**
 * Panic, never return.
 *
 * The current implementation will only halt the CPU.
 */
BS3_DECL(void) Bs3Panic_c16(void);
BS3_DECL(void) Bs3Panic_c32(void); /**< @copydoc Bs3Panic_c16  */
BS3_DECL(void) Bs3Panic_c64(void); /**< @copydoc Bs3Panic_c16  */
#define Bs3Panic BS3_CMN_NM(Bs3Panic) /**< Selects #Bs3Panic_c16, #Bs3Panic_c32 or #Bs3Panic_c64. */

/**
 * Shutdown the system, never returns.
 *
 * This currently only works for VMs.  When running on real systems it will
 * just halt the CPU.
 */
BS3_DECL(void) Bs3Shutdown_c16(void);
BS3_DECL(void) Bs3Shutdown_c32(void); /**< @copydoc Bs3Shutdown_c16 */
BS3_DECL(void) Bs3Shutdown_c64(void); /**< @copydoc Bs3Shutdown_c16 */
#define Bs3Shutdown BS3_CMN_NM(Bs3Shutdown) /**< Selects #Bs3Shutdown_c16, #Bs3Shutdown_c32 or #Bs3Shutdown_c64. */

/**
 * Prints a 32-bit unsigned value as hex to the screen.
 *
 * @param   uValue      The 32-bit value.
 */
BS3_DECL(void) Bs3PrintU32_c16(uint32_t uValue);
BS3_DECL(void) Bs3PrintU32_c32(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c64(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
#define Bs3PrintU32 BS3_CMN_NM(Bs3PrintU32) /**< Selects #Bs3PrintU32_c16, #Bs3PrintU32_c32 or #Bs3PrintU32_c64. */

/**
 * Formats and prints a string to the screen.
 *
 * @param   pszFormat       The format string.  See #Bs3PrintFV for supported
 *                          format types and flags.
 * @param   ...             Format arguments.
 */
BS3_DECL(size_t) Bs3PrintF_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(size_t) Bs3PrintF_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3PrintF_c16 */
BS3_DECL(size_t) Bs3PrintF_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3PrintF_c16 */
#define Bs3PrintF BS3_CMN_NM(Bs3PrintF) /**< Selects #Bs3PrintF_c16, #Bs3PrintF_c32 or #Bs3PrintF_c64. */

/**
 * Formats and prints a string to the screen, va_list version.
 *
 * Supported types:
 *      - %RI8, %RI16, %RI32, %RI64
 *      - %RU8, %RU16, %RU32, %RU64
 *      - %RX8, %RX16, %RX32, %RX64
 *      - %i, %d
 *      - %u
 *      - %x
 *      - %c
 *      - %p (far pointer)
 *      - %s (far pointer)
 *
 * @param   pszFormat       The format string.  See #Bs3PrintFV for supported
 *                          format types and flags.
 * @param   va              Format arguments.
 */
BS3_DECL(size_t) Bs3PrintFV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(size_t) Bs3PrintFV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3PrintFV_c16 */
BS3_DECL(size_t) Bs3PrintFV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3PrintFV_c16 */
#define Bs3PrintFV BS3_CMN_NM(Bs3PrintFV) /**< Selects #Bs3PrintFV_c16, #Bs3PrintFV_c32 or #Bs3PrintFV_c64. */

/**
 * Prints a string to the screen.
 *
 * @param   pszString       The string to print.
 */
BS3_DECL(void) Bs3PrintStr_c16(const char BS3_FAR *pszString);
BS3_DECL(void) Bs3PrintStr_c32(const char BS3_FAR *pszString); /**< @copydoc Bs3PrintStr_c16 */
BS3_DECL(void) Bs3PrintStr_c64(const char BS3_FAR *pszString); /**< @copydoc Bs3PrintStr_c16 */
#define Bs3PrintStr BS3_CMN_NM(Bs3PrintStr) /**< Selects #Bs3PrintStr_c16, #Bs3PrintStr_c32 or #Bs3PrintStr_c64. */

/**
 * Prints a char to the screen.
 *
 * @param   ch              The character to print.
 */
BS3_DECL(void) Bs3PrintChr_c16(char ch);
BS3_DECL(void) Bs3PrintChr_c32(char ch); /**< @copydoc Bs3PrintChr_c16 */
BS3_DECL(void) Bs3PrintChr_c64(char ch); /**< @copydoc Bs3PrintChr_c16 */
#define Bs3PrintChr BS3_CMN_NM(Bs3PrintChr) /**< Selects #Bs3PrintChr_c16, #Bs3PrintChr_c32 or #Bs3PrintChr_c64. */


/**
 * Finds the length of a zero terminated string.
 *
 * @returns String length in chars/bytes.
 * @param   pszString       The string to examine.
 */
BS3_DECL(size_t) Bs3StrLen_c16(const char BS3_FAR *pszString);
BS3_DECL(size_t) Bs3StrLen_c32(const char BS3_FAR *pszString); /** @copydoc Bs3StrLen_c16 */
BS3_DECL(size_t) Bs3StrLen_c64(const char BS3_FAR *pszString); /** @copydoc Bs3StrLen_c16 */
#define Bs3StrLen BS3_CMN_NM(Bs3StrLen) /**< Selects #Bs3StrLen_c16, #Bs3StrLen_c32 or #Bs3StrLen_c64. */

/**
 * Finds the length of a zero terminated string, but with a max length.
 *
 * @returns String length in chars/bytes, or @a cchMax if no zero-terminator
 *           was found before we reached the limit.
 * @param   pszString       The string to examine.
 * @param   cchMax          The max length to examine.
 */
BS3_DECL(size_t) Bs3StrNLen_c16(const char BS3_FAR *pszString, size_t cchMax);
BS3_DECL(size_t) Bs3StrNLen_c32(const char BS3_FAR *pszString, size_t cchMax); /** @copydoc Bs3StrNLen_c16 */
BS3_DECL(size_t) Bs3StrNLen_c64(const char BS3_FAR *pszString, size_t cchMax); /** @copydoc Bs3StrNLen_c16 */
#define Bs3StrNLen BS3_CMN_NM(Bs3StrNLen) /**< Selects #Bs3StrNLen_c16, #Bs3StrNLen_c32 or #Bs3StrNLen_c64. */

/**
 * CRT style unsafe strcpy.
 *
 * @returns pszDst.
 * @param   pszDst          The destination buffer.  Must be large enough to
 *                          hold the source string.
 * @param   pszSrc          The source string.
 */
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c16(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc);
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c32(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc); /** @copydoc Bs3StrCpy_c16 */
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c64(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc); /** @copydoc Bs3StrCpy_c16 */
#define Bs3StrCpy BS3_CMN_NM(Bs3StrCpy) /**< Selects #Bs3StrCpy_c16, #Bs3StrCpy_c32 or #Bs3StrCpy_c64. */

/**
 * CRT style memcpy.
 *
 * @returns pvDst
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemCpy_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemCpy_c16 */
#define Bs3MemCpy BS3_CMN_NM(Bs3MemCpy) /**< Selects #Bs3MemCpy_c16, #Bs3MemCpy_c32 or #Bs3MemCpy_c64. */

/**
 * GNU (?) style mempcpy.
 *
 * @returns pvDst + cbCopy
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemPCpy_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemPCpy_c16 */
#define Bs3MemPCpy BS3_CMN_NM(Bs3MemPCpy) /**< Selects #Bs3MemPCpy_c16, #Bs3MemPCpy_c32 or #Bs3MemPCpy_c64. */

/**
 * CRT style memmove (overlapping buffers is fine).
 *
 * @returns pvDst
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemMove_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemMove_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemMove_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemMove_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemMove_c16 */
#define Bs3MemMove BS3_CMN_NM(Bs3MemMove) /**< Selects #Bs3MemMove_c16, #Bs3MemMove_c32 or #Bs3MemMove_c64. */



/**
 * Equivalent to RTTestCreate + RTTestBanner.
 *
 * @param   pszTest         The test name.
 */
BS3_DECL(void) Bs3TestInit_c16(const char BS3_FAR *pszTest);
BS3_DECL(void) Bs3TestInit_c32(const char BS3_FAR *pszTest); /**< @copydoc Bs3TestInit_c16 */
BS3_DECL(void) Bs3TestInit_c64(const char BS3_FAR *pszTest); /**< @copydoc Bs3TestInit_c16 */
#define Bs3TestInit BS3_CMN_NM(Bs3TestInit) /**< Selects #Bs3TestInit_c16, #Bs3TestInit_c32 or #Bs3TestInit_c64. */


/** @} */



/** @defgroup grp_bs3kit_mode   Mode Specific Functions and Data
 *
 * The mode specific functions come in bit count variations and CPU mode
 * variations.  The bs3kit-template-header.h/mac defines the BS3_NM macro to
 * mangle a function or variable name according to the target CPU mode.  In
 * non-templated code, it's common to spell the name out in full.
 *
 * @{
 */


/** @} */

#endif

