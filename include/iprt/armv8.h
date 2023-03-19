/** @file
 * IPRT - ARMv8 (AArch64 and AArch32) Structures and Definitions.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_armv8_h
#define IPRT_INCLUDED_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifndef VBOX_FOR_DTRACE_LIB
# include <iprt/types.h>
# include <iprt/assert.h>
#else
# pragma D depends_on library vbox-types.d
#endif

/** @defgroup grp_rt_armv8   ARMv8 Types and Definitions
 * @ingroup grp_rt
 * @{
 */

/**
 * SPSR_EL2 (according to chapter C5.2.19)
 */
typedef union ARMV8SPSREL2
{
    /** The plain unsigned view. */
    uint64_t        u;
    /** The 8-bit view. */
    uint8_t         au8[8];
    /** The 16-bit view. */
    uint16_t        au16[4];
    /** The 32-bit view. */
    uint32_t        au32[2];
    /** The 64-bit view. */
    uint64_t        u64;
} ARMV8SPSREL2;
/** Pointer to SPSR_EL2. */
typedef ARMV8SPSREL2 *PARMV8SPSREL2;
/** Pointer to const SPSR_EL2. */
typedef const ARMV8SPSREL2 *PCXARMV8SPSREL2;


/** @name SPSR_EL2 (When exception is taken from AArch64 state)
 * @{
 */
/** Bit 0 - 3 - M - AArch64 Exception level and selected stack pointer. */
#define ARMV8_SPSR_EL2_AARCH64_M                    (RT_BIT_64(0) | RT_BIT_64(1) | RT_BIT_64(2) | RT_BIT_64(3))
#define ARMV8_SPSR_EL2_AARCH64_GET_M(a_Spsr)        ((a_Spsr) & ARMV8_SPSR_EL2_AARCH64_M)
/** Bit 0 - SP - Selected stack pointer. */
#define ARMV8_SPSR_EL2_AARCH64_SP                   RT_BIT_64(0)
#define ARMV8_SPSR_EL2_AARCH64_SP_BIT               0
/** Bit 1 - Reserved (read as zero). */
#define ARMV8_SPSR_EL2_AARCH64_RSVD_1               RT_BIT_64(1)
/** Bit 2 - 3 - EL - Exception level. */
#define ARMV8_SPSR_EL2_AARCH64_EL                   (RT_BIT_64(2) | RT_BIT_64(3))
#define ARMV8_SPSR_EL2_AARCH64_EL_SHIFT             2
#define ARMV8_SPSR_EL2_AARCH64_GET_EL(a_Spsr)       (((a_Spsr) >> ARMV8_SPSR_EL2_AARCH64_EL_SHIFT) & 3)
#define ARMV8_SPSR_EL2_AARCH64_SET_EL(a_El)         ((a_El) << ARMV8_SPSR_EL2_AARCH64_EL_SHIFT)
/** Bit 4 - M[4] - Execution state (0 means AArch64, when 1 this contains a AArch32 state). */
#define ARMV8_SPSR_EL2_AARCH64_M4                   RT_BIT_64(4)
#define ARMV8_SPSR_EL2_AARCH64_M4_BIT               4
/** Bit 5 - Reserved (read as zero). */
#define ARMV8_SPSR_EL2_AARCH64_RSVD_5               RT_BIT_64(5)
/** Bit 6 - I - FIQ interrupt mask. */
#define ARMV8_SPSR_EL2_AARCH64_F                    RT_BIT_64(6)
#define ARMV8_SPSR_EL2_AARCH64_F_BIT                6
/** Bit 7 - I - IRQ interrupt mask. */
#define ARMV8_SPSR_EL2_AARCH64_I                    RT_BIT_64(7)
#define ARMV8_SPSR_EL2_AARCH64_I_BIT                7
/** Bit 8 - A - SError interrupt mask. */
#define ARMV8_SPSR_EL2_AARCH64_A                    RT_BIT_64(8)
#define ARMV8_SPSR_EL2_AARCH64_A_BIT                8
/** Bit 9 - D - Debug Exception mask. */
#define ARMV8_SPSR_EL2_AARCH64_D                    RT_BIT_64(9)
#define ARMV8_SPSR_EL2_AARCH64_D_BIT                9
/** Bit 10 - 11 - BTYPE - Branch Type indicator. */
#define ARMV8_SPSR_EL2_AARCH64_BYTPE                (RT_BIT_64(10) | RT_BIT_64(11))
#define ARMV8_SPSR_EL2_AARCH64_BYTPE_SHIFT          10
#define ARMV8_SPSR_EL2_AARCH64_GET_BYTPE(a_Spsr)    (((a_Spsr) >> ARMV8_SPSR_EL2_AARCH64_BYTPE_SHIFT) & 3)
/** Bit 12 - SSBS - Speculative Store Bypass. */
#define ARMV8_SPSR_EL2_AARCH64_SSBS                 RT_BIT_64(12)
#define ARMV8_SPSR_EL2_AARCH64_SSBS_BIT             12
/** Bit 13 - ALLINT - All IRQ or FIQ interrupts mask. */
#define ARMV8_SPSR_EL2_AARCH64_ALLINT               RT_BIT_64(13)
#define ARMV8_SPSR_EL2_AARCH64_ALLINT_BIT           13
/** Bit 14 - 19 - Reserved (read as zero). */
#define ARMV8_SPSR_EL2_AARCH64_RSVD_14_19           (  RT_BIT_64(14) | RT_BIT_64(15) | RT_BIT_64(16) \
                                                     | RT_BIT_64(17) | RT_BIT_64(18) | RT_BIT_64(19))
/** Bit 20 - IL - Illegal Execution State flag. */
#define ARMV8_SPSR_EL2_AARCH64_IL                   RT_BIT_64(20)
#define ARMV8_SPSR_EL2_AARCH64_IL_BIT               20
/** Bit 21 - SS - Software Step flag. */
#define ARMV8_SPSR_EL2_AARCH64_SS                   RT_BIT_64(21)
#define ARMV8_SPSR_EL2_AARCH64_SS_BIT               21
/** Bit 22 - PAN - Privileged Access Never flag. */
#define ARMV8_SPSR_EL2_AARCH64_PAN                  RT_BIT_64(25)
#define ARMV8_SPSR_EL2_AARCH64_PAN_BIT              22
/** Bit 23 - UAO - User Access Override flag. */
#define ARMV8_SPSR_EL2_AARCH64_UAO                  RT_BIT_64(23)
#define ARMV8_SPSR_EL2_AARCH64_UAO_BIT              23
/** Bit 24 - DIT - Data Independent Timing flag. */
#define ARMV8_SPSR_EL2_AARCH64_DIT                  RT_BIT_64(24)
#define ARMV8_SPSR_EL2_AARCH64_DIT_BIT              24
/** Bit 25 - TCO - Tag Check Override flag. */
#define ARMV8_SPSR_EL2_AARCH64_TCO                  RT_BIT_64(25)
#define ARMV8_SPSR_EL2_AARCH64_TCO_BIT              25
/** Bit 26 - 27 - Reserved (read as zero). */
#define ARMV8_SPSR_EL2_AARCH64_RSVD_26_27           (RT_BIT_64(26) | RT_BIT_64(27))
/** Bit 28 - V - Overflow condition flag. */
#define ARMV8_SPSR_EL2_AARCH64_V                    RT_BIT_64(28)
#define ARMV8_SPSR_EL2_AARCH64_V_BIT                28
/** Bit 29 - C - Carry condition flag. */
#define ARMV8_SPSR_EL2_AARCH64_C                    RT_BIT_64(29)
#define ARMV8_SPSR_EL2_AARCH64_C_BIT                29
/** Bit 30 - Z - Zero condition flag. */
#define ARMV8_SPSR_EL2_AARCH64_Z                    RT_BIT_64(30)
#define ARMV8_SPSR_EL2_AARCH64_Z_BIT                30
/** Bit 31 - N - Negative condition flag. */
#define ARMV8_SPSR_EL2_AARCH64_N                    RT_BIT_64(31)
#define ARMV8_SPSR_EL2_AARCH64_N_BIT                31
/** Bit 32 - 63 - Reserved (read as zero). */
#define ARMV8_SPSR_EL2_AARCH64_RSVD_32_63           (UINT64_C(0xffffffff00000000))
/** Checks whether the given SPSR value contains a AARCH64 execution state. */
#define ARMV8_SPSR_EL2_IS_AARCH64_STATE(a_Spsr)     (!((a_Spsr) & ARMV8_SPSR_EL2_AARCH64_M4))
/** @} */

/** @name Aarch64 Exception levels
 * @{ */
/** Exception Level 0 - User mode. */
#define ARMV8_AARCH64_EL_0                          0
/** Exception Level 1 - Supervisor mode. */
#define ARMV8_AARCH64_EL_1                          1
/** Exception Level 2 - Hypervisor mode. */
#define ARMV8_AARCH64_EL_2                          2
/** @} */


/** @} */

#endif /* !IPRT_INCLUDED_armv8_h */

