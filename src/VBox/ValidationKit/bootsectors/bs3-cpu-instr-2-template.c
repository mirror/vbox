/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-2, C code template.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN

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


#endif /* BS3_INSTANTIATING_CMN */



/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE


#endif /* BS3_INSTANTIATING_MODE */

