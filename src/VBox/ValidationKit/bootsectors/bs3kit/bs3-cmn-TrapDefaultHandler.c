/* $Id$ */
/** @file
 * BS3Kit - Bs3TrapDefaultHandler
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
#include "bs3kit-template-header.h"


BS3_DECL(void) Bs3TrapDefaultHandler(PBS3TRAPFRAME pTrapFrame)
{
#if TMPL_BITS != 64
    /*
     * Deal with GPs in V8086 mode.
     */
Bs3Printf("bXcpt=%#x\n", pTrapFrame->bXcpt);
Bs3Printf("bXcpt=%#x\n", pTrapFrame->bXcpt);
Bs3Printf("eflags=%#RX32 (%d)\n", pTrapFrame->Ctx.rflags.u32, RT_BOOL(pTrapFrame->Ctx.rflags.u32 & X86_EFL_VM));
Bs3Printf("cs=%#x\n", pTrapFrame->Ctx.cs);
for (;;) { }
    if (    pTrapFrame->bXcpt == X86_XCPT_GP
        && (pTrapFrame->Ctx.rflags.u32 & X86_EFL_VM)
        && pTrapFrame->Ctx.cs == BS3_SEL_TEXT16)
    {
        bool                    fHandled    = true;
        uint8_t                 cBitsOpcode = 16;
        uint8_t                 bOpCode;
        uint8_t const BS3_FAR  *pbCodeStart;
        uint8_t const BS3_FAR  *pbCode;
        uint16_t BS3_FAR       *pusStack;
for (;;) { }

        pusStack    = (uint16_t      BS3_FAR *)BS3_MAKE_PROT_R0PTR_FROM_REAL(pTrapFrame->Ctx.ss, pTrapFrame->Ctx.rsp.u16);
        pbCode      = (uint8_t const BS3_FAR *)BS3_MAKE_PROT_R0PTR_FROM_REAL(pTrapFrame->Ctx.cs, pTrapFrame->Ctx.rip.u16);
        pbCodeStart = pbCode;

        bOpCode = *++pbCode;
        if (bOpCode == 0x66)
        {
            cBitsOpcode = 32;
            bOpCode = *++pbCode;
        }

        /* INT xx: Real mode behaviour, but intercepting and implementing most of our syscall interface. */
        if (bOpCode == 0xcd)
        {
            uint8_t bVector = *++pbCode;
            if (bVector == BS3_TRAP_SYSCALL)
            {
                /* Minimal syscall. */
                if (pTrapFrame->Ctx.rax.u16 == BS3_SYSCALL_PRINT_CHR)
                    Bs3PrintChr(pTrapFrame->Ctx.rax.u8);
                else if (   pTrapFrame->Ctx.rax.u16 == BS3_SYSCALL_TO_RING0
                         || pTrapFrame->Ctx.rax.u16 == BS3_SYSCALL_TO_RING1
                         || pTrapFrame->Ctx.rax.u16 == BS3_SYSCALL_TO_RING2
                         || pTrapFrame->Ctx.rax.u16 == BS3_SYSCALL_TO_RING3)
                {
                    Bs3RegCtxConvertToRingX(&pTrapFrame->Ctx, pTrapFrame->Ctx.rax.u16 - BS3_SYSCALL_TO_RING0);
for (;;) { }
                }
                else
                    Bs3Panic();
            }
            else
            {
                /* Real mode behaviour. */
                uint16_t BS3_FAR *pusIvte = (uint16_t BS3_FAR *)BS3_MAKE_PROT_R0PTR_FROM_REAL(0, 0);
                pusIvte += (uint16_t)bVector *2;

                pusStack[0] = pTrapFrame->Ctx.rflags.u16;
                pusStack[1] = pTrapFrame->Ctx.cs;
                pusStack[2] = pTrapFrame->Ctx.rip.u16 + (uint16_t)(pbCode - pbCodeStart);

                pTrapFrame->Ctx.rip.u16 = pusIvte[0];
                pTrapFrame->Ctx.cs      = pusIvte[1];
                pTrapFrame->Ctx.rflags.u16 &= ~X86_EFL_IF; /** @todo this isn't all, but it'll do for now, I hope. */
                Bs3RegCtxRestore(&pTrapFrame->Ctx, 0/*fFlags*/); /* does not return. */
            }
        }
        /* PUSHF: Real mode behaviour. */
        else if (bOpCode == 0x9c)
        {
            if (cBitsOpcode == 32)
                *pusStack++ = pTrapFrame->Ctx.rflags.au16[1] & ~(X86_EFL_VM | X86_EFL_RF);
            *pusStack++ = pTrapFrame->Ctx.rflags.u16;
            pTrapFrame->Ctx.rsp.u16 += cBitsOpcode / 8;
        }
        /* POPF:  Real mode behaviour. */
        else if (bOpCode == 0x9d)
        {
            if (cBitsOpcode == 32)
            {
                pTrapFrame->Ctx.rflags.u32 &= ~X86_EFL_POPF_BITS;
                pTrapFrame->Ctx.rflags.u32 |= X86_EFL_POPF_BITS & *(uint32_t const *)pusStack;
            }
            else
            {
                pTrapFrame->Ctx.rflags.u32 &= ~(X86_EFL_POPF_BITS | UINT32_C(0xffff0000)) & ~X86_EFL_RF;
                pTrapFrame->Ctx.rflags.u16 |= (uint16_t)X86_EFL_POPF_BITS & *pusStack;
            }
            pTrapFrame->Ctx.rsp.u16 -= cBitsOpcode / 8;
        }
        /* CLI: Real mode behaviour. */
        else if (bOpCode == 0xfa)
            pTrapFrame->Ctx.rflags.u16 &= ~X86_EFL_IF;
        /* STI: Real mode behaviour. */
        else if (bOpCode == 0xfb)
            pTrapFrame->Ctx.rflags.u16 |= X86_EFL_IF;
        /* Unexpected. */
        else
            fHandled = false;
for (;;) { }
        if (fHandled)
        {
            pTrapFrame->Ctx.rip.u16 += (uint16_t)(pbCode - pbCodeStart);
            Bs3RegCtxRestore(&pTrapFrame->Ctx, 0 /*fFlags*/); /* does not return. */
            return;
        }
    }

#endif

    Bs3TrapPrintFrame(pTrapFrame);
    Bs3Panic();
}

