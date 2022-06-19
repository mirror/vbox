/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-2, C code template.
 */

/*
 * Copyright (C) 2007-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
# if ARCH_BITS == 64
typedef struct BS3CI2FSGSBASE
{
    const char *pszDesc;
    bool        f64BitOperand;
    FPFNBS3FAR  pfnWorker;
    uint8_t     offWorkerUd2;
    FPFNBS3FAR  pfnVerifyWorker;
    uint8_t     offVerifyWorkerUd2;
} BS3CI2FSGSBASE;
# endif
#endif


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_mul_xBX_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_imul_xBX_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_imul_xCX_xBX_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_div_xBX_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_idiv_xBX_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_RDX_2_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_L1);
# if ARCH_BITS == 64
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_X1);
# endif
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_V1);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_V15);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_DSxDI_68_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_DSxDI_36_icebp);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_RBX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_FSxBX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_EBX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_FSxBX_icebp);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_bextr_RAX_RBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_bextr_RAX_FSxBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_bextr_EAX_EBX_ECX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_bextr_EAX_FSxBX_ECX_icebp);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shlx_RAX_RBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shlx_RAX_FSxBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shlx_EAX_EBX_ECX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shlx_EAX_FSxBX_ECX_icebp);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_sarx_RAX_RBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_sarx_RAX_FSxBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_sarx_EAX_EBX_ECX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_sarx_EAX_FSxBX_ECX_icebp);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shrx_RAX_RBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shrx_RAX_FSxBX_RCX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shrx_EAX_EBX_ECX_icebp);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_shrx_EAX_FSxBX_ECX_icebp);

# if ARCH_BITS == 64
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_lock_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_o16_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_lock_o16_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_repz_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_lock_repz_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_repnz_cmpxchg16b_rdi_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_lock_repnz_cmpxchg16b_rdi_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrfsbase_rbx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrfsbase_ebx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrfsbase_rbx_rdfsbase_rcx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrfsbase_ebx_rdfsbase_ecx_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrgsbase_rbx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrgsbase_ebx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrgsbase_rbx_rdgsbase_rcx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_wrgsbase_ebx_rdgsbase_ecx_ud2);

extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rdfsbase_rbx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rdfsbase_ebx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rdgsbase_rbx_ud2);
extern FNBS3FAR     BS3_CMN_NM(bs3CpuInstr2_rdgsbase_ebx_ud2);
# endif
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
# if ARCH_BITS == 64
static BS3CI2FSGSBASE const s_aWrFsBaseWorkers[] =
{
    { "wrfsbase rbx", true,  BS3_CMN_NM(bs3CpuInstr2_wrfsbase_rbx_ud2), 5, BS3_CMN_NM(bs3CpuInstr2_wrfsbase_rbx_rdfsbase_rcx_ud2), 15 },
    { "wrfsbase ebx", false, BS3_CMN_NM(bs3CpuInstr2_wrfsbase_ebx_ud2), 4, BS3_CMN_NM(bs3CpuInstr2_wrfsbase_ebx_rdfsbase_ecx_ud2), 13 },
};

static BS3CI2FSGSBASE const s_aWrGsBaseWorkers[] =
{
    { "wrgsbase rbx", true,  BS3_CMN_NM(bs3CpuInstr2_wrgsbase_rbx_ud2), 5, BS3_CMN_NM(bs3CpuInstr2_wrgsbase_rbx_rdgsbase_rcx_ud2), 15 },
    { "wrgsbase ebx", false, BS3_CMN_NM(bs3CpuInstr2_wrgsbase_ebx_ud2), 4, BS3_CMN_NM(bs3CpuInstr2_wrgsbase_ebx_rdgsbase_ecx_ud2), 13 },
};

static BS3CI2FSGSBASE const s_aRdFsBaseWorkers[] =
{
    { "rdfsbase rbx", true,  BS3_CMN_NM(bs3CpuInstr2_rdfsbase_rbx_ud2), 5, BS3_CMN_NM(bs3CpuInstr2_wrfsbase_rbx_rdfsbase_rcx_ud2), 15 },
    { "rdfsbase ebx", false, BS3_CMN_NM(bs3CpuInstr2_rdfsbase_ebx_ud2), 4, BS3_CMN_NM(bs3CpuInstr2_wrfsbase_ebx_rdfsbase_ecx_ud2), 13 },
};

static BS3CI2FSGSBASE const s_aRdGsBaseWorkers[] =
{
    { "rdgsbase rbx", true,  BS3_CMN_NM(bs3CpuInstr2_rdgsbase_rbx_ud2), 5, BS3_CMN_NM(bs3CpuInstr2_wrgsbase_rbx_rdgsbase_rcx_ud2), 15 },
    { "rdgsbase ebx", false, BS3_CMN_NM(bs3CpuInstr2_rdgsbase_ebx_ud2), 4, BS3_CMN_NM(bs3CpuInstr2_wrgsbase_ebx_rdgsbase_ecx_ud2), 13 },
};
# endif
#endif /* BS3_INSTANTIATING_CMN - global */


/*
 * Common code.
 * Common code.
 * Common code.
 */
#ifdef BS3_INSTANTIATING_CMN

BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_mul)(uint8_t bMode)
{
#define MUL_CHECK_EFLAGS_ZERO  (uint16_t)(X86_EFL_AF | X86_EFL_ZF)
#define MUL_CHECK_EFLAGS       (uint16_t)(X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF)

    static const struct
    {
        RTCCUINTREG uInAX;
        RTCCUINTREG uInBX;
        RTCCUINTREG uOutDX;
        RTCCUINTREG uOutAX;
        uint16_t    fFlags;
    } s_aTests[] =
    {
        {   1,      1,
            0,      1,      0 },
        {   2,      2,
            0,      4,      0 },
        {   RTCCUINTREG_MAX, RTCCUINTREG_MAX,
            RTCCUINTREG_MAX-1,  1,                  X86_EFL_CF | X86_EFL_OF },
        {   RTCCINTREG_MAX,  RTCCINTREG_MAX,
            RTCCINTREG_MAX / 2, 1,                  X86_EFL_CF | X86_EFL_OF },
        {   1, RTCCUINTREG_MAX,
            0, RTCCUINTREG_MAX,                     X86_EFL_PF | X86_EFL_SF },
        {   1, RTCCINTREG_MAX,
            0, RTCCINTREG_MAX,                      X86_EFL_PF },
        {   2, RTCCINTREG_MAX,
            0, RTCCUINTREG_MAX - 1,                 X86_EFL_SF },
        {   (RTCCUINTREG)RTCCINTREG_MAX + 1, 2,
            1, 0,                                   X86_EFL_PF | X86_EFL_CF | X86_EFL_OF },
        {   (RTCCUINTREG)RTCCINTREG_MAX / 2 + 1, 3,
            0, ((RTCCUINTREG)RTCCINTREG_MAX / 2 + 1) * 3, X86_EFL_PF | X86_EFL_SF },
    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j, k;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);
    Bs3RegCtxSetRipCsFromCurPtr(&Ctx, BS3_CMN_NM(bs3CpuInstr2_mul_xBX_ud2));
    for (k = 0; k < 2; k++)
    {
        Ctx.rflags.u16 |= MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO;
        for (j = 0; j < 2; j++)
        {
            for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
            {
                if (k == 0)
                {
                    Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                }
                else
                {
                    Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                }
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                if (TrapFrame.bXcpt != X86_XCPT_UD)
                    Bs3TestFailedF("Expected #UD got %#x", TrapFrame.bXcpt);
                else if (   TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX
                         || TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX
                         ||    (TrapFrame.Ctx.rflags.u16 & (MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO))
                            != (s_aTests[i].fFlags & MUL_CHECK_EFLAGS) )
                {
                    Bs3TestFailedF("test #%i failed: input %#" RTCCUINTREG_XFMT " * %#" RTCCUINTREG_XFMT,
                                   i, s_aTests[i].uInAX, s_aTests[i].uInBX);

                    if (TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX)
                        Bs3TestFailedF("Expected xAX = %#RX" RT_XSTR(ARCH_BITS) " got %#RX" RT_XSTR(ARCH_BITS),
                                       s_aTests[i].uOutAX, TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS));
                    if (TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX)
                        Bs3TestFailedF("Expected xDX = %#RX" RT_XSTR(ARCH_BITS) " got %#RX" RT_XSTR(ARCH_BITS),
                                       s_aTests[i].uOutDX, TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS));
                    if (   (TrapFrame.Ctx.rflags.u16 & (MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO))
                        != (s_aTests[i].fFlags & MUL_CHECK_EFLAGS) )
                        Bs3TestFailedF("Expected EFLAGS = %#06RX16, got %#06RX16", s_aTests[i].fFlags & MUL_CHECK_EFLAGS,
                                       TrapFrame.Ctx.rflags.u16 & (MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO));
                }
            }
            Ctx.rflags.u16 &= ~(MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO);
        }
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_imul)(uint8_t bMode)
{
#define IMUL_CHECK_EFLAGS_ZERO  (uint16_t)(X86_EFL_AF | X86_EFL_ZF)
#define IMUL_CHECK_EFLAGS       (uint16_t)(X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF)
    static const struct
    {
        RTCCUINTREG uInAX;
        RTCCUINTREG uInBX;
        RTCCUINTREG uOutDX;
        RTCCUINTREG uOutAX;
        uint16_t    fFlags;
    } s_aTests[] =
    {
        /* two positive values. */
        {   1,      1,
            0,      1,      0 },
        {   2,      2,
            0,      4,      0 },
        {   RTCCINTREG_MAX, RTCCINTREG_MAX,
            RTCCINTREG_MAX/2, 1,                    X86_EFL_CF | X86_EFL_OF },
        {   1, RTCCINTREG_MAX,
            0, RTCCINTREG_MAX,                      X86_EFL_PF },
        {   2, RTCCINTREG_MAX,
            0, RTCCUINTREG_MAX - 1U,                X86_EFL_CF | X86_EFL_OF | X86_EFL_SF },
        {   2, RTCCINTREG_MAX / 2,
            0, RTCCINTREG_MAX - 1U,                 0 },
        {   2, (RTCCINTREG_MAX / 2 + 1),
            0, (RTCCUINTREG)RTCCINTREG_MAX + 1U,    X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF },
        {   4, (RTCCINTREG_MAX / 2 + 1),
            1, 0,                                   X86_EFL_CF | X86_EFL_OF | X86_EFL_PF },

        /* negative and positive */
        {   -4,     3,
            -1,     -12,                            X86_EFL_SF },
        {   32,     -127,
            -1,     -4064,                          X86_EFL_SF },
        {   RTCCINTREG_MIN, 1,
            -1, RTCCINTREG_MIN,                     X86_EFL_SF | X86_EFL_PF },
        {   RTCCINTREG_MIN, 2,
            -1,     0,                              X86_EFL_CF | X86_EFL_OF | X86_EFL_PF },
        {   RTCCINTREG_MIN, 3,
            -2,     RTCCINTREG_MIN,                 X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF },
        {   RTCCINTREG_MIN, 4,
            -2,     0,                              X86_EFL_CF | X86_EFL_OF | X86_EFL_PF },
        {   RTCCINTREG_MIN, RTCCINTREG_MAX,
            RTCCINTREG_MIN / 2, RTCCINTREG_MIN,     X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF },
        {   RTCCINTREG_MIN, RTCCINTREG_MAX - 1,
            RTCCINTREG_MIN / 2 + 1, 0,              X86_EFL_CF | X86_EFL_OF | X86_EFL_PF },

        /* two negative values. */
        {   -4,     -63,
            0,      252,                            X86_EFL_PF },
        {   RTCCINTREG_MIN, RTCCINTREG_MIN,
            RTCCUINTREG_MAX / 4 + 1, 0,             X86_EFL_CF | X86_EFL_OF | X86_EFL_PF },
        {   RTCCINTREG_MIN, RTCCINTREG_MIN + 1,
            RTCCUINTREG_MAX / 4, RTCCINTREG_MIN,    X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_PF},
        {   RTCCINTREG_MIN + 1, RTCCINTREG_MIN + 1,
            RTCCUINTREG_MAX / 4, 1,                 X86_EFL_CF | X86_EFL_OF },

    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j, k;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);
    Bs3RegCtxSetRipCsFromCurPtr(&Ctx, BS3_CMN_NM(bs3CpuInstr2_imul_xBX_ud2));

    for (k = 0; k < 2; k++)
    {
        Ctx.rflags.u16 |= MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO;
        for (j = 0; j < 2; j++)
        {
            for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
            {
                if (k == 0)
                {
                    Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                }
                else
                {
                    Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                }
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                if (TrapFrame.bXcpt != X86_XCPT_UD)
                    Bs3TestFailedF("Expected #UD got %#x", TrapFrame.bXcpt);
                else if (   TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX
                         || TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX
                         ||    (TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO))
                            != (s_aTests[i].fFlags & IMUL_CHECK_EFLAGS) )
                {
                    Bs3TestFailedF("test #%i failed: input %#" RTCCUINTREG_XFMT " * %#" RTCCUINTREG_XFMT,
                                   i, s_aTests[i].uInAX, s_aTests[i].uInBX);

                    if (TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX)
                        Bs3TestFailedF("Expected xAX = %#RX" RT_XSTR(ARCH_BITS) " got %#RX" RT_XSTR(ARCH_BITS),
                                       s_aTests[i].uOutAX, TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS));
                    if (TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX)
                        Bs3TestFailedF("Expected xDX = %#RX" RT_XSTR(ARCH_BITS) " got %#RX" RT_XSTR(ARCH_BITS),
                                       s_aTests[i].uOutDX, TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS));
                    if (   (TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO))
                        != (s_aTests[i].fFlags & IMUL_CHECK_EFLAGS) )
                        Bs3TestFailedF("Expected EFLAGS = %#06RX16, got %#06RX16", s_aTests[i].fFlags & IMUL_CHECK_EFLAGS,
                                       TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO));
                }
            }
        }
    }

    /*
     * Repeat for the truncating two operand version.
     */
    Bs3RegCtxSetRipCsFromCurPtr(&Ctx, BS3_CMN_NM(bs3CpuInstr2_imul_xCX_xBX_ud2));

    for (k = 0; k < 2; k++)
    {
        Ctx.rflags.u16 |= MUL_CHECK_EFLAGS | MUL_CHECK_EFLAGS_ZERO;
        for (j = 0; j < 2; j++)
        {
            for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
            {
                if (k == 0)
                {
                    Ctx.rcx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                }
                else
                {
                    Ctx.rcx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
                    Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
                }
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                if (TrapFrame.bXcpt != X86_XCPT_UD)
                    Bs3TestFailedF("Expected #UD got %#x", TrapFrame.bXcpt);
                else if (   TrapFrame.Ctx.rcx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX
                         || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                         || TrapFrame.Ctx.rbx.u != Ctx.rbx.u
                         ||    (TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO))
                            != (s_aTests[i].fFlags & IMUL_CHECK_EFLAGS) )
                {
                    Bs3TestFailedF("test #%i failed: input %#" RTCCUINTREG_XFMT " * %#" RTCCUINTREG_XFMT,
                                   i, s_aTests[i].uInAX, s_aTests[i].uInBX);

                    if (TrapFrame.Ctx.rcx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX)
                        Bs3TestFailedF("Expected xAX = %#RX" RT_XSTR(ARCH_BITS) " got %#RX" RT_XSTR(ARCH_BITS),
                                       s_aTests[i].uOutAX, TrapFrame.Ctx.rcx.RT_CONCAT(u,ARCH_BITS));
                    if (   (TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO))
                        != (s_aTests[i].fFlags & IMUL_CHECK_EFLAGS) )
                        Bs3TestFailedF("Expected EFLAGS = %#06RX16, got %#06RX16", s_aTests[i].fFlags & IMUL_CHECK_EFLAGS,
                                       TrapFrame.Ctx.rflags.u16 & (IMUL_CHECK_EFLAGS | IMUL_CHECK_EFLAGS_ZERO));
                }
            }
        }
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_div)(uint8_t bMode)
{
#define DIV_CHECK_EFLAGS (uint16_t)(X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
    static const struct
    {
        RTCCUINTREG uInDX;
        RTCCUINTREG uInAX;
        RTCCUINTREG uInBX;
        RTCCUINTREG uOutAX;
        RTCCUINTREG uOutDX;
        uint8_t     bXcpt;
    } s_aTests[] =
    {
        {   0,    1,                            1,
            1,    0,                                                    X86_XCPT_UD },
        {   0,    5,                            2,
            2,    1,                                                    X86_XCPT_UD },
        {   0,    0,                            0,
            0,    0,                                                    X86_XCPT_DE },
        { RTCCUINTREG_MAX, RTCCUINTREG_MAX,     0,
            0,    0,                                                    X86_XCPT_DE },
        { RTCCUINTREG_MAX, RTCCUINTREG_MAX,     1,
            0,    0,                                                    X86_XCPT_DE },
        { RTCCUINTREG_MAX, RTCCUINTREG_MAX,     RTCCUINTREG_MAX,
            0,    0,                                                    X86_XCPT_DE },
        { RTCCUINTREG_MAX - 1, RTCCUINTREG_MAX, RTCCUINTREG_MAX,
            RTCCUINTREG_MAX, RTCCUINTREG_MAX - 1,                       X86_XCPT_UD },
    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);
    Bs3RegCtxSetRipCsFromCurPtr(&Ctx, BS3_CMN_NM(bs3CpuInstr2_div_xBX_ud2));

    /*
     * Do the tests twice, first with all flags set, then once again with
     * flags cleared.  The flags are not touched by my intel skylake CPU.
     */
    Ctx.rflags.u16 |= DIV_CHECK_EFLAGS;
    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
            Ctx.rdx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInDX;
            Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);

            if (   TrapFrame.bXcpt                          != s_aTests[i].bXcpt
                || (   s_aTests[i].bXcpt == X86_XCPT_UD
                    ?    TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX
                      || TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX
                      || (TrapFrame.Ctx.rflags.u16 & DIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & DIV_CHECK_EFLAGS)
                    :    TrapFrame.Ctx.rax.u != Ctx.rax.u
                      || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                      || (TrapFrame.Ctx.rflags.u16 & DIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & DIV_CHECK_EFLAGS) ) )
            {
                Bs3TestFailedF("test #%i failed: input %#" RTCCUINTREG_XFMT ":%" RTCCUINTREG_XFMT " / %#" RTCCUINTREG_XFMT,
                               i, s_aTests[i].uInDX, s_aTests[i].uInAX, s_aTests[i].uInBX);
                if (TrapFrame.bXcpt != s_aTests[i].bXcpt)
                    Bs3TestFailedF("Expected bXcpt = %#x, got %#x", s_aTests[i].bXcpt, TrapFrame.bXcpt);
                if (s_aTests[i].bXcpt == X86_XCPT_UD)
                {
                    if (TrapFrame.Ctx.rax.RT_CONCAT(u, ARCH_BITS) != s_aTests[i].uOutAX)
                        Bs3TestFailedF("Expected xAX = %#" RTCCUINTREG_XFMT ", got %#" RTCCUINTREG_XFMT,
                                       s_aTests[i].uOutAX, TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS));
                    if (TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX)
                        Bs3TestFailedF("Expected xDX = %#" RTCCUINTREG_XFMT ", got %#" RTCCUINTREG_XFMT,
                                       s_aTests[i].uOutDX, TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS));
                    if ((TrapFrame.Ctx.rflags.u16 & DIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & DIV_CHECK_EFLAGS))
                        Bs3TestFailedF("Expected EFLAGS = %#06RX16, got %#06RX16",
                                       Ctx.rflags.u16 & DIV_CHECK_EFLAGS, TrapFrame.Ctx.rflags.u16 & DIV_CHECK_EFLAGS);
                }
            }
        }
        Ctx.rflags.u16 &= ~DIV_CHECK_EFLAGS;
    }

    return 0;
}



BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_idiv)(uint8_t bMode)
{
#define IDIV_CHECK_EFLAGS (uint16_t)(X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
    static const struct
    {
        RTCCUINTREG uInDX;
        RTCCUINTREG uInAX;
        RTCCUINTREG uInBX;
        RTCCUINTREG uOutAX;
        RTCCUINTREG uOutDX;
        uint8_t     bXcpt;
    } s_aTests[] =
    {
        {   0,    0,                            0,
            0,    0,                                                    X86_XCPT_DE },
        {   RTCCINTREG_MAX, RTCCINTREG_MAX,     0,
            0,    0,                                                    X86_XCPT_DE },
        /* two positive values. */
        {   0,    1,                    1,
            1,    0,                                                    X86_XCPT_UD },
        {   0,    5,                    2,
            2,    1,                                                    X86_XCPT_UD },
        {   RTCCINTREG_MAX / 2, RTCCUINTREG_MAX / 2,        RTCCINTREG_MAX,
            RTCCINTREG_MAX, RTCCINTREG_MAX - 1,                         X86_XCPT_UD },
        {   RTCCINTREG_MAX / 2, RTCCUINTREG_MAX / 2 + 1,    RTCCINTREG_MAX,
            RTCCINTREG_MAX, RTCCINTREG_MAX - 1,                         X86_XCPT_DE },
        /* negative dividend, positive divisor. */
        {   -1,  -7,                    2,
            -3,  -1,                                                    X86_XCPT_UD },
        {   RTCCINTREG_MIN / 2 + 1, 0,                      RTCCINTREG_MAX,
            RTCCINTREG_MIN + 2, RTCCINTREG_MIN + 2,                     X86_XCPT_UD },
        {   RTCCINTREG_MIN / 2,     0,                      RTCCINTREG_MAX,
            0,    0,                                                    X86_XCPT_DE },
        /* positive dividend, negative divisor. */
        {   0,    7,                    -2,
            -3,   1,                                                    X86_XCPT_UD },
        {   RTCCINTREG_MAX / 2 + 1, RTCCINTREG_MAX,         RTCCINTREG_MIN,
            RTCCINTREG_MIN,     RTCCINTREG_MAX,                         X86_XCPT_UD },
        {   RTCCINTREG_MAX / 2 + 1, (RTCCUINTREG)RTCCINTREG_MAX+1, RTCCINTREG_MIN,
            0,    0,                                                    X86_XCPT_DE },
        /* negative dividend, negative divisor. */
        {   -1,  -7,                    -2,
            3,   -1,                                                    X86_XCPT_UD },
        {   RTCCINTREG_MIN / 2, 1,                          RTCCINTREG_MIN,
            RTCCINTREG_MAX, RTCCINTREG_MIN + 1,                         X86_XCPT_UD },
        {   RTCCINTREG_MIN / 2, 2,                          RTCCINTREG_MIN,
            RTCCINTREG_MAX, RTCCINTREG_MIN + 2,                         X86_XCPT_UD },
        {   RTCCINTREG_MIN / 2, 0,                          RTCCINTREG_MIN,
            0, 0,                                                       X86_XCPT_DE },
    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);
    Bs3RegCtxSetRipCsFromCurPtr(&Ctx, BS3_CMN_NM(bs3CpuInstr2_idiv_xBX_ud2));

    /*
     * Do the tests twice, first with all flags set, then once again with
     * flags cleared.  The flags are not touched by my intel skylake CPU.
     */
    Ctx.rflags.u16 |= IDIV_CHECK_EFLAGS;
    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            Ctx.rax.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInAX;
            Ctx.rdx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInDX;
            Ctx.rbx.RT_CONCAT(u,ARCH_BITS) = s_aTests[i].uInBX;
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);

            if (   TrapFrame.bXcpt                          != s_aTests[i].bXcpt
                || (   s_aTests[i].bXcpt == X86_XCPT_UD
                    ?    TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutAX
                      || TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX
                      || (TrapFrame.Ctx.rflags.u16 & IDIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & IDIV_CHECK_EFLAGS)
                    :    TrapFrame.Ctx.rax.u != Ctx.rax.u
                      || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                      || (TrapFrame.Ctx.rflags.u16 & IDIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & IDIV_CHECK_EFLAGS) ) )
            {
                Bs3TestFailedF("test #%i failed: input %#" RTCCUINTREG_XFMT ":%" RTCCUINTREG_XFMT " / %#" RTCCUINTREG_XFMT,
                               i, s_aTests[i].uInDX, s_aTests[i].uInAX, s_aTests[i].uInBX);
                if (TrapFrame.bXcpt != s_aTests[i].bXcpt)
                    Bs3TestFailedF("Expected bXcpt = %#x, got %#x", s_aTests[i].bXcpt, TrapFrame.bXcpt);
                if (s_aTests[i].bXcpt == X86_XCPT_UD)
                {
                    if (TrapFrame.Ctx.rax.RT_CONCAT(u, ARCH_BITS) != s_aTests[i].uOutAX)
                        Bs3TestFailedF("Expected xAX = %#" RTCCUINTREG_XFMT ", got %#" RTCCUINTREG_XFMT,
                                       s_aTests[i].uOutAX, TrapFrame.Ctx.rax.RT_CONCAT(u,ARCH_BITS));
                    if (TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS) != s_aTests[i].uOutDX)
                        Bs3TestFailedF("Expected xDX = %#" RTCCUINTREG_XFMT ", got %#" RTCCUINTREG_XFMT,
                                       s_aTests[i].uOutDX, TrapFrame.Ctx.rdx.RT_CONCAT(u,ARCH_BITS));
                    if ((TrapFrame.Ctx.rflags.u16 & IDIV_CHECK_EFLAGS) != (Ctx.rflags.u16 & IDIV_CHECK_EFLAGS))
                        Bs3TestFailedF("Expected EFLAGS = %#06RX16, got %#06RX16",
                                       Ctx.rflags.u16 & IDIV_CHECK_EFLAGS, TrapFrame.Ctx.rflags.u16 & IDIV_CHECK_EFLAGS);
                }
            }
        }
        Ctx.rflags.u16 &= ~IDIV_CHECK_EFLAGS;
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_rorx)(uint8_t bMode)
{
    static const struct
    {
        FPFNBS3FAR      pfnWorker;
        bool            fMemSrc;
        bool            fOkay;
        RTCCUINTXREG    uIn;
        RTCCUINTXREG    uOut;
    } s_aTests[] =
    {
        /* 64 bits register width (32 bits in 32- and 16-bit modes): */
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_RDX_2_icebp),      false, true,    // #0
            0,                      /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_RDX_2_icebp),      false, true,    // #1
            ~(RTCCUINTXREG)2,       /* -> */ ~(RTCCUINTXREG)0 >> 1 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_DSxDI_68_icebp),   true,  true,    // #2
            0,                      /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_RBX_DSxDI_68_icebp),   true,  true,    // #3
            ~(RTCCUINTXREG)2,       /* -> */ (RTCCUINTXREG_MAX >> 4) | (~(RTCCUINTXREG)2 << (sizeof(RTCCUINTXREG) * 8 - 4)) },

        /* 32 bits register width: */
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp),      false, true,    // #4
            0,                      /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp),      false, true,    // #5
            ~(RTCCUINTXREG)2,       /* -> */ (RTCCUINTXREG)(~(uint32_t)0 >> 1) },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_DSxDI_36_icebp),   true,  true,    // #6
            0,                      /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_DSxDI_36_icebp),   true,  true,    // #7
            ~(RTCCUINTXREG)2,       /* -> */ (RTCCUINTXREG)UINT32_C(0xdfffffff) },

        /* encoding tests: */
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_L1),   false, false,   // #8
            RTCCUINTXREG_MAX,       /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_V1),   false, false,   // #9
            RTCCUINTXREG_MAX,       /* -> */ 0 },
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_V15),  false, false,   // #10
            RTCCUINTXREG_MAX,       /* -> */ 0 },
# if ARCH_BITS == 64 /* The VEX.X=0 encoding mean LES instruction in 32-bit and 16-bit mode. */
        {   BS3_CMN_NM(bs3CpuInstr2_rorx_EBX_EDX_2_icebp_X1),   false, true,    // #11
            UINT32_C(0xf1e2d3c5),   /* -> */ (RTCCUINTXREG)UINT32_C(0x7c78b4f1) },
# endif
    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j;
    uint32_t        uStdExtFeatEbx = 0;
    bool            fSupportsRorX;

    if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
        ASMCpuIdExSlow(7, 0, 0, 0, NULL, &uStdExtFeatEbx, NULL, NULL);
    fSupportsRorX = RT_BOOL(uStdExtFeatEbx & X86_CPUID_STEXT_FEATURE_EBX_BMI2);

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);

    /*
     * Do the tests twice, first with all flags set, then once again with
     * flags cleared.  The flags are not supposed to be touched at all.
     */
    Ctx.rflags.u16 |= X86_EFL_STATUS_BITS;
    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            bool const    fOkay       = !BS3_MODE_IS_RM_OR_V86(bMode) && s_aTests[i].fOkay && fSupportsRorX;
            uint8_t const bExpectXcpt = fOkay ? X86_XCPT_DB : X86_XCPT_UD;
            uint64_t      uExpectRbx, uExpectRip;
            RTCCUINTXREG  uMemSrc, uMemSrcExpect;
            Ctx.rbx.uCcXReg = RTCCUINTXREG_MAX * 1019;
            if (!s_aTests[i].fMemSrc)
            {
                Ctx.rdx.uCcXReg         = s_aTests[i].uIn;
                uMemSrcExpect = uMemSrc = ~s_aTests[i].uIn;
            }
            else
            {
                Ctx.rdx.uCcXReg         = ~s_aTests[i].uIn;
                uMemSrcExpect = uMemSrc = s_aTests[i].uIn;
                Bs3RegCtxSetGrpDsFromCurPtr(&Ctx, &Ctx.rdi, &uMemSrc);
            }
            Bs3RegCtxSetRipCsFromCurPtr(&Ctx, s_aTests[i].pfnWorker);
            uExpectRbx = fOkay ? s_aTests[i].uOut : Ctx.rbx.u;
            uExpectRip = Ctx.rip.u + (fOkay ? 6 + 1 : 0);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);

            if (   TrapFrame.bXcpt != bExpectXcpt
                || TrapFrame.Ctx.rip.u != uExpectRip
                || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                || TrapFrame.Ctx.rbx.u != uExpectRbx
                /* check that nothing else really changed: */
                || (TrapFrame.Ctx.rflags.u16 & X86_EFL_STATUS_BITS) != (Ctx.rflags.u16 & X86_EFL_STATUS_BITS)
                || TrapFrame.Ctx.rax.u != Ctx.rax.u
                || TrapFrame.Ctx.rcx.u != Ctx.rcx.u
                || TrapFrame.Ctx.rsp.u != Ctx.rsp.u
                || TrapFrame.Ctx.rbp.u != Ctx.rbp.u
                || TrapFrame.Ctx.rsi.u != Ctx.rsi.u
                || TrapFrame.Ctx.rdi.u != Ctx.rdi.u
                || uMemSrc != uMemSrcExpect
               )
            {
                Bs3TestFailedF("test #%i failed: input %#" RTCCUINTXREG_XFMT, i, s_aTests[i].uIn);
                if (TrapFrame.bXcpt != bExpectXcpt)
                    Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bExpectXcpt, TrapFrame.bXcpt);
                if (TrapFrame.Ctx.rip.u != uExpectRip)
                    Bs3TestFailedF("Expected RIP = %#06RX64, got %#06RX64", uExpectRip, TrapFrame.Ctx.rip.u);
                if (TrapFrame.Ctx.rdx.u != Ctx.rdx.u)
                    Bs3TestFailedF("Expected RDX = %#06RX64, got %#06RX64 (src)", Ctx.rdx.u, TrapFrame.Ctx.rdx.u);
                if (TrapFrame.Ctx.rbx.u != uExpectRbx)
                    Bs3TestFailedF("Expected RBX = %#06RX64, got %#06RX64 (dst)", uExpectRbx, TrapFrame.Ctx.rbx.u);

                if ((TrapFrame.Ctx.rflags.u16 & X86_EFL_STATUS_BITS) != (Ctx.rflags.u16 & X86_EFL_STATUS_BITS))
                    Bs3TestFailedF("Expected EFLAGS = %#06RX64, got %#06RX64",
                                   Ctx.rflags.u16 & X86_EFL_STATUS_BITS, TrapFrame.Ctx.rflags.u16 & X86_EFL_STATUS_BITS);
                if (TrapFrame.Ctx.rax.u != Ctx.rax.u)
                    Bs3TestFailedF("Expected RAX = %#06RX64, got %#06RX64", Ctx.rax.u, TrapFrame.Ctx.rax.u);
                if (TrapFrame.Ctx.rcx.u != Ctx.rcx.u)
                    Bs3TestFailedF("Expected RCX = %#06RX64, got %#06RX64", Ctx.rcx.u, TrapFrame.Ctx.rcx.u);
                if (TrapFrame.Ctx.rsp.u != Ctx.rsp.u)
                    Bs3TestFailedF("Expected RSP = %#06RX64, got %#06RX64", Ctx.rsp.u, TrapFrame.Ctx.rsp.u);
                if (TrapFrame.Ctx.rbp.u != Ctx.rbp.u)
                    Bs3TestFailedF("Expected RBP = %#06RX64, got %#06RX64", Ctx.rbp.u, TrapFrame.Ctx.rbp.u);
                if (TrapFrame.Ctx.rsi.u != Ctx.rsi.u)
                    Bs3TestFailedF("Expected RSI = %#06RX64, got %#06RX64", Ctx.rsi.u, TrapFrame.Ctx.rsi.u);
                if (TrapFrame.Ctx.rdi.u != Ctx.rdi.u)
                    Bs3TestFailedF("Expected RDI = %#06RX64, got %#06RX64", Ctx.rdi.u, TrapFrame.Ctx.rdi.u);
                if (uMemSrc != uMemSrcExpect)
                    Bs3TestFailedF("Expected uMemSrc = %#06RX64, got %#06RX64", (uint64_t)uMemSrcExpect, (uint64_t)uMemSrc);
            }
        }
        Ctx.rflags.u16 &= ~X86_EFL_STATUS_BITS;
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_andn)(uint8_t bMode)
{
#define ANDN_CHECK_EFLAGS  (uint16_t)(X86_EFL_CF | X86_EFL_ZF | X86_EFL_OF | X86_EFL_SF)
#define ANDN_IGNORE_EFLAGS (uint16_t)(X86_EFL_AF | X86_EFL_PF) /* undefined, ignoring for now */
    static const struct
    {
        FPFNBS3FAR      pfnWorker;
        bool            fMemSrc;
        uint8_t         cbInstr;
        RTCCUINTXREG    uSrc1;
        RTCCUINTXREG    uSrc2;
        RTCCUINTXREG    uOut;
        uint16_t        fEFlags;
    } s_aTests[] =
    {
        /* 64 bits register width (32 bits in 32- and 16-bit modes): */
        {   BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_RBX_icebp),    false,    5,                            // #0
            0,                      0,                      /* -> */ 0,                 X86_EFL_ZF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_RBX_icebp),    false,    5,                            // #1
            2,                      ~(RTCCUINTXREG)3,       /* -> */ ~(RTCCUINTXREG)3,  X86_EFL_SF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_FSxBX_icebp),  true,     6,                            // #2
            0,                      0,                      /* -> */ 0,                 X86_EFL_ZF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_RAX_RCX_FSxBX_icebp),  true,     6,                            // #3
            2,                      ~(RTCCUINTXREG)3,       /* -> */ ~(RTCCUINTXREG)3,  X86_EFL_SF },

        /* 32-bit register width */
        {   BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_EBX_icebp),    false,    5,                            // #4
            0,                      0,                      /* -> */ 0,                 X86_EFL_ZF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_EBX_icebp),    false,    5,                            // #5
            2,                      ~(RTCCUINTXREG)7,       /* -> */ ~(uint32_t)7,      X86_EFL_SF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_FSxBX_icebp),  true,     6,                            // #6
            0,                      0,                      /* -> */ 0,                 X86_EFL_ZF },
        {   BS3_CMN_NM(bs3CpuInstr2_andn_EAX_ECX_FSxBX_icebp),  true,     6,                            // #7
            2,                      ~(RTCCUINTXREG)7,       /* -> */ ~(uint32_t)7,      X86_EFL_SF },

    };

    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j;
    uint32_t        uStdExtFeatEbx = 0;
    bool            fSupportsAndN;

    if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
        ASMCpuIdExSlow(7, 0, 0, 0, NULL, &uStdExtFeatEbx, NULL, NULL);
    fSupportsAndN = RT_BOOL(uStdExtFeatEbx & X86_CPUID_STEXT_FEATURE_EBX_BMI1);

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);

    /*
     * Do the tests twice, first with all flags set, then once again with
     * flags cleared.  The flags are not supposed to be touched at all.
     */
    Ctx.rflags.u16 |= X86_EFL_STATUS_BITS;
    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            bool const    fOkay       = !BS3_MODE_IS_RM_OR_V86(bMode) && fSupportsAndN;
            uint8_t const bExpectXcpt = fOkay ? X86_XCPT_DB : X86_XCPT_UD;
            uint64_t      uExpectRax, uExpectRip;
            RTCCUINTXREG  uMemSrc2, uMemSrc2Expect;

            Ctx.rax.uCcXReg = RTCCUINTXREG_MAX * 1019;
            Ctx.rcx.uCcXReg = s_aTests[i].uSrc1;
            if (!s_aTests[i].fMemSrc)
            {
                Ctx.rbx.uCcXReg           = s_aTests[i].uSrc2;
                uMemSrc2Expect = uMemSrc2 = ~s_aTests[i].uSrc2;
            }
            else
            {
                uMemSrc2Expect = uMemSrc2 = s_aTests[i].uSrc2;
                Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, &uMemSrc2);
            }
            Bs3RegCtxSetRipCsFromCurPtr(&Ctx, s_aTests[i].pfnWorker);
            uExpectRax = fOkay ? s_aTests[i].uOut : Ctx.rax.u;
            uExpectRip = Ctx.rip.u + (fOkay ? s_aTests[i].cbInstr + 1 : 0);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);

            if (   TrapFrame.bXcpt != bExpectXcpt
                || TrapFrame.Ctx.rip.u != uExpectRip
                || TrapFrame.Ctx.rcx.u != Ctx.rcx.u
                || TrapFrame.Ctx.rbx.u != Ctx.rbx.u
                || TrapFrame.Ctx.rax.u != uExpectRax
                /* check that nothing else really changed: */
                ||    (TrapFrame.Ctx.rflags.u16 & ANDN_CHECK_EFLAGS)
                   != ((fOkay ? s_aTests[i].fEFlags : Ctx.rflags.u16) & ANDN_CHECK_EFLAGS)
                ||    (TrapFrame.Ctx.rflags.u16 & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS)
                   != (Ctx.rflags.u16           & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS)
                || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                || TrapFrame.Ctx.rsp.u != Ctx.rsp.u
                || TrapFrame.Ctx.rbp.u != Ctx.rbp.u
                || TrapFrame.Ctx.rsi.u != Ctx.rsi.u
                || TrapFrame.Ctx.rdi.u != Ctx.rdi.u
                || uMemSrc2 != uMemSrc2Expect
               )
            {
                Bs3TestFailedF("test #%i failed: input %#" RTCCUINTXREG_XFMT ", %#" RTCCUINTXREG_XFMT, i, s_aTests[i].uSrc1, s_aTests[i].uSrc2);
                if (TrapFrame.bXcpt != bExpectXcpt)
                    Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bExpectXcpt, TrapFrame.bXcpt);
                if (TrapFrame.Ctx.rip.u != uExpectRip)
                    Bs3TestFailedF("Expected RIP = %#06RX64, got %#06RX64", uExpectRip, TrapFrame.Ctx.rip.u);
                if (TrapFrame.Ctx.rax.u != uExpectRax)
                    Bs3TestFailedF("Expected RAX = %#06RX64, got %#06RX64", uExpectRax, TrapFrame.Ctx.rax.u);
                if (TrapFrame.Ctx.rcx.u != Ctx.rcx.u)
                    Bs3TestFailedF("Expected RCX = %#06RX64, got %#06RX64", Ctx.rcx.u, TrapFrame.Ctx.rcx.u);
                if (TrapFrame.Ctx.rbx.u != Ctx.rbx.u)
                    Bs3TestFailedF("Expected RBX = %#06RX64, got %#06RX64 (dst)", Ctx.rbx.u, TrapFrame.Ctx.rbx.u);
                if (   (TrapFrame.Ctx.rflags.u16 & ANDN_CHECK_EFLAGS)
                    != ((fOkay ? s_aTests[i].fEFlags : Ctx.rflags.u16) & ANDN_CHECK_EFLAGS))
                    Bs3TestFailedF("Expected EFLAGS = %#06RX64, got %#06RX64 (output)",
                                   (fOkay ? s_aTests[i].fEFlags : Ctx.rflags.u16) & ANDN_CHECK_EFLAGS, TrapFrame.Ctx.rflags.u16 & ANDN_CHECK_EFLAGS);
                if (   (TrapFrame.Ctx.rflags.u16 & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS)
                    != (Ctx.rflags.u16           & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS))
                    Bs3TestFailedF("Expected EFLAGS = %#06RX64, got %#06RX64 (immutable)",
                                   Ctx.rflags.u16           & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS,
                                   TrapFrame.Ctx.rflags.u16 & ~(ANDN_CHECK_EFLAGS | ANDN_IGNORE_EFLAGS) & X86_EFL_STATUS_BITS);

                if (TrapFrame.Ctx.rdx.u != Ctx.rdx.u)
                    Bs3TestFailedF("Expected RDX = %#06RX64, got %#06RX64 (src)", Ctx.rdx.u, TrapFrame.Ctx.rdx.u);
                if (TrapFrame.Ctx.rsp.u != Ctx.rsp.u)
                    Bs3TestFailedF("Expected RSP = %#06RX64, got %#06RX64", Ctx.rsp.u, TrapFrame.Ctx.rsp.u);
                if (TrapFrame.Ctx.rbp.u != Ctx.rbp.u)
                    Bs3TestFailedF("Expected RBP = %#06RX64, got %#06RX64", Ctx.rbp.u, TrapFrame.Ctx.rbp.u);
                if (TrapFrame.Ctx.rsi.u != Ctx.rsi.u)
                    Bs3TestFailedF("Expected RSI = %#06RX64, got %#06RX64", Ctx.rsi.u, TrapFrame.Ctx.rsi.u);
                if (TrapFrame.Ctx.rdi.u != Ctx.rdi.u)
                    Bs3TestFailedF("Expected RDI = %#06RX64, got %#06RX64", Ctx.rdi.u, TrapFrame.Ctx.rdi.u);
                if (uMemSrc2 != uMemSrc2Expect)
                    Bs3TestFailedF("Expected uMemSrc2 = %#06RX64, got %#06RX64", (uint64_t)uMemSrc2Expect, (uint64_t)uMemSrc2);
            }
        }
        Ctx.rflags.u16 &= ~X86_EFL_STATUS_BITS;
    }

    return 0;
}

/*
 * For testing BEXTR, SHLX SARX & SHRX.
 */
typedef struct BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T
{
    RTCCUINTXREG    uSrc1;
    RTCCUINTXREG    uSrc2;
    RTCCUINTXREG    uOut;
    uint16_t        fEflOut;
} BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T;

typedef struct BS3CPUINSTR2_TEST_Gy_Ey_By_T
{
    FPFNBS3FAR      pfnWorker;
    bool            fMemSrc;
    uint8_t         cbInstr;
    uint8_t         cSubTests;
    BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const *paSubTests;
} BS3CPUINSTR2_TEST_Gy_Ey_By_T;

static uint8_t bs3CpuInstr2_Common_Gy_Ey_By(uint8_t bMode, BS3CPUINSTR2_TEST_Gy_Ey_By_T const *paTests, unsigned cTests,
                                            uint32_t fStdExtFeatEbx, uint16_t fEflCheck, uint16_t fEflIgnore)
{
    BS3REGCTX       Ctx;
    BS3TRAPFRAME    TrapFrame;
    unsigned        i, j, k;
    uint32_t        uStdExtFeatEbx = 0;
    bool            fSupportsInstr;

    fEflCheck &= ~fEflIgnore;

    if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
        ASMCpuIdExSlow(7, 0, 0, 0, NULL, &uStdExtFeatEbx, NULL, NULL);
    fSupportsInstr = RT_BOOL(uStdExtFeatEbx & fStdExtFeatEbx);

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);

    /*
     * Do the tests twice, first with all flags set, then once again with
     * flags cleared.  The flags are not supposed to be touched at all.
     */
    Ctx.rflags.u16 |= X86_EFL_STATUS_BITS;
    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < cTests; i++)
        {
            for (k = 0; k < paTests[i].cSubTests; k++)
            {
                bool const    fOkay       = !BS3_MODE_IS_RM_OR_V86(bMode) && fSupportsInstr;
                uint8_t const bExpectXcpt = fOkay ? X86_XCPT_DB : X86_XCPT_UD;
                uint64_t      uExpectRax, uExpectRip;
                RTCCUINTXREG  uMemSrc1, uMemSrc1Expect;

                Ctx.rax.uCcXReg = RTCCUINTXREG_MAX * 1019;
                Ctx.rcx.uCcXReg = paTests[i].paSubTests[k].uSrc2;
                if (!paTests[i].fMemSrc)
                {
                    Ctx.rbx.uCcXReg           = paTests[i].paSubTests[k].uSrc1;
                    uMemSrc1Expect = uMemSrc1 = ~paTests[i].paSubTests[k].uSrc1;
                }
                else
                {
                    uMemSrc1Expect = uMemSrc1 = paTests[i].paSubTests[k].uSrc1;
                    Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, &uMemSrc1);
                }
                Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paTests[i].pfnWorker);
                uExpectRax = fOkay ? paTests[i].paSubTests[k].uOut : Ctx.rax.u;
                uExpectRip = Ctx.rip.u + (fOkay ? paTests[i].cbInstr + 1 : 0);
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);

                if (   TrapFrame.bXcpt != bExpectXcpt
                    || TrapFrame.Ctx.rip.u != uExpectRip
                    || TrapFrame.Ctx.rcx.u != Ctx.rcx.u
                    || TrapFrame.Ctx.rbx.u != Ctx.rbx.u
                    || TrapFrame.Ctx.rax.u != uExpectRax
                    /* check that nothing else really changed: */
                    ||    (TrapFrame.Ctx.rflags.u16 & fEflCheck)
                       != ((fOkay ? paTests[i].paSubTests[k].fEflOut : Ctx.rflags.u16) & fEflCheck)
                    ||    (TrapFrame.Ctx.rflags.u16 & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS)
                       != (Ctx.rflags.u16           & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS)
                    || TrapFrame.Ctx.rdx.u != Ctx.rdx.u
                    || TrapFrame.Ctx.rsp.u != Ctx.rsp.u
                    || TrapFrame.Ctx.rbp.u != Ctx.rbp.u
                    || TrapFrame.Ctx.rsi.u != Ctx.rsi.u
                    || TrapFrame.Ctx.rdi.u != Ctx.rdi.u
                    || uMemSrc1 != uMemSrc1Expect
                   )
                {
                    Bs3TestFailedF("test #%i/%i failed: input %#" RTCCUINTXREG_XFMT ", %#" RTCCUINTXREG_XFMT,
                                   i, k, paTests[i].paSubTests[k].uSrc1, paTests[i].paSubTests[k].uSrc2);
                    if (TrapFrame.bXcpt != bExpectXcpt)
                        Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bExpectXcpt, TrapFrame.bXcpt);
                    if (TrapFrame.Ctx.rip.u != uExpectRip)
                        Bs3TestFailedF("Expected RIP = %#06RX64, got %#06RX64", uExpectRip, TrapFrame.Ctx.rip.u);
                    if (TrapFrame.Ctx.rax.u != uExpectRax)
                        Bs3TestFailedF("Expected RAX = %#06RX64, got %#06RX64", uExpectRax, TrapFrame.Ctx.rax.u);
                    if (TrapFrame.Ctx.rcx.u != Ctx.rcx.u)
                        Bs3TestFailedF("Expected RCX = %#06RX64, got %#06RX64", Ctx.rcx.u, TrapFrame.Ctx.rcx.u);
                    if (TrapFrame.Ctx.rbx.u != Ctx.rbx.u)
                        Bs3TestFailedF("Expected RBX = %#06RX64, got %#06RX64 (dst)", Ctx.rbx.u, TrapFrame.Ctx.rbx.u);
                    if (   (TrapFrame.Ctx.rflags.u16 & fEflCheck)
                        != ((fOkay ? paTests[i].paSubTests[k].fEflOut : Ctx.rflags.u16) & fEflCheck))
                        Bs3TestFailedF("Expected EFLAGS = %#06RX64, got %#06RX64 (output)",
                                       (fOkay ? paTests[i].paSubTests[k].fEflOut : Ctx.rflags.u16) & fEflCheck,
                                       TrapFrame.Ctx.rflags.u16 & fEflCheck);
                    if (   (TrapFrame.Ctx.rflags.u16 & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS)
                        != (Ctx.rflags.u16           & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS))
                        Bs3TestFailedF("Expected EFLAGS = %#06RX64, got %#06RX64 (immutable)",
                                       Ctx.rflags.u16           & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS,
                                       TrapFrame.Ctx.rflags.u16 & ~(fEflCheck | fEflIgnore) & X86_EFL_STATUS_BITS);

                    if (TrapFrame.Ctx.rdx.u != Ctx.rdx.u)
                        Bs3TestFailedF("Expected RDX = %#06RX64, got %#06RX64 (src)", Ctx.rdx.u, TrapFrame.Ctx.rdx.u);
                    if (TrapFrame.Ctx.rsp.u != Ctx.rsp.u)
                        Bs3TestFailedF("Expected RSP = %#06RX64, got %#06RX64", Ctx.rsp.u, TrapFrame.Ctx.rsp.u);
                    if (TrapFrame.Ctx.rbp.u != Ctx.rbp.u)
                        Bs3TestFailedF("Expected RBP = %#06RX64, got %#06RX64", Ctx.rbp.u, TrapFrame.Ctx.rbp.u);
                    if (TrapFrame.Ctx.rsi.u != Ctx.rsi.u)
                        Bs3TestFailedF("Expected RSI = %#06RX64, got %#06RX64", Ctx.rsi.u, TrapFrame.Ctx.rsi.u);
                    if (TrapFrame.Ctx.rdi.u != Ctx.rdi.u)
                        Bs3TestFailedF("Expected RDI = %#06RX64, got %#06RX64", Ctx.rdi.u, TrapFrame.Ctx.rdi.u);
                    if (uMemSrc1 != uMemSrc1Expect)
                        Bs3TestFailedF("Expected uMemSrc1 = %#06RX64, got %#06RX64", (uint64_t)uMemSrc1Expect, (uint64_t)uMemSrc1);
                }
            }
        }
        Ctx.rflags.u16 &= ~X86_EFL_STATUS_BITS;
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_bextr)(uint8_t bMode)
{
    /* 64 bits register width (32 bits in 32- and 16-bit modes): */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests64[] =
    {
        {   0,                  RT_MAKE_U16(0, 0),   /* -> */ 0, X86_EFL_ZF },
        {   0,                  RT_MAKE_U16(16, 33), /* -> */ 0, X86_EFL_ZF },
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(2, 4), /* -> */   0xe, 0},
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(40, 8), /* -> */  ARCH_BITS == 64 ? 0xff : 0x00, ARCH_BITS == 64 ? 0 : X86_EFL_ZF },
    };

    /* 32-bit register width */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests32[] =
    {
        {   0,                  RT_MAKE_U16(0, 0),   /* -> */ 0,        X86_EFL_ZF },
        {   0,                  RT_MAKE_U16(16, 18), /* -> */ 0,        X86_EFL_ZF },
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(2, 4), /* -> */   0xe,      0 },
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(24, 8), /* -> */  0xff,     0 },
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(31, 9), /* -> */  1,        0 },
        {   ~(RTCCUINTXREG)7,   RT_MAKE_U16(42, 8), /* -> */  0,        X86_EFL_ZF },
    };

    static BS3CPUINSTR2_TEST_Gy_Ey_By_T const s_aTests[] =
    {
        { BS3_CMN_NM(bs3CpuInstr2_bextr_RAX_RBX_RCX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_bextr_RAX_FSxBX_RCX_icebp), true,  6, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_bextr_EAX_EBX_ECX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
        { BS3_CMN_NM(bs3CpuInstr2_bextr_EAX_FSxBX_ECX_icebp), true,  6, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
    };
    return bs3CpuInstr2_Common_Gy_Ey_By(bMode, s_aTests, RT_ELEMENTS(s_aTests), X86_CPUID_STEXT_FEATURE_EBX_BMI1,
                                        X86_EFL_STATUS_BITS, X86_EFL_AF | X86_EFL_SF | X86_EFL_PF);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_shlx)(uint8_t bMode)
{
    /* 64 bits register width (32 bits in 32- and 16-bit modes): */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests64[] =
    {
        {   0,                  0,                  /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)3,   /* -> */    0, 0 },
        {   ~(RTCCUINTXREG)7,   8,                  /* -> */    ~(RTCCUINTXREG)0x7ff, 0},
        {   ~(RTCCUINTXREG)7,   40,                 /* -> */    ~(RTCCUINTXREG)7 << (ARCH_BITS == 64 ? 40 : 8), 0 },
        {   ~(RTCCUINTXREG)7,   72,                 /* -> */    ~(RTCCUINTXREG)7 << 8, 0 },
    };

    /* 32-bit register width */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests32[] =
    {
        {   0,                  0,                  /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)9,   /* -> */    0, 0 },
        {   ~(RTCCUINTXREG)7,   8,                  /* -> */    UINT32_C(0xfffff800), 0 },
        {   ~(RTCCUINTXREG)7,   8,                  /* -> */    UINT32_C(0xfffff800), 0 },
    };

    static BS3CPUINSTR2_TEST_Gy_Ey_By_T const s_aTests[] =
    {
        { BS3_CMN_NM(bs3CpuInstr2_shlx_RAX_RBX_RCX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_shlx_RAX_FSxBX_RCX_icebp), true,  6, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_shlx_EAX_EBX_ECX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
        { BS3_CMN_NM(bs3CpuInstr2_shlx_EAX_FSxBX_ECX_icebp), true,  6, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
    };
    return bs3CpuInstr2_Common_Gy_Ey_By(bMode, s_aTests, RT_ELEMENTS(s_aTests), X86_CPUID_STEXT_FEATURE_EBX_BMI1,
                                        0, 0);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_sarx)(uint8_t bMode)
{
    /* 64 bits register width (32 bits in 32- and 16-bit modes): */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests64[] =
    {
        {   0,                  0,                  /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)3,   /* -> */    0, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 1), RTCCINTXREG_BITS - 1,        /* -> */ ~(RTCCUINTXREG)0, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 1), RTCCINTXREG_BITS - 1 + 64,   /* -> */ ~(RTCCUINTXREG)0, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 2), RTCCINTXREG_BITS - 3,        /* -> */ 2, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 2), RTCCINTXREG_BITS - 3 + 64,   /* -> */ 2, 0 },
    };

    /* 32-bit register width */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests32[] =
    {
        {   0,                  0,                      /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)9,       /* -> */    0, 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0x7fffffff), 24,    /* -> */    UINT32_C(0xffffff80), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0x7fffffff), 24+32, /* -> */    UINT32_C(0xffffff80), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0xbfffffff), 24,    /* -> */    UINT32_C(0x40), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0xbfffffff), 24+32, /* -> */    UINT32_C(0x40), 0 },
    };

    static BS3CPUINSTR2_TEST_Gy_Ey_By_T const s_aTests[] =
    {
        { BS3_CMN_NM(bs3CpuInstr2_sarx_RAX_RBX_RCX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_sarx_RAX_FSxBX_RCX_icebp), true,  6, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_sarx_EAX_EBX_ECX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
        { BS3_CMN_NM(bs3CpuInstr2_sarx_EAX_FSxBX_ECX_icebp), true,  6, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
    };
    return bs3CpuInstr2_Common_Gy_Ey_By(bMode, s_aTests, RT_ELEMENTS(s_aTests), X86_CPUID_STEXT_FEATURE_EBX_BMI1,
                                        0, 0);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_shrx)(uint8_t bMode)
{
    /* 64 bits register width (32 bits in 32- and 16-bit modes): */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests64[] =
    {
        {   0,                  0,                  /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)3,   /* -> */    0, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 1), RTCCINTXREG_BITS - 1,        /* -> */ 1, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 1), RTCCINTXREG_BITS - 1 + 64,   /* -> */ 1, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 2), RTCCINTXREG_BITS - 3,        /* -> */ 2, 0 },
        {   (RTCCUINTXREG)1 << (RTCCINTXREG_BITS - 2), RTCCINTXREG_BITS - 3 + 64,   /* -> */ 2, 0 },
    };

    /* 32-bit register width */
    static BS3CPUINSTR2_SUBTEST_Gy_Ey_By_T const s_aSubTests32[] =
    {
        {   0,                  0,                      /* -> */    0, 0 },
        {   0,                  ~(RTCCUINTXREG)9,       /* -> */    0, 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0x7fffffff), 24,    /* -> */    UINT32_C(0x80), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0x7fffffff), 24+32, /* -> */    UINT32_C(0x80), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0xbfffffff), 24,    /* -> */    UINT32_C(0x40), 0 },
        {   ~(RTCCUINTXREG)UINT32_C(0xbfffffff), 24+32, /* -> */    UINT32_C(0x40), 0 },
    };

    static BS3CPUINSTR2_TEST_Gy_Ey_By_T const s_aTests[] =
    {
        { BS3_CMN_NM(bs3CpuInstr2_shrx_RAX_RBX_RCX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_shrx_RAX_FSxBX_RCX_icebp), true,  6, RT_ELEMENTS(s_aSubTests64), s_aSubTests64 },
        { BS3_CMN_NM(bs3CpuInstr2_shrx_EAX_EBX_ECX_icebp),   false, 5, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
        { BS3_CMN_NM(bs3CpuInstr2_shrx_EAX_FSxBX_ECX_icebp), true,  6, RT_ELEMENTS(s_aSubTests32), s_aSubTests32 },
    };
    return bs3CpuInstr2_Common_Gy_Ey_By(bMode, s_aTests, RT_ELEMENTS(s_aTests), X86_CPUID_STEXT_FEATURE_EBX_BMI1,
                                        0, 0);
}


# if ARCH_BITS == 64

BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_cmpxchg16b)(uint8_t bMode)
{
    BS3REGCTX       Ctx;
    BS3REGCTX       ExpectCtx;
    BS3TRAPFRAME    TrapFrame;
    RTUINT128U      au128[3];
    PRTUINT128U     pau128       = RT_ALIGN_PT(&au128[0], sizeof(RTUINT128U), PRTUINT128U);
    bool const      fSupportCX16 = RT_BOOL(ASMCpuId_ECX(1) & X86_CPUID_FEATURE_ECX_CX16);
    unsigned        iFlags;
    unsigned        offBuf;
    unsigned        iMatch;
    unsigned        iWorker;
    static struct
    {
        bool        fLocked;
        uint8_t     offUd2;
        FNBS3FAR   *pfnWorker;
    } const s_aWorkers[] =
    {
        {   false,  4,  BS3_CMN_NM(bs3CpuInstr2_cmpxchg16b_rdi_ud2) },
        {   false,  5,  BS3_CMN_NM(bs3CpuInstr2_o16_cmpxchg16b_rdi_ud2) },
        {   false,  5,  BS3_CMN_NM(bs3CpuInstr2_repz_cmpxchg16b_rdi_ud2) },
        {   false,  5,  BS3_CMN_NM(bs3CpuInstr2_repnz_cmpxchg16b_rdi_ud2) },
        {   true, 1+4,  BS3_CMN_NM(bs3CpuInstr2_lock_cmpxchg16b_rdi_ud2) },
        {   true, 1+5,  BS3_CMN_NM(bs3CpuInstr2_lock_o16_cmpxchg16b_rdi_ud2) },
        {   true, 1+5,  BS3_CMN_NM(bs3CpuInstr2_lock_repz_cmpxchg16b_rdi_ud2) },
        {   true, 1+5,  BS3_CMN_NM(bs3CpuInstr2_lock_repnz_cmpxchg16b_rdi_ud2) },
    };

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&ExpectCtx, 0, sizeof(ExpectCtx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));
    Bs3MemSet(pau128, 0, sizeof(pau128[0]) * 2);

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);
    if (!fSupportCX16)
        Bs3TestPrintf("Note! CMPXCHG16B is not supported by the CPU!\n");

    /*
     * One loop with the normal variant and one with the locked one
     */
    g_usBs3TestStep = 0;
    for (iWorker = 0; iWorker < RT_ELEMENTS(s_aWorkers); iWorker++)
    {
        Bs3RegCtxSetRipCsFromCurPtr(&Ctx, s_aWorkers[iWorker].pfnWorker);

        /*
         * One loop with all status flags set, and one with them clear.
         */
        Ctx.rflags.u16 |= X86_EFL_STATUS_BITS;
        for (iFlags = 0; iFlags < 2; iFlags++)
        {
            Bs3MemCpy(&ExpectCtx, &Ctx, sizeof(ExpectCtx));

            for (offBuf = 0; offBuf < sizeof(RTUINT128U); offBuf++)
            {
#  define CX16_OLD_LO       UINT64_C(0xabb6345dcc9c4bbd)
#  define CX16_OLD_HI       UINT64_C(0x7b06ea35749549ab)
#  define CX16_MISMATCH_LO  UINT64_C(0xbace3e3590f18981)
#  define CX16_MISMATCH_HI  UINT64_C(0x9b385e8bfd5b4000)
#  define CX16_STORE_LO     UINT64_C(0x5cbd27d251f6559b)
#  define CX16_STORE_HI     UINT64_C(0x17ff434ed1b54963)

                PRTUINT128U pBuf = (PRTUINT128U)&pau128->au8[offBuf];

                ExpectCtx.rax.u = Ctx.rax.u = CX16_MISMATCH_LO;
                ExpectCtx.rdx.u = Ctx.rdx.u = CX16_MISMATCH_HI;
                for (iMatch = 0; iMatch < 2; iMatch++)
                {
                    uint8_t bExpectXcpt;
                    pBuf->s.Lo = CX16_OLD_LO;
                    pBuf->s.Hi = CX16_OLD_HI;
                    ExpectCtx.rdi.u = Ctx.rdi.u = (uintptr_t)pBuf;
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                    g_usBs3TestStep++;
                    //Bs3TestPrintf("Test: iFlags=%d offBuf=%d iMatch=%u iWorker=%u\n", iFlags, offBuf, iMatch, iWorker);
                    bExpectXcpt = X86_XCPT_UD;
                    if (fSupportCX16)
                    {
                        if (offBuf & 15)
                        {
                            bExpectXcpt = X86_XCPT_GP;
                            ExpectCtx.rip.u = Ctx.rip.u;
                            ExpectCtx.rflags.u32 = Ctx.rflags.u32;
                        }
                        else
                        {
                            ExpectCtx.rax.u = CX16_OLD_LO;
                            ExpectCtx.rdx.u = CX16_OLD_HI;
                            if (iMatch & 1)
                                ExpectCtx.rflags.u32 = Ctx.rflags.u32 | X86_EFL_ZF;
                            else
                                ExpectCtx.rflags.u32 = Ctx.rflags.u32 & ~X86_EFL_ZF;
                            ExpectCtx.rip.u = Ctx.rip.u + s_aWorkers[iWorker].offUd2;
                        }
                        ExpectCtx.rflags.u32 |= X86_EFL_RF;
                    }
                    if (   !Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &ExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/,
                                                 0 /*fExtraEfl*/, "lm64", 0 /*idTestStep*/)
                        || TrapFrame.bXcpt != bExpectXcpt)
                    {
                        if (TrapFrame.bXcpt != bExpectXcpt)
                            Bs3TestFailedF("Expected bXcpt=#%x, got %#x (%#x)", bExpectXcpt, TrapFrame.bXcpt, TrapFrame.uErrCd);
                        Bs3TestFailedF("^^^ iWorker=%d iFlags=%d offBuf=%d iMatch=%u\n", iWorker, iFlags, offBuf, iMatch);
                        ASMHalt();
                    }

                    ExpectCtx.rax.u = Ctx.rax.u = CX16_OLD_LO;
                    ExpectCtx.rdx.u = Ctx.rdx.u = CX16_OLD_HI;
                }
            }
            Ctx.rflags.u16 &= ~X86_EFL_STATUS_BITS;
        }
    }

    return 0;
}


static void bs3CpuInstr2_fsgsbase_ExpectUD(uint8_t bMode, PBS3REGCTX pCtx, PBS3REGCTX pExpectCtx, PBS3TRAPFRAME pTrapFrame)
{
    pCtx->rbx.u  = 0;
    Bs3MemCpy(pExpectCtx, pCtx, sizeof(*pExpectCtx));
    Bs3TrapSetJmpAndRestore(pCtx, pTrapFrame);
    pExpectCtx->rip.u       = pCtx->rip.u;
    pExpectCtx->rflags.u32 |= X86_EFL_RF;
    if (   !Bs3TestCheckRegCtxEx(&pTrapFrame->Ctx, pExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, "lm64",
                                 0 /*idTestStep*/)
        || pTrapFrame->bXcpt != X86_XCPT_UD)
    {
        Bs3TestFailedF("Expected #UD, got %#x (%#x)", pTrapFrame->bXcpt, pTrapFrame->uErrCd);
        ASMHalt();
    }
}


static bool bs3CpuInstr2_fsgsbase_VerifyWorker(uint8_t bMode, PBS3REGCTX pCtx, PBS3REGCTX pExpectCtx, PBS3TRAPFRAME pTrapFrame,
                                               BS3CI2FSGSBASE const *pFsGsBaseWorker, unsigned *puIter)
{
    bool     fPassed = true;
    unsigned iValue  = 0;
    static const struct
    {
        bool      fGP;
        uint64_t  u64Base;
    } s_aValues64[] =
    {
        { false, UINT64_C(0x0000000000000000) },
        { false, UINT64_C(0x0000000000000001) },
        { false, UINT64_C(0x0000000000000010) },
        { false, UINT64_C(0x0000000000000123) },
        { false, UINT64_C(0x0000000000001234) },
        { false, UINT64_C(0x0000000000012345) },
        { false, UINT64_C(0x0000000000123456) },
        { false, UINT64_C(0x0000000001234567) },
        { false, UINT64_C(0x0000000012345678) },
        { false, UINT64_C(0x0000000123456789) },
        { false, UINT64_C(0x000000123456789a) },
        { false, UINT64_C(0x00000123456789ab) },
        { false, UINT64_C(0x0000123456789abc) },
        { false, UINT64_C(0x00007ffffeefefef) },
        { false, UINT64_C(0x00007fffffffffff) },
        {  true, UINT64_C(0x0000800000000000) },
        {  true, UINT64_C(0x0000800000000000) },
        {  true, UINT64_C(0x0000800000000333) },
        {  true, UINT64_C(0x0001000000000000) },
        {  true, UINT64_C(0x0012000000000000) },
        {  true, UINT64_C(0x0123000000000000) },
        {  true, UINT64_C(0x1234000000000000) },
        {  true, UINT64_C(0xffff300000000000) },
        {  true, UINT64_C(0xffff7fffffffffff) },
        {  true, UINT64_C(0xffff7fffffffffff) },
        { false, UINT64_C(0xffff800000000000) },
        { false, UINT64_C(0xffffffffffeefefe) },
        { false, UINT64_C(0xffffffffffffffff) },
        { false, UINT64_C(0xffffffffffffffff) },
        { false, UINT64_C(0x00000000efefefef) },
        { false, UINT64_C(0x0000000080204060) },
        { false, UINT64_C(0x00000000ddeeffaa) },
        { false, UINT64_C(0x00000000fdecdbca) },
        { false, UINT64_C(0x000000006098456b) },
        { false, UINT64_C(0x0000000098506099) },
        { false, UINT64_C(0x00000000206950bc) },
        { false, UINT64_C(0x000000009740395d) },
        { false, UINT64_C(0x0000000064a9455e) },
        { false, UINT64_C(0x00000000d20b6eff) },
        { false, UINT64_C(0x0000000085296d46) },
        { false, UINT64_C(0x0000000007000039) },
        { false, UINT64_C(0x000000000007fe00) },
    };

    Bs3RegCtxSetRipCsFromCurPtr(pCtx, pFsGsBaseWorker->pfnVerifyWorker);
    if (pFsGsBaseWorker->f64BitOperand)
    {
        for (iValue = 0; iValue < RT_ELEMENTS(s_aValues64); iValue++)
        {
            bool const fGP = s_aValues64[iValue].fGP;

            pCtx->rbx.u  = s_aValues64[iValue].u64Base;
            pCtx->rcx.u  = 0;
            pCtx->cr4.u |= X86_CR4_FSGSBASE;
            Bs3MemCpy(pExpectCtx, pCtx, sizeof(*pExpectCtx));
            Bs3TrapSetJmpAndRestore(pCtx, pTrapFrame);
            pExpectCtx->rip.u       = pCtx->rip.u + (!fGP ? pFsGsBaseWorker->offVerifyWorkerUd2 : 0);
            pExpectCtx->rbx.u       = !fGP ? 0 : s_aValues64[iValue].u64Base;
            pExpectCtx->rcx.u       = !fGP ? s_aValues64[iValue].u64Base : 0;
            pExpectCtx->rflags.u32 |= X86_EFL_RF;
            if (  !Bs3TestCheckRegCtxEx(&pTrapFrame->Ctx, pExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/,
                                        0 /*fExtraEfl*/,    "lm64", 0 /*idTestStep*/)
                || (fGP && pTrapFrame->bXcpt != X86_XCPT_GP))
            {
                if (fGP && pTrapFrame->bXcpt != X86_XCPT_GP)
                    Bs3TestFailedF("Expected #GP, got %#x (%#x)", pTrapFrame->bXcpt, pTrapFrame->uErrCd);
                else
                    Bs3TestFailedF("iValue=%u\n", iValue);
                fPassed = false;
                break;
            }
        }
    }
    else
    {
        for (iValue = 0; iValue < RT_ELEMENTS(s_aValues64); iValue++)
        {
            pCtx->rbx.u  =  s_aValues64[iValue].u64Base;
            pCtx->rcx.u  = ~s_aValues64[iValue].u64Base;
            pCtx->cr4.u |= X86_CR4_FSGSBASE;
            Bs3MemCpy(pExpectCtx, pCtx, sizeof(*pExpectCtx));
            Bs3TrapSetJmpAndRestore(pCtx, pTrapFrame);
            pExpectCtx->rip.u       = pCtx->rip.u + pFsGsBaseWorker->offVerifyWorkerUd2;
            pExpectCtx->rbx.u       = 0;
            pExpectCtx->rcx.u       = s_aValues64[iValue].u64Base & UINT64_C(0x00000000ffffffff);
            pExpectCtx->rflags.u32 |= X86_EFL_RF;
            if (!Bs3TestCheckRegCtxEx(&pTrapFrame->Ctx, pExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/,
                                      0 /*fExtraEfl*/, "lm64", 0 /*idTestStep*/))
            {
                Bs3TestFailedF("iValue=%u\n", iValue);
                fPassed = false;
                break;
            }
        }
    }

    *puIter = iValue;
    return fPassed;
}


static void bs3CpuInstr2_rdfsbase_rdgsbase_Common(uint8_t bMode, BS3CI2FSGSBASE const *paFsGsBaseWorkers,
                                                  unsigned cFsGsBaseWorkers, uint32_t idxFsGsBaseMsr)
{
    BS3REGCTX         Ctx;
    BS3REGCTX         ExpectCtx;
    BS3TRAPFRAME      TrapFrame;
    unsigned          iWorker;
    unsigned          iIter;
    uint32_t          uDummy;
    uint32_t          uStdExtFeatEbx;
    bool              fSupportsFsGsBase;

    ASMCpuId_Idx_ECX(7, 0, &uDummy, &uStdExtFeatEbx, &uDummy, &uDummy);
    fSupportsFsGsBase = RT_BOOL(uStdExtFeatEbx & X86_CPUID_STEXT_FEATURE_EBX_FSGSBASE);

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&ExpectCtx, 0, sizeof(ExpectCtx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);

    for (iWorker = 0; iWorker < cFsGsBaseWorkers; iWorker++)
    {
        Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paFsGsBaseWorkers[iWorker].pfnWorker);
        if (fSupportsFsGsBase)
        {
            uint64_t const uBaseAddr = ASMRdMsr(idxFsGsBaseMsr);

            /* CR4.FSGSBASE disabled -> #UD. */
            Ctx.cr4.u &= ~X86_CR4_FSGSBASE;
            bs3CpuInstr2_fsgsbase_ExpectUD(bMode, &Ctx, &ExpectCtx, &TrapFrame);

            /* Read and verify existing base address. */
            Ctx.rbx.u  = 0;
            Ctx.cr4.u |= X86_CR4_FSGSBASE;
            Bs3MemCpy(&ExpectCtx, &Ctx, sizeof(ExpectCtx));
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
            ExpectCtx.rip.u       = Ctx.rip.u + paFsGsBaseWorkers[iWorker].offWorkerUd2;
            ExpectCtx.rbx.u       = uBaseAddr;
            ExpectCtx.rflags.u32 |= X86_EFL_RF;
            if (!Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &ExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, "lm64",
                                      0 /*idTestStep*/))
            {
                ASMHalt();
            }

            /* Write, read and verify series of base addresses. */
            if (!bs3CpuInstr2_fsgsbase_VerifyWorker(bMode, &Ctx, &ExpectCtx, &TrapFrame, &paFsGsBaseWorkers[iWorker], &iIter))
            {
                Bs3TestFailedF("^^^ %s: iWorker=%u iIter=%u\n", paFsGsBaseWorkers[iWorker].pszDesc, iWorker, iIter);
                ASMHalt();
            }

            /* Restore original base address. */
            ASMWrMsr(idxFsGsBaseMsr, uBaseAddr);

            /* Clean used GPRs. */
            Ctx.rbx.u = 0;
            Ctx.rcx.u = 0;
        }
        else
        {
            /* Unsupported by CPUID -> #UD. */
            Bs3TestPrintf("Note! FSGSBASE is not supported by the CPU!\n");
            bs3CpuInstr2_fsgsbase_ExpectUD(bMode, &Ctx, &ExpectCtx, &TrapFrame);
        }
    }
}


static void bs3CpuInstr2_wrfsbase_wrgsbase_Common(uint8_t bMode, BS3CI2FSGSBASE const *paFsGsBaseWorkers,
                                                  unsigned cFsGsBaseWorkers, uint32_t idxFsGsBaseMsr)
{
    BS3REGCTX         Ctx;
    BS3REGCTX         ExpectCtx;
    BS3TRAPFRAME      TrapFrame;
    unsigned          iWorker;
    unsigned          iIter;
    uint32_t          uDummy;
    uint32_t          uStdExtFeatEbx;
    bool              fSupportsFsGsBase;

    ASMCpuId_Idx_ECX(7, 0, &uDummy, &uStdExtFeatEbx, &uDummy, &uDummy);
    fSupportsFsGsBase = RT_BOOL(uStdExtFeatEbx & X86_CPUID_STEXT_FEATURE_EBX_FSGSBASE);

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&ExpectCtx, 0, sizeof(ExpectCtx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 512);

    for (iWorker = 0; iWorker < cFsGsBaseWorkers; iWorker++)
    {
        Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paFsGsBaseWorkers[iWorker].pfnWorker);
        if (fSupportsFsGsBase)
        {
            uint64_t const uBaseAddr = ASMRdMsr(idxFsGsBaseMsr);

            /* CR4.FSGSBASE disabled -> #UD. */
            Ctx.cr4.u &= ~X86_CR4_FSGSBASE;
            bs3CpuInstr2_fsgsbase_ExpectUD(bMode, &Ctx, &ExpectCtx, &TrapFrame);

            /* Write a base address. */
            Ctx.rbx.u  = 0xa0000;
            Ctx.cr4.u |= X86_CR4_FSGSBASE;
            Bs3MemCpy(&ExpectCtx, &Ctx, sizeof(ExpectCtx));
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
            ExpectCtx.rip.u       = Ctx.rip.u + paFsGsBaseWorkers[iWorker].offWorkerUd2;
            ExpectCtx.rflags.u32 |= X86_EFL_RF;
            if (!Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &ExpectCtx, 0 /*cbPcAdjust*/, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, "lm64",
                                      0 /*idTestStep*/))
            {
                ASMHalt();
            }

            /* Write and read back series of base addresses. */
            if (!bs3CpuInstr2_fsgsbase_VerifyWorker(bMode, &Ctx, &ExpectCtx, &TrapFrame, &paFsGsBaseWorkers[iWorker], &iIter))
            {
                Bs3TestFailedF("^^^ %s: iWorker=%u iIter=%u\n", paFsGsBaseWorkers[iWorker].pszDesc, iWorker, iIter);
                ASMHalt();
            }

            /* Restore original base address. */
            ASMWrMsr(idxFsGsBaseMsr, uBaseAddr);

            /* Clean used GPRs. */
            Ctx.rbx.u = 0;
            Ctx.rcx.u = 0;
        }
        else
        {
            /* Unsupported by CPUID -> #UD. */
            Bs3TestPrintf("Note! FSGSBASE is not supported by the CPU!\n");
            bs3CpuInstr2_fsgsbase_ExpectUD(bMode, &Ctx, &ExpectCtx, &TrapFrame);
        }
    }
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_wrfsbase)(uint8_t bMode)
{
    bs3CpuInstr2_wrfsbase_wrgsbase_Common(bMode, s_aWrFsBaseWorkers, RT_ELEMENTS(s_aWrFsBaseWorkers), MSR_K8_FS_BASE);
    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_wrgsbase)(uint8_t bMode)
{
    bs3CpuInstr2_wrfsbase_wrgsbase_Common(bMode, s_aWrGsBaseWorkers, RT_ELEMENTS(s_aWrGsBaseWorkers), MSR_K8_GS_BASE);
    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_rdfsbase)(uint8_t bMode)
{
    bs3CpuInstr2_rdfsbase_rdgsbase_Common(bMode, s_aRdFsBaseWorkers, RT_ELEMENTS(s_aRdFsBaseWorkers), MSR_K8_FS_BASE);
    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_rdgsbase)(uint8_t bMode)
{
    bs3CpuInstr2_rdfsbase_rdgsbase_Common(bMode, s_aRdGsBaseWorkers, RT_ELEMENTS(s_aRdGsBaseWorkers), MSR_K8_GS_BASE);
    return 0;
}

# endif /* ARCH_BITS == 64 */

#endif /* BS3_INSTANTIATING_CMN */



/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE


#endif /* BS3_INSTANTIATING_MODE */

