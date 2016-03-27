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
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
# undef MyBs3Idt
# undef MY_SYS_SEL_R0_CS
# undef MY_SYS_SEL_R0_CS_CNF
# undef MY_SYS_SEL_R0_DS
# undef MY_SYS_SEL_R0_SS
# if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              BS3_DATA_NM(Bs3Idt16)
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS16
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS16_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS16
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_SS16
# elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              BS3_DATA_NM(Bs3Idt32)
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS32
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS32_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS32
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_SS32
# elif BS3_MODE_IS_64BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              BS3_DATA_NM(Bs3Idt64)
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS64
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS64_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS64
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_DS64
# else
#  error "TMPL_MODE"
# endif
#undef  CHECK_MEMBER
#define CHECK_MEMBER(a_szName, a_szFmt, a_Actual, a_Expected) \
    do \
    { \
        if ((a_Actual) == (a_Expected)) { /* likely */ } \
        else Bs3TestFailedF("%u - %s: " a_szName "=" a_szFmt " expected " a_szFmt, uLine, pszMode, (a_Actual), (a_Expected)); \
    } while (0)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_Int80)(void);
extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_Int81)(void);
extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_Int82)(void);
extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_Int83)(void);


#if TMPL_MODE == BS3_MODE_PE16 || TMPL_MODE == BS3_MODE_PE16_32 || TMPL_MODE == BS3_MODE_LM64

/**
 * Compares trap stuff.
 */
static void bs3CpuBasic2_CompareTrapCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                         const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}


/**
 * Compares trap stuff.
 */
static void bs3CpuBasic2_CompareTrapCtx2(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                         uint16_t uHandlerCs, const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    CHECK_MEMBER("uHandlerCs", "%#06x", pTrapCtx->uHandlerCs,   uHandlerCs);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares trap stuff.
 */
static void bs3CpuBasic2_CompareGpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
                                      const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        X86_XCPT_GP);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       (uint64_t)uErrCd);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/,
                         f16BitHandler ? 0 : X86_EFL_RF,
                         pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares trap stuff.
 */
static void bs3CpuBasic2_CompareNpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
                                      const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        X86_XCPT_NP);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       (uint64_t)uErrCd);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/,
                         f16BitHandler ? 0 : X86_EFL_RF,
                         pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

#endif /* once for each bitcount */


#if TMPL_MODE == BS3_MODE_PE16 || TMPL_MODE == BS3_MODE_PE16_32
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
    Ctx.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_Int80));
# if TMPL_BITS == 32
    BS3_DATA_NM(g_uBs3TrapEipHint) = Ctx.rip.u32;
# endif

    /*
     * We'll be using IDT entry 80 and 81 here. The first one will be
     * accessible from all DPLs, the latter not. So, start with setting
     * the DPLs.
     */
    MyBs3Idt[0x80].Gate.u2Dpl = 3;
    MyBs3Idt[0x81].Gate.u2Dpl = 0;

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
    TMPL_NM(Bs3TrapInit)();
    return bRet;
}



BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_RaiseXcpt1)(uint8_t bMode)
{
    uint8_t         bRet = 0;
#if !BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx80;
    BS3REGCTX       Ctx81;
    BS3REGCTX       Ctx82;
    BS3REGCTX       Ctx83;
    BS3REGCTX       CtxTmp;
    PBS3REGCTX      apCtx8x[4];
    unsigned        iCtx;
    unsigned        iRing;
    unsigned        i, j, k;
    unsigned        uLine;
    const char     *pszMode = BS3_DATA_NM(TMPL_NM(g_szBs3ModeName));
    bool const      f16BitSys = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

    //uLine = 0; NOREF(uLine); NOREF(pszMode); NOREF(f16BitSys);

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx80, sizeof(Ctx80));
    Bs3MemZero(&Ctx81, sizeof(Ctx81));
    Bs3MemZero(&Ctx82, sizeof(Ctx82));
    Bs3MemZero(&Ctx83, sizeof(Ctx83));
    Bs3MemZero(&CtxTmp, sizeof(CtxTmp));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

    /* Context array. */
    apCtx8x[0] = &Ctx80;
    apCtx8x[1] = &Ctx81;
    apCtx8x[2] = &Ctx82;
    apCtx8x[3] = &Ctx83;

    /*
     * IDT entry 80 thru 83 are assigned DPLs according to the number.
     * (We'll be useing more, but this'll do for now.)
     */
    MyBs3Idt[0x80].Gate.u2Dpl = 0;
    MyBs3Idt[0x81].Gate.u2Dpl = 1;
    MyBs3Idt[0x82].Gate.u2Dpl = 2;
    MyBs3Idt[0x83].Gate.u2Dpl = 3;

    Bs3RegCtxSave(&Ctx80);
    Ctx80.rsp.u -= 0x80;
    Ctx80.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_Int80));
# if TMPL_BITS == 32
    BS3_DATA_NM(g_uBs3TrapEipHint) = Ctx80.rip.u32;
# endif
    Bs3MemCpy(&Ctx81, &Ctx80, sizeof(Ctx80));
    Ctx81.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_Int81));
    Bs3MemCpy(&Ctx82, &Ctx80, sizeof(Ctx80));
    Ctx82.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_Int82));
    Bs3MemCpy(&Ctx83, &Ctx80, sizeof(Ctx80));
    Ctx83.rip.u  = (uintptr_t)BS3_FP_OFF(&TMPL_NM(bs3CpuBasic2_Int83));

    /*
     * Check that all the above gates work from ring-0.
     */
    for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
    {
# if TMPL_BITS == 32
        BS3_DATA_NM(g_uBs3TrapEipHint) = apCtx8x[iCtx]->rip.u32;
# endif
        Bs3TrapSetJmpAndRestore(apCtx8x[iCtx], &TrapCtx);
        bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, apCtx8x[iCtx], 2 /*int 80h*/, 0x80+iCtx /*bXcpt*/, pszMode, iCtx);
    }

    /*
     * Check that the gate DPL checks works.
     */
    uLine = 100;
    for (iRing = 0; iRing <= 3; iRing++)
    {
        for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
        {
            Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
# if TMPL_BITS == 32
            BS3_DATA_NM(g_uBs3TrapEipHint) = CtxTmp.rip.u32;
# endif
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            uLine++;
            if (iCtx < iRing)
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                          f16BitSys, pszMode, uLine);
            else
                bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &CtxTmp, 2 /*int 8xh*/, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
        }
    }

    /*
     * Modify the gate CS value and run the handler at a different CPL.
     * Throw RPL variations into the mix (completely ignored) together
     * with gate presence.
     *      1. CPL <= GATE.DPL
     *      2. GATE.P
     *      3. GATE.CS.DPL <= CPL (non-conforming segments)
     */
    uLine = 1000;
    for (i = 0; i <= 3; i++)
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
# if TMPL_BITS == 32
                BS3_DATA_NM(g_uBs3TrapEipHint) = apCtx8x[iCtx]->rip.u32;
# endif
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);

                for (j = 0; j <= 3; j++)
                {
                    uint16_t const uCs = (MY_SYS_SEL_R0_CS | j) + (i << BS3_SEL_RING_SHIFT);
                    for (k = 0; k < 2; k++)
                    {
                        uLine++;
                        /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", uLine,  iCtx,  iRing, i, uCs);*/
                        MyBs3Idt[0x80+iCtx].Gate.u16Sel = uCs;
                        MyBs3Idt[0x80+iCtx].Gate.u1Present = k;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        /*Bs3TrapPrintFrame(&TrapCtx);*/
                        if (iCtx < iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                      f16BitSys, pszMode, uLine);
                        else if (k == 0)
                            bs3CpuBasic2_CompareNpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                      f16BitSys, pszMode, uLine);
                        else if (i > iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL, f16BitSys, pszMode, uLine);
                        else
                        {
                            uint16_t uExpectedCs = uCs & X86_SEL_MASK_OFF_RPL;
                            if (i <= iCtx && i <= iRing)
                                uExpectedCs |= i;
                            bs3CpuBasic2_CompareTrapCtx2(&TrapCtx, &CtxTmp, 2 /*int 8xh*/, 0x80 + iCtx /*bXcpt*/,
                                                         uExpectedCs, pszMode, uLine);
                        }
                    }
                }

                MyBs3Idt[0x80+iCtx].Gate.u16Sel = MY_SYS_SEL_R0_CS;
                MyBs3Idt[0x80+iCtx].Gate.u1Present = 1;
            }
        }
    }
    BS3_ASSERT(uLine < 2000);

    /*
     * Modify the gate CS value with a conforming segment.
     */
#if 0
    uLine = 2000;
    for (i = 0; i <= 3; i++)
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
                uint16_t const uCs = (MY_SYS_SEL_R0_CS_CNF | i) + (i << BS3_SEL_RING_SHIFT);
                uLine++;
                /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", uLine,  iCtx,  iRing, i, uCs);*/
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
                MyBs3Idt[0x80+iCtx].Gate.u16Sel = uCs;
# if TMPL_BITS == 32
                BS3_DATA_NM(g_uBs3TrapEipHint) = CtxTmp.rip.u32;
# endif
                Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                /*Bs3TrapPrintFrame(&TrapCtx);*/
                if (iCtx < iRing)
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                              f16BitSys, pszMode, uLine);
                else if (i > iRing)
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL, f16BitSys, pszMode, uLine);
                else
                    bs3CpuBasic2_CompareTrapCtx1(&TrapCtx, &CtxTmp, 2 /*int 8xh*/, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
                MyBs3Idt[0x80+iCtx].Gate.u16Sel = MY_SYS_SEL_R0_CS;
            }
        }
    }
    BS3_ASSERT(uLine < 3000);
#endif

# if BS3_MODE_IS_64BIT_SYS(TMPL_MODE)
    /*
     * The gates must be 64-bit.
     */
    uLine = 3000;
    for (i = 0; i <= 3; i++)
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);

                for (j = 0; j < 2; j++)
                {
                    static const uint16_t s_auCSes[2] = { BS3_SEL_R0_CS16, BS3_SEL_R0_CS32 };
                    uint16_t uCs = (s_auCSes[j] | i) + (i << BS3_SEL_RING_SHIFT);
                    uLine++;
                    /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", uLine,  iCtx,  iRing, i, uCs);*/
                    MyBs3Idt[0x80+iCtx].Gate.u16Sel = uCs;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    /*Bs3TrapPrintFrame(&TrapCtx);*/
                    if (iCtx < iRing)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);
                    else
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL, f16BitSys, pszMode, uLine);
                }
                MyBs3Idt[0x80+iCtx].Gate.u16Sel = MY_SYS_SEL_R0_CS;
            }
        }
    }
    BS3_ASSERT(uLine < 4000);
# endif

    /*
     * Check invalid gate types.
     */
    uLine = 32000;
    for (iRing = 0; iRing <= 3; iRing++)
    {
        static const uint16_t s_auCSes[]      = { BS3_SEL_R0_CS16, BS3_SEL_R0_CS32, BS3_SEL_R0_CS64,
                                                  BS3_SEL_TSS16, BS3_SEL_TSS32, BS3_SEL_TSS64, 0, BS3_SEL_SPARE_1f };
# if BS3_MODE_IS_64BIT_SYS(TMPL_MODE)
        static uint16_t const s_auInvlTypes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
                                                  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                                  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
        uint16_t        const cInvTypes       = RT_ELEMENTS(s_auInvlTypes);
# else
        static uint16_t const s_auInvlTypes[] = { 0, 1, 2, 3, 8, 9, 10, 11, 13,
                                                  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                                  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                                                  /*286:*/ 12, 14, 15 };
        uint16_t const        cInvTypes       = (BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) < BS3CPU_80386
                                              ? RT_ELEMENTS(s_auInvlTypes) : RT_ELEMENTS(s_auInvlTypes) - 3;
# endif
        for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
        {
            unsigned iType;

            Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
# if TMPL_BITS == 32
            BS3_DATA_NM(g_uBs3TrapEipHint) = CtxTmp.rip.u32;
# endif
            for (iType = 0; iType < cInvTypes; iType++)
            {
                uint8_t const bSavedType = MyBs3Idt[0x80 + iCtx].Gate.u4Type;
                MyBs3Idt[0x80 + iCtx].Gate.u1DescType = s_auInvlTypes[iType] >> 4;
                MyBs3Idt[0x80 + iCtx].Gate.u4Type     = s_auInvlTypes[iType] & 0xf;

                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < RT_ELEMENTS(s_auCSes); j++)
                    {
                        uint16_t uCs = (unsigned)(s_auCSes[j] - BS3_SEL_R0_FIRST) < (unsigned)(4 << BS3_SEL_RING_SHIFT)
                                     ? (s_auCSes[j] | i) + (i << BS3_SEL_RING_SHIFT)
                                     : s_auCSes[j] | i;
                        /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x type=%#x\n", uLine, iCtx, iRing, i, uCs, s_auInvlTypes[iType]);*/
                        MyBs3Idt[0x80 + iCtx].Gate.u16Sel = uCs;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        uLine++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);

                        /* Mark it not-present to check that invalid type takes precedence. */
                        MyBs3Idt[0x80 + iCtx].Gate.u1Present = 0;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        uLine++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);
                        MyBs3Idt[0x80 + iCtx].Gate.u1Present = 1;
                    }
                }

                MyBs3Idt[0x80 + iCtx].Gate.u16Sel     = MY_SYS_SEL_R0_CS;
                MyBs3Idt[0x80 + iCtx].Gate.u4Type     = bSavedType;
                MyBs3Idt[0x80 + iCtx].Gate.u1DescType = 0;
                MyBs3Idt[0x80 + iCtx].Gate.u1Present  = 1;
            }
        }
    }
    BS3_ASSERT(uLine < 62000U && uLine > 32000U);

#else
    bRet = BS3TESTDOMODE_SKIPPED;
#endif

    /*
     * Re-initialize the IDT.
     */
    TMPL_NM(Bs3TrapInit)();
    return bRet;
}




BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_iret)(uint8_t bMode)
{
    NOREF(bMode);
    return BS3TESTDOMODE_SKIPPED;
}


#endif /* BS3_INSTANTIATING_MODE */

