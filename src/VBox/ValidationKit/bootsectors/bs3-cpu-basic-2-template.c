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

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx)(void);


# if TMPL_MODE == BS3_MODE_PE16 \
  || TMPL_MODE == BS3_MODE_PE16_32

/**
 * Compares trap stuff.
 */
static void bs3CpuBasic2_CompareTrapCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                         const char *pszMode, unsigned uLine)
{
    uint16_t    cErrorsBefore = Bs3TestSubErrorCount();

#define CHECK_MEMBER(a_szName, a_szFmt, a_Actual, a_Expected) \
    do \
    { \
        if ((a_Actual) == (a_Expected)) { /* likely */ } \
        else Bs3TestFailedF("%u - %s: " a_szName "=" a_szFmt " expected " a_szFmt, uLine, pszMode, (a_Actual), (a_Expected)); \
    } while (0)

    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
        Bs3TrapPrintFrame(pTrapCtx);
}


/**
 * Worker for bs3CpuBasic2_TssGateEsp that tests the INT 80 from outer rings.
 */
static void bs3CpuBasic2_TssGateEsp_AltStackOuterRing(PCBS3REGCTX pCtx, uint8_t bRing, uint8_t *pbAltStack, size_t cbAltStack,
                                                      bool f16BitStack, bool f16BitTss, bool f16BitHandler,
                                                      const char *pszMode, unsigned uLine)
{
    uint8_t const   cbIretFrame = f16BitHandler ? 5*2 : 5*4;
    BS3REGCTX       Ctx2;
    BS3TRAPFRAME    TrapCtx;
    uint8_t        *pbTmp;

    Bs3MemCpy(&Ctx2, pCtx, sizeof(Ctx2));
    Bs3RegCtxConvertToRingX(&Ctx2, bRing);

    if (pbAltStack)
    {
        Ctx2.rsp.u = Bs3SelPtrToFlat(pbAltStack + 0x1980);
        Bs3MemZero(pbAltStack, cbAltStack);
    }

    Bs3TrapSetJmpAndRestore(&Ctx2, &TrapCtx);

    if (!f16BitStack && f16BitTss)
        Ctx2.rsp.u &= UINT16_MAX;

    bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx2, 2 /*int 80h*/, 0x80 /*bXcpt*/, pszMode, uLine);
    CHECK_MEMBER("bCpl", "%u", TrapCtx.Ctx.bCpl, bRing);
    CHECK_MEMBER("cbIretFrame", "%#x", TrapCtx.cbIretFrame, cbIretFrame);

    if (pbAltStack)
    {
        uint64_t uExpectedRsp = (f16BitTss ? Bs3Tss16.sp0 : Bs3Tss32.esp0) - cbIretFrame;
        if (f16BitStack)
        {
            uExpectedRsp &= UINT16_MAX;
            uExpectedRsp |= Ctx2.rsp.u & ~(uint64_t)UINT16_MAX;
        }
        if (   TrapCtx.uHandlerRsp != uExpectedRsp
            || TrapCtx.uHandlerSs  != (f16BitTss ? Bs3Tss16.ss0 : Bs3Tss32.ss0))
            Bs3TestFailedF("%u - %s: handler SS:ESP=%04x:%08RX64, expected %04x:%08RX16\n", uLine, pszMode,
                           TrapCtx.uHandlerSs, TrapCtx.uHandlerRsp, Bs3Tss16.ss0, uExpectedRsp);

        pbTmp = (uint8_t *)ASMMemFirstNonZero(pbAltStack, cbAltStack);
        if ((f16BitStack || TrapCtx.uHandlerRsp <= UINT16_MAX) && pbTmp != NULL)
            Bs3TestFailedF("%u - %s: someone touched the alt stack (%p) with SS:ESP=%04x:%#RX32: %p=%02x\n", uLine, pszMode,
                           pbAltStack, Ctx2.ss, Ctx2.rsp.u32, pbTmp, *pbTmp);
        else if (!f16BitStack && TrapCtx.uHandlerRsp > UINT16_MAX && pbTmp == NULL)
            Bs3TestFailedF("%u - %s: the alt stack (%p) was not used SS:ESP=%04x:%#RX32\n", uLine, pszMode,
                           pbAltStack, Ctx2.ss, Ctx2.rsp.u32);
    }
}

#endif


BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t         bRet = 0;
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx, Ctx2;
    uint8_t        *pbTmp;
    unsigned        uLine;
    const char     *pszMode = BS3_DATA_NM(TMPL_NM(g_szBs3ModeName));
    bool const      f16BitSys = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

    pbTmp = NULL; NOREF(pbTmp); uLine = 0; NOREF(uLine); NOREF(pszMode); NOREF(f16BitSys);

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&Ctx2, sizeof(Ctx2));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

#if TMPL_MODE == BS3_MODE_PE16 \
 || TMPL_MODE == BS3_MODE_PE16_32 \
 || TMPL_MODE == BS3_MODE_PP16 \
 || TMPL_MODE == BS3_MODE_PP16_32 \
 || TMPL_MODE == BS3_MODE_PAE16 \
 || TMPL_MODE == BS3_MODE_PAE16_32 \
 || TMPL_MODE == BS3_MODE_PE32

    Bs3RegCtxSave(&Ctx);
    Ctx.rsp.u -= 0x80;
    Ctx.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx));
# if TMPL_BITS == 32
    BS3_DATA_NM(g_uBs3TrapEipHint) = Ctx.rip.u32;
# endif

    /*
     * We'll be using IDT entry 80 and 81 here. The first one will be
     * accessible from all DPLs, the latter not. So, start with setting
     * the DPLs.
     */
# if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
    BS3_DATA_NM(Bs3Idt16)[0x80].Gate.u2Dpl = 3;
    BS3_DATA_NM(Bs3Idt16)[0x81].Gate.u2Dpl = 0;
# else
    BS3_DATA_NM(Bs3Idt32)[0x80].Gate.u2Dpl = 3;
    BS3_DATA_NM(Bs3Idt32)[0x81].Gate.u2Dpl = 0;
# endif

    /*
     * Check that the basic stuff works first.
     */
    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
    bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx, 2 /*int 80h*/, 0x80 /*bXcpt*/, pszMode, __LINE__);

    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, NULL, 0, f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);
    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 2, NULL, 0, f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);
    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 3, NULL, 0, f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);

    /*
     * Check that the upper part of ESP is preserved when doing .
     */
    if ((BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        size_t const cbAltStack = _8K;
        uint8_t *pbAltStack = Bs3MemAllocZ(BS3MEMKIND_TILED, cbAltStack);
        if (pbAltStack)
        {
            /* same ring */
            uLine = __LINE__;
            Bs3MemCpy(&Ctx2, &Ctx, sizeof(Ctx2));
            Ctx2.rsp.u = Bs3SelPtrToFlat(pbAltStack + 0x1980);
            if (Bs3TrapSetJmp(&TrapCtx))
                Bs3RegCtxRestore(&Ctx2, 0); /* (does not return) */
            bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &Ctx2, 2 /*int 80h*/, 0x80 /*bXcpt*/, pszMode, uLine);
# if TMPL_BITS == 16
            if ((pbTmp = (uint8_t *)ASMMemFirstNonZero(pbAltStack, cbAltStack)) != NULL)
                Bs3TestFailedF("%u - %s: someone touched the alt stack (%p) with SS:ESP=%04x:%#RX32: %p=%02x\n",
                               uLine, pszMode, pbAltStack, Ctx2.ss, Ctx2.rsp.u32, pbTmp, *pbTmp);
# else
            if (ASMMemIsZero(pbAltStack, cbAltStack))
                Bs3TestFailedF("%u - %s: alt stack wasn't used despite SS:ESP=%04x:%#RX32\n",
                               uLine, pszMode, Ctx2.ss, Ctx2.rsp.u32);
# endif

            /* Different rings (load SS0:SP0 from TSS). */
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                      f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 2, pbAltStack, cbAltStack,
                                                      f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 3, pbAltStack, cbAltStack,
                                                      f16BitSys, f16BitSys, f16BitSys, pszMode, __LINE__);

            /* Different rings but switch the SS bitness in the TSS. */
# if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
            Bs3Tss16.ss0 = BS3_SEL_R0_SS32;
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                      false, f16BitSys, f16BitSys, pszMode, __LINE__);
            Bs3Tss16.ss0 = BS3_SEL_R0_SS16;
# else
            Bs3Tss32.ss0 = BS3_SEL_R0_SS16;
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                      true,  f16BitSys, f16BitSys, pszMode, __LINE__);
            Bs3Tss32.ss0 = BS3_SEL_R0_SS32;
# endif

            Bs3MemFree(pbAltStack, cbAltStack);
        }
        else
            Bs3TestPrintf("%s: Skipping ESP check, alloc failed\n", pszMode);
    }
    else
        Bs3TestPrintf("%s: Skipping ESP check, CPU too old\n", pszMode);

#else
    bRet = BS3TESTDOMODE_SKIPPED;
#endif

    /*
     * Re-initialize the IDT.
     */
#if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
    Bs3Trap16Init();
#elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap32Init();
#elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap64Init();
#endif

    return bRet;
}


BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_iret)(uint8_t bMode)
{
    NOREF(bMode);
    return BS3TESTDOMODE_SKIPPED;
}


#endif /* BS3_INSTANTIATING_MODE */

