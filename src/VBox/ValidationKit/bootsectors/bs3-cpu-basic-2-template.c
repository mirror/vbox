/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-basic-2, C code template.
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


#ifdef BS3_INSTANTIATING_MODE

extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx)(void);

# if TMPL_MODE == BS3_MODE_PE16 \
  || TMPL_MODE == BS3_MODE_PE16_32

static void bs3CpuBasic2_CompareTrapCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                         unsigned uStep)
{
    const char *pszMode = BS3_DATA_NM(TMPL_NM(g_szBs3ModeName));
    uint16_t    cErrorsBefore = Bs3TestSubErrorCount();

#define CHECK_MEMBER(a_szName, a_szFmt, a_Actual, a_Expected) \
    do { \
        if ((a_Actual) == (a_Expected)) { /* likely */ } \
        else Bs3TestFailedF("%u - %s: " a_szName "=" a_szFmt " expected " a_szFmt, uStep, pszMode, (a_Actual), (a_Expected)); \
    } while (0)

    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("rax",     "%08RX64",  pTrapCtx->Ctx.rax.u,    pStartCtx->rax.u);
    CHECK_MEMBER("rcx",     "%08RX64",  pTrapCtx->Ctx.rcx.u,    pStartCtx->rcx.u);
    CHECK_MEMBER("rdx",     "%08RX64",  pTrapCtx->Ctx.rdx.u,    pStartCtx->rdx.u);
    CHECK_MEMBER("rbx",     "%08RX64",  pTrapCtx->Ctx.rbx.u,    pStartCtx->rbx.u);
    CHECK_MEMBER("rsp",     "%08RX64",  pTrapCtx->Ctx.rsp.u,    pStartCtx->rsp.u);
    CHECK_MEMBER("rbp",     "%08RX64",  pTrapCtx->Ctx.rbp.u,    pStartCtx->rbp.u);
    CHECK_MEMBER("rsi",     "%08RX64",  pTrapCtx->Ctx.rsi.u,    pStartCtx->rsi.u);
    CHECK_MEMBER("rdi",     "%08RX64",  pTrapCtx->Ctx.rdi.u,    pStartCtx->rdi.u);
    CHECK_MEMBER("r8",      "%08RX64",  pTrapCtx->Ctx.r8.u,     pStartCtx->r8.u);
    CHECK_MEMBER("r9",      "%08RX64",  pTrapCtx->Ctx.r9.u,     pStartCtx->r9.u);
    CHECK_MEMBER("r10",     "%08RX64",  pTrapCtx->Ctx.r10.u,    pStartCtx->r10.u);
    CHECK_MEMBER("r11",     "%08RX64",  pTrapCtx->Ctx.r11.u,    pStartCtx->r11.u);
    CHECK_MEMBER("r12",     "%08RX64",  pTrapCtx->Ctx.r12.u,    pStartCtx->r12.u);
    CHECK_MEMBER("r13",     "%08RX64",  pTrapCtx->Ctx.r13.u,    pStartCtx->r13.u);
    CHECK_MEMBER("r14",     "%08RX64",  pTrapCtx->Ctx.r14.u,    pStartCtx->r14.u);
    CHECK_MEMBER("r15",     "%08RX64",  pTrapCtx->Ctx.r15.u,    pStartCtx->r15.u);
    CHECK_MEMBER("rflags",  "%08RX64",  pTrapCtx->Ctx.rflags.u, pStartCtx->rflags.u);
    CHECK_MEMBER("rip",     "%08RX64",  pTrapCtx->Ctx.rip.u,    pStartCtx->rip.u + cbIpAdjust);
    CHECK_MEMBER("cs",      "%04RX16",  pTrapCtx->Ctx.cs,       pStartCtx->cs);
    CHECK_MEMBER("ds",      "%04RX16",  pTrapCtx->Ctx.ds,       pStartCtx->ds);
    CHECK_MEMBER("es",      "%04RX16",  pTrapCtx->Ctx.es,       pStartCtx->es);
    CHECK_MEMBER("fs",      "%04RX16",  pTrapCtx->Ctx.fs,       pStartCtx->fs);
    CHECK_MEMBER("gs",      "%04RX16",  pTrapCtx->Ctx.gs,       pStartCtx->gs);
    CHECK_MEMBER("tr",      "%04RX16",  pTrapCtx->Ctx.tr,       pStartCtx->tr);
    CHECK_MEMBER("ldtr",    "%04RX16",  pTrapCtx->Ctx.ldtr,     pStartCtx->ldtr);
    CHECK_MEMBER("bMode",   "%#04x",    pTrapCtx->Ctx.bMode,    pStartCtx->bMode);
    CHECK_MEMBER("bCpl",    "%u",       pTrapCtx->Ctx.bCpl,     pStartCtx->bCpl);
    CHECK_MEMBER("cr0",     "%08RX32",  pTrapCtx->Ctx.cr0.u,    pStartCtx->cr0.u);
    CHECK_MEMBER("cr2",     "%08RX32",  pTrapCtx->Ctx.cr2.u,    pStartCtx->cr2.u);
    CHECK_MEMBER("cr3",     "%08RX32",  pTrapCtx->Ctx.cr3.u,    pStartCtx->cr3.u);
    CHECK_MEMBER("cr4",     "%08RX32",  pTrapCtx->Ctx.cr4.u,    pStartCtx->cr4.u);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
        Bs3TrapPrintFrame(pTrapCtx);
}
#endif

AssertCompileMemberOffset(BS3REGCTX, ss, 0x9a);

BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t         bRet = 0;
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx;

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

# if TMPL_MODE == BS3_MODE_PE16 \
  || TMPL_MODE == BS3_MODE_PE16_32

    Bs3RegCtxSave(&Ctx);
    Ctx.rsp.u -= 0x80;
    Ctx.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx));
# if TMPL_BITS == 32
    BS3_DATA_NM(g_uBs3TrapEipHint) = Ctx.rip.u32;
# endif
    Bs3Printf("esp=%#llx\n", Ctx.rsp.u);

    /*
     * Check that the stuff works first.
     */
    if (Bs3TrapSetJmp(&TrapCtx))
    {
        Bs3RegCtxRestore(&Ctx, 0); /* (does not return) */
    }
    /* trapped. */
    bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx, 2 /*int 80h*/, 0x80 /*bXcpt*/, 1/*bStep*/);
    Bs3Printf("esp=%#llx\n", Ctx.rsp.u);

Bs3Printf("trapped\n");


# else
    bRet = BS3TESTDOMODE_SKIPPED;
# endif

    /*
     * Re-initialize the IDT.
     */
#  if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
    Bs3Trap16Init();
#  elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap32Init();
#  elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap64Init();
#  endif

    return bRet;
}


#endif /* BS3_INSTANTIATING_MODE */
