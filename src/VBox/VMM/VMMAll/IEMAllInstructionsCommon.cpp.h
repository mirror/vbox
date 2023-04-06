/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation, Common Bits.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Repeats a_fn four times.  For decoding tables. */
#define IEMOP_X4(a_fn) a_fn, a_fn, a_fn, a_fn


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifndef TST_IEM_CHECK_MC

/** Function table for the BSF instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsf =
{
    NULL,  NULL,
    iemAImpl_bsf_u16, NULL,
    iemAImpl_bsf_u32, NULL,
    iemAImpl_bsf_u64, NULL
};

/** Function table for the BSF instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsf_amd =
{
    NULL,  NULL,
    iemAImpl_bsf_u16_amd, NULL,
    iemAImpl_bsf_u32_amd, NULL,
    iemAImpl_bsf_u64_amd, NULL
};

/** Function table for the BSF instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsf_intel =
{
    NULL,  NULL,
    iemAImpl_bsf_u16_intel, NULL,
    iemAImpl_bsf_u32_intel, NULL,
    iemAImpl_bsf_u64_intel, NULL
};

/** EFLAGS variation selection table for the BSF instruction. */
IEM_STATIC const IEMOPBINSIZES * const g_iemAImpl_bsf_eflags[] =
{
    &g_iemAImpl_bsf,
    &g_iemAImpl_bsf_intel,
    &g_iemAImpl_bsf_amd,
    &g_iemAImpl_bsf,
};

/** Function table for the BSR instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsr =
{
    NULL,  NULL,
    iemAImpl_bsr_u16, NULL,
    iemAImpl_bsr_u32, NULL,
    iemAImpl_bsr_u64, NULL
};

/** Function table for the BSR instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsr_amd =
{
    NULL,  NULL,
    iemAImpl_bsr_u16_amd, NULL,
    iemAImpl_bsr_u32_amd, NULL,
    iemAImpl_bsr_u64_amd, NULL
};

/** Function table for the BSR instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsr_intel =
{
    NULL,  NULL,
    iemAImpl_bsr_u16_intel, NULL,
    iemAImpl_bsr_u32_intel, NULL,
    iemAImpl_bsr_u64_intel, NULL
};

/** EFLAGS variation selection table for the BSR instruction. */
IEM_STATIC const IEMOPBINSIZES * const g_iemAImpl_bsr_eflags[] =
{
    &g_iemAImpl_bsr,
    &g_iemAImpl_bsr_intel,
    &g_iemAImpl_bsr_amd,
    &g_iemAImpl_bsr,
};

/** Function table for the IMUL instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_imul_two =
{
    NULL,  NULL,
    iemAImpl_imul_two_u16, NULL,
    iemAImpl_imul_two_u32, NULL,
    iemAImpl_imul_two_u64, NULL
};

/** Function table for the IMUL instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_imul_two_amd =
{
    NULL,  NULL,
    iemAImpl_imul_two_u16_amd, NULL,
    iemAImpl_imul_two_u32_amd, NULL,
    iemAImpl_imul_two_u64_amd, NULL
};

/** Function table for the IMUL instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_imul_two_intel =
{
    NULL,  NULL,
    iemAImpl_imul_two_u16_intel, NULL,
    iemAImpl_imul_two_u32_intel, NULL,
    iemAImpl_imul_two_u64_intel, NULL
};

/** EFLAGS variation selection table for the IMUL instruction. */
IEM_STATIC const IEMOPBINSIZES * const g_iemAImpl_imul_two_eflags[] =
{
    &g_iemAImpl_imul_two,
    &g_iemAImpl_imul_two_intel,
    &g_iemAImpl_imul_two_amd,
    &g_iemAImpl_imul_two,
};

/** EFLAGS variation selection table for the 16-bit IMUL instruction. */
IEM_STATIC PFNIEMAIMPLBINU16 const g_iemAImpl_imul_two_u16_eflags[] =
{
    iemAImpl_imul_two_u16,
    iemAImpl_imul_two_u16_intel,
    iemAImpl_imul_two_u16_amd,
    iemAImpl_imul_two_u16,
};

/** EFLAGS variation selection table for the 32-bit IMUL instruction. */
IEM_STATIC PFNIEMAIMPLBINU32 const g_iemAImpl_imul_two_u32_eflags[] =
{
    iemAImpl_imul_two_u32,
    iemAImpl_imul_two_u32_intel,
    iemAImpl_imul_two_u32_amd,
    iemAImpl_imul_two_u32,
};

/** EFLAGS variation selection table for the 64-bit IMUL instruction. */
IEM_STATIC PFNIEMAIMPLBINU64 const g_iemAImpl_imul_two_u64_eflags[] =
{
    iemAImpl_imul_two_u64,
    iemAImpl_imul_two_u64_intel,
    iemAImpl_imul_two_u64_amd,
    iemAImpl_imul_two_u64,
};

/** Function table for the ROL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rol =
{
    iemAImpl_rol_u8,
    iemAImpl_rol_u16,
    iemAImpl_rol_u32,
    iemAImpl_rol_u64
};

/** Function table for the ROL instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rol_amd =
{
    iemAImpl_rol_u8_amd,
    iemAImpl_rol_u16_amd,
    iemAImpl_rol_u32_amd,
    iemAImpl_rol_u64_amd
};

/** Function table for the ROL instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rol_intel =
{
    iemAImpl_rol_u8_intel,
    iemAImpl_rol_u16_intel,
    iemAImpl_rol_u32_intel,
    iemAImpl_rol_u64_intel
};

/** EFLAGS variation selection table for the ROL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_rol_eflags[] =
{
    &g_iemAImpl_rol,
    &g_iemAImpl_rol_intel,
    &g_iemAImpl_rol_amd,
    &g_iemAImpl_rol,
};


/** Function table for the ROR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_ror =
{
    iemAImpl_ror_u8,
    iemAImpl_ror_u16,
    iemAImpl_ror_u32,
    iemAImpl_ror_u64
};

/** Function table for the ROR instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_ror_amd =
{
    iemAImpl_ror_u8_amd,
    iemAImpl_ror_u16_amd,
    iemAImpl_ror_u32_amd,
    iemAImpl_ror_u64_amd
};

/** Function table for the ROR instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_ror_intel =
{
    iemAImpl_ror_u8_intel,
    iemAImpl_ror_u16_intel,
    iemAImpl_ror_u32_intel,
    iemAImpl_ror_u64_intel
};

/** EFLAGS variation selection table for the ROR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_ror_eflags[] =
{
    &g_iemAImpl_ror,
    &g_iemAImpl_ror_intel,
    &g_iemAImpl_ror_amd,
    &g_iemAImpl_ror,
};


/** Function table for the RCL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcl =
{
    iemAImpl_rcl_u8,
    iemAImpl_rcl_u16,
    iemAImpl_rcl_u32,
    iemAImpl_rcl_u64
};

/** Function table for the RCL instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcl_amd =
{
    iemAImpl_rcl_u8_amd,
    iemAImpl_rcl_u16_amd,
    iemAImpl_rcl_u32_amd,
    iemAImpl_rcl_u64_amd
};

/** Function table for the RCL instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcl_intel =
{
    iemAImpl_rcl_u8_intel,
    iemAImpl_rcl_u16_intel,
    iemAImpl_rcl_u32_intel,
    iemAImpl_rcl_u64_intel
};

/** EFLAGS variation selection table for the RCL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_rcl_eflags[] =
{
    &g_iemAImpl_rcl,
    &g_iemAImpl_rcl_intel,
    &g_iemAImpl_rcl_amd,
    &g_iemAImpl_rcl,
};


/** Function table for the RCR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcr =
{
    iemAImpl_rcr_u8,
    iemAImpl_rcr_u16,
    iemAImpl_rcr_u32,
    iemAImpl_rcr_u64
};

/** Function table for the RCR instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcr_amd =
{
    iemAImpl_rcr_u8_amd,
    iemAImpl_rcr_u16_amd,
    iemAImpl_rcr_u32_amd,
    iemAImpl_rcr_u64_amd
};

/** Function table for the RCR instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcr_intel =
{
    iemAImpl_rcr_u8_intel,
    iemAImpl_rcr_u16_intel,
    iemAImpl_rcr_u32_intel,
    iemAImpl_rcr_u64_intel
};

/** EFLAGS variation selection table for the RCR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_rcr_eflags[] =
{
    &g_iemAImpl_rcr,
    &g_iemAImpl_rcr_intel,
    &g_iemAImpl_rcr_amd,
    &g_iemAImpl_rcr,
};


/** Function table for the SHL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shl =
{
    iemAImpl_shl_u8,
    iemAImpl_shl_u16,
    iemAImpl_shl_u32,
    iemAImpl_shl_u64
};

/** Function table for the SHL instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shl_amd =
{
    iemAImpl_shl_u8_amd,
    iemAImpl_shl_u16_amd,
    iemAImpl_shl_u32_amd,
    iemAImpl_shl_u64_amd
};

/** Function table for the SHL instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shl_intel =
{
    iemAImpl_shl_u8_intel,
    iemAImpl_shl_u16_intel,
    iemAImpl_shl_u32_intel,
    iemAImpl_shl_u64_intel
};

/** EFLAGS variation selection table for the SHL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_shl_eflags[] =
{
    &g_iemAImpl_shl,
    &g_iemAImpl_shl_intel,
    &g_iemAImpl_shl_amd,
    &g_iemAImpl_shl,
};


/** Function table for the SHR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shr =
{
    iemAImpl_shr_u8,
    iemAImpl_shr_u16,
    iemAImpl_shr_u32,
    iemAImpl_shr_u64
};

/** Function table for the SHR instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shr_amd =
{
    iemAImpl_shr_u8_amd,
    iemAImpl_shr_u16_amd,
    iemAImpl_shr_u32_amd,
    iemAImpl_shr_u64_amd
};

/** Function table for the SHR instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shr_intel =
{
    iemAImpl_shr_u8_intel,
    iemAImpl_shr_u16_intel,
    iemAImpl_shr_u32_intel,
    iemAImpl_shr_u64_intel
};

/** EFLAGS variation selection table for the SHR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_shr_eflags[] =
{
    &g_iemAImpl_shr,
    &g_iemAImpl_shr_intel,
    &g_iemAImpl_shr_amd,
    &g_iemAImpl_shr,
};


/** Function table for the SAR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_sar =
{
    iemAImpl_sar_u8,
    iemAImpl_sar_u16,
    iemAImpl_sar_u32,
    iemAImpl_sar_u64
};

/** Function table for the SAR instruction, AMD EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_sar_amd =
{
    iemAImpl_sar_u8_amd,
    iemAImpl_sar_u16_amd,
    iemAImpl_sar_u32_amd,
    iemAImpl_sar_u64_amd
};

/** Function table for the SAR instruction, Intel EFLAGS variant. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_sar_intel =
{
    iemAImpl_sar_u8_intel,
    iemAImpl_sar_u16_intel,
    iemAImpl_sar_u32_intel,
    iemAImpl_sar_u64_intel
};

/** EFLAGS variation selection table for the SAR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES * const g_iemAImpl_sar_eflags[] =
{
    &g_iemAImpl_sar,
    &g_iemAImpl_sar_intel,
    &g_iemAImpl_sar_amd,
    &g_iemAImpl_sar,
};


/** Function table for the MUL instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_mul =
{
    iemAImpl_mul_u8,
    iemAImpl_mul_u16,
    iemAImpl_mul_u32,
    iemAImpl_mul_u64
};

/** Function table for the MUL instruction, AMD EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_mul_amd =
{
    iemAImpl_mul_u8_amd,
    iemAImpl_mul_u16_amd,
    iemAImpl_mul_u32_amd,
    iemAImpl_mul_u64_amd
};

/** Function table for the MUL instruction, Intel EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_mul_intel =
{
    iemAImpl_mul_u8_intel,
    iemAImpl_mul_u16_intel,
    iemAImpl_mul_u32_intel,
    iemAImpl_mul_u64_intel
};

/** EFLAGS variation selection table for the MUL instruction. */
IEM_STATIC const IEMOPMULDIVSIZES * const g_iemAImpl_mul_eflags[] =
{
    &g_iemAImpl_mul,
    &g_iemAImpl_mul_intel,
    &g_iemAImpl_mul_amd,
    &g_iemAImpl_mul,
};

/** EFLAGS variation selection table for the 8-bit MUL instruction. */
IEM_STATIC PFNIEMAIMPLMULDIVU8 const g_iemAImpl_mul_u8_eflags[] =
{
    iemAImpl_mul_u8,
    iemAImpl_mul_u8_intel,
    iemAImpl_mul_u8_amd,
    iemAImpl_mul_u8
};


/** Function table for the IMUL instruction working implicitly on rAX. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_imul =
{
    iemAImpl_imul_u8,
    iemAImpl_imul_u16,
    iemAImpl_imul_u32,
    iemAImpl_imul_u64
};

/** Function table for the IMUL instruction working implicitly on rAX, AMD EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_imul_amd =
{
    iemAImpl_imul_u8_amd,
    iemAImpl_imul_u16_amd,
    iemAImpl_imul_u32_amd,
    iemAImpl_imul_u64_amd
};

/** Function table for the IMUL instruction working implicitly on rAX, Intel EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_imul_intel =
{
    iemAImpl_imul_u8_intel,
    iemAImpl_imul_u16_intel,
    iemAImpl_imul_u32_intel,
    iemAImpl_imul_u64_intel
};

/** EFLAGS variation selection table for the IMUL instruction. */
IEM_STATIC const IEMOPMULDIVSIZES * const g_iemAImpl_imul_eflags[] =
{
    &g_iemAImpl_imul,
    &g_iemAImpl_imul_intel,
    &g_iemAImpl_imul_amd,
    &g_iemAImpl_imul,
};

/** EFLAGS variation selection table for the 8-bit IMUL instruction. */
IEM_STATIC PFNIEMAIMPLMULDIVU8 const g_iemAImpl_imul_u8_eflags[] =
{
    iemAImpl_imul_u8,
    iemAImpl_imul_u8_intel,
    iemAImpl_imul_u8_amd,
    iemAImpl_imul_u8
};


/** Function table for the DIV instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_div =
{
    iemAImpl_div_u8,
    iemAImpl_div_u16,
    iemAImpl_div_u32,
    iemAImpl_div_u64
};

/** Function table for the DIV instruction, AMD EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_div_amd =
{
    iemAImpl_div_u8_amd,
    iemAImpl_div_u16_amd,
    iemAImpl_div_u32_amd,
    iemAImpl_div_u64_amd
};

/** Function table for the DIV instruction, Intel EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_div_intel =
{
    iemAImpl_div_u8_intel,
    iemAImpl_div_u16_intel,
    iemAImpl_div_u32_intel,
    iemAImpl_div_u64_intel
};

/** EFLAGS variation selection table for the DIV instruction. */
IEM_STATIC const IEMOPMULDIVSIZES * const g_iemAImpl_div_eflags[] =
{
    &g_iemAImpl_div,
    &g_iemAImpl_div_intel,
    &g_iemAImpl_div_amd,
    &g_iemAImpl_div,
};

/** EFLAGS variation selection table for the 8-bit DIV instruction. */
IEM_STATIC PFNIEMAIMPLMULDIVU8 const g_iemAImpl_div_u8_eflags[] =
{
    iemAImpl_div_u8,
    iemAImpl_div_u8_intel,
    iemAImpl_div_u8_amd,
    iemAImpl_div_u8
};


/** Function table for the IDIV instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_idiv =
{
    iemAImpl_idiv_u8,
    iemAImpl_idiv_u16,
    iemAImpl_idiv_u32,
    iemAImpl_idiv_u64
};

/** Function table for the IDIV instruction, AMD EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_idiv_amd =
{
    iemAImpl_idiv_u8_amd,
    iemAImpl_idiv_u16_amd,
    iemAImpl_idiv_u32_amd,
    iemAImpl_idiv_u64_amd
};

/** Function table for the IDIV instruction, Intel EFLAGS variation. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_idiv_intel =
{
    iemAImpl_idiv_u8_intel,
    iemAImpl_idiv_u16_intel,
    iemAImpl_idiv_u32_intel,
    iemAImpl_idiv_u64_intel
};

/** EFLAGS variation selection table for the IDIV instruction. */
IEM_STATIC const IEMOPMULDIVSIZES * const g_iemAImpl_idiv_eflags[] =
{
    &g_iemAImpl_idiv,
    &g_iemAImpl_idiv_intel,
    &g_iemAImpl_idiv_amd,
    &g_iemAImpl_idiv,
};

/** EFLAGS variation selection table for the 8-bit IDIV instruction. */
IEM_STATIC PFNIEMAIMPLMULDIVU8 const g_iemAImpl_idiv_u8_eflags[] =
{
    iemAImpl_idiv_u8,
    iemAImpl_idiv_u8_intel,
    iemAImpl_idiv_u8_amd,
    iemAImpl_idiv_u8
};


/** Function table for the SHLD instruction. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shld =
{
    iemAImpl_shld_u16,
    iemAImpl_shld_u32,
    iemAImpl_shld_u64,
};

/** Function table for the SHLD instruction, AMD EFLAGS variation. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shld_amd =
{
    iemAImpl_shld_u16_amd,
    iemAImpl_shld_u32_amd,
    iemAImpl_shld_u64_amd
};

/** Function table for the SHLD instruction, Intel EFLAGS variation. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shld_intel =
{
    iemAImpl_shld_u16_intel,
    iemAImpl_shld_u32_intel,
    iemAImpl_shld_u64_intel
};

/** EFLAGS variation selection table for the SHLD instruction. */
IEM_STATIC const IEMOPSHIFTDBLSIZES * const g_iemAImpl_shld_eflags[] =
{
    &g_iemAImpl_shld,
    &g_iemAImpl_shld_intel,
    &g_iemAImpl_shld_amd,
    &g_iemAImpl_shld
};

/** Function table for the SHRD instruction. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shrd =
{
    iemAImpl_shrd_u16,
    iemAImpl_shrd_u32,
    iemAImpl_shrd_u64
};

/** Function table for the SHRD instruction, AMD EFLAGS variation. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shrd_amd =
{
    iemAImpl_shrd_u16_amd,
    iemAImpl_shrd_u32_amd,
    iemAImpl_shrd_u64_amd
};

/** Function table for the SHRD instruction, Intel EFLAGS variation. */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shrd_intel =
{
    iemAImpl_shrd_u16_intel,
    iemAImpl_shrd_u32_intel,
    iemAImpl_shrd_u64_intel
};

/** EFLAGS variation selection table for the SHRD instruction. */
IEM_STATIC const IEMOPSHIFTDBLSIZES * const g_iemAImpl_shrd_eflags[] =
{
    &g_iemAImpl_shrd,
    &g_iemAImpl_shrd_intel,
    &g_iemAImpl_shrd_amd,
    &g_iemAImpl_shrd
};


# ifndef IEM_WITHOUT_ASSEMBLY
/** Function table for the VPXOR instruction */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpand          = { iemAImpl_vpand_u128,   iemAImpl_vpand_u256 };
/** Function table for the VPXORN instruction */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpandn         = { iemAImpl_vpandn_u128,  iemAImpl_vpandn_u256 };
/** Function table for the VPOR instruction */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpor           = { iemAImpl_vpor_u128,    iemAImpl_vpor_u256 };
/** Function table for the VPXOR instruction */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpxor          = { iemAImpl_vpxor_u128,   iemAImpl_vpxor_u256 };
# endif

/** Function table for the VPAND instruction, software fallback. */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpand_fallback = { iemAImpl_vpand_u128_fallback,  iemAImpl_vpand_u256_fallback };
/** Function table for the VPANDN instruction, software fallback. */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpandn_fallback= { iemAImpl_vpandn_u128_fallback, iemAImpl_vpandn_u256_fallback };
/** Function table for the VPOR instruction, software fallback. */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpor_fallback  = { iemAImpl_vpor_u128_fallback,   iemAImpl_vpor_u256_fallback };
/** Function table for the VPXOR instruction, software fallback. */
IEM_STATIC const IEMOPMEDIAF3 g_iemAImpl_vpxor_fallback = { iemAImpl_vpxor_u128_fallback,  iemAImpl_vpxor_u256_fallback };

#endif /* !TST_IEM_CHECK_MC */



/** Opcodes 0xf1, 0xd6. */
FNIEMOP_DEF(iemOp_Invalid)
{
    IEMOP_MNEMONIC(Invalid, "Invalid");
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte . */
FNIEMOPRM_DEF(iemOp_InvalidWithRM)
{
    RT_NOREF_PV(bRm);
    IEMOP_MNEMONIC(InvalidWithRm, "InvalidWithRM");
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte where intel decodes any additional address encoding
 *  bytes. */
FNIEMOPRM_DEF(iemOp_InvalidWithRMNeedDecode)
{
    IEMOP_MNEMONIC(InvalidWithRMNeedDecode, "InvalidWithRMNeedDecode");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
    }
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte where both AMD and Intel decodes any additional
 *  address encoding bytes. */
FNIEMOPRM_DEF(iemOp_InvalidWithRMAllNeeded)
{
    IEMOP_MNEMONIC(InvalidWithRMAllNeeded, "InvalidWithRMAllNeeded");
#ifndef TST_IEM_CHECK_MC
    if (IEM_IS_MODRM_MEM_MODE(bRm))
    {
        RTGCPTR      GCPtrEff;
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
#endif
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte where intel requires 8-byte immediate.
 * Intel will also need SIB and displacement if bRm indicates memory. */
FNIEMOPRM_DEF(iemOp_InvalidWithRMNeedImm8)
{
    IEMOP_MNEMONIC(InvalidWithRMNeedImm8, "InvalidWithRMNeedImm8");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
        uint8_t bImm8;  IEM_OPCODE_GET_NEXT_U8(&bImm8);  RT_NOREF(bRm);
    }
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte where intel requires 8-byte immediate.
 * Both AMD and Intel also needs SIB and displacement according to bRm. */
FNIEMOPRM_DEF(iemOp_InvalidWithRMAllNeedImm8)
{
    IEMOP_MNEMONIC(InvalidWithRMAllNeedImm8, "InvalidWithRMAllNeedImm8");
#ifndef TST_IEM_CHECK_MC
    if (IEM_IS_MODRM_MEM_MODE(bRm))
    {
        RTGCPTR      GCPtrEff;
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
#endif
    uint8_t bImm8;  IEM_OPCODE_GET_NEXT_U8(&bImm8);  RT_NOREF(bRm);
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid opcode where intel requires Mod R/M sequence. */
FNIEMOP_DEF(iemOp_InvalidNeedRM)
{
    IEMOP_MNEMONIC(InvalidNeedRM, "InvalidNeedRM");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm); RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
    }
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid opcode where both AMD and Intel requires Mod R/M sequence. */
FNIEMOP_DEF(iemOp_InvalidAllNeedRM)
{
    IEMOP_MNEMONIC(InvalidAllNeedRM, "InvalidAllNeedRM");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm); RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
    if (IEM_IS_MODRM_MEM_MODE(bRm))
    {
        RTGCPTR      GCPtrEff;
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
#endif
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid opcode where intel requires Mod R/M sequence and 8-byte
 *  immediate. */
FNIEMOP_DEF(iemOp_InvalidNeedRMImm8)
{
    IEMOP_MNEMONIC(InvalidNeedRMImm8, "InvalidNeedRMImm8");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t bRm;  IEM_OPCODE_GET_NEXT_U8(&bRm);  RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
        uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm); RT_NOREF(bImm);
    }
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid opcode where intel requires a 3rd escape byte and a Mod R/M
 *  sequence. */
FNIEMOP_DEF(iemOp_InvalidNeed3ByteEscRM)
{
    IEMOP_MNEMONIC(InvalidNeed3ByteEscRM, "InvalidNeed3ByteEscRM");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t b3rd; IEM_OPCODE_GET_NEXT_U8(&b3rd); RT_NOREF(b3rd);
        uint8_t bRm;  IEM_OPCODE_GET_NEXT_U8(&bRm);  RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
    }
    IEMOP_HLP_DONE_DECODING();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid opcode where intel requires a 3rd escape byte, Mod R/M sequence, and
 *  a 8-byte immediate. */
FNIEMOP_DEF(iemOp_InvalidNeed3ByteEscRMImm8)
{
    IEMOP_MNEMONIC(InvalidNeed3ByteEscRMImm8, "InvalidNeed3ByteEscRMImm8");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t b3rd; IEM_OPCODE_GET_NEXT_U8(&b3rd); RT_NOREF(b3rd);
        uint8_t bRm;  IEM_OPCODE_GET_NEXT_U8(&bRm);  RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        if (IEM_IS_MODRM_MEM_MODE(bRm))
        {
            RTGCPTR      GCPtrEff;
            VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 1, &GCPtrEff);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
        }
#endif
        uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm); RT_NOREF(bImm);
        IEMOP_HLP_DONE_DECODING();
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}

