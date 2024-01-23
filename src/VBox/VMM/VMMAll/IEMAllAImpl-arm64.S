/* $Id$ */
/** @file
 * IEM - Instruction Implementation in Assembly, ARM64 variant.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*       Header Files                                                                                                             *
*********************************************************************************************************************************/
#include <iprt/asmdefs-arm.h>
#include <iprt/x86.h>


#if RT_CLANG_PREREQ(15, 0)
        .arch_extension flagm   /* not necessary */
#else
        /* clang 12.0.x defaults to apple-a12. M1 is more similar to A14, I guess.
           For some reason the +crc make cfinv work (with clang 12). 'flagm' isn't
           recognized, nor is the 'fmi' in the error message for cfinv.  'flagm'
           work for v15 and is enabled by default it seems. */
        .cpu            apple-a14+crc
#endif


.macro CALC_EFLAGS, regEfl, regResult, regLeft, regRight, regTmp, fSkipFlags=0
        /*
         * Translate the arm NZCV bits into corresponding EFLAGS bits.
         */
 .if \fSkipFlags == 0 || \fSkipFlags == X86_EFL_OF
#if 0
        /* Maybe just a tiny bit slow than the next one. */
        mrs     \regTmp, NZCV                           /* [31] = N; [30] = Z; [29] = C; [29] = V */
  .ifeq \fSkipFlags & X86_EFL_OF
        lsr     \regTmp, \regTmp, #28
        bfi     \regEfl, \regTmp, #X86_EFL_OF_BIT, #1
        lsr     \regTmp, \regTmp, #1
  .else
        lsr     \regTmp, \regTmp, #29
  .endif
        eor     \regTmp, \regTmp, #1                    /* inverts the carry flag to x86 style. */
        bfi     \regEfl, \regTmp, #X86_EFL_CF_BIT, #1   /* CF(0) = C */
        lsr     \regTmp, \regTmp, #1
        bfi     \regEfl, \regTmp, #X86_EFL_ZF_BIT, #2   /* SF(7),ZF(6) = NZ */
#else
        /* This seems to be the faster one... */
        cfinv
        mrs     \regTmp, NZCV                           /* [31] = N; [30] = Z; [29] = C; [29] = V */
  .ifeq (\fSkipFlags & X86_EFL_OF)
        lsr     \regTmp, \regTmp, #28
        bfi     \regEfl, \regTmp, #X86_EFL_OF_BIT, #1
        lsr     \regTmp, \regTmp, #1
  .else
        lsr     \regTmp, \regTmp, #29
  .endif
        bfi     \regEfl, \regTmp, #X86_EFL_CF_BIT, #1   /* CF(0) = C */
        lsr     \regTmp, \regTmp, #1
        bfi     \regEfl, \regTmp, #X86_EFL_ZF_BIT, #2   /* SF(7),ZF(6) = NZ */
#endif
 .else
        /* Definitely slower than the above two, but easier to handle wrt skipping parts. */
  .ifeq \fSkipFlags & X86_EFL_ZF
        cset    \regTmp, eq
        bfi     \regEfl, \regTmp, #X86_EFL_ZF_BIT, #1
  .endif
  .ifeq \fSkipFlags & X86_EFL_CF
        cset    \regTmp, cc
        bfi     \regEfl, \regTmp, #X86_EFL_CF_BIT, #1
  .endif
  .ifeq \fSkipFlags & X86_EFL_OF
        cset    \regTmp, vs
        bfi     \regEfl, \regTmp, #X86_EFL_OF_BIT, #1
  .endif
  .ifeq \fSkipFlags & X86_EFL_SF
        cset    \regTmp, mi
        bfi     \regEfl, \regTmp, #X86_EFL_SF_BIT, #1
  .endif
 .endif


        /*
         * Parity calculation for low byte of the result (sucks that there is no popcount for gprs).
         */
        eor     \regTmp, \regResult, \regResult, LSR #4
        eor     \regTmp, \regTmp, \regTmp, LSR #2
        eor     \regTmp, \regTmp, \regTmp, LSR #1
        eor     \regTmp, \regTmp, #1
        bfi     \regEfl, \regTmp, #X86_EFL_PF_BIT, #1   /* PF(2) = popcount(w9 & 0xff) & 1 ^ 1 */

        /*
         * Auxilary carry / borrow flag.  This is related to 8-bit BCD.
         */
        eor     \regTmp, \regLeft, \regRight
        eor     \regTmp, \regTmp, \regResult
        lsr     \regTmp, \regTmp, #X86_EFL_AF_BIT
        bfi     \regEfl, \regTmp, #X86_EFL_AF_BIT, #1   /* AF(4) = (w8 ^ w1 ^ w9 & X86_EFL_AF) >> X86_EFL_AF_BIT */

        /* done */
.endm


BEGINCODE
        .p2align        2
        .private_extern NAME(iemAImpl_placeholder)
        .globl          NAME(iemAImpl_placeholder)
NAME(iemAImpl_placeholder):
        brk #1
        ret

/* Some sketches.

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
        .p2align        2
        .private_extern NAME(iemAImpl_xchg_u8_locked)
        .globl          NAME(iemAImpl_xchg_u8_locked)
NAME(iemAImpl_xchg_u8_locked):
        ldrb    w2, [x1]
        swpalb  w2, w2, [x0]
        strb    w2, [x1]
        ret

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_locked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
        .p2align        2
        .private_extern NAME(iemAImpl_xchg_u16_locked)
        .globl          NAME(iemAImpl_xchg_u16_locked)
NAME(iemAImpl_xchg_u16_locked):
        ldrh    w2, [x1]
        swpalh  w2, w2, [x0]
        strh    w2, [x1]
        ret

// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_locked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
// IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_locked,(uint64_t *pu64Mem, uint64_t *pu64Reg));

*/


/* IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg)); */

/*
 * The CMP instruction.
 */

/* void iemAImpl_cmp_u8(uint8_t const *puDst, uint8_t uSrc, uint32_t *pEFlags); */
        .p2align        2
        .private_extern NAME(iemAImpl_sub_u8)
        .globl          NAME(iemAImpl_sub_u8)
NAME(iemAImpl_sub_u8):
        .cfi_startproc
        /* Do the subtraction. */
        ldrb    w8, [x0]
        /*and     w1, w1, #0xff - should not be necessary. */
        subs    w9, w8, w1                      /* w9 = w8 (*puDst) - w1 (uSrc)  */
        setf8   w9
        strb    w9, [x0]

        /* Load EFLAGS. */
        ldr     w10, [x2]                       /* w10 = eflags; CF=0 PF=2 AF=4 ZF=6 SF=7 OF=11 */
        and     w9, w9, #0xffff
        CALC_EFLAGS x10, x9, x8, x1, x11, X86_EFL_OF

        /* The overflow flag calc done by setf16 isn't correct for subtraction, so we have to
           figure it out ourselves. (See IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC for details.) */
        eor     w11, w8, w1                     /* input dst ^ source (simplified from ~(dst ^ (source ^ 0x8000)) ). */
        eor     w12, w8, w9
        and     w11, w12, w11
        lsr     w11, w11, #7
        bfi     w10, w11, #X86_EFL_OF_BIT, #1

        /* Done with EFLAGS. */
        str     w10, [x2]
        ret
        .cfi_endproc


/* void iemAImpl_cmp_u16(uint16_t const *puDst, uint16_t uSrc, uint32_t *pEFlags); */
        .p2align        2
        .private_extern NAME(iemAImpl_sub_u16)
        .globl          NAME(iemAImpl_sub_u16)
NAME(iemAImpl_sub_u16):
        .cfi_startproc
        /* Do the subtraction. */
        ldrh    w8, [x0]
        /*and     w1, w1, #0xffff - should not be necessary. */
        subs    w9, w8, w1                      /* w9 = w8 (*puDst) - w1 (uSrc)  */
        setf16  w9
        strh    w9, [x0]

        /* Load EFLAGS. */
        ldr     w10, [x2]                       /* w10 = eflags; CF=0 PF=2 AF=4 ZF=6 SF=7 OF=11 */
        and     w9, w9, #0xffff
        CALC_EFLAGS x10, x9, x8, x1, x11, X86_EFL_OF

        /* The overflow flag calc done by setf16 isn't correct for subtraction, so we have to
           figure it out ourselves. (See IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC for details.) */
        eor     w11, w8, w1                     /* input dst ^ source (simplified from ~(dst ^ (source ^ 0x8000)) ). */
        eor     w12, w8, w9
        and     w11, w12, w11
        lsr     w11, w11, #15
        bfi     w10, w11, #X86_EFL_OF_BIT, #1

        /* Done with EFLAGS. */
        str     w10, [x2]
        ret
        .cfi_endproc


/* void iemAImpl_cmp_u32(uint32_t const *puDst, uint32_t uSrc, uint32_t *pEFlags); */
        .p2align        2
        .private_extern NAME(iemAImpl_sub_u32)
        .globl          NAME(iemAImpl_sub_u32)
NAME(iemAImpl_sub_u32):
        .cfi_startproc
        /* Do the subtraction. */
        ldr     w8, [x0]
        subs    w9, w8, w1                      /* w9 = w8 (*puDst) - w1 (uSrc)  */
        str     w9, [x0]

        /* Load EFLAGS. */
        ldr     w10, [x2]                       /* w10 = eflags; CF=0 PF=2 AF=4 ZF=6 SF=7 OF=11 */

#if 0
        /* Translate the arm NZCV bits into corresponding EFLAGS bits. */
#if 0   /* maybe just a tiny bit slow than the next one. */
        mrs     x11, NZCV                       /* w11[31] = N; w11[30] = Z; w11[29] = C; w11[29] = V */
        lsr     w11, w11, #28
        bfi     w10, w11, #X86_EFL_OF_BIT, #1
        lsr     w11, w11, #1
        eor     w11, w11, #1                    /* inverts the carry flag to x86 style. */
        bfi     w10, w11, #X86_EFL_CF_BIT, #1   /* CF(0) = C */
        lsr     w11, w11, #1
        bfi     w10, w11, #X86_EFL_ZF_BIT, #2   /* SF(7),ZF(6) = NZ */
#elif 1 /* seems the faster one... */
        cfinv
        mrs     x11, NZCV                       /* w11[31] = N; w11[30] = Z; w11[29] = C; w11[29] = V */
        lsr     w11, w11, #28
        bfi     w10, w11, #X86_EFL_OF_BIT, #1
        lsr     w11, w11, #1
        bfi     w10, w11, #X86_EFL_CF_BIT, #1   /* CF(0) = C */
        lsr     w11, w11, #1
        bfi     w10, w11, #X86_EFL_ZF_BIT, #2   /* SF(7),ZF(6) = NZ */
#else
        cset    w11, eq
        bfi     w10, w11, #X86_EFL_ZF_BIT, #1
        cset    w11, cc
        bfi     w10, w11, #X86_EFL_CF_BIT, #1
        cset    w11, vs
        bfi     w10, w11, #X86_EFL_OF_BIT, #1
        cset    w11, mi
        bfi     w10, w11, #X86_EFL_SF_BIT, #1
#endif

        /* Parity calculation for low byte of the result (sucks that there is no popcount for gprs). */
        eor     w11, w9, w9, LSR #4
        eor     w11, w11, w11, LSR #2
        eor     w11, w11, w11, LSR #1
        eor     w11, w11, #1
        bfi     w10, w11, #X86_EFL_PF_BIT, #1   /* PF(2) = popcount(w9 & 0xff) & 1 ^ 1 */

        /* Auxilary carry / borrow flag.  This is related to 8-bit BCD. */
        eor     w11, w8, w1
        eor     w11, w11, w9
        lsr     w11, w11, #X86_EFL_AF_BIT
        bfi     w10, w11, #X86_EFL_AF_BIT, #1   /* AF(4) = (w8 ^ w1 ^ w9 & X86_EFL_AF) >> X86_EFL_AF_BIT */
#else
        CALC_EFLAGS x10, x9, x8, x1, x11
#endif

        str     w10, [x2]
        ret
        .cfi_endproc


/* void iemAImpl_cmp_u64(uint64_t const *puDst, uint64_t uSrc, uint32_t *pEFlags); */
        .p2align        2
        .private_extern NAME(iemAImpl_sub_u64)
        .globl          NAME(iemAImpl_sub_u64)
NAME(iemAImpl_sub_u64):
        .cfi_startproc
        /* Do the subtraction. */
        ldr     x8, [x0]
        subs    x9, x8, x1                      /* x9 = x8 (*puDst) - x1 (uSrc)  */
        str     x9, [x0]

        /* Load EFLAGS. */
        ldr     w10, [x2]                       /* w10 = eflags; CF=0 PF=2 AF=4 ZF=6 SF=7 OF=11 */
        CALC_EFLAGS x10, x9, x8, x1, x11

        str     w10, [x2]
        ret
        .cfi_endproc
