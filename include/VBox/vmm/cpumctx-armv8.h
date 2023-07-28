/** @file
 * CPUM - CPU Monitor(/ Manager), Context Structures for the ARMv8 emulation/virtualization.
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

#ifndef VBOX_INCLUDED_vmm_cpumctx_armv8_h
#define VBOX_INCLUDED_vmm_cpumctx_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifndef VBOX_FOR_DTRACE_LIB
# include <iprt/assertcompile.h>
# include <VBox/types.h>
#else
# pragma D depends_on library arm.d
#endif


RT_C_DECLS_BEGIN

/** @defgroup grp_cpum_ctx  The CPUM Context Structures
 * @ingroup grp_cpum
 * @{
 */

/** A general register (union). */
typedef union CPUMCTXGREG
{
    /** X<n> register view. */
    uint64_t            x;
    /** 32-bit W<n>view. */
    uint32_t            w;
} CPUMCTXGREG;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXGREG, 8);
#endif


/**
 * V<n> register union.
 */
typedef union CPUMCTXVREG
{
    /** V Register view. */
    RTUINT128U  v;
    /** 8-bit view. */
    uint8_t     au8[16];
    /** 16-bit view. */
    uint16_t    au16[8];
    /** 32-bit view. */
    uint32_t    au32[4];
    /** 64-bit view. */
    uint64_t    au64[2];
    /** Signed 8-bit view. */
    int8_t      ai8[16];
    /** Signed 16-bit view. */
    int16_t     ai16[8];
    /** Signed 32-bit view. */
    int32_t     ai32[4];
    /** Signed 64-bit view. */
    int64_t     ai64[2];
    /** 128-bit view. (yeah, very helpful) */
    uint128_t   au128[1];
    /** Single precision floating point view. */
    RTFLOAT32U  ar32[4];
    /** Double precision floating point view. */
    RTFLOAT64U  ar64[2];
} CPUMCTXVREG;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXVREG, 16);
#endif
/** Pointer to an V<n> register state. */
typedef CPUMCTXVREG *PCPUMCTXVREG;
/** Pointer to a const V<n> register state. */
typedef CPUMCTXVREG const *PCCPUMCTXVREG;


/**
 * A system level register.
 */
typedef union CPUMCTXSYSREG
{
    /** 64-bit view. */
    uint64_t    u64;
    /** 32-bit view. */
    uint32_t    u32;
} CPUMCTXSYSREG;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXSYSREG, 8);
#endif


/**
 * A debug register state (control and value), these are held together
 * because they will be accessed together very often and thus minimizes
 * stress on the cache.
 */
typedef struct CPUMCTXSYSREGDBG
{
    /** The control register. */
    CPUMCTXSYSREG   Ctrl;
    /** The value register. */
    CPUMCTXSYSREG   Value;
} CPUMCTXSYSREGDBG;
/** Pointer to a debug register state. */
typedef CPUMCTXSYSREGDBG *PCPUMCTXSYSREGDBG;
/** Pointer to a const debug register state. */
typedef const CPUMCTXSYSREGDBG *PCCPUMCTXSYSREGDBG;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXSYSREGDBG, 16);
#endif


/**
 * A pointer authentication key register state (low and high), these are held together
 * because they will be accessed together very often and thus minimizes
 * stress on the cache.
 */
typedef struct CPUMCTXSYSREGPAKEY
{
    /** The low key register. */
    CPUMCTXSYSREG   Low;
    /** The high key register. */
    CPUMCTXSYSREG   High;
} CPUMCTXSYSREGPAKEY;
/** Pointer to a pointer authentication key register state. */
typedef CPUMCTXSYSREGPAKEY *PCPUMCTXSYSREGPAKEY;
/** Pointer to a const pointer authentication key register state. */
typedef const CPUMCTXSYSREGPAKEY *PCCPUMCTXSYSREGPAKEY;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCTXSYSREGPAKEY, 16);
#endif


/**
 * CPU context.
 */
typedef struct CPUMCTX
{
    /** The general purpose register array view. */
    CPUMCTXGREG         aGRegs[31];
    /** The NEON SIMD & FP register array view. */
    CPUMCTXVREG         aVRegs[32];
    /** The stack registers (EL0, EL1). */
    CPUMCTXSYSREG       aSpReg[2];
    /** The program counter. */
    CPUMCTXSYSREG       Pc;
    /** The SPSR (Saved Program Status Register) (EL1 only). */
    CPUMCTXSYSREG       Spsr;
    /** The ELR (Exception Link Register) (EL1 only). */
    CPUMCTXSYSREG       Elr;
    /** The SCTLR_EL1 register. */
    CPUMCTXSYSREG       Sctlr;
    /** THe TCR_EL1 register. */
    CPUMCTXSYSREG       Tcr;
    /** The TTBR0_EL1 register. */
    CPUMCTXSYSREG       Ttbr0;
    /** The TTBR1_EL1 register. */
    CPUMCTXSYSREG       Ttbr1;
    /** The VBAR_EL1 register. */
    CPUMCTXSYSREG       VBar;
    /** Breakpoint registers, DBGB{C,V}n_EL1. */
    CPUMCTXSYSREGDBG    aBp[16];
    /** Watchpoint registers, DBGW{C,V}n_EL1. */
    CPUMCTXSYSREGDBG    aWp[16];
    /** APDA key register state. */
    CPUMCTXSYSREGPAKEY  Apda;
    /** APDB key register state. */
    CPUMCTXSYSREGPAKEY  Apdb;
    /** APGA key register state. */
    CPUMCTXSYSREGPAKEY  Apga;
    /** APIA key register state. */
    CPUMCTXSYSREGPAKEY  Apia;
    /** APIB key register state. */
    CPUMCTXSYSREGPAKEY  Apib;

    /** Floating point control register. */
    uint64_t            fpcr;
    /** Floating point status register. */
    uint64_t            fpsr;
    /** The internal PSTATE state (as given from SPSR_EL2). */
    uint64_t            fPState;

    uint32_t            fPadding0;

    /** OS lock status accessed through OSLAR_EL1 and OSLSR_EL1. */
    bool                fOsLck;

    uint8_t             afPadding1[7];

    /** Externalized state tracker, CPUMCTX_EXTRN_XXX. */
    uint64_t            fExtrn;

    /** The CNTV_CTL_EL0 register, always synced during VM-exit. */
    uint64_t            CntvCtlEl0;
    /** The CNTV_CVAL_EL0 register, always synced during VM-exit. */
    uint64_t            CntvCValEl0;

    uint64_t            au64Padding2[4];
} CPUMCTX;


#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSizeAlignment(CPUMCTX, 64);
AssertCompileSizeAlignment(CPUMCTX, 32);
AssertCompileSizeAlignment(CPUMCTX, 16);
AssertCompileSizeAlignment(CPUMCTX, 8);
#endif /* !VBOX_FOR_DTRACE_LIB */


/** @name CPUMCTX_EXTRN_XXX
 * Used for parts of the CPUM state that is externalized and needs fetching
 * before use.
 *
 * @{ */
/** External state keeper: Invalid.  */
#define CPUMCTX_EXTRN_KEEPER_INVALID            UINT64_C(0x0000000000000000)
/** External state keeper: NEM. */
#define CPUMCTX_EXTRN_KEEPER_NEM                UINT64_C(0x0000000000000001)
/** External state keeper mask. */
#define CPUMCTX_EXTRN_KEEPER_MASK               UINT64_C(0x0000000000000003)

/** The PC register value is kept externally. */
#define CPUMCTX_EXTRN_PC                        UINT64_C(0x0000000000000004)
/** The SPSR register values are kept externally. */
#define CPUMCTX_EXTRN_SPSR                      UINT64_C(0x0000000000000008)
/** The ELR register values are kept externally. */
#define CPUMCTX_EXTRN_ELR                       UINT64_C(0x0000000000000010)
/** The SP register values are kept externally. */
#define CPUMCTX_EXTRN_SP                        UINT64_C(0x0000000000000020)
/** The PSTATE value is kept externally. */
#define CPUMCTX_EXTRN_PSTATE                    UINT64_C(0x0000000000000040)
/** The SCTRL_EL1/TCR_EL1/TTBR{0,1}_EL1 system registers are kept externally. */
#define CPUMCTX_EXTRN_SCTLR_TCR_TTBR            UINT64_C(0x0000000000000080)

/** The X0 register value is kept externally. */
#define CPUMCTX_EXTRN_X0                        UINT64_C(0x0000000000000100)
/** The X1 register value is kept externally. */
#define CPUMCTX_EXTRN_X1                        UINT64_C(0x0000000000000200)
/** The X2 register value is kept externally. */
#define CPUMCTX_EXTRN_X2                        UINT64_C(0x0000000000000400)
/** The X3 register value is kept externally. */
#define CPUMCTX_EXTRN_X3                        UINT64_C(0x0000000000000800)
/** The LR (X30) register value is kept externally. */
#define CPUMCTX_EXTRN_LR                        UINT64_C(0x0000000000001000)
/** The FP (X29) register value is kept externally. */
#define CPUMCTX_EXTRN_FP                        UINT64_C(0x0000000000002000)
/** The X4 through X28 register values are kept externally. */
#define CPUMCTX_EXTRN_X4_X28                    UINT64_C(0x0000000000004000)
/** General purpose registers mask. */
#define CPUMCTX_EXTRN_GPRS_MASK                 UINT64_C(0x0000000000007f00)

/** The NEON SIMD & FP registers V0 through V31 are kept externally. */
#define CPUMCTX_EXTRN_V0_V31                    UINT64_C(0x0000000000002000)
/** The FPCR (Floating Point Control Register) is kept externally. */
#define CPUMCTX_EXTRN_FPCR                      UINT64_C(0x0000000000004000)
/** The FPSR (Floating Point Status Register) is kept externally. */
#define CPUMCTX_EXTRN_FPSR                      UINT64_C(0x0000000000008000)

/** Debug system registers are kept externally. */
#define CPUMCTX_EXTRN_SYSREG_DEBUG              UINT64_C(0x0000000000010000)
/** PAuth key system registers are kept externally. */
#define CPUMCTX_EXTRN_SYSREG_PAUTH_KEYS         UINT64_C(0x0000000000020000)
/** Various system registers (rarely accessed) are kept externally. */
#define CPUMCTX_EXTRN_SYSREG_MISC               UINT64_C(0x0000000000040000)

/** Mask of bits the keepers can use for state tracking. */
#define CPUMCTX_EXTRN_KEEPER_STATE_MASK         UINT64_C(0xffff000000000000)

/** All CPUM state bits, not including keeper specific ones. */
#define CPUMCTX_EXTRN_ALL                       UINT64_C(0x00000ffffffffffc)
/** All CPUM state bits, including keeper specific ones. */
#define CPUMCTX_EXTRN_ABSOLUTELY_ALL            UINT64_C(0xfffffffffffffffc)
/** @} */

/** @}  */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_cpumctx_armv8_h */

