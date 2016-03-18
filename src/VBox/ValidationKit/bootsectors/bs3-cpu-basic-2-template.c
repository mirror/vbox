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

#include <iprt/asm.h>


extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx)(void);

# if TMPL_MODE == BS3_MODE_PE16 \
  || TMPL_MODE == BS3_MODE_PE16_32

static void bs3CpuBasic2_CompareTrapCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                         unsigned uStep)
{
    const char *pszMode = BS3_DATA_NM(TMPL_NM(g_szBs3ModeName));
    uint16_t    cErrorsBefore = Bs3TestSubErrorCount();

#define CHECK_MEMBER(a_szName, a_szFmt, a_Actual, a_Expected) \
    do \
    { \
        if ((a_Actual) == (a_Expected)) { /* likely */ } \
        else Bs3TestFailedF("%u - %s: " a_szName "=" a_szFmt " expected " a_szFmt, uStep, pszMode, (a_Actual), (a_Expected)); \
    } while (0)

    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, pszMode, uStep);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
        Bs3TrapPrintFrame(pTrapCtx);
}
#endif


BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t         bRet = 0;
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx, Ctx2;
    uint8_t        *pbTmp;

    pbTmp = NULL; NOREF(pbTmp);

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&Ctx2, sizeof(Ctx2));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

# if TMPL_MODE == BS3_MODE_PE16 \
  || TMPL_MODE == BS3_MODE_PE16_32

    Bs3RegCtxSave(&Ctx);
    Ctx.rsp.u -= 0x80;
    Ctx.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx));
# if TMPL_BITS == 32
    BS3_DATA_NM(g_uBs3TrapEipHint) = Ctx.rip.u32;
# endif

    /*
     * Check that the stuff works first.
     */
    if (Bs3TrapSetJmp(&TrapCtx))
        Bs3RegCtxRestore(&Ctx, 0); /* (does not return) */
    bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx, 2 /*int 80h*/, 0x80 /*bXcpt*/, 1/*bStep*/);

    /*
     * Check that the upper part of ESP is preserved when doing .
     */
    if ((BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        size_t const cbAltStack = _8K;
        uint8_t *pbAltStack = Bs3MemAllocZ(BS3MEMKIND_TILED, cbAltStack);
        if (pbAltStack)
        {
            Bs3MemCpy(&Ctx2, &Ctx, sizeof(Ctx2));
            Ctx2.rsp.u = Bs3SelPtrToFlat(pbAltStack + 0x1980);
            if (Bs3TrapSetJmp(&TrapCtx))
                Bs3RegCtxRestore(&Ctx2, 0); /* (does not return) */
            bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx2, 2 /*int 80h*/, 0x80 /*bXcpt*/, 2/*bStep*/);
#  if TMPL_BITS == 16
            if ((pbTmp = (uint8_t *)ASMMemFirstNonZero(pbAltStack, cbAltStack)) != NULL)
                Bs3TestFailedF("%s: someone touched the alt stack (%p) with CS:ESP=%04x:%#RX32: %p=%02x\n",
                               BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)), pbAltStack, Ctx2.ss, Ctx2.rsp.u32, pbTmp, *pbTmp);
#  else
            if (ASMMemIsZero(pbAltStack, cbAltStack))
                Bs3TestFailedF("%s: alt stack wasn't used despite CS:ESP=%04x:%#RX32\n",
                               BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)), Ctx2.ss, Ctx2.rsp.u32);
#  endif
            Bs3MemFree(pbAltStack, cbAltStack);
        }
        else
            Bs3TestPrintf("%s: Skipping ESP check, alloc failed\n", BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)));
    }
    else
        Bs3TestPrintf("%s: Skipping ESP check, CPU too old\n", BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)));

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
