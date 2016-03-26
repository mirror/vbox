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

/*
 * We normally don't want the noreturn / aborts attributes as they mess up stack traces.
 *
 * Note! pragma aux <fnname> aborts can only be used with functions
 *       implemented in C and functions that does not have parameters.
 */
#define BS3_KIT_WITH_NO_RETURN
#ifndef BS3_KIT_WITH_NO_RETURN
# undef  DECL_NO_RETURN
# define DECL_NO_RETURN(type) type
#endif

/*
 * We may want to reuse some IPRT code in the common name space, so we
 * redefine the RT_MANGLER to work like BS3_CMN_NM.  (We cannot use
 * BS3_CMN_NM yet, as we need to include IPRT headers with function
 * declarations before we can define it. Thus the duplciate effort.)
 */
#define RT_MANGLER(a_Name) RT_CONCAT3(a_Name,_c,ARCH_BITS)
#include <iprt/mangling.h>
#include <iprt/x86.h>
#include <iprt/err.h>



RT_C_DECLS_BEGIN

/** @defgroup grp_bs3kit     BS3Kit
 * @{ */

/** @name Execution modes.
 * @{ */
#define BS3_MODE_INVALID    UINT8_C(0x00)
#define BS3_MODE_RM         UINT8_C(0x01)  /**< real mode. */
#define BS3_MODE_PE16       UINT8_C(0x11)  /**< 16-bit protected mode kernel+tss, running 16-bit code, unpaged. */
#define BS3_MODE_PE16_32    UINT8_C(0x12)  /**< 16-bit protected mode kernel+tss, running 32-bit code, unpaged. */
#define BS3_MODE_PE16_V86   UINT8_C(0x13)  /**< 16-bit protected mode kernel+tss, running virtual 8086 mode code, unpaged. */
#define BS3_MODE_PE32       UINT8_C(0x22)  /**< 32-bit protected mode kernel+tss, running 32-bit code, unpaged. */
#define BS3_MODE_PE32_16    UINT8_C(0x21)  /**< 32-bit protected mode kernel+tss, running 16-bit code, unpaged. */
#define BS3_MODE_PEV86      UINT8_C(0x23)  /**< 32-bit protected mode kernel+tss, running virtual 8086 mode code, unpaged. */
#define BS3_MODE_PP16       UINT8_C(0x31)  /**< 16-bit protected mode kernel+tss, running 16-bit code, paged. */
#define BS3_MODE_PP16_32    UINT8_C(0x32)  /**< 16-bit protected mode kernel+tss, running 32-bit code, paged. */
#define BS3_MODE_PP16_V86   UINT8_C(0x33)  /**< 16-bit protected mode kernel+tss, running virtual 8086 mode code, paged. */
#define BS3_MODE_PP32       UINT8_C(0x42)  /**< 32-bit protected mode kernel+tss, running 32-bit code, paged. */
#define BS3_MODE_PP32_16    UINT8_C(0x41)  /**< 32-bit protected mode kernel+tss, running 16-bit code, paged. */
#define BS3_MODE_PPV86      UINT8_C(0x43)  /**< 32-bit protected mode kernel+tss, running virtual 8086 mode code, paged. */
#define BS3_MODE_PAE16      UINT8_C(0x51)  /**< 16-bit protected mode kernel+tss, running 16-bit code, PAE paging. */
#define BS3_MODE_PAE16_32   UINT8_C(0x52)  /**< 16-bit protected mode kernel+tss, running 32-bit code, PAE paging. */
#define BS3_MODE_PAE16_V86  UINT8_C(0x53)  /**< 16-bit protected mode kernel+tss, running virtual 8086 mode, PAE paging. */
#define BS3_MODE_PAE32      UINT8_C(0x62)  /**< 32-bit protected mode kernel+tss, running 32-bit code, PAE paging. */
#define BS3_MODE_PAE32_16   UINT8_C(0x61)  /**< 32-bit protected mode kernel+tss, running 16-bit code, PAE paging. */
#define BS3_MODE_PAEV86     UINT8_C(0x63)  /**< 32-bit protected mode kernel+tss, running virtual 8086 mode, PAE paging. */
#define BS3_MODE_LM16       UINT8_C(0x71)  /**< 16-bit long mode (paged), kernel+tss always 64-bit. */
#define BS3_MODE_LM32       UINT8_C(0x72)  /**< 32-bit long mode (paged), kernel+tss always 64-bit. */
#define BS3_MODE_LM64       UINT8_C(0x74)  /**< 64-bit long mode (paged), kernel+tss always 64-bit. */

#define BS3_MODE_CODE_MASK  UINT8_C(0x0f)  /**< Running code mask. */
#define BS3_MODE_CODE_16    UINT8_C(0x01)  /**< Running 16-bit code. */
#define BS3_MODE_CODE_32    UINT8_C(0x02)  /**< Running 32-bit code. */
#define BS3_MODE_CODE_V86   UINT8_C(0x03)  /**< Running 16-bit virtual 8086 code. */
#define BS3_MODE_CODE_64    UINT8_C(0x04)  /**< Running 64-bit code. */

#define BS3_MODE_SYS_MASK   UINT8_C(0xf0)  /**< kernel+tss mask. */
#define BS3_MODE_SYS_RM     UINT8_C(0x00)  /**< Real mode kernel+tss. */
#define BS3_MODE_SYS_PE16   UINT8_C(0x10)  /**< 16-bit protected mode kernel+tss. */
#define BS3_MODE_SYS_PE32   UINT8_C(0x20)  /**< 32-bit protected mode kernel+tss. */
#define BS3_MODE_SYS_PP16   UINT8_C(0x30)  /**< 16-bit paged protected mode kernel+tss. */
#define BS3_MODE_SYS_PP32   UINT8_C(0x40)  /**< 32-bit paged protected mode kernel+tss. */
#define BS3_MODE_SYS_PAE16  UINT8_C(0x50)  /**< 16-bit PAE paged protected mode kernel+tss. */
#define BS3_MODE_SYS_PAE32  UINT8_C(0x60)  /**< 32-bit PAE paged protected mode kernel+tss. */
#define BS3_MODE_SYS_LM     UINT8_C(0x70)  /**< 64-bit (paged) long mode protected mode kernel+tss. */

/** Whether the mode has paging enabled. */
#define BS3_MODE_IS_PAGED(a_fMode)              ((a_fMode) >= BS3_MODE_PP16)

/** Whether the mode is running v8086 code. */
#define BS3_MODE_IS_V86(a_fMode)                (((a_fMode) & BS3_MODE_CODE_MASK) == BS3_MODE_CODE_V86)
/** Whether the we're executing in real mode or v8086 mode. */
#define BS3_MODE_IS_RM_OR_V86(a_fMode)          ((a_fMode) == BS3_MODE_RM || BS3_MODE_IS_V86(a_fMode))
/** Whether the mode is running 16-bit code, except v8086. */
#define BS3_MODE_IS_16BIT_CODE_NO_V86(a_fMode)  (((a_fMode) & BS3_MODE_CODE_MASK) == BS3_MODE_CODE_16)
/** Whether the mode is running 16-bit code (includes v8086). */
#define BS3_MODE_IS_16BIT_CODE(a_fMode)         (BS3_MODE_IS_16BIT_CODE_NO_V86(a_fMode) || BS3_MODE_IS_V86(a_fMode))
/** Whether the mode is running 32-bit code. */
#define BS3_MODE_IS_32BIT_CODE(a_fMode)         (((a_fMode) & BS3_MODE_CODE_MASK) == BS3_MODE_CODE_32)
/** Whether the mode is running 64-bit code. */
#define BS3_MODE_IS_64BIT_CODE(a_fMode)         (((a_fMode) & BS3_MODE_CODE_MASK) == BS3_MODE_CODE_64)

/** Whether the system is in real mode. */
#define BS3_MODE_IS_RM_SYS(a_fMode)             (((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_RM)
/** Whether the system is some 16-bit mode that isn't real mode. */
#define BS3_MODE_IS_16BIT_SYS_NO_RM(a_fMode)    (   ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PE16 \
                                                 || ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PP16 \
                                                 || ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PAE16)
/** Whether the system is some 16-bit mode (includes real mode). */
#define BS3_MODE_IS_16BIT_SYS(a_fMode)          (BS3_MODE_IS_16BIT_SYS_NO_RM(a_fMode) || BS3_MODE_IS_RM_SYS(a_fMode))
/** Whether the system is some 32-bit mode. */
#define BS3_MODE_IS_32BIT_SYS(a_fMode)          (   ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PE32 \
                                                 || ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PP32 \
                                                 || ((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_PAE32)
/** Whether the system is long mode. */
#define BS3_MODE_IS_64BIT_SYS(a_fMode)          (((a_fMode) & BS3_MODE_SYS_MASK) == BS3_MODE_SYS_LM)

/** @todo testcase: How would long-mode handle a 16-bit TSS loaded prior to the switch? (mainly stack switching wise) Hopefully, it will tripple fault, right? */
/** @} */


/** @name BS3_ADDR_XXX - Static Memory Allocation
 * @{ */
/** The flat load address for the code after the bootsector. */
#define BS3_ADDR_LOAD           0x10000
/** Where we save the boot registers during init.
 * Located right before the code. */
#define BS3_ADDR_REG_SAVE       (BS3_ADDR_LOAD - sizeof(BS3REGCTX) - 8)
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

#define BS3_SEL_RING_SHIFT          8      /**< For the formula: BS3_SEL_R0_XXX + ((cs & 3) << BS3_SEL_RING_SHIFT) */
#define BS3_SEL_RING_SUB_MASK       0x00f8 /**< Mask for getting the sub-selector. For use with BS3_SEL_R*_FIRST. */

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

#define BS3_SEL_TILED               0x0600 /**< 16-bit data tiling: First - base=0x00000000, limit=64KB, DPL=3. */
#define BS3_SEL_TILED_LAST          0x0df8 /**< 16-bit data tiling: Last  - base=0x00ff0000, limit=64KB, DPL=3. */
#define BS3_SEL_TILED_AREA_SIZE     0x001000000 /**< 16-bit data tiling: Size of addressable area, in bytes. (16 MB) */

#define BS3_SEL_FREE_PART1          0x0e00 /**< Free selector space - part \#1. */
#define BS3_SEL_FREE_PART1_LAST     0x0ff8 /**< Free selector space - part \#1, last entry. */

#define BS3_SEL_TEXT16              0x1000 /**< The BS3TEXT16 selector. */

#define BS3_SEL_FREE_PART2          0x1008 /**< Free selector space - part \#2. */
#define BS3_SEL_FREE_PART2_LAST     0x17f8 /**< Free selector space - part \#2, last entry. */

#define BS3_SEL_TILED_R0            0x1800 /**< 16-bit data/stack tiling: First - base=0x00000000, limit=64KB, DPL=0. */
#define BS3_SEL_TILED_R0_LAST       0x1ff8 /**< 16-bit data/stack tiling: Last  - base=0x00ff0000, limit=64KB, DPL=0. */

#define BS3_SEL_SYSTEM16            0x2000 /**< The BS3SYSTEM16 selector. */

#define BS3_SEL_FREE_PART3          0x2008 /**< Free selector space - part \#3. */
#define BS3_SEL_FREE_PART3_LAST     0x26f8 /**< Free selector space - part \#3, last entry. */

#define BS3_SEL_DATA16              0x2700 /**< The BS3DATA16 selector. */

#define BS3_SEL_GDT_LIMIT           0x2707 /**< The GDT limit. */
/** @} */


/** @def BS3_FAR
 * For indicating far pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_NEAR
 * For indicating near pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_FAR_CODE
 * For indicating far 16-bit functions.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_NEAR_CODE
 * For indicating near 16-bit functions.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_FAR_DATA
 * For indicating far 16-bit external data, i.e. in a segment other than DATA16.
 * Does nothing in 32-bit and 64-bit code. */
#ifdef M_I86
# define BS3_FAR            __far
# define BS3_NEAR           __near
# define BS3_FAR_CODE       __far
# define BS3_NEAR_CODE      __near
# define BS3_FAR_DATA       __far
#else
# define BS3_FAR
# define BS3_NEAR
# define BS3_FAR_CODE
# define BS3_NEAR_CODE
# define BS3_FAR_DATA
#endif

#if ARCH_BITS == 16 || defined(DOXYGEN_RUNNING)
/** @def BS3_FP_SEG
 * Get the selector (segment) part of a far pointer.
 *
 * @returns selector.
 * @param   a_pv        Far pointer.
 */
# define BS3_FP_SEG(a_pv)            ((uint16_t)(__segment)(void BS3_FAR *)(a_pv))
/** @def BS3_FP_OFF
 * Get the segment offset part of a far pointer.
 *
 * For sake of convenience, this works like a uintptr_t cast in 32-bit and
 * 64-bit code.
 *
 * @returns offset.
 * @param   a_pv        Far pointer.
 */
# define BS3_FP_OFF(a_pv)            ((uint16_t)(void __near *)(a_pv))
/** @def BS3_FP_MAKE
 * Create a far pointer.
 *
 * @returns Far pointer.
 * @param   a_uSeg      The selector/segment.
 * @param   a_off       The offset into the segment.
 */
# define BS3_FP_MAKE(a_uSeg, a_off)  (((__segment)(a_uSeg)) :> ((void __near *)(a_off)))
#else
# define BS3_FP_OFF(a_pv)            ((uintptr_t)(a_pv))
#endif

/** @def BS3_MAKE_PROT_PTR_FROM_FLAT
 * Creates a protected mode pointer from a flat address.
 *
 * For sake of convenience, this macro also works in 32-bit and 64-bit mode,
 * only there it doesn't return a far pointer but a flat point.
 *
 * @returns far void pointer if 16-bit code, near/flat void pointer in 32-bit
 *          and 64-bit.
 * @param   a_uFlat     Flat address in the first 16MB. */
#if ARCH_BITS == 16
# define BS3_MAKE_PROT_R0PTR_FROM_FLAT(a_uFlat)  \
    BS3_FP_MAKE(((uint16_t)(a_uFlat >> 16) << 3) + BS3_SEL_TILED, (uint16_t)(a_uFlat))
#else
# define BS3_MAKE_PROT_R0PTR_FROM_FLAT(a_uFlat)  ((void *)(uintptr_t)(a_uFlat))
#endif

/** @def BS3_MAKE_PROT_R0PTR_FROM_REAL
 * Creates a protected mode pointer from a far real mode address.
 *
 * For sake of convenience, this macro also works in 32-bit and 64-bit mode,
 * only there it doesn't return a far pointer but a flat point.
 *
 * @returns far void pointer if 16-bit code, near/flat void pointer in 32-bit
 *          and 64-bit.
 * @param   a_uSeg      The selector/segment.
 * @param   a_off       The offset into the segment.
 */
#if ARCH_BITS == 16
# define BS3_MAKE_PROT_R0PTR_FROM_REAL(a_uSeg, a_off) BS3_MAKE_PROT_R0PTR_FROM_FLAT(((uint32_t)(a_uSeg) << 4) + (uint16_t)(a_off))
#else
# define BS3_MAKE_PROT_R0PTR_FROM_REAL(a_uSeg, a_off) ( (void *)(uintptr_t)(((uint32_t)(a_uSeg) << 4) + (uint16_t)(a_off)) )
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
 *
 * Until we outgrow BS3TEXT16, we use all near functions in 16-bit.
 *
 * @param a_Type        The return type. */
#ifdef IN_BS3KIT
# define BS3_DECL(a_Type)   DECLEXPORT(a_Type) BS3_NEAR_CODE BS3_CALL
#else
# define BS3_DECL(a_Type)   DECLIMPORT(a_Type) BS3_NEAR_CODE BS3_CALL
#endif

/** @def BS3_DECL_CALLBACK
 * Declares a BS3Kit callback function (typically static).
 *
 * Until we outgrow BS3TEXT16, we use all near functions in 16-bit.
 *
 * @param a_Type        The return type. */
#ifdef IN_BS3KIT
# define BS3_DECL_CALLBACK(a_Type)   a_Type BS3_NEAR_CODE BS3_CALL
#else
# define BS3_DECL_CALLBACK(a_Type)   a_Type BS3_NEAR_CODE BS3_CALL
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
 * Example: extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdt)
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


/** The system call vector. */
#define BS3_TRAP_SYSCALL        UINT8_C(0x20)

/** @name System call numbers (ax).
 * Paramenters are generally passed in registers specific to each system call.
 * @{ */
/** Print char (cl). */
#define BS3_SYSCALL_PRINT_CHR   UINT16_C(0x0001)
/** Print string (pointer in ds:[e]si, length in cx). */
#define BS3_SYSCALL_PRINT_STR   UINT16_C(0x0002)
/** Switch to ring-0. */
#define BS3_SYSCALL_TO_RING0    UINT16_C(0x0003)
/** Switch to ring-1. */
#define BS3_SYSCALL_TO_RING1    UINT16_C(0x0004)
/** Switch to ring-2. */
#define BS3_SYSCALL_TO_RING2    UINT16_C(0x0005)
/** Switch to ring-3. */
#define BS3_SYSCALL_TO_RING3    UINT16_C(0x0006)
/** @} */



/** @defgroup grp_bs3kit_system System structures
 * @{ */
/** The GDT, indexed by BS3_SEL_XXX shifted by 3. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdt)[(BS3_SEL_GDT_LIMIT + 1) / 8];

extern X86DESC64 BS3_FAR_DATA BS3_DATA_NM(Bs3Gdt_Ldt);                   /**< @see BS3_SEL_LDT */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss16);                  /**< @see BS3_SEL_TSS16  */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss16DoubleFault);       /**< @see BS3_SEL_TSS16_DF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss16Spare0);            /**< @see BS3_SEL_TSS16_SPARE0 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss16Spare1);            /**< @see BS3_SEL_TSS16_SPARE1 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32);                  /**< @see BS3_SEL_TSS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32DoubleFault);       /**< @see BS3_SEL_TSS32_DF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32Spare0);            /**< @see BS3_SEL_TSS32_SPARE0 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32Spare1);            /**< @see BS3_SEL_TSS32_SPARE1 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32IobpIntRedirBm);    /**< @see BS3_SEL_TSS32_IOBP_IRB */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss32IntRedirBm);        /**< @see BS3_SEL_TSS32_IRB */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss64);                  /**< @see BS3_SEL_TSS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss64Spare0);            /**< @see BS3_SEL_TSS64_SPARE0 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss64Spare1);            /**< @see BS3_SEL_TSS64_SPARE1 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_Tss64Iobp);              /**< @see BS3_SEL_TSS64_IOBP */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_MMIO16);              /**< @see BS3_SEL_VMMDEV_MMIO16 */

extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_First);               /**< @see BS3_SEL_R0_FIRST */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS16);                /**< @see BS3_SEL_R0_CS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_DS16);                /**< @see BS3_SEL_R0_DS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_SS16);                /**< @see BS3_SEL_R0_SS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS32);                /**< @see BS3_SEL_R0_CS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_DS32);                /**< @see BS3_SEL_R0_DS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_SS32);                /**< @see BS3_SEL_R0_SS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS64);                /**< @see BS3_SEL_R0_CS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_DS64);                /**< @see BS3_SEL_R0_DS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS16_EO);             /**< @see BS3_SEL_R0_CS16_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS16_CNF);            /**< @see BS3_SEL_R0_CS16_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS16_CND_EO);         /**< @see BS3_SEL_R0_CS16_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS32_EO);             /**< @see BS3_SEL_R0_CS32_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS32_CNF);            /**< @see BS3_SEL_R0_CS32_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS32_CNF_EO);         /**< @see BS3_SEL_R0_CS32_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS64_EO);             /**< @see BS3_SEL_R0_CS64_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS64_CNF);            /**< @see BS3_SEL_R0_CS64_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R0_CS64_CNF_EO);         /**< @see BS3_SEL_R0_CS64_CNF_EO */

extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_First);               /**< @see BS3_SEL_R1_FIRST */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS16);                /**< @see BS3_SEL_R1_CS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_DS16);                /**< @see BS3_SEL_R1_DS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_SS16);                /**< @see BS3_SEL_R1_SS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS32);                /**< @see BS3_SEL_R1_CS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_DS32);                /**< @see BS3_SEL_R1_DS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_SS32);                /**< @see BS3_SEL_R1_SS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS64);                /**< @see BS3_SEL_R1_CS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_DS64);                /**< @see BS3_SEL_R1_DS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS16_EO);             /**< @see BS3_SEL_R1_CS16_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS16_CNF);            /**< @see BS3_SEL_R1_CS16_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS16_CND_EO);         /**< @see BS3_SEL_R1_CS16_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS32_EO);             /**< @see BS3_SEL_R1_CS32_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS32_CNF);            /**< @see BS3_SEL_R1_CS32_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS32_CNF_EO);         /**< @see BS3_SEL_R1_CS32_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS64_EO);             /**< @see BS3_SEL_R1_CS64_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS64_CNF);            /**< @see BS3_SEL_R1_CS64_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R1_CS64_CNF_EO);         /**< @see BS3_SEL_R1_CS64_CNF_EO */

extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_First);               /**< @see BS3_SEL_R2_FIRST */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS16);                /**< @see BS3_SEL_R2_CS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_DS16);                /**< @see BS3_SEL_R2_DS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_SS16);                /**< @see BS3_SEL_R2_SS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS32);                /**< @see BS3_SEL_R2_CS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_DS32);                /**< @see BS3_SEL_R2_DS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_SS32);                /**< @see BS3_SEL_R2_SS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS64);                /**< @see BS3_SEL_R2_CS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_DS64);                /**< @see BS3_SEL_R2_DS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS16_EO);             /**< @see BS3_SEL_R2_CS16_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS16_CNF);            /**< @see BS3_SEL_R2_CS16_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS16_CND_EO);         /**< @see BS3_SEL_R2_CS16_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS32_EO);             /**< @see BS3_SEL_R2_CS32_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS32_CNF);            /**< @see BS3_SEL_R2_CS32_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS32_CNF_EO);         /**< @see BS3_SEL_R2_CS32_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS64_EO);             /**< @see BS3_SEL_R2_CS64_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS64_CNF);            /**< @see BS3_SEL_R2_CS64_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R2_CS64_CNF_EO);         /**< @see BS3_SEL_R2_CS64_CNF_EO */

extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_First);               /**< @see BS3_SEL_R3_FIRST */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS16);                /**< @see BS3_SEL_R3_CS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_DS16);                /**< @see BS3_SEL_R3_DS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_SS16);                /**< @see BS3_SEL_R3_SS16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS32);                /**< @see BS3_SEL_R3_CS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_DS32);                /**< @see BS3_SEL_R3_DS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_SS32);                /**< @see BS3_SEL_R3_SS32 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS64);                /**< @see BS3_SEL_R3_CS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_DS64);                /**< @see BS3_SEL_R3_DS64 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS16_EO);             /**< @see BS3_SEL_R3_CS16_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS16_CNF);            /**< @see BS3_SEL_R3_CS16_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS16_CND_EO);         /**< @see BS3_SEL_R3_CS16_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS32_EO);             /**< @see BS3_SEL_R3_CS32_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS32_CNF);            /**< @see BS3_SEL_R3_CS32_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS32_CNF_EO);         /**< @see BS3_SEL_R3_CS32_CNF_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS64_EO);             /**< @see BS3_SEL_R3_CS64_EO */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS64_CNF);            /**< @see BS3_SEL_R3_CS64_CNF */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_R3_CS64_CNF_EO);         /**< @see BS3_SEL_R3_CS64_CNF_EO */

extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare00); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_00 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare01); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_01 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare02); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_02 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare03); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_03 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare04); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_04 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare05); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_05 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare06); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_06 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare07); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_07 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare08); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_08 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare09); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_09 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0a); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0a */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0b); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0b */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0c); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0c */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0d); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0d */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0e); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0e */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare0f); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_0f */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare10); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_10 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare11); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_11 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare12); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_12 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare13); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_13 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare14); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_14 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare15); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_15 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare16); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_16 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare17); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_17 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare18); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_18 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare19); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_19 */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1a); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1a */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1b); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1b */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1c); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1c */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1d); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1d */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1e); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1e */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteSpare1f); /**< GDT entry for playing with in testcases. @see BS3_SEL_SPARE_1f */

/** GDTs setting up the tiled 16-bit access to the first 16 MBs of memory.
 * @see BS3_SEL_TILED, BS3_SEL_TILED_LAST, BS3_SEL_TILED_AREA_SIZE */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteTiled)[256];
/** Free GDTes, part \#1. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteFreePart1)[64];
/** The BS3TEXT16/BS3CLASS16CODE GDT entry. @see BS3_SEL_TEXT16   */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_CODE16);
/** Free GDTes, part \#2. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteFreePart2)[511];
/** The BS3SYSTEM16 GDT entry. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_SYSTEM16);
/** Free GDTes, part \#3. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdteFreePart3)[223];
/** The BS3DATA16/BS3_FAR_DATA GDT entry. */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3Gdte_DATA16);
/** The end of the GDT (exclusive). */
extern X86DESC BS3_FAR_DATA BS3_DATA_NM(Bs3GdtEnd);

/** The default 16-bit TSS. */
extern X86TSS16  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss16);
extern X86TSS16  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss16DoubleFault);
extern X86TSS16  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss16Spare0);
extern X86TSS16  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss16Spare1);
/** The default 32-bit TSS. */
extern X86TSS32  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss32);
extern X86TSS32  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss32DoubleFault);
extern X86TSS32  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss32Spare0);
extern X86TSS32  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss32Spare1);
/** The default 64-bit TSS. */
extern X86TSS64  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss64);
extern X86TSS64  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss64Spare0);
extern X86TSS64  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss64Spare1);
extern X86TSS64  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss64WithIopb);
extern X86TSS32  BS3_FAR_DATA BS3_DATA_NM(Bs3Tss32WithIopb);
/** Interrupt redirection bitmap used by Bs3Tss32WithIopb. */
extern uint8_t   BS3_FAR_DATA BS3_DATA_NM(Bs3SharedIntRedirBm)[32];
/** I/O permission bitmap used by Bs3Tss32WithIopb and Bs3Tss64WithIopb. */
extern uint8_t   BS3_FAR_DATA BS3_DATA_NM(Bs3SharedIobp)[8192+2];
/** End of the I/O permission bitmap (exclusive). */
extern uint8_t   BS3_FAR_DATA BS3_DATA_NM(Bs3SharedIobpEnd);
/** 16-bit IDT. */
extern X86DESC   BS3_FAR_DATA BS3_DATA_NM(Bs3Idt16)[256];
/** 32-bit IDT. */
extern X86DESC   BS3_FAR_DATA BS3_DATA_NM(Bs3Idt32)[256];
/** 64-bit IDT. */
extern X86DESC64 BS3_FAR_DATA BS3_DATA_NM(Bs3Idt64)[256];
/** Structure for the LIDT instruction for loading the 16-bit IDT. */
extern X86XDTR64 BS3_FAR_DATA BS3_DATA_NM(Bs3Lidt_Idt16);
/** Structure for the LIDT instruction for loading the 32-bit IDT. */
extern X86XDTR64 BS3_FAR_DATA BS3_DATA_NM(Bs3Lidt_Idt32);
/** Structure for the LIDT instruction for loading the 64-bit IDT. */
extern X86XDTR64 BS3_FAR_DATA BS3_DATA_NM(Bs3Lidt_Idt64);
/** Structure for the LIDT instruction for loading the real mode interrupt
 *  vector table.. */
extern X86XDTR64 BS3_FAR_DATA BS3_DATA_NM(Bs3Lidt_Ivt);
/** Structure for the LGDT instruction for loading the GDT. */
extern X86XDTR64 BS3_FAR_DATA BS3_DATA_NM(Bs3Lgdt_Gdt);
/** The LDT (all entries are empty, fill in for testing). */
extern X86DESC   BS3_FAR_DATA BS3_DATA_NM(Bs3Ldt)[118];
/** The end of the LDT (exclusive).   */
extern X86DESC   BS3_FAR_DATA BS3_DATA_NM(Bs3LdtEnd);

/** @} */


/** @name Segment start and end markers, sizes.
 * @{ */
/** Start of the BS3TEXT16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text16_StartOfSegment);
/** End of the BS3TEXT16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text16_EndOfSegment);
/** The size of the BS3TEXT16 segment.   */
extern uint16_t BS3_FAR_DATA BS3_DATA_NM(Bs3Text16_Size);

/** Start of the BS3SYSTEM16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3System16_StartOfSegment);
/** End of the BS3SYSTEM16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3System16_EndOfSegment);

/** Start of the BS3DATA16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data16_StartOfSegment);
/** End of the BS3DATA16 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data16_EndOfSegment);

/** Start of the BS3TEXT32 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text32_StartOfSegment);
/** Start of the BS3TEXT32 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text32_EndOfSegment);

/** Start of the BS3DATA32 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data32_StartOfSegment);
/** Start of the BS3DATA32 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data32_EndOfSegment);

/** Start of the BS3TEXT64 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text64_StartOfSegment);
/** Start of the BS3TEXT64 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Text64_EndOfSegment);

/** Start of the BS3DATA64 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data64_StartOfSegment);
/** Start of the BS3DATA64 segment.   */
extern uint8_t  BS3_FAR_DATA BS3_DATA_NM(Bs3Data64_EndOfSegment);

/** The size of the Data16, Text32, Text64, Data32 and Data64 blob. */
extern uint32_t BS3_FAR_DATA BS3_DATA_NM(Bs3Data16Thru64Text32And64_TotalSize);
/** The total image size (from Text16 thu Data64). */
extern uint32_t BS3_FAR_DATA BS3_DATA_NM(Bs3TotalImageSize);
/** @} */


/** Lower case hex digits. */
extern char const BS3_DATA_NM(g_achBs3HexDigits)[16+1];
/** Upper case hex digits. */
extern char const BS3_DATA_NM(g_achBs3HexDigitsUpper)[16+1];


/** The current mode (BS3_MODE_XXX) of CPU \#0. */
extern uint8_t    BS3_DATA_NM(g_bBs3CurrentMode);

/** Hint for 16-bit trap handlers regarding the high word of EIP. */
extern uint32_t   BS3_DATA_NM(g_uBs3TrapEipHint);


#ifdef __WATCOMC__
/**
 * Executes the SMSW instruction and returns the value.
 *
 * @returns Machine status word.
 */
uint16_t Bs3AsmSmsw(void);
# pragma aux Bs3AsmSmsw = \
        ".286" \
        "smsw ax" \
        value [ax] modify exact [ax] nomemory;
#endif


/** @def BS3_IS_PROTECTED_MODE
 * @returns true if protected mode, false if not. */
#if ARCH_BITS != 16
# define BS3_IS_PROTECTED_MODE() (true)
#else
# if 1
#  define BS3_IS_PROTECTED_MODE() (!BS3_MODE_IS_RM_SYS(BS3_DATA_NM(g_bBs3CurrentMode)))
# else
#  define BS3_IS_PROTECTED_MODE() (Bs3AsmSmsw() & 1 /*PE*/)
# endif
#endif



/** @defgroup bs3kit_cross_ptr  Cross context pointer type
 *
 * The cross context pointer type is
 *
 * @{ */

/**
 * Cross context pointer base type.
 */
#pragma pack(4)
typedef union BS3XPTR
{
    /** The flat pointer.   */
    uint32_t        uFlat;
    /** 16-bit view. */
    struct
    {
        uint16_t    uLow;
        uint16_t    uHigh;
    } u;
#if ARCH_BITS == 16
    /** 16-bit near pointer. */
    void __near    *pvNear;
#elif ARCH_BITS == 32
    /** 32-bit pointer. */
    void           *pvRaw;
#endif
} BS3XPTR;
#pragma pack()


/** @def BS3_XPTR_DEF_INTERNAL
 * Internal worker.
 *
 * @param   a_Scope     RT_NOTHING if structure or global, static or extern
 *                      otherwise.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 * @internal
 */
#if ARCH_BITS == 16
# define BS3_XPTR_DEF_INTERNAL(a_Scope, a_Type, a_Name) \
    a_Scope union \
    { \
        BS3XPTR         XPtr; \
        a_Type __near  *pNearTyped; \
    } a_Name
#elif ARCH_BITS == 32
# define BS3_XPTR_DEF_INTERNAL(a_Scope, a_Type, a_Name) \
    a_Scope union \
    { \
        BS3XPTR         XPtr; \
        a_Type         *pTyped; \
    } a_Name
#elif ARCH_BITS == 64
# define BS3_XPTR_DEF_INTERNAL(a_Scope, a_Type, a_Name) \
    a_Scope union \
    { \
        BS3XPTR         XPtr; \
        a_Type         *pTyped; \
    } a_Name
#else
# error "ARCH_BITS"
#endif

/** @def BS3_XPTR_DEF_MEMBER
 * Defines a pointer member that can be shared by all CPU modes.
 *
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#define BS3_XPTR_MEMBER(a_Type, a_Name) BS3_XPTR_DEF_INTERNAL(RT_NOTHING, a_Type, a_Name)

/** @def BS3_XPTR_DEF_AUTO
 * Defines a pointer static variable for working with an XPTR.
 *
 * This is typically used to convert flat pointers into context specific
 * pointers.
 *
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#define BS3_XPTR_AUTO(a_Type, a_Name) BS3_XPTR_DEF_INTERNAL(RT_NOTHING, a_Type, a_Name)

/** @def BS3_XPTR_SET
 * Sets a cross context pointer.
 *
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 * @param   a_uFlatPtr  The flat pointer value to assign.  If the x-pointer is
 *                      used in real mode, this must be less than 1MB.
 *                      Otherwise the limit is 16MB (due to selector tiling).
 */
#define BS3_XPTR_SET_FLAT(a_Type, a_Name, a_uFlatPtr) \
    do { a_Name.XPtr.uFlat = (a_uFlatPtr); } while (0)

/** @def BS3_XPTR_GET_FLAT
 * Gets the flat address of a cross context pointer.
 *
 * @returns 32-bit flat pointer.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#define BS3_XPTR_GET_FLAT(a_Type, a_Name) (a_Name.XPtr.uFlat)

/** @def BS3_XPTR_GET_FLAT_LOW
 * Gets the low 16 bits of the flat address.
 *
 * @returns Low 16 bits of the flat pointer.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#define BS3_XPTR_GET_FLAT_LOW(a_Type, a_Name) (a_Name.XPtr.u.uLow)


#if ARCH_BITS == 16

/**
 * Gets the current ring number.
 * @returns Ring number.
 */
DECLINLINE(uint16_t) Bs3Sel16GetCurRing(void);
# pragma aux Bs3Sel16GetCurRing = \
            "mov ax, ss" \
            "and ax, 3" \
            value [ax] modify exact [ax] nomemory;

/**
 * Converts the high word of a flat pointer into a 16-bit selector.
 *
 * This makes use of the tiled area.  It also handles real mode.
 *
 * @returns Segment selector value.
 * @param   uHigh       The high part of flat pointer.
 * @sa BS3_XPTR_GET, BS3_XPTR_SET
 */
DECLINLINE(__segment) Bs3Sel16HighFlatPtrToSelector(uint16_t uHigh)
{
    if (BS3_IS_PROTECTED_MODE())
        return (__segment)(((uHigh << 3) + BS3_SEL_TILED) | Bs3Sel16GetCurRing());
    return (__segment)(uHigh << 12);
}

#endif /* ARCH_BITS == 16*/

/** @def BS3_XPTR_GET
 * Gets the current context pointer value.
 *
 * @returns Usable pointer.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#if ARCH_BITS == 16
# define BS3_XPTR_GET(a_Type, a_Name) \
    ((a_Type BS3_FAR *)BS3_FP_MAKE(Bs3Sel16HighFlatPtrToSelector((a_Name).XPtr.u.uHigh), (a_Name).pNearTyped))
#elif ARCH_BITS == 32
# define BS3_XPTR_GET(a_Type, a_Name)       ((a_Name).pTyped)
#elif ARCH_BITS == 64
# define BS3_XPTR_GET(a_Type, a_Name)       ((a_Type *)(uintptr_t)(a_Name).XPtr.uFlat)
#else
# error "ARCH_BITS"
#endif

/** @def BS3_XPTR_SET
 * Gets the current context pointer value.
 *
 * @returns Usable pointer.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 * @param   a_pValue    The new pointer value, current context pointer.
 */
#if ARCH_BITS == 16
# define BS3_XPTR_SET(a_Type, a_Name, a_pValue) \
    do { \
        a_Type BS3_FAR *pTypeCheck = (a_pValue); \
        if (BS3_IS_PROTECTED_MODE()) \
        { \
            (a_Name).XPtr.u.uLow  = BS3_FP_OFF(pTypeCheck); \
            (a_Name).XPtr.u.uHigh = ((BS3_FP_SEG(pTypeCheck) & UINT16_C(0xfff8)) - BS3_SEL_TILED) >> 3; \
        } \
        else \
            (a_Name).XPtr.uFlat = BS3_FP_OFF(pTypeCheck) + ((uint32_t)BS3_FP_SEG(pTypeCheck) << 4); \
    } while (0)
#elif ARCH_BITS == 32
# define BS3_XPTR_SET(a_Type, a_Name, a_pValue) \
    do { (a_Name).pTyped = (a_pValue); } while (0)
#elif ARCH_BITS == 64
# define BS3_XPTR_SET(a_Type, a_Name, a_pValue) \
    do { \
        a_Type *pTypeCheck  = (a_pValue);  \
        (a_Name).XPtr.uFlat = (uint32_t)(uintptr_t)pTypeCheck; \
    } while (0)
#else
# error "ARCH_BITS"
#endif


/** @def BS3_XPTR_IS_NULL
 * Checks if the cross context pointer is NULL.
 *
 * @returns true if NULL, false if not.
 * @param   a_Type      The type we're pointing to.
 * @param   a_Name      The member or variable name.
 */
#define BS3_XPTR_IS_NULL(a_Type, a_Name)    ((a_Name).XPtr.uFlat == 0)

/**
 * Gets a working pointer from a 32-bit flat address.
 *
 * @returns Current context pointer.
 * @param   uFlatPtr    The flat address to convert.
 */
DECLINLINE(void BS3_FAR *) Bs3XptrFlatToCurrent(uint32_t uFlatPtr)
{
    BS3_XPTR_AUTO(void, pTmp);
    BS3_XPTR_SET_FLAT(void, pTmp, uFlatPtr);
    return BS3_XPTR_GET(void, pTmp);
}

/** @} */



/** @defgroup grp_bs3kit_cmn    Common Functions and Data
 *
 * The common functions comes in three variations: 16-bit, 32-bit and 64-bit.
 * Templated code uses the #BS3_CMN_NM macro to mangle the name according to the
 * desired
 *
 * @{
 */

#ifdef BS3_STRICT
# define BS3_ASSERT(a_Expr) do { if (!!(a_Expr)) { /* likely */ } else { Bs3Panic(); } } while (0) /**< @todo later */
#else
# define BS3_ASSERT(a_Expr) do { } while (0)
#endif

/**
 * Panic, never return.
 *
 * The current implementation will only halt the CPU.
 */
BS3_DECL(DECL_NO_RETURN(void)) Bs3Panic_c16(void);
BS3_DECL(DECL_NO_RETURN(void)) Bs3Panic_c32(void); /**< @copydoc Bs3Panic_c16  */
BS3_DECL(DECL_NO_RETURN(void)) Bs3Panic_c64(void); /**< @copydoc Bs3Panic_c16  */
#define Bs3Panic BS3_CMN_NM(Bs3Panic) /**< Selects #Bs3Panic_c16, #Bs3Panic_c32 or #Bs3Panic_c64. */
#if !defined(BS3_KIT_WITH_NO_RETURN) && defined(__WATCOMC__)
# pragma aux Bs3Panic_c16 __aborts
# pragma aux Bs3Panic_c32 __aborts
#endif

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
 * Prints a 32-bit unsigned value as decimal to the screen.
 *
 * @param   uValue      The 32-bit value.
 */
BS3_DECL(void) Bs3PrintU32_c16(uint32_t uValue);
BS3_DECL(void) Bs3PrintU32_c32(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c64(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
#define Bs3PrintU32 BS3_CMN_NM(Bs3PrintU32) /**< Selects #Bs3PrintU32_c16, #Bs3PrintU32_c32 or #Bs3PrintU32_c64. */

/**
 * Prints a 32-bit unsigned value as hex to the screen.
 *
 * @param   uValue      The 32-bit value.
 */
BS3_DECL(void) Bs3PrintX32_c16(uint32_t uValue);
BS3_DECL(void) Bs3PrintX32_c32(uint32_t uValue); /**< @copydoc Bs3PrintX32_c16 */
BS3_DECL(void) Bs3PrintX32_c64(uint32_t uValue); /**< @copydoc Bs3PrintX32_c16 */
#define Bs3PrintX32 BS3_CMN_NM(Bs3PrintX32) /**< Selects #Bs3PrintX32_c16, #Bs3PrintX32_c32 or #Bs3PrintX32_c64. */

/**
 * Formats and prints a string to the screen.
 *
 * See #Bs3StrFormatV_c16 for supported format types.
 *
 * @param   pszFormat       The format string.
 * @param   ...             Format arguments.
 */
BS3_DECL(size_t) Bs3Printf_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(size_t) Bs3Printf_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3Printf_c16 */
BS3_DECL(size_t) Bs3Printf_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3Printf_c16 */
#define Bs3Printf BS3_CMN_NM(Bs3Printf) /**< Selects #Bs3Printf_c16, #Bs3Printf_c32 or #Bs3Printf_c64. */

/**
 * Formats and prints a string to the screen, va_list version.
 *
 * See #Bs3Format_c16 for supported format types.
 *
 * @param   pszFormat       The format string.
 * @param   va              Format arguments.
 */
BS3_DECL(size_t) Bs3PrintfV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(size_t) Bs3PrintfV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3PrintfV_c16 */
BS3_DECL(size_t) Bs3PrintfV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3PrintfV_c16 */
#define Bs3PrintfV BS3_CMN_NM(Bs3PrintfV) /**< Selects #Bs3PrintfV_c16, #Bs3PrintfV_c32 or #Bs3PrintfV_c64. */

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
 * An output function for #Bs3StrFormatV.
 *
 * @returns Number of characters written.
 * @param   ch      The character to write. Zero in the final call.
 * @param   pvUser  User argument supplied to #Bs3StrFormatV.
 */
typedef BS3_DECL_CALLBACK(size_t) FNBS3STRFORMATOUTPUT(char ch, void BS3_FAR *pvUser);
/** Pointer to an output function for #Bs3StrFormatV. */
typedef FNBS3STRFORMATOUTPUT *PFNBS3STRFORMATOUTPUT;

/**
 * Formats a string, sending the output to @a pfnOutput.
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
 * @returns Sum of @a pfnOutput return values.
 * @param   pszFormat   The format string.
 * @param   va          Format arguments.
 * @param   pfnOutput   The output function.
 * @param   pvUser      The user argument for the output function.
 */
BS3_DECL(size_t) Bs3StrFormatV_c16(const char BS3_FAR *pszFormat, va_list va,
                                   PFNBS3STRFORMATOUTPUT pfnOutput, void BS3_FAR *pvUser);
/** @copydoc Bs3StrFormatV_c16  */
BS3_DECL(size_t) Bs3StrFormatV_c32(const char BS3_FAR *pszFormat, va_list va,
                                   PFNBS3STRFORMATOUTPUT pfnOutput, void BS3_FAR *pvUser);
/** @copydoc Bs3StrFormatV_c16  */
BS3_DECL(size_t) Bs3StrFormatV_c64(const char BS3_FAR *pszFormat, va_list va,
                                   PFNBS3STRFORMATOUTPUT pfnOutput, void BS3_FAR *pvUser);
#define Bs3StrFormatV BS3_CMN_NM(Bs3StrFormatV) /**< Selects #Bs3StrFormatV_c16, #Bs3StrFormatV_c32 or #Bs3StrFormatV_c64. */

/**
 * Formats a string into a buffer.
 *
 * See #Bs3Format_c16 for supported format types.
 *
 * @returns The length of the formatted string (excluding terminator).
 *          This will be higher or equal to @c cbBuf in case of an overflow.
 * @param   pszBuf      The output buffer.
 * @param   cbBuf       The size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   va          Format arguments.
 */
BS3_DECL(size_t) Bs3StrPrintfV_c16(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, va_list va);
/** @copydoc Bs3StrPrintfV_c16  */
BS3_DECL(size_t) Bs3StrPrintfV_c32(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, va_list va);
/** @copydoc Bs3StrPrintfV_c16  */
BS3_DECL(size_t) Bs3StrPrintfV_c64(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, va_list va);
#define Bs3StrPrintfV BS3_CMN_NM(Bs3StrPrintfV) /**< Selects #Bs3StrPrintfV_c16, #Bs3StrPrintfV_c32 or #Bs3StrPrintfV_c64. */

/**
 * Formats a string into a buffer.
 *
 * See #Bs3Format_c16 for supported format types.
 *
 * @returns The length of the formatted string (excluding terminator).
 *          This will be higher or equal to @c cbBuf in case of an overflow.
 * @param   pszBuf      The output buffer.
 * @param   cbBuf       The size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   ...         Format arguments.
 */
BS3_DECL(size_t) Bs3StrPrintf_c16(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, ...);
/** @copydoc Bs3StrPrintf_c16  */
BS3_DECL(size_t) Bs3StrPrintf_c32(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, ...);
/** @copydoc Bs3StrPrintf_c16  */
BS3_DECL(size_t) Bs3StrPrintf_c64(char BS3_FAR *pszBuf, size_t cbBuf, const char BS3_FAR *pszFormat, ...);
#define Bs3StrPrintf BS3_CMN_NM(Bs3StrPrintf) /**< Selects #Bs3StrPrintf_c16, #Bs3StrPrintf_c32 or #Bs3StrPrintf_c64. */


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
 * BSD style bzero.
 *
 * @param   pvDst           The buffer to be zeroed.
 * @param   cbDst           The number of bytes to zero.
 */
BS3_DECL(void) Bs3MemZero_c16(void BS3_FAR *pvDst, size_t cbDst);
BS3_DECL(void) Bs3MemZero_c32(void BS3_FAR *pvDst, size_t cbDst); /** @copydoc Bs3MemZero_c16 */
BS3_DECL(void) Bs3MemZero_c64(void BS3_FAR *pvDst, size_t cbDst); /** @copydoc Bs3MemZero_c16 */
#define Bs3MemZero BS3_CMN_NM(Bs3MemZero) /**< Selects #Bs3MemZero_c16, #Bs3MemZero_c32 or #Bs3MemZero_c64. */


/**
 * Converts a protected mode 32-bit far pointer to a 32-bit flat address.
 *
 * @returns 32-bit flat address.
 * @param   off             The segment offset.
 * @param   uSel            The protected mode segment selector.
 */
BS3_DECL(uint32_t) Bs3SelProtFar32ToFlat32_c16(uint32_t off, uint16_t uSel);
BS3_DECL(uint32_t) Bs3SelProtFar32ToFlat32_c32(uint32_t off, uint16_t uSel); /**< @copydoc Bs3SelProtFar32ToFlat32_c16 */
BS3_DECL(uint32_t) Bs3SelProtFar32ToFlat32_c64(uint32_t off, uint16_t uSel); /**< @copydoc Bs3SelProtFar32ToFlat32_c16 */
#define Bs3SelProtFar32ToFlat32 BS3_CMN_NM(Bs3SelProtFar32ToFlat32) /**< Selects #Bs3SelProtFar32ToFlat32_c16, #Bs3SelProtFar32ToFlat32_c32 or #Bs3SelProtFar32ToFlat32_c64. */


/**
 * Converts a current mode 32-bit far pointer to a 32-bit flat address.
 *
 * @returns 32-bit flat address.
 * @param   off             The segment offset.
 * @param   uSel            The current mode segment selector.
 */
BS3_DECL(uint32_t) Bs3SelFar32ToFlat32_c16(uint32_t off, uint16_t uSel);
BS3_DECL(uint32_t) Bs3SelFar32ToFlat32_c32(uint32_t off, uint16_t uSel); /**< @copydoc Bs3SelFar32ToFlat32_c16 */
BS3_DECL(uint32_t) Bs3SelFar32ToFlat32_c64(uint32_t off, uint16_t uSel); /**< @copydoc Bs3SelFar32ToFlat32_c16 */
#define Bs3SelFar32ToFlat32 BS3_CMN_NM(Bs3SelFar32ToFlat32) /**< Selects #Bs3SelFar32ToFlat32_c16, #Bs3SelFar32ToFlat32_c32 or #Bs3SelFar32ToFlat32_c64. */

/**
 * Gets a 32-bit flat address from a working poitner.
 *
 * @returns 32-bit flat address.
 * @param   pv          Current context pointer.
 */
DECLINLINE(uint32_t) Bs3SelPtrToFlat(void BS3_FAR *pv)
{
#if ARCH_BITS == 16
    return Bs3SelFar32ToFlat32(BS3_FP_OFF(pv), BS3_FP_SEG(pv));
#else
    return (uint32_t)(uintptr_t)pv;
#endif
}


/**
 * Slab control structure list head.
 *
 * The slabs on the list must all have the same chunk size.
 */
typedef struct BS3SLABHEAD
{
    /** Pointer to the first slab. */
    BS3_XPTR_MEMBER(struct BS3SLABCTL, pFirst);
    /** The allocation chunk size. */
    uint16_t                        cbChunk;
    /** Number of slabs in the list. */
    uint16_t                        cSlabs;
    /** Number of chunks in the list. */
    uint32_t                        cChunks;
    /** Number of free chunks. */
    uint32_t                        cFreeChunks;
} BS3SLABHEAD;
/** Pointer to a slab list head. */
typedef BS3SLABHEAD BS3_FAR *PBS3SLABHEAD;

/**
 * Allocation slab control structure.
 *
 * This may live at the start of the slab for 4KB slabs, while in a separate
 * static location for the larger ones.
 */
typedef struct BS3SLABCTL
{
    /** Pointer to the next slab control structure in this list. */
    BS3_XPTR_MEMBER(struct BS3SLABCTL, pNext);
    /** Pointer to the slab list head. */
    BS3_XPTR_MEMBER(BS3SLABHEAD,    pHead);
    /** The base address of the slab. */
    BS3_XPTR_MEMBER(uint8_t,        pbStart);
    /** Number of chunks in this slab. */
    uint16_t                        cChunks;
    /** Number of currently free chunks. */
    uint16_t                        cFreeChunks;
    /** The chunk size. */
    uint16_t                        cbChunk;
    /** The shift count corresponding to cbChunk.
     * This is for turning a chunk number into a byte offset and vice versa. */
    uint16_t                        cChunkShift;
    /** Bitmap where set bits indicates allocated blocks (variable size,
     * multiple of 4). */
    uint8_t                         bmAllocated[4];
} BS3SLABCTL;
/** Pointer to a bs3kit slab control structure. */
typedef BS3SLABCTL BS3_FAR *PBS3SLABCTL;

/** The chunks must all be in the same 16-bit segment tile. */
#define BS3_SLAB_ALLOC_F_SAME_TILE      UINT16_C(0x0001)

/**
 * Initializes a slab.
 *
 * @param   pSlabCtl        The slab control structure to initialize.
 * @param   cbSlabCtl       The size of the slab control structure.
 * @param   uFlatSlabPtr    The base address of the slab.
 * @param   cbSlab          The size of the slab.
 * @param   cbChunk         The chunk size.
 */
BS3_DECL(void) Bs3SlabInit_c16(PBS3SLABCTL pSlabCtl, size_t cbSlabCtl, uint32_t uFlatSlabPtr, uint32_t cbSlab, uint16_t cbChunk);
/** @copydoc Bs3SlabInit_c16 */
BS3_DECL(void) Bs3SlabInit_c32(PBS3SLABCTL pSlabCtl, size_t cbSlabCtl, uint32_t uFlatSlabPtr, uint32_t cbSlab, uint16_t cbChunk);
/** @copydoc Bs3SlabInit_c16 */
BS3_DECL(void) Bs3SlabInit_c64(PBS3SLABCTL pSlabCtl, size_t cbSlabCtl, uint32_t uFlatSlabPtr, uint32_t cbSlab, uint16_t cbChunk);
#define Bs3SlabInit BS3_CMN_NM(Bs3SlabInit) /**< Selects #Bs3SlabInit_c16, #Bs3SlabInit_c32 or #Bs3SlabInit_c64. */

/**
 * Allocates one chunk from a slab.
 *
 * @returns Pointer to a chunk on success, NULL if we're out of chunks.
 * @param   pSlabCtl        The slab constrol structure to allocate from.
 */
BS3_DECL(void BS3_FAR *) Bs3SlabAlloc_c16(PBS3SLABCTL pSlabCtl);
BS3_DECL(void BS3_FAR *) Bs3SlabAlloc_c32(PBS3SLABCTL pSlabCtl); /**< @copydoc Bs3SlabAlloc_c16 */
BS3_DECL(void BS3_FAR *) Bs3SlabAlloc_c64(PBS3SLABCTL pSlabCtl); /**< @copydoc Bs3SlabAlloc_c16 */
#define Bs3SlabAlloc BS3_CMN_NM(Bs3SlabAlloc) /**< Selects #Bs3SlabAlloc_c16, #Bs3SlabAlloc_c32 or #Bs3SlabAlloc_c64. */

/**
 * Allocates one or more chunks rom a slab.
 *
 * @returns Pointer to the request number of chunks on success, NULL if we're
 *          out of chunks.
 * @param   pSlabCtl        The slab constrol structure to allocate from.
 * @param   cChunks         The number of contiguous chunks we want.
 * @param   fFlags          Flags, see BS3_SLAB_ALLOC_F_XXX
 */
BS3_DECL(void BS3_FAR *) Bs3SlabAllocEx_c16(PBS3SLABCTL pSlabCtl, uint16_t cChunks, uint16_t fFlags);
BS3_DECL(void BS3_FAR *) Bs3SlabAllocEx_c32(PBS3SLABCTL pSlabCtl, uint16_t cChunks, uint16_t fFlags); /**< @copydoc Bs3SlabAllocEx_c16 */
BS3_DECL(void BS3_FAR *) Bs3SlabAllocEx_c64(PBS3SLABCTL pSlabCtl, uint16_t cChunks, uint16_t fFlags); /**< @copydoc Bs3SlabAllocEx_c16 */
#define Bs3SlabAllocEx BS3_CMN_NM(Bs3SlabAllocEx) /**< Selects #Bs3SlabAllocEx_c16, #Bs3SlabAllocEx_c32 or #Bs3SlabAllocEx_c64. */

/**
 * Frees one or more chunks from a slab.
 *
 * @returns Number of chunks actually freed.  When correctly used, this will
 *          match the @a cChunks parameter, of course.
 * @param   pSlabCtl        The slab constrol structure to free from.
 * @param   uFlatChunkPtr   The flat address of the chunks to free.
 * @param   cChunks         The number of contiguous chunks to free.
 */
BS3_DECL(uint16_t) Bs3SlabFree_c16(PBS3SLABCTL pSlabCtl, uint32_t uFlatChunkPtr, uint16_t cChunks);
BS3_DECL(uint16_t) Bs3SlabFree_c32(PBS3SLABCTL pSlabCtl, uint32_t uFlatChunkPtr, uint16_t cChunks); /**< @copydoc Bs3SlabFree_c16 */
BS3_DECL(uint16_t) Bs3SlabFree_c64(PBS3SLABCTL pSlabCtl, uint32_t uFlatChunkPtr, uint16_t cChunks); /**< @copydoc Bs3SlabFree_c16 */
#define Bs3SlabFree BS3_CMN_NM(Bs3SlabFree) /**< Selects #Bs3SlabFree_c16, #Bs3SlabFree_c32 or #Bs3SlabFree_c64. */


/**
 * Initializes the given slab list head.
 *
 * @param   pHead       The slab list head.
 * @param   cbChunk     The chunk size.
 */
BS3_DECL(void) Bs3SlabListInit_c16(PBS3SLABHEAD pHead, uint16_t cbChunk);
BS3_DECL(void) Bs3SlabListInit_c32(PBS3SLABHEAD pHead, uint16_t cbChunk); /**< @copydoc Bs3SlabListInit_c16 */
BS3_DECL(void) Bs3SlabListInit_c64(PBS3SLABHEAD pHead, uint16_t cbChunk); /**< @copydoc Bs3SlabListInit_c16 */
#define Bs3SlabListInit BS3_CMN_NM(Bs3SlabListInit) /**< Selects #Bs3SlabListInit_c16, #Bs3SlabListInit_c32 or #Bs3SlabListInit_c64. */

/**
 * Adds an initialized slab control structure to the list.
 *
 * @param   pHead           The slab list head to add it to.
 * @param   pSlabCtl        The slab control structure to add.
 */
BS3_DECL(void) Bs3SlabListAdd_c16(PBS3SLABHEAD pHead, PBS3SLABCTL pSlabCtl);
/** @copydoc Bs3SlabListAdd_c16 */
BS3_DECL(void) Bs3SlabListAdd_c32(PBS3SLABHEAD pHead, PBS3SLABCTL pSlabCtl);
/** @copydoc Bs3SlabListAdd_c16 */
BS3_DECL(void) Bs3SlabListAdd_c64(PBS3SLABHEAD pHead, PBS3SLABCTL pSlabCtl);
#define Bs3SlabListAdd BS3_CMN_NM(Bs3SlabListAdd) /**< Selects #Bs3SlabListAdd_c16, #Bs3SlabListAdd_c32 or #Bs3SlabListAdd_c64. */

/**
 * Allocates one chunk.
 *
 * @returns Pointer to a chunk on success, NULL if we're out of chunks.
 * @param   pHead           The slab list to allocate from.
 */
BS3_DECL(void BS3_FAR *) Bs3SlabListAlloc_c16(PBS3SLABHEAD pHead);
BS3_DECL(void BS3_FAR *) Bs3SlabListAlloc_c32(PBS3SLABHEAD pHead); /**< @copydoc Bs3SlabListAlloc_c16 */
BS3_DECL(void BS3_FAR *) Bs3SlabListAlloc_c64(PBS3SLABHEAD pHead); /**< @copydoc Bs3SlabListAlloc_c16 */
#define Bs3SlabListAlloc BS3_CMN_NM(Bs3SlabListAlloc) /**< Selects #Bs3SlabListAlloc_c16, #Bs3SlabListAlloc_c32 or #Bs3SlabListAlloc_c64. */

/**
 * Allocates one or more chunks.
 *
 * @returns Pointer to the request number of chunks on success, NULL if we're
 *          out of chunks.
 * @param   pHead           The slab list to allocate from.
 * @param   cChunks         The number of contiguous chunks we want.
 * @param   fFlags          Flags, see BS3_SLAB_ALLOC_F_XXX
 */
BS3_DECL(void BS3_FAR *) Bs3SlabListAllocEx_c16(PBS3SLABHEAD pHead, uint16_t cChunks, uint16_t fFlags);
BS3_DECL(void BS3_FAR *) Bs3SlabListAllocEx_c32(PBS3SLABHEAD pHead, uint16_t cChunks, uint16_t fFlags); /**< @copydoc Bs3SlabListAllocEx_c16 */
BS3_DECL(void BS3_FAR *) Bs3SlabListAllocEx_c64(PBS3SLABHEAD pHead, uint16_t cChunks, uint16_t fFlags); /**< @copydoc Bs3SlabListAllocEx_c16 */
#define Bs3SlabListAllocEx BS3_CMN_NM(Bs3SlabListAllocEx) /**< Selects #Bs3SlabListAllocEx_c16, #Bs3SlabListAllocEx_c32 or #Bs3SlabListAllocEx_c64. */

/**
 * Frees one or more chunks from a slab list.
 *
 * @param   pHead           The slab list to allocate from.
 * @param   pvChunks        Pointer to the first chunk to free.
 * @param   cChunks         The number of contiguous chunks to free.
 */
BS3_DECL(void) Bs3SlabListFree_c16(PBS3SLABHEAD pHead, void BS3_FAR *pvChunks, uint16_t cChunks);
BS3_DECL(void) Bs3SlabListFree_c32(PBS3SLABHEAD pHead, void BS3_FAR *pvChunks, uint16_t cChunks); /**< @copydoc Bs3SlabListFree_c16 */
BS3_DECL(void) Bs3SlabListFree_c64(PBS3SLABHEAD pHead, void BS3_FAR *pvChunks, uint16_t cChunks); /**< @copydoc Bs3SlabListFree_c16 */
#define Bs3SlabListFree BS3_CMN_NM(Bs3SlabListFree) /**< Selects #Bs3SlabListFree_c16, #Bs3SlabListFree_c32 or #Bs3SlabListFree_c64. */

/**
 * Allocation addressing constraints.
 */
typedef enum BS3MEMKIND
{
    /** Invalid zero type. */
    BS3MEMKIND_INVALID = 0,
    /** Real mode addressable memory. */
    BS3MEMKIND_REAL,
    /** Memory addressable using the 16-bit protected mode tiling. */
    BS3MEMKIND_TILED,
    /** Memory addressable using 32-bit flat addressing. */
    BS3MEMKIND_FLAT32,
    /** Memory addressable using 64-bit flat addressing. */
    BS3MEMKIND_FLAT64,
    /** End of valid types. */
    BS3MEMKIND_END,
} BS3MEMKIND;

/**
 * Allocates low memory.
 *
 * @returns Pointer to a chunk on success, NULL if we're out of chunks.
 * @param   enmKind     The kind of addressing constraints imposed on the
 *                      allocation.
 * @param   cb          How much to allocate.  Must be 4KB or less.
 */
BS3_DECL(void BS3_FAR *) Bs3MemAlloc_c16(BS3MEMKIND enmKind, size_t cb);
BS3_DECL(void BS3_FAR *) Bs3MemAlloc_c32(BS3MEMKIND enmKind, size_t cb); /**< @copydoc Bs3MemAlloc_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemAlloc_c64(BS3MEMKIND enmKind, size_t cb); /**< @copydoc Bs3MemAlloc_c16 */
#define Bs3MemAlloc BS3_CMN_NM(Bs3MemAlloc) /**< Selects #Bs3MemAlloc_c16, #Bs3MemAlloc_c32 or #Bs3MemAlloc_c64. */

/**
 * Allocates zero'ed memory.
 *
 * @param   enmKind     The kind of addressing constraints imposed on the
 *                      allocation.
 * @param   cb          How much to allocate.  Must be 4KB or less.
 */
BS3_DECL(void BS3_FAR *) Bs3MemAllocZ_c16(BS3MEMKIND enmKind, size_t cb);
BS3_DECL(void BS3_FAR *) Bs3MemAllocZ_c32(BS3MEMKIND enmKind, size_t cb); /**< @copydoc Bs3MemAllocZ_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemAllocZ_c64(BS3MEMKIND enmKind, size_t cb); /**< @copydoc Bs3MemAllocZ_c16 */
#define Bs3MemAllocZ BS3_CMN_NM(Bs3MemAllocZ) /**< Selects #Bs3MemAllocZ_c16, #Bs3MemAllocZ_c32 or #Bs3MemAllocZ_c64. */

/**
 * Frees memory.
 *
 * @returns Pointer to a chunk on success, NULL if we're out of chunks.
 * @param   pv          The memory to free (returned by #Bs3MemAlloc).
 * @param   cb          The size of the allocation.
 */
BS3_DECL(void) Bs3MemFree_c16(void BS3_FAR *pv, size_t cb);
BS3_DECL(void) Bs3MemFree_c32(void BS3_FAR *pv, size_t cb); /**< @copydoc Bs3MemFree_c16 */
BS3_DECL(void) Bs3MemFree_c64(void BS3_FAR *pv, size_t cb); /**< @copydoc Bs3MemFree_c16 */
#define Bs3MemFree BS3_CMN_NM(Bs3MemFree) /**< Selects #Bs3MemFree_c16, #Bs3MemFree_c32 or #Bs3MemFree_c64. */


/**
 * Enables the A20 gate.
 */
BS3_DECL(void) Bs3A20Enable_c16(void);
BS3_DECL(void) Bs3A20Enable_c32(void); /**< @copydoc Bs3A20Enable_c16 */
BS3_DECL(void) Bs3A20Enable_c64(void); /**< @copydoc Bs3A20Enable_c16 */
#define Bs3A20Enable BS3_CMN_NM(Bs3A20Enable) /**< Selects #Bs3A20Enable_c16, #Bs3A20Enable_c32 or #Bs3A20Enable_c64. */

/**
 * Enables the A20 gate via the keyboard controller
 */
BS3_DECL(void) Bs3A20EnableViaKbd_c16(void);
BS3_DECL(void) Bs3A20EnableViaKbd_c32(void); /**< @copydoc Bs3A20EnableViaKbd_c16 */
BS3_DECL(void) Bs3A20EnableViaKbd_c64(void); /**< @copydoc Bs3A20EnableViaKbd_c16 */
#define Bs3A20EnableViaKbd BS3_CMN_NM(Bs3A20EnableViaKbd) /**< Selects #Bs3A20EnableViaKbd_c16, #Bs3A20EnableViaKbd_c32 or #Bs3A20EnableViaKbd_c64. */

/**
 * Enables the A20 gate via the PS/2 control port A.
 */
BS3_DECL(void) Bs3A20EnableViaPortA_c16(void);
BS3_DECL(void) Bs3A20EnableViaPortA_c32(void); /**< @copydoc Bs3A20EnableViaPortA_c16 */
BS3_DECL(void) Bs3A20EnableViaPortA_c64(void); /**< @copydoc Bs3A20EnableViaPortA_c16 */
#define Bs3A20EnableViaPortA BS3_CMN_NM(Bs3A20EnableViaPortA) /**< Selects #Bs3A20EnableViaPortA_c16, #Bs3A20EnableViaPortA_c32 or #Bs3A20EnableViaPortA_c64. */

/**
 * Disables the A20 gate.
 */
BS3_DECL(void) Bs3A20Disable_c16(void);
BS3_DECL(void) Bs3A20Disable_c32(void); /**< @copydoc Bs3A20Disable_c16 */
BS3_DECL(void) Bs3A20Disable_c64(void); /**< @copydoc Bs3A20Disable_c16 */
#define Bs3A20Disable BS3_CMN_NM(Bs3A20Disable) /**< Selects #Bs3A20Disable_c16, #Bs3A20Disable_c32 or #Bs3A20Disable_c64. */

/**
 * Disables the A20 gate via the keyboard controller
 */
BS3_DECL(void) Bs3A20DisableViaKbd_c16(void);
BS3_DECL(void) Bs3A20DisableViaKbd_c32(void); /**< @copydoc Bs3A20DisableViaKbd_c16 */
BS3_DECL(void) Bs3A20DisableViaKbd_c64(void); /**< @copydoc Bs3A20DisableViaKbd_c16 */
#define Bs3A20DisableViaKbd BS3_CMN_NM(Bs3A20DisableViaKbd) /**< Selects #Bs3A20DisableViaKbd_c16, #Bs3A20DisableViaKbd_c32 or #Bs3A20DisableViaKbd_c64. */

/**
 * Disables the A20 gate via the PS/2 control port A.
 */
BS3_DECL(void) Bs3A20DisableViaPortA_c16(void);
BS3_DECL(void) Bs3A20DisableViaPortA_c32(void); /**< @copydoc Bs3A20DisableViaPortA_c16 */
BS3_DECL(void) Bs3A20DisableViaPortA_c64(void); /**< @copydoc Bs3A20DisableViaPortA_c16 */
#define Bs3A20DisableViaPortA BS3_CMN_NM(Bs3A20DisableViaPortA) /**< Selects #Bs3A20DisableViaPortA_c16, #Bs3A20DisableViaPortA_c32 or #Bs3A20DisableViaPortA_c64. */


/**
 * Initializes root page tables for page protected mode (PP16, PP32).
 *
 * @returns IPRT status code.
 * @remarks Must not be called in real-mode!
 */
BS3_DECL(int) Bs3PagingInitRootForPP_c16(void);
BS3_DECL(int) Bs3PagingInitRootForPP_c32(void); /**< @copydoc Bs3PagingInitRootForPP_c16 */
BS3_DECL(int) Bs3PagingInitRootForPP_c64(void); /**< @copydoc Bs3PagingInitRootForPP_c16 */
#define Bs3PagingInitRootForPP BS3_CMN_NM(Bs3PagingInitRootForPP) /**< Selects #Bs3PagingInitRootForPP_c16, #Bs3PagingInitRootForPP_c32 or #Bs3PagingInitRootForPP_c64. */

/**
 * Initializes root page tables for PAE page protected mode (PAE16, PAE32).
 *
 * @returns IPRT status code.
 * @remarks The default long mode page tables depends on the PAE ones.
 * @remarks Must not be called in real-mode!
 */
BS3_DECL(int) Bs3PagingInitRootForPAE_c16(void);
BS3_DECL(int) Bs3PagingInitRootForPAE_c32(void); /**< @copydoc Bs3PagingInitRootForPAE_c16 */
BS3_DECL(int) Bs3PagingInitRootForPAE_c64(void); /**< @copydoc Bs3PagingInitRootForPAE_c16 */
#define Bs3PagingInitRootForPAE BS3_CMN_NM(Bs3PagingInitRootForPAE) /**< Selects #Bs3PagingInitRootForPAE_c16, #Bs3PagingInitRootForPAE_c32 or #Bs3PagingInitRootForPAE_c64. */

/**
 * Initializes root page tables for long mode (LM16, LM32, LM64).
 *
 * @returns IPRT status code.
 * @remarks The default long mode page tables depends on the PAE ones.
 * @remarks Must not be called in real-mode!
 */
BS3_DECL(int) Bs3PagingInitRootForLM_c16(void);
BS3_DECL(int) Bs3PagingInitRootForLM_c32(void); /**< @copydoc Bs3PagingInitRootForLM_c16 */
BS3_DECL(int) Bs3PagingInitRootForLM_c64(void); /**< @copydoc Bs3PagingInitRootForLM_c16 */
#define Bs3PagingInitRootForLM BS3_CMN_NM(Bs3PagingInitRootForLM) /**< Selects #Bs3PagingInitRootForLM_c16, #Bs3PagingInitRootForLM_c32 or #Bs3PagingInitRootForLM_c64. */


/**
 * Waits for the keyboard controller to become ready.
 */
BS3_DECL(void) Bs3KbdWait_c16(void);
BS3_DECL(void) Bs3KbdWait_c32(void); /**< @copydoc Bs3KbdWait_c16 */
BS3_DECL(void) Bs3KbdWait_c64(void); /**< @copydoc Bs3KbdWait_c16 */
#define Bs3KbdWait BS3_CMN_NM(Bs3KbdWait) /**< Selects #Bs3KbdWait_c16, #Bs3KbdWait_c32 or #Bs3KbdWait_c64. */

/**
 * Sends a read command to the keyboard controller and gets the result.
 *
 * The caller is responsible for making sure the keyboard controller is ready
 * for a command (call #Bs3KbdWait if unsure).
 *
 * @returns      The value read is returned (in al).
 * @param        bCmd            The read command.
 */
BS3_DECL(uint8_t) Bs3KbdRead_c16(uint8_t bCmd);
BS3_DECL(uint8_t) Bs3KbdRead_c32(uint8_t bCmd); /**< @copydoc Bs3KbdRead_c16 */
BS3_DECL(uint8_t) Bs3KbdRead_c64(uint8_t bCmd); /**< @copydoc Bs3KbdRead_c16 */
#define Bs3KbdRead BS3_CMN_NM(Bs3KbdRead) /**< Selects #Bs3KbdRead_c16, #Bs3KbdRead_c32 or #Bs3KbdRead_c64. */

/**
 * Sends a write command to the keyboard controller and then sends the data.
 *
 * The caller is responsible for making sure the keyboard controller is ready
 * for a command (call #Bs3KbdWait if unsure).
 *
 * @param        bCmd           The write command.
 * @param        bData          The data to write.
 */
BS3_DECL(void) Bs3KbdWrite_c16(uint8_t bCmd, uint8_t bData);
BS3_DECL(void) Bs3KbdWrite_c32(uint8_t bCmd, uint8_t bData); /**< @copydoc Bs3KbdWrite_c16 */
BS3_DECL(void) Bs3KbdWrite_c64(uint8_t bCmd, uint8_t bData); /**< @copydoc Bs3KbdWrite_c16 */
#define Bs3KbdWrite BS3_CMN_NM(Bs3KbdWrite) /**< Selects #Bs3KbdWrite_c16, #Bs3KbdWrite_c32 or #Bs3KbdWrite_c64. */


/**
 * BS3 integer register.
 */
typedef union BS3REG
{
    /** 8-bit unsigned integer. */
    uint8_t     u8;
    /** 16-bit unsigned integer. */
    uint16_t    u16;
    /** 32-bit unsigned integer. */
    uint32_t    u32;
    /** 64-bit unsigned integer. */
    uint64_t    u64;
    /** Full unsigned integer. */
    uint64_t    u;
    /** High/low byte view. */
    struct
    {
        uint8_t bLo;
        uint8_t bHi;
    } b;
    /** 8-bit view. */
    uint8_t     au8[8];
    /** 16-bit view. */
    uint16_t    au16[4];
    /** 32-bit view. */
    uint32_t    au32[2];
} BS3REG;
/** Pointer to an integer register. */
typedef BS3REG BS3_FAR *PBS3REG;
/** Pointer to a const integer register. */
typedef BS3REG const BS3_FAR *PCBS3REG;

/**
 * Register context (without FPU).
 */
typedef struct BS3REGCTX
{
    BS3REG      rax;                    /**< 0x00  */
    BS3REG      rcx;                    /**< 0x08  */
    BS3REG      rdx;                    /**< 0x10  */
    BS3REG      rbx;                    /**< 0x18  */
    BS3REG      rsp;                    /**< 0x20  */
    BS3REG      rbp;                    /**< 0x28  */
    BS3REG      rsi;                    /**< 0x30  */
    BS3REG      rdi;                    /**< 0x38  */
    BS3REG      r8;                     /**< 0x40  */
    BS3REG      r9;                     /**< 0x48  */
    BS3REG      r10;                    /**< 0x50  */
    BS3REG      r11;                    /**< 0x58  */
    BS3REG      r12;                    /**< 0x60  */
    BS3REG      r13;                    /**< 0x68  */
    BS3REG      r14;                    /**< 0x70  */
    BS3REG      r15;                    /**< 0x78  */
    BS3REG      rflags;                 /**< 0x80  */
    BS3REG      rip;                    /**< 0x88  */
    uint16_t    cs;                     /**< 0x90  */
    uint16_t    ds;                     /**< 0x92  */
    uint16_t    es;                     /**< 0x94  */
    uint16_t    fs;                     /**< 0x96  */
    uint16_t    gs;                     /**< 0x98  */
    uint16_t    ss;                     /**< 0x9a  */
    uint16_t    tr;                     /**< 0x9c  */
    uint16_t    ldtr;                   /**< 0x9e  */
    uint8_t     bMode;                  /**< 0xa0:  BS3_MODE_XXX. */
    uint8_t     bCpl;                   /**< 0xa1: 0-3, 0 is used for real mode. */
    uint8_t     abPadding[6];           /**< 0xa2  */
    BS3REG      cr0;                    /**< 0xa8  */
    BS3REG      cr2;                    /**< 0xb0  */
    BS3REG      cr3;                    /**< 0xb8  */
    BS3REG      cr4;                    /**< 0xc0  */
} BS3REGCTX;
/** Pointer to a register context. */
typedef BS3REGCTX BS3_FAR *PBS3REGCTX;
/** Pointer to a const register context. */
typedef BS3REGCTX const BS3_FAR *PCBS3REGCTX;

/**
 * Saves the current register context.
 *
 * @param   pRegCtx     Where to store the register context.
 */
BS3_DECL(void) Bs3RegCtxSave_c16(PCBS3REGCTX pRegCtx);
BS3_DECL(void) Bs3RegCtxSave_c32(PCBS3REGCTX pRegCtx); /**< @copydoc Bs3RegCtxSave_c16 */
BS3_DECL(void) Bs3RegCtxSave_c64(PCBS3REGCTX pRegCtx); /**< @copydoc Bs3RegCtxSave_c16 */
#define Bs3RegCtxSave BS3_CMN_NM(Bs3RegCtxSave) /**< Selects #Bs3RegCtxSave_c16, #Bs3RegCtxSave_c32 or #Bs3RegCtxSave_c64. */

/**
 * Transforms a register context to a different ring.
 *
 * @param   pRegCtx     The register context.
 * @param   bRing       The target ring (0..3).
 */
BS3_DECL(void) Bs3RegCtxConvertToRingX_c16(PBS3REGCTX pRegCtx, uint8_t bRing);
BS3_DECL(void) Bs3RegCtxConvertToRingX_c32(PBS3REGCTX pRegCtx, uint8_t bRing); /**< @copydoc Bs3RegCtxConvertToRingX_c16 */
BS3_DECL(void) Bs3RegCtxConvertToRingX_c64(PBS3REGCTX pRegCtx, uint8_t bRing); /**< @copydoc Bs3RegCtxConvertToRingX_c16 */
#define Bs3RegCtxConvertToRingX BS3_CMN_NM(Bs3RegCtxConvertToRingX) /**< Selects #Bs3RegCtxConvertToRingX_c16, #Bs3RegCtxConvertToRingX_c32 or #Bs3RegCtxConvertToRingX_c64. */

/**
 * Restores a register context.
 *
 * @param   pRegCtx     The register context to be restored and resumed.
 * @param   fFlags      BS3REGCTXRESTORE_F_XXX.
 *
 * @remarks Caller must be in ring-0!
 * @remarks Does not return.
 */
BS3_DECL(DECL_NO_RETURN(void)) Bs3RegCtxRestore_c16(PCBS3REGCTX pRegCtx, uint16_t fFlags);
BS3_DECL(DECL_NO_RETURN(void)) Bs3RegCtxRestore_c32(PCBS3REGCTX pRegCtx, uint16_t fFlags); /**< @copydoc Bs3RegCtxRestore_c16 */
BS3_DECL(DECL_NO_RETURN(void)) Bs3RegCtxRestore_c64(PCBS3REGCTX pRegCtx, uint16_t fFlags); /**< @copydoc Bs3RegCtxRestore_c16 */
#define Bs3RegCtxRestore BS3_CMN_NM(Bs3RegCtxRestore) /**< Selects #Bs3RegCtxRestore_c16, #Bs3RegCtxRestore_c32 or #Bs3RegCtxRestore_c64. */
#if !defined(BS3_KIT_WITH_NO_RETURN) && defined(__WATCOMC__)
# pragma aux Bs3RegCtxRestore_c16 "_Bs3RegCtxRestore_aborts_c16" __aborts
# pragma aux Bs3RegCtxRestore_c32 "_Bs3RegCtxRestore_aborts_c32" __aborts
#endif

/** Skip restoring the CRx registers. */
#define BS3REGCTXRESTORE_F_SKIP_CRX     UINT16_C(0x0001)

/**
 * Prints the register context.
 *
 * @param   pRegCtx     The register context to be printed.
 */
BS3_DECL(void) Bs3RegCtxPrint_c16(PCBS3REGCTX pRegCtx);
BS3_DECL(void) Bs3RegCtxPrint_c32(PCBS3REGCTX pRegCtx); /**< @copydoc Bs3RegCtxPrint_c16 */
BS3_DECL(void) Bs3RegCtxPrint_c64(PCBS3REGCTX pRegCtx); /**< @copydoc Bs3RegCtxPrint_c16 */
#define Bs3RegCtxPrint BS3_CMN_NM(Bs3RegCtxPrint) /**< Selects #Bs3RegCtxPrint_c16, #Bs3RegCtxPrint_c32 or #Bs3RegCtxPrint_c64. */


/**
 * Trap frame.
 */
typedef struct BS3TRAPFRAME
{
    /** 0x00: Exception/interrupt number. */
    uint8_t     bXcpt;
    /** 0x01: The size of the IRET frame. */
    uint8_t     cbIretFrame;
    /** 0x02: The handler CS. */
    uint16_t    uHandlerCs;
    /** 0x04: The handler SS. */
    uint16_t    uHandlerSs;
    /** 0x06: Explicit alignment. */
    uint16_t    usAlignment;
    /** 0x08: The handler RSP (pointer to the iret frame, skipping ErrCd). */
    uint64_t    uHandlerRsp;
    /** 0x10: The handler RFLAGS value. */
    uint64_t    fHandlerRfl;
    /** 0x18: The error code (if applicable). */
    uint64_t    uErrCd;
    /** 0x20: The register context. */
    BS3REGCTX   Ctx;
} BS3TRAPFRAME;
/** Pointer to a trap frame. */
typedef BS3TRAPFRAME BS3_FAR *PBS3TRAPFRAME;
/** Pointer to a const trap frame.   */
typedef BS3TRAPFRAME const BS3_FAR *PCBS3TRAPFRAME;



/**
 * Initializes 16-bit (protected mode) trap handling.
 *
 * @remarks Does not install 16-bit trap handling, just initializes the
 *          structures.
 */
BS3_DECL(void) Bs3Trap16Init_c16(void);
BS3_DECL(void) Bs3Trap16Init_c32(void); /**< @copydoc Bs3Trap16Init_c16 */
BS3_DECL(void) Bs3Trap16Init_c64(void); /**< @copydoc Bs3Trap16Init_c16 */
#define Bs3Trap16Init BS3_CMN_NM(Bs3Trap16Init) /**< Selects #Bs3Trap16Init_c16, #Bs3Trap16Init_c32 or #Bs3Trap16Init_c64. */

/**
 * Initializes 16-bit (protected mode) trap handling, extended version.
 *
 * @param   f386Plus    Set if the CPU is 80386 or later and
 *                      extended registers should be saved.  Once initialized
 *                      with this parameter set to @a true, the effect cannot be
 *                      reversed.
 *
 * @remarks Does not install 16-bit trap handling, just initializes the
 *          structures.
 */
BS3_DECL(void) Bs3Trap16InitEx_c16(bool f386Plus);
BS3_DECL(void) Bs3Trap16InitEx_c32(bool f386Plus); /**< @copydoc Bs3Trap16InitEx_c16 */
BS3_DECL(void) Bs3Trap16InitEx_c64(bool f386Plus); /**< @copydoc Bs3Trap16InitEx_c16 */
#define Bs3Trap16InitEx BS3_CMN_NM(Bs3Trap16InitEx) /**< Selects #Bs3Trap16InitEx_c16, #Bs3Trap16InitEx_c32 or #Bs3Trap16InitEx_c64. */

/**
 * Initializes 32-bit trap handling.
 *
 * @remarks Does not install 32-bit trap handling, just initializes the
 *          structures.
 */
BS3_DECL(void) Bs3Trap32Init_c16(void);
BS3_DECL(void) Bs3Trap32Init_c32(void); /**< @copydoc Bs3Trap32Init_c16 */
BS3_DECL(void) Bs3Trap32Init_c64(void); /**< @copydoc Bs3Trap32Init_c16 */
#define Bs3Trap32Init BS3_CMN_NM(Bs3Trap32Init) /**< Selects #Bs3Trap32Init_c16, #Bs3Trap32Init_c32 or #Bs3Trap32Init_c64. */

/**
 * Initializes 64-bit trap handling
 *
 * @remarks Does not install 64-bit trap handling, just initializes the
 *          structures.
 */
BS3_DECL(void) Bs3Trap64Init_c16(void);
BS3_DECL(void) Bs3Trap64Init_c32(void); /**< @copydoc Bs3Trap64Init_c16 */
BS3_DECL(void) Bs3Trap64Init_c64(void); /**< @copydoc Bs3Trap64Init_c16 */
#define Bs3Trap64Init BS3_CMN_NM(Bs3Trap64Init) /**< Selects #Bs3Trap64Init_c16, #Bs3Trap64Init_c32 or #Bs3Trap64Init_c64. */

/**
 * Modifies the 16-bit IDT entry (protected mode) specified by @a iIdt.
 *
 * @param   iIdt        The index of the IDT entry to set.
 * @param   bType       The gate type (X86_SEL_TYPE_SYS_XXX).
 * @param   bDpl        The DPL.
 * @param   uSel        The handler selector.
 * @param   off         The handler offset (if applicable).
 * @param   cParams     The parameter count (for call gates).
 */
BS3_DECL(void) Bs3Trap16SetGate_c16(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint16_t off, uint8_t cParams);
BS3_DECL(void) Bs3Trap16SetGate_c32(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint16_t off, uint8_t cParams); /**< @copydoc Bs3Trap16SetGate_c16 */
BS3_DECL(void) Bs3Trap16SetGate_c64(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint16_t off, uint8_t cParams); /**< @copydoc Bs3Trap16SetGate_c16 */
#define Bs3Trap16SetGate BS3_CMN_NM(Bs3Trap16SetGate) /**< Selects #Bs3Trap16SetGate_c16, #Bs3Trap16SetGate_c32 or #Bs3Trap16SetGate_c64. */

/** The address of Bs3Trap16GenericEntries.
 * Bs3Trap16GenericEntries is an array of interrupt/trap/whatever entry
 * points, 8 bytes each, that will create a register frame and call the generic
 * C compatible trap handlers. */
extern uint32_t BS3_DATA_NM(g_Bs3Trap16GenericEntriesFlatAddr);

/**
 * Modifies the 32-bit IDT entry specified by @a iIdt.
 *
 * @param   iIdt        The index of the IDT entry to set.
 * @param   bType       The gate type (X86_SEL_TYPE_SYS_XXX).
 * @param   bDpl        The DPL.
 * @param   uSel        The handler selector.
 * @param   off         The handler offset (if applicable).
 * @param   cParams     The parameter count (for call gates).
 */
BS3_DECL(void) Bs3Trap32SetGate_c16(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint32_t off, uint8_t cParams);
BS3_DECL(void) Bs3Trap32SetGate_c32(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint32_t off, uint8_t cParams); /**< @copydoc Bs3Trap32SetGate_c16 */
BS3_DECL(void) Bs3Trap32SetGate_c64(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint32_t off, uint8_t cParams); /**< @copydoc Bs3Trap32SetGate_c16 */
#define Bs3Trap32SetGate BS3_CMN_NM(Bs3Trap32SetGate) /**< Selects #Bs3Trap32SetGate_c16, #Bs3Trap32SetGate_c32 or #Bs3Trap32SetGate_c64. */

/** The address of Bs3Trap32GenericEntries.
 * Bs3Trap32GenericEntries is an array of interrupt/trap/whatever entry
 * points, 8 bytes each, that will create a register frame and call the generic
 * C compatible trap handlers. */
extern uint32_t BS3_DATA_NM(g_Bs3Trap32GenericEntriesFlatAddr);

/**
 * Modifies the 64-bit IDT entry specified by @a iIdt.
 *
 * @param   iIdt        The index of the IDT entry to set.
 * @param   bType       The gate type (X86_SEL_TYPE_SYS_XXX).
 * @param   bDpl        The DPL.
 * @param   uSel        The handler selector.
 * @param   off         The handler offset (if applicable).
 * @param   bIst        The interrupt stack to use.
 */
BS3_DECL(void) Bs3Trap64SetGate_c16(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint64_t off, uint8_t bIst);
BS3_DECL(void) Bs3Trap64SetGate_c32(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint64_t off, uint8_t bIst); /**< @copydoc Bs3Trap64SetGate_c16 */
BS3_DECL(void) Bs3Trap64SetGate_c64(uint8_t iIdt, uint8_t bType, uint8_t bDpl, uint16_t uSel, uint64_t off, uint8_t bIst); /**< @copydoc Bs3Trap64SetGate_c16 */
#define Bs3Trap64SetGate BS3_CMN_NM(Bs3Trap64SetGate) /**< Selects #Bs3Trap64SetGate_c16, #Bs3Trap64SetGate_c32 or #Bs3Trap64SetGate_c64. */

/** The address of Bs3Trap64GenericEntries.
 * Bs3Trap64GenericEntries is an array of interrupt/trap/whatever entry
 * points, 8 bytes each, that will create a register frame and call the generic
 * C compatible trap handlers. */
extern uint32_t BS3_DATA_NM(g_Bs3Trap64GenericEntriesFlatAddr);

/**
 * C-style trap handler.
 *
 * Upon return Bs3Trap16ResumeFrame_c16, #Bs3Trap32ResumeFrame_c32, or
 * Bs3Trap64ResumeFrame_c64 will be called depending on the current template
 * context.
 *
 * @param   pTrapFrame  The trap frame.  Registers can be modified.
 */
typedef BS3_DECL_CALLBACK(void) FNBS3TRAPHANDLER(PBS3TRAPFRAME pTrapFrame);
/** Pointer to a trap handler (current template context). */
typedef FNBS3TRAPHANDLER *PFNBS3TRAPHANDLER;

/**
 * Sets a trap handler (C/C++/assembly) for the current bitness.
 *
 * When using a 32-bit IDT, only #Bs3TrapSetHandler_c32 will have any effect.
 * Likewise, when using a 16-bit IDT, only Bs3TrapSetHandler_c16 will make any
 * difference.  Ditto 64-bit.
 *
 * Rational: It's mainly a C API, can't easily mix function pointers from other
 * bit counts in C.  Use assembly helpers or something if that is necessary.
 * Besides, most of the real trap handling goes thru the default handler with
 * help of trap records.
 *
 * @returns Previous handler.
 * @param   iIdt        The index of the IDT entry to set.
 * @param   pfnHandler  Pointer to the handler.
 */
BS3_DECL(PFNBS3TRAPHANDLER) Bs3TrapSetHandler_c16(uint8_t iIdt, PFNBS3TRAPHANDLER pfnHandler);
BS3_DECL(PFNBS3TRAPHANDLER) Bs3TrapSetHandler_c32(uint8_t iIdt, PFNBS3TRAPHANDLER pfnHandler); /**< @copydoc Bs3Trap32SetHandler_c16 */
BS3_DECL(PFNBS3TRAPHANDLER) Bs3TrapSetHandler_c64(uint8_t iIdt, PFNBS3TRAPHANDLER pfnHandler); /**< @copydoc Bs3Trap32SetHandler_c16 */
#define Bs3Trap32SetHandler BS3_CMN_NM(Bs3Trap32SetHandler) /**< Selects #Bs3Trap32SetHandler_c16, #Bs3Trap32SetHandler_c32 or #Bs3Trap32SetHandler_c64. */

/**
 * Default C/C++ trap handler.
 *
 * This will check trap record and panic if no match was found.
 *
 * @param   pTrapFrame      Trap frame of the trap to handle.
 */
BS3_DECL(void) Bs3TrapDefaultHandler_c16(PBS3TRAPFRAME pTrapFrame);
BS3_DECL(void) Bs3TrapDefaultHandler_c32(PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapDefaultHandler_c16 */
BS3_DECL(void) Bs3TrapDefaultHandler_c64(PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapDefaultHandler_c16 */
#define Bs3TrapDefaultHandler BS3_CMN_NM(Bs3TrapDefaultHandler) /**< Selects #Bs3TrapDefaultHandler_c16, #Bs3TrapDefaultHandler_c32 or #Bs3TrapDefaultHandler_c64. */

/**
 * Prints the trap frame (to screen).
 * @param   pTrapFrame      Trap frame to print.
 */
BS3_DECL(void) Bs3TrapPrintFrame_c16(PCBS3TRAPFRAME pTrapFrame);
BS3_DECL(void) Bs3TrapPrintFrame_c32(PCBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapPrintFrame_c16 */
BS3_DECL(void) Bs3TrapPrintFrame_c64(PCBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapPrintFrame_c16 */
#define Bs3TrapPrintFrame BS3_CMN_NM(Bs3TrapPrintFrame) /**< Selects #Bs3TrapPrintFrame_c16, #Bs3TrapPrintFrame_c32 or #Bs3TrapPrintFrame_c64. */

/**
 * Sets up a long jump from a trap handler.
 *
 * The long jump will only be performed onced, but will catch any kind of trap,
 * fault, interrupt or irq.
 *
 * @retval  true on the initial call.
 * @retval  false on trap return.
 * @param   pTrapFrame      Where to store the trap information when
 *                          returning @c false.
 * @sa      #Bs3TrapUnsetJmp
 */
BS3_DECL(DECL_RETURNS_TWICE(bool)) Bs3TrapSetJmp_c16(PBS3TRAPFRAME pTrapFrame);
BS3_DECL(DECL_RETURNS_TWICE(bool)) Bs3TrapSetJmp_c32(PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapSetJmp_c16 */
BS3_DECL(DECL_RETURNS_TWICE(bool)) Bs3TrapSetJmp_c64(PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapSetJmp_c16 */
#define Bs3TrapSetJmp BS3_CMN_NM(Bs3TrapSetJmp) /**< Selects #Bs3TrapSetJmp_c16, #Bs3TrapSetJmp_c32 or #Bs3TrapSetJmp_c64. */

/**
 * Combination of #Bs3TrapSetJmp and #Bs3RegCtxRestore.
 *
 * @param   pCtxRestore     The context to restore.
 * @param   pTrapFrame      Where to store the trap information.
 */
BS3_DECL(void) Bs3TrapSetJmpAndRestore_c16(PCBS3REGCTX pCtxRestore, PBS3TRAPFRAME pTrapFrame);
BS3_DECL(void) Bs3TrapSetJmpAndRestore_c32(PCBS3REGCTX pCtxRestore, PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapSetJmpAndRestore_c16 */
BS3_DECL(void) Bs3TrapSetJmpAndRestore_c64(PCBS3REGCTX pCtxRestore, PBS3TRAPFRAME pTrapFrame); /**< @copydoc Bs3TrapSetJmpAndRestore_c16 */
#define Bs3TrapSetJmpAndRestore BS3_CMN_NM(Bs3TrapSetJmpAndRestore) /**< Selects #Bs3TrapSetJmpAndRestore_c16, #Bs3TrapSetJmpAndRestore_c32 or #Bs3TrapSetJmpAndRestore_c64. */

/**
 * Disables a previous #Bs3TrapSetJmp call.
 */
BS3_DECL(void) Bs3TrapUnsetJmp_c16(void);
BS3_DECL(void) Bs3TrapUnsetJmp_c32(void); /**< @copydoc Bs3TrapUnsetJmp_c16 */
BS3_DECL(void) Bs3TrapUnsetJmp_c64(void); /**< @copydoc Bs3TrapUnsetJmp_c16 */
#define Bs3TrapUnsetJmp BS3_CMN_NM(Bs3TrapUnsetJmp) /**< Selects #Bs3TrapUnsetJmp_c16, #Bs3TrapUnsetJmp_c32 or #Bs3TrapUnsetJmp_c64. */


/**
 * Equivalent to RTTestCreate + RTTestBanner.
 *
 * @param   pszTest         The test name.
 */
BS3_DECL(void) Bs3TestInit_c16(const char BS3_FAR *pszTest);
BS3_DECL(void) Bs3TestInit_c32(const char BS3_FAR *pszTest); /**< @copydoc Bs3TestInit_c16 */
BS3_DECL(void) Bs3TestInit_c64(const char BS3_FAR *pszTest); /**< @copydoc Bs3TestInit_c16 */
#define Bs3TestInit BS3_CMN_NM(Bs3TestInit) /**< Selects #Bs3TestInit_c16, #Bs3TestInit_c32 or #Bs3TestInit_c64. */


/**
 * Equivalent to RTTestSummaryAndDestroy.
 */
BS3_DECL(void) Bs3TestTerm_c16(void);
BS3_DECL(void) Bs3TestTerm_c32(void); /**< @copydoc Bs3TestTerm_c16 */
BS3_DECL(void) Bs3TestTerm_c64(void); /**< @copydoc Bs3TestTerm_c16 */
#define Bs3TestTerm BS3_CMN_NM(Bs3TestTerm) /**< Selects #Bs3TestTerm_c16, #Bs3TestTerm_c32 or #Bs3TestTerm_c64. */

/**
 * Equivalent to RTTestISub.
 */
BS3_DECL(void) Bs3TestSub_c16(const char BS3_FAR *pszSubTest);
BS3_DECL(void) Bs3TestSub_c32(const char BS3_FAR *pszSubTest); /**< @copydoc Bs3TestSub_c16 */
BS3_DECL(void) Bs3TestSub_c64(const char BS3_FAR *pszSubTest); /**< @copydoc Bs3TestSub_c16 */
#define Bs3TestSub BS3_CMN_NM(Bs3TestSub) /**< Selects #Bs3TestSub_c16, #Bs3TestSub_c32 or #Bs3TestSub_c64. */

/**
 * Equivalent to RTTestIFailedF.
 */
BS3_DECL(void) Bs3TestSubF_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(void) Bs3TestSubF_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestSubF_c16 */
BS3_DECL(void) Bs3TestSubF_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestSubF_c16 */
#define Bs3TestSubF BS3_CMN_NM(Bs3TestSubF) /**< Selects #Bs3TestSubF_c16, #Bs3TestSubF_c32 or #Bs3TestSubF_c64. */

/**
 * Equivalent to RTTestISubV.
 */
BS3_DECL(void) Bs3TestSubV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(void) Bs3TestSubV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestSubV_c16 */
BS3_DECL(void) Bs3TestSubV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestSubV_c16 */
#define Bs3TestSubV BS3_CMN_NM(Bs3TestSubV) /**< Selects #Bs3TestSubV_c16, #Bs3TestSubV_c32 or #Bs3TestSubV_c64. */

/**
 * Equivalent to RTTestISubDone.
 */
BS3_DECL(void) Bs3TestSubDone_c16(void);
BS3_DECL(void) Bs3TestSubDone_c32(void); /**< @copydoc Bs3TestSubDone_c16 */
BS3_DECL(void) Bs3TestSubDone_c64(void); /**< @copydoc Bs3TestSubDone_c16 */
#define Bs3TestSubDone BS3_CMN_NM(Bs3TestSubDone) /**< Selects #Bs3TestSubDone_c16, #Bs3TestSubDone_c32 or #Bs3TestSubDone_c64. */

/**
 * Equivalent to RTTestSubErrorCount.
 */
BS3_DECL(uint16_t) Bs3TestSubErrorCount_c16(void);
BS3_DECL(uint16_t) Bs3TestSubErrorCount_c32(void); /**< @copydoc Bs3TestSubErrorCount_c16 */
BS3_DECL(uint16_t) Bs3TestSubErrorCount_c64(void); /**< @copydoc Bs3TestSubErrorCount_c16 */
#define Bs3TestSubErrorCount BS3_CMN_NM(Bs3TestSubErrorCount) /**< Selects #Bs3TestSubErrorCount_c16, #Bs3TestSubErrorCount_c32 or #Bs3TestSubErrorCount_c64. */

/**
 * Equivalent to RTTestIPrintf with RTTESTLVL_ALWAYS.
 *
 * @param   pszFormat   What to print, format string.  Explicit newline char.
 * @param   ...         String format arguments.
 */
BS3_DECL(void) Bs3TestPrintf_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(void) Bs3TestPrintf_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestPrintf_c16 */
BS3_DECL(void) Bs3TestPrintf_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestPrintf_c16 */
#define Bs3TestPrintf BS3_CMN_NM(Bs3TestPrintf) /**< Selects #Bs3TestPrintf_c16, #Bs3TestPrintf_c32 or #Bs3TestPrintf_c64. */

/**
 * Equivalent to RTTestIPrintfV with RTTESTLVL_ALWAYS.
 *
 * @param   pszFormat   What to print, format string.  Explicit newline char.
 * @param   va          String format arguments.
 */
BS3_DECL(void) Bs3TestPrintfV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(void) Bs3TestPrintfV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestPrintfV_c16 */
BS3_DECL(void) Bs3TestPrintfV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestPrintfV_c16 */
#define Bs3TestPrintfV BS3_CMN_NM(Bs3TestPrintfV) /**< Selects #Bs3TestPrintfV_c16, #Bs3TestPrintfV_c32 or #Bs3TestPrintfV_c64. */

/**
 * Equivalent to RTTestIFailed.
 */
BS3_DECL(void) Bs3TestFailed_c16(const char BS3_FAR *pszMessage);
BS3_DECL(void) Bs3TestFailed_c32(const char BS3_FAR *pszMessage); /**< @copydoc Bs3TestFailed_c16 */
BS3_DECL(void) Bs3TestFailed_c64(const char BS3_FAR *pszMessage); /**< @copydoc Bs3TestFailed_c16 */
#define Bs3TestFailed BS3_CMN_NM(Bs3TestFailed) /**< Selects #Bs3TestFailed_c16, #Bs3TestFailed_c32 or #Bs3TestFailed_c64. */

/**
 * Equivalent to RTTestIFailedF.
 */
BS3_DECL(void) Bs3TestFailedF_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(void) Bs3TestFailedF_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestFailedF_c16 */
BS3_DECL(void) Bs3TestFailedF_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestFailedF_c16 */
#define Bs3TestFailedF BS3_CMN_NM(Bs3TestFailedF) /**< Selects #Bs3TestFailedF_c16, #Bs3TestFailedF_c32 or #Bs3TestFailedF_c64. */

/**
 * Equivalent to RTTestIFailedV.
 */
BS3_DECL(void) Bs3TestFailedV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(void) Bs3TestFailedV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestFailedV_c16 */
BS3_DECL(void) Bs3TestFailedV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestFailedV_c16 */
#define Bs3TestFailedV BS3_CMN_NM(Bs3TestFailedV) /**< Selects #Bs3TestFailedV_c16, #Bs3TestFailedV_c32 or #Bs3TestFailedV_c64. */

/**
 * Equivalent to RTTestISkipped.
 *
 * @param   pszWhy          Optional reason why it's being skipped.
 */
BS3_DECL(void) Bs3TestSkipped_c16(const char BS3_FAR *pszWhy);
BS3_DECL(void) Bs3TestSkipped_c32(const char BS3_FAR *pszWhy); /**< @copydoc Bs3TestSkipped_c16 */
BS3_DECL(void) Bs3TestSkipped_c64(const char BS3_FAR *pszWhy); /**< @copydoc Bs3TestSkipped_c16 */
#define Bs3TestSkipped BS3_CMN_NM(Bs3TestSkipped) /**< Selects #Bs3TestSkipped_c16, #Bs3TestSkipped_c32 or #Bs3TestSkipped_c64. */

/**
 * Equivalent to RTTestISkippedF.
 *
 * @param   pszFormat       Optional reason why it's being skipped.
 * @param   ...             Format arguments.
 */
BS3_DECL(void) Bs3TestSkippedF_c16(const char BS3_FAR *pszFormat, ...);
BS3_DECL(void) Bs3TestSkippedF_c32(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestSkippedF_c16 */
BS3_DECL(void) Bs3TestSkippedF_c64(const char BS3_FAR *pszFormat, ...); /**< @copydoc Bs3TestSkippedF_c16 */
#define Bs3TestSkippedF BS3_CMN_NM(Bs3TestSkippedF) /**< Selects #Bs3TestSkippedF_c16, #Bs3TestSkippedF_c32 or #Bs3TestSkippedF_c64. */

/**
 * Equivalent to RTTestISkippedV.
 *
 * @param   pszFormat       Optional reason why it's being skipped.
 * @param   va              Format arguments.
 */
BS3_DECL(void) Bs3TestSkippedV_c16(const char BS3_FAR *pszFormat, va_list va);
BS3_DECL(void) Bs3TestSkippedV_c32(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestSkippedV_c16 */
BS3_DECL(void) Bs3TestSkippedV_c64(const char BS3_FAR *pszFormat, va_list va); /**< @copydoc Bs3TestSkippedV_c16 */
#define Bs3TestSkippedV BS3_CMN_NM(Bs3TestSkippedV) /**< Selects #Bs3TestSkippedV_c16, #Bs3TestSkippedV_c32 or #Bs3TestSkippedV_c64. */

/**
 * Compares two register contexts, with PC and SP adjustments.
 *
 * Differences will be reported as test failures.
 *
 * @returns true if equal, false if not.
 * @param   pActualCtx      The actual register context.
 * @param   pExpectedCtx    Expected register context.
 * @param   cbPcAdjust      Program counter adjustment (applied to pExpectedCtx).
 * @param   cbSpAdjust      Stack pointer adjustment (applied to pExpectedCtx).
 * @param   pszMode         CPU mode or some other helpful text.
 * @param   idTestStep      Test step identifier.
 */
BS3_DECL(bool) Bs3TestCheckRegCtxEx_c16(PCBS3REGCTX pActualCtx, PCBS3REGCTX pExpectedCtx, uint16_t cbPcAdjust,
                                        int16_t cbSpAdjust, const char *pszMode, uint16_t idTestStep);
BS3_DECL(bool) Bs3TestCheckRegCtxEx_c32(PCBS3REGCTX pActualCtx, PCBS3REGCTX pExpectedCtx, uint16_t cbPcAdjust,
                                        int16_t cbSpAdjust, const char *pszMode, uint16_t idTestStep); /** @copydoc Bs3TestCheckRegCtxEx_c16 */
BS3_DECL(bool) Bs3TestCheckRegCtxEx_c64(PCBS3REGCTX pActualCtx, PCBS3REGCTX pExpectedCtx, uint16_t cbPcAdjust,
                                        int16_t cbSpAdjust, const char *pszMode, uint16_t idTestStep); /** @copydoc Bs3TestCheckRegCtxEx_c16 */
#define Bs3TestCheckRegCtxEx BS3_CMN_NM(Bs3TestCheckRegCtxEx) /**< Selects #Bs3TestCheckRegCtxEx_c16, #Bs3TestCheckRegCtxEx_c32 or #Bs3TestCheckRegCtxEx_c64. */

/**
 * Performs the testing for the given mode.
 *
 * This is called with the CPU already switch to that mode.
 *
 * @returns 0 on success or directly Bs3TestFailed calls, non-zero to indicate
 *          where the test when wrong. Special value BS3TESTDOMODE_SKIPPED
 *          should be returned to indicate that the test has been skipped.
 * @param   bMode       The current CPU mode.
 */
typedef BS3_DECL_CALLBACK(uint8_t) FNBS3TESTDOMODE(uint8_t bMode);
/** Near pointer to a test (for 16-bit code). */
typedef FNBS3TESTDOMODE               *PFNBS3TESTDOMODE;
/** Far pointer to a test (for 32-bit and 64-bit code, will be flatten). */
typedef FNBS3TESTDOMODE BS3_FAR_CODE  *FPFNBS3TESTDOMODE;

/** Special FNBS3TESTDOMODE return code for indicating a skipped mode test.  */
#define BS3TESTDOMODE_SKIPPED       UINT8_MAX

/**
 * Mode sub-test entry.
 *
 * This can only be passed around to functions with the same bit count, as it
 * contains function pointers.  In 16-bit mode, the 16-bit pointers are near and
 * implies BS3TEXT16, whereas the 32-bit and 64-bit pointers are far real mode
 * addresses that will be converted to flat address prior to calling them.
 * Similarly, in 32-bit and 64-bit the addresses are all flat and the 16-bit
 * ones will be converted to BS3TEXT16 based addresses prior to calling.
 */
typedef struct BS3TESTMODEENTRY
{
    const char * BS3_FAR    pszSubTest;

    PFNBS3TESTDOMODE        pfnDoRM;

    PFNBS3TESTDOMODE        pfnDoPE16;
    FPFNBS3TESTDOMODE       pfnDoPE16_32;
    PFNBS3TESTDOMODE        pfnDoPE16_V86;
    FPFNBS3TESTDOMODE       pfnDoPE32;
    PFNBS3TESTDOMODE        pfnDoPE32_16;
    PFNBS3TESTDOMODE        pfnDoPEV86;

    PFNBS3TESTDOMODE        pfnDoPP16;
    FPFNBS3TESTDOMODE       pfnDoPP16_32;
    PFNBS3TESTDOMODE        pfnDoPP16_V86;
    FPFNBS3TESTDOMODE       pfnDoPP32;
    PFNBS3TESTDOMODE        pfnDoPP32_16;
    PFNBS3TESTDOMODE        pfnDoPPV86;

    PFNBS3TESTDOMODE        pfnDoPAE16;
    FPFNBS3TESTDOMODE       pfnDoPAE16_32;
    PFNBS3TESTDOMODE        pfnDoPAE16_V86;
    FPFNBS3TESTDOMODE       pfnDoPAE32;
    PFNBS3TESTDOMODE        pfnDoPAE32_16;
    PFNBS3TESTDOMODE        pfnDoPAEV86;

    PFNBS3TESTDOMODE        pfnDoLM16;
    FPFNBS3TESTDOMODE       pfnDoLM32;
    FPFNBS3TESTDOMODE       pfnDoLM64;

} BS3TESTMODEENTRY;
/** Pointer to a mode sub-test entry. */
typedef BS3TESTMODEENTRY const *PCBS3TESTMODEENTRY;

/** Produces a BS3TESTMODEENTRY initializer for common (c16,c32,c64) test
 *  functions. */
#define BS3TESTMODEENTRY_CMN(a_szTest, a_BaseNm) \
    {   /*pszSubTest =*/ a_szTest, \
        /*RM*/        RT_CONCAT(a_BaseNm, _c16), \
        /*PE16*/      RT_CONCAT(a_BaseNm, _c16), \
        /*PE16_32*/   RT_CONCAT(a_BaseNm, _c32), \
        /*PE16_V86*/  RT_CONCAT(a_BaseNm, _c16), \
        /*PE32*/      RT_CONCAT(a_BaseNm, _c32), \
        /*PE32_16*/   RT_CONCAT(a_BaseNm, _c16), \
        /*PEV86*/     RT_CONCAT(a_BaseNm, _c16), \
        /*PP16*/      RT_CONCAT(a_BaseNm, _c16), \
        /*PP16_32*/   RT_CONCAT(a_BaseNm, _c32), \
        /*PP16_V86*/  RT_CONCAT(a_BaseNm, _c16), \
        /*PP32*/      RT_CONCAT(a_BaseNm, _c32), \
        /*PP32_16*/   RT_CONCAT(a_BaseNm, _c16), \
        /*PPV86*/     RT_CONCAT(a_BaseNm, _c16), \
        /*PAE16*/     RT_CONCAT(a_BaseNm, _c16), \
        /*PAE16_32*/  RT_CONCAT(a_BaseNm, _c32), \
        /*PAE16_V86*/ RT_CONCAT(a_BaseNm, _c16), \
        /*PAE32*/     RT_CONCAT(a_BaseNm, _c32), \
        /*PAE32_16*/  RT_CONCAT(a_BaseNm, _c16), \
        /*PAEV86*/    RT_CONCAT(a_BaseNm, _c16), \
        /*LM16*/      RT_CONCAT(a_BaseNm, _c16), \
        /*LM32*/      RT_CONCAT(a_BaseNm, _c32), \
        /*LM64*/      RT_CONCAT(a_BaseNm, _c64), \
    }

/** A set of standard protypes to go with #BS3TESTMODEENTRY_CMN. */
#define BS3TESTMODE_PROTOTYPES_CMN(a_BaseNm) \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _c16); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _c32); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _c64)

/** Produces a BS3TESTMODEENTRY initializer for a full set of mode test
 *  functions. */
#define BS3TESTMODEENTRY_MODE(a_szTest, a_BaseNm) \
    {   /*pszSubTest =*/ a_szTest, \
        /*RM*/        RT_CONCAT(a_BaseNm, _rm), \
        /*PE16*/      RT_CONCAT(a_BaseNm, _pe16), \
        /*PE16_32*/   RT_CONCAT(a_BaseNm, _pe16_32), \
        /*PE16_V86*/  RT_CONCAT(a_BaseNm, _pe16_v86), \
        /*PE32*/      RT_CONCAT(a_BaseNm, _pe32), \
        /*PE32_16*/   RT_CONCAT(a_BaseNm, _pe32_16), \
        /*PEV86*/     RT_CONCAT(a_BaseNm, _pev86), \
        /*PP16*/      RT_CONCAT(a_BaseNm, _pp16), \
        /*PP16_32*/   RT_CONCAT(a_BaseNm, _pp16_32), \
        /*PP16_V86*/  RT_CONCAT(a_BaseNm, _pp16_v86), \
        /*PP32*/      RT_CONCAT(a_BaseNm, _pp32), \
        /*PP32_16*/   RT_CONCAT(a_BaseNm, _pp32_16), \
        /*PPV86*/     RT_CONCAT(a_BaseNm, _ppv86), \
        /*PAE16*/     RT_CONCAT(a_BaseNm, _pae16), \
        /*PAE16_32*/  RT_CONCAT(a_BaseNm, _pae16_32), \
        /*PAE16_V86*/ RT_CONCAT(a_BaseNm, _pae16_v86), \
        /*PAE32*/     RT_CONCAT(a_BaseNm, _pae32), \
        /*PAE32_16*/  RT_CONCAT(a_BaseNm, _pae32_16), \
        /*PAEV86*/    RT_CONCAT(a_BaseNm, _paev86), \
        /*LM16*/      RT_CONCAT(a_BaseNm, _lm16), \
        /*LM32*/      RT_CONCAT(a_BaseNm, _lm32), \
        /*LM64*/      RT_CONCAT(a_BaseNm, _lm64), \
    }

/** A set of standard protypes to go with #BS3TESTMODEENTRY_MODE. */
#define BS3TESTMODE_PROTOTYPES_MODE(a_BaseNm) \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _rm); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pe16); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pe16_32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pe16_v86); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pe32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pe32_16); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pev86); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pp16); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pp16_32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pp16_v86); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pp32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pp32_16); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _ppv86); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pae16); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pae16_32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pae16_v86); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _pae32); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _pae32_16); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _paev86); \
    FNBS3TESTDOMODE                 RT_CONCAT(a_BaseNm, _lm16); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _lm32); \
    FNBS3TESTDOMODE BS3_FAR_CODE    RT_CONCAT(a_BaseNm, _lm64)

/** @} */


/**
 * Initializes all of boot sector kit \#3.
 */
BS3_DECL(void) Bs3InitAll_rm(void);

/**
 * Initializes the REAL and TILED memory pools.
 *
 * For proper operation on OLDer CPUs, call #Bs3CpuDetect_mmm first.
 */
BS3_DECL(void) Bs3InitMemory_rm(void);



/** @defgroup grp_bs3kit_mode   Mode Specific Functions and Data
 *
 * The mode specific functions come in bit count variations and CPU mode
 * variations.  The bs3kit-template-header.h/mac defines the BS3_NM macro to
 * mangle a function or variable name according to the target CPU mode.  In
 * non-templated code, it's common to spell the name out in full.
 *
 * @{
 */


/**
 * Macro for reducing typing.
 *
 * Doxygen knows how to expand this, well, kind of.
 */
#define BS3_MODE_EXPAND_PROTOTYPES(a_RetType, a_BaseFnNm, a_Parameters) \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_rm)       a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pe16)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pe16_32)  a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pe16_v86) a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pe32)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pe32_16)  a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pev86)    a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pp16)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pp16_32)  a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pp16_v86) a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pp32)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pp32_16)  a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_ppv86)    a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pae16)    a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pae16_32) a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pae16_v86)a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pae32)    a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_pae32_16) a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_paev86)   a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_lm16)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_lm32)     a_Parameters; \
    BS3_DECL(a_RetType) RT_CONCAT(a_BaseFnNm,_lm64)     a_Parameters

/**
 * Macro for reducing typing.
 *
 * Doxygen knows how to expand this, well, kind of.
 */
#define BS3_MODE_EXPAND_EXTERN_DATA16(a_VarType, a_VarName, a_Suffix) \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_rm))       a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pe16))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pe16_32))  a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pe16_v86)) a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pe32))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pe32_16))  a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pev86))    a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pp16))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pp16_32))  a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pp16_v86)) a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pp32))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pp32_16))  a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_ppv86))    a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pae16))    a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pae16_32)) a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pae16_v86))a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pae32))    a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_pae32_16)) a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_paev86))   a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_lm16))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_lm32))     a_Suffix; \
    extern a_VarType BS3_FAR_DATA BS3_DATA_NM(RT_CONCAT(a_VarName,_lm64))     a_Suffix


/** The TMPL_MODE_STR value for each mode.
 * These are all in DATA16 so they can be accessed from any code.  */
BS3_MODE_EXPAND_EXTERN_DATA16(const char, g_szBs3ModeName, []);

/**
 * Basic CPU detection.
 *
 * This sets the #g_bBs3CpuDetected global variable to the return value.
 *
 * @returns BS3CPU_XXX value with the BS3CPU_F_CPUID flag set depending on
 *          capabilities.
 */
BS3_MODE_EXPAND_PROTOTYPES(uint8_t, Bs3CpuDetect,(void));

/** @name BS3CPU_XXX - CPU detected by BS3CpuDetect_c16() and friends.
 * @{ */
#define BS3CPU_8086                 UINT16_C(0x0001)    /**< Both 8086 and 8088. */
#define BS3CPU_V20                  UINT16_C(0x0002)    /**< Both NEC V20, V30 and relatives. */
#define BS3CPU_80186                UINT16_C(0x0003)    /**< Both 80186 and 80188. */
#define BS3CPU_80286                UINT16_C(0x0004)
#define BS3CPU_80386                UINT16_C(0x0005)
#define BS3CPU_80486                UINT16_C(0x0006)
#define BS3CPU_Pentium              UINT16_C(0x0007)
#define BS3CPU_PPro                 UINT16_C(0x0008)
#define BS3CPU_PProOrNewer          UINT16_C(0x0009)
/** CPU type mask.  This is a full byte so it's possible to use byte access
 * without and AND'ing to get the type value. */
#define BS3CPU_TYPE_MASK            UINT16_C(0x00ff)
/** Flag indicating that the CPUID instruction is supported by the CPU. */
#define BS3CPU_F_CPUID              UINT16_C(0x0100)
/** Flag indicating that extend CPUID leaves are available (at least two).   */
#define BS3CPU_F_CPUID_EXT_LEAVES   UINT16_C(0x0200)
/** Flag indicating that the CPU supports PAE. */
#define BS3CPU_F_PAE                UINT16_C(0x0400)
/** Flag indicating that the CPU supports long mode. */
#define BS3CPU_F_LONG_MODE          UINT16_C(0x0800)
/** @} */

/** The return value of #Bs3CpuDetect_mmm. (Initial value is BS3CPU_TYPE_MASK.) */
extern uint16_t BS3_DATA_NM(g_uBs3CpuDetected);

/**
 * Initializes trap handling for the current system.
 *
 * Calls the appropriate Bs3Trap16Init, Bs3Trap32Init or Bs3Trap64Init function.
 */
BS3_MODE_EXPAND_PROTOTYPES(void, Bs3TrapInit,(void));

/**
 * Executes the array of tests in every possibly mode.
 *
 * @param   paEntries       The mode sub-test entries.
 * @param   cEntries        The number of sub-test entries.
 */
BS3_MODE_EXPAND_PROTOTYPES(void, Bs3TestDoModes, (PCBS3TESTMODEENTRY paEntries, size_t cEntries));


/** @} */

RT_C_DECLS_END


#endif

