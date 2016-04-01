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
#define bs3CpuBasic2_CompareIntCtx1 BS3_CMN_NM(bs3CpuBasic2_CompareIntCtx1)
void bs3CpuBasic2_CompareIntCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint8_t bXcpt,
                                  const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 2 /*int xx*/, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}


/**
 * Compares trap stuff.
 */
#define bs3CpuBasic2_CompareTrapCtx2 BS3_CMN_NM(bs3CpuBasic2_CompareTrapCtx2)
void bs3CpuBasic2_CompareTrapCtx2(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust, uint8_t bXcpt,
                                  uint16_t uHandlerCs, const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    CHECK_MEMBER("uHandlerCs", "%#06x", pTrapCtx->uHandlerCs,   uHandlerCs);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares \#GP trap.
 */
#define bs3CpuBasic2_CompareGpCtx BS3_CMN_NM(bs3CpuBasic2_CompareGpCtx)
void bs3CpuBasic2_CompareGpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
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
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares \#NP trap.
 */
#define bs3CpuBasic2_CompareNpCtx BS3_CMN_NM(bs3CpuBasic2_CompareNpCtx)
void bs3CpuBasic2_CompareNpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
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
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares \#SS trap.
 */
#define bs3CpuBasic2_CompareSsCtx BS3_CMN_NM(bs3CpuBasic2_CompareSsCtx)
void bs3CpuBasic2_CompareSsCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
                               const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        X86_XCPT_SS);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       (uint64_t)uErrCd);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/,
                         f16BitHandler ? 0 : X86_EFL_RF,
                         pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares \#TS trap.
 */
#define bs3CpuBasic2_CompareTsCtx BS3_CMN_NM(bs3CpuBasic2_CompareTsCtx)
void bs3CpuBasic2_CompareTsCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f16BitHandler,
                               const char *pszMode, unsigned uLine)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        X86_XCPT_TS);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       (uint64_t)uErrCd);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/,
                         f16BitHandler ? 0 : X86_EFL_RF,
                         pszMode, uLine);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

/**
 * Compares \#PF trap.
 */
#define bs3CpuBasic2_ComparePfCtx BS3_CMN_NM(bs3CpuBasic2_ComparePfCtx)
void bs3CpuBasic2_ComparePfCtx(PCBS3TRAPFRAME pTrapCtx, PBS3REGCTX pStartCtx, uint16_t uErrCd, uint64_t uCr2Expected,
                               bool f16BitHandler, const char *pszMode, unsigned uLine)
{
    uint64_t const uCr2Saved     = pStartCtx->cr2.u;
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        X86_XCPT_PF);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       (uint64_t)uErrCd);
    pStartCtx->cr2.u = uCr2Expected;
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/,
                         f16BitHandler ? 0 : X86_EFL_RF,
                         pszMode, uLine);
    pStartCtx->cr2.u = uCr2Saved;
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
//Bs3TestPrintf("%s\n",  __FUNCTION__);
        Bs3TrapPrintFrame(pTrapCtx);
ASMHalt();
    }
}

#define bs3CpuBasic2_RaiseXcpt1Common BS3_CMN_NM(bs3CpuBasic2_RaiseXcpt1Common)
static void bs3CpuBasic2_RaiseXcpt1Common(uint8_t const bMode, const char * const pszMode, bool const f16BitSys,
                                          uint16_t const uSysR0Cs, uint16_t const uSysR0CsConf, uint16_t const uSysR0Ss,
                                          PX86DESC const paIdt, unsigned const cIdteShift)
{
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx80;
    BS3REGCTX       Ctx81;
    BS3REGCTX       Ctx82;
    BS3REGCTX       Ctx83;
    BS3REGCTX       CtxTmp;
    PBS3REGCTX      apCtx8x[4];
    unsigned        iCtx;
    unsigned        iRing;
    unsigned        iDpl;
    unsigned        iRpl;
    unsigned        i, j, k;
    unsigned        uLine;
    uint32_t        uExpected;
# if TMPL_BITS == 16
    bool const      f386Plus = (BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) >= BS3CPU_80386;
# else
    bool const      f386Plus = true;
    int             rc;
    uint8_t        *pbIdtCopyAlloc;
    PX86DESC        pIdtCopy;
    const unsigned  cbIdte = 1 << (3 + cIdteShift);
    RTCCUINTXREG    uCr0Saved = ASMGetCR0();
    RTGDTR          GdtrSaved;
# endif
    RTIDTR          IdtrSaved;
    RTIDTR          Idtr;

    ASMGetIDTR(&IdtrSaved);
# if TMPL_BITS != 16
    ASMGetGDTR(&GdtrSaved);
# endif

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

# if TMPL_BITS != 16
    /* Allocate memory for playing around with the IDT. */
    pbIdtCopyAlloc = NULL;
    if (BS3_MODE_IS_PAGED(bMode))
        pbIdtCopyAlloc = Bs3MemAlloc(BS3MEMKIND_FLAT32, 12*_1K);
# endif

    /*
     * IDT entry 80 thru 83 are assigned DPLs according to the number.
     * (We'll be useing more, but this'll do for now.)
     */
    paIdt[0x80 << cIdteShift].Gate.u2Dpl = 0;
    paIdt[0x81 << cIdteShift].Gate.u2Dpl = 1;
    paIdt[0x82 << cIdteShift].Gate.u2Dpl = 2;
    paIdt[0x83 << cIdteShift].Gate.u2Dpl = 3;

    Bs3RegCtxSave(&Ctx80);
    Ctx80.rsp.u -= 0x300;
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
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, apCtx8x[iCtx], 0x80+iCtx /*bXcpt*/, pszMode, iCtx);
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
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
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
                    uint16_t const uCs = (uSysR0Cs | j) + (i << BS3_SEL_RING_SHIFT);
                    for (k = 0; k < 2; k++)
                    {
                        uLine++;
                        /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", uLine,  iCtx,  iRing, i, uCs);*/
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = k;
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

                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 1;
            }
        }
    }
    BS3_ASSERT(uLine < 2800);


    /*
     * Various CS and SS related faults
     * We temporarily reconfigure gate 80 and 83 with new CS selectors, the
     * latter have a CS.DPL of 2 for testing ring transisions and SS loading
     * without making it impossible to handle faults.
     *
     * Check #NP(CS). Pit #NP(CS) against #GP(CS) due to DPL.
     * Ditto for stack.
     * Pit SS trouble against CS trouble.
     * Pit both against gate DPL trouble.
     */
    uLine = 2800;
    BS3_DATA_NM(Bs3GdteTestPage00) = BS3_DATA_NM(Bs3Gdt)[uSysR0Cs >> X86_SEL_SHIFT];
    BS3_DATA_NM(Bs3GdteTestPage00).Gen.u1Present = 0;
    paIdt[0x80 << cIdteShift].Gate.u16Sel = BS3_SEL_TEST_PAGE_00;

    /* CS.PRESENT = 0 */
    Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00, f16BitSys, pszMode, uLine);
    uLine++;

    /* Check that GATE.DPL is checked before CS.PRESENT. */
    for (iRing = 1; iRing < 4; iRing++)
    {
        Bs3MemCpy(&CtxTmp, &Ctx80, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, (0x80 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                  f16BitSys, pszMode, uLine);
        uLine++;
    }

    /* CS.DPL mismatch takes precedence over CS.PRESENT = 0. */
    Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00, f16BitSys, pszMode, uLine);
    uLine++;
    for (iDpl = 1; iDpl < 4; iDpl++)
    {
        BS3_DATA_NM(Bs3GdteTestPage00).Gen.u2Dpl = iDpl;
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00, f16BitSys, pszMode, uLine);
        uLine++;
    }

    /* Test SS. */
    if (!BS3_MODE_IS_64BIT_SYS(bMode))
    {
        uint16_t BS3_FAR *puTssSs2  = BS3_MODE_IS_16BIT_SYS(bMode) ? &BS3_DATA_NM(Bs3Tss16).ss2 : &BS3_DATA_NM(Bs3Tss32).ss2;
        uint16_t const    uSavedSs2 = *puTssSs2;

        /* Make the handler execute in ring-2. */
        BS3_DATA_NM(Bs3GdteTestPage02) = BS3_DATA_NM(Bs3Gdt)[(uSysR0Cs + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        paIdt[0x83 << cIdteShift].Gate.u16Sel = BS3_SEL_TEST_PAGE_02 | 2;

        Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, 3); /* yeah, from 3 so SS:xSP is reloaded. */
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83, pszMode, uLine);
        uLine++;

        /* Create a SS.DPL=2 stack segment and check that SS2.RPL matters and
           that we get #SS if the selector isn't present. */
        BS3_DATA_NM(Bs3GdteTestPage03) = BS3_DATA_NM(Bs3Gdt)[(uSysR0Ss + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        for (iDpl = 0; iDpl < 4; iDpl++)
        {
            BS3_DATA_NM(Bs3GdteTestPage03).Gen.u2Dpl = iDpl;
            for (k = 0; k < 2; k++)
            {
                BS3_DATA_NM(Bs3GdteTestPage03).Gen.u1Present = k == 0;
                for (iRpl = 0; iRpl < 4; iRpl++)
                {
                    *puTssSs2 = BS3_SEL_TEST_PAGE_03 | iRpl;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    if (iRpl != 2 || iRpl != iDpl)
                        bs3CpuBasic2_CompareTsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03, f16BitSys, pszMode, uLine);
                    else if (k != 0)
                        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03, f16BitSys, pszMode, uLine);
                    else
                    {
                        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83, pszMode, uLine);
                        if (TrapCtx.uHandlerSs != (BS3_SEL_TEST_PAGE_03 | 2))
                            Bs3TestFailedF("%u - %s: uHandlerSs=%#x expected %#x\n", uLine, pszMode,
                                           TrapCtx.uHandlerSs, BS3_SEL_TEST_PAGE_03 | 2);
                    }
                    uLine++;

                    /* Modify the gate DPL to check that this is checked before SS.DPL and SS.PRESENT. */
                    paIdt[0x83 << cIdteShift].Gate.u2Dpl = 2;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, (0x83 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                              f16BitSys, pszMode, uLine);
                    paIdt[0x83 << cIdteShift].Gate.u2Dpl = 3;
                    uLine++;

                    /* Check the the CS.DPL check is done before the SS ones. Restoring the ring-0 INT 83
                       context triggers the CS.DPL < CPL check. */
                    Bs3TrapSetJmpAndRestore(&Ctx83, &TrapCtx);
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx83, BS3_SEL_TEST_PAGE_02, f16BitSys, pszMode, uLine);
                    uLine++;

                    /* Now mark the CS selector not present and check that that also triggers before SS stuff. */
                    BS3_DATA_NM(Bs3GdteTestPage02).Gen.u1Present = 0;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_02, f16BitSys, pszMode, uLine);
                    BS3_DATA_NM(Bs3GdteTestPage02).Gen.u1Present = 1;
                    uLine++;
                }
            }
        }

        *puTssSs2 = uSavedSs2;
        paIdt[0x83 << cIdteShift].Gate.u16Sel = uSysR0Cs;
    }
    paIdt[0x80 << cIdteShift].Gate.u16Sel = uSysR0Cs;

    /*
     * Modify the gate CS value with a conforming segment.
     */
    uLine = 2000;
    for (i = 0; i <= 3; i++) /* cs.dpl */
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
# if TMPL_BITS == 32
                BS3_DATA_NM(g_uBs3TrapEipHint) = CtxTmp.rip.u32;
# endif

                for (j = 0; j <= 3; j++) /* rpl */
                {
                    uint16_t const uCs = (uSysR0CsConf | j) + (i << BS3_SEL_RING_SHIFT);
                    /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", uLine,  iCtx,  iRing, i, uCs);*/
                    paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    //Bs3TestPrintf("%u/%u/%u/%u: cs=%04x hcs=%04x xcpt=%02x\n", i, iRing, iCtx, j, uCs, TrapCtx.uHandlerCs, TrapCtx.bXcpt);
                    /*Bs3TrapPrintFrame(&TrapCtx);*/
                    uLine++;
                    if (iCtx < iRing)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);
                    else if (i > iRing)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL, f16BitSys, pszMode, uLine);
                    else
                        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
                }
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
            }
        }
    }
    BS3_ASSERT(uLine < 3000);

    /*
     * The gates must be 64-bit in long mode.
     */
    if (cIdteShift != 0)
    {
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
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        /*Bs3TrapPrintFrame(&TrapCtx);*/
                        if (iCtx < iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                      f16BitSys, pszMode, uLine);
                        else
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL, f16BitSys, pszMode, uLine);
                    }
                    paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
                }
            }
        }
        BS3_ASSERT(uLine < 4000);
    }

    /*
     * IDT limit check.
     */
    uLine = 5000;
    i = (0x80 << (cIdteShift + 3)) - 1;
    j = (0x82 << (cIdteShift + 3)) - 1;
    k = (0x83 << (cIdteShift + 3)) - 1;
    for (; i <= k; i++, uLine++)
    {
        Idtr = IdtrSaved;
        Idtr.cbIdt  = i;
        ASMSetIDTR(&Idtr);
        Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
        if (i < j)
            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx81, (0x81 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                      f16BitSys, pszMode, uLine);
        else
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/, pszMode, uLine);
    }
    ASMSetIDTR(&IdtrSaved);
    BS3_ASSERT(uLine < 5100);

# if TMPL_BITS != 16 /* Only do the paging related stuff in 32-bit and 64-bit modes. */

    /*
     * IDT page not present. Placing the IDT copy such that 0x80 is on the
     * first page and 0x81 is on the second page.  We need proceed to move
     * it down byte by byte to check that any inaccessible byte means #PF.
     *
     * Note! We must reload the alternative IDTR for each run as any kind of
     *       printing to the string (like error reporting) will cause a switch
     *       to real mode and back, reloading the default IDTR.
     */
    uLine = 5200;
    if (BS3_MODE_IS_PAGED(bMode) && pbIdtCopyAlloc)
    {
        uint32_t const uCr2Expected = Bs3SelPtrToFlat(pbIdtCopyAlloc) + _4K;
        for (j = 0; j < cbIdte; j++)
        {
            pIdtCopy = (PX86DESC)&pbIdtCopyAlloc[_4K - cbIdte * 0x81 - j];
            Bs3MemCpy(pIdtCopy, paIdt, cbIdte * 256);

            Idtr.cbIdt = IdtrSaved.cbIdt;
            Idtr.pIdt  = Bs3SelPtrToFlat(pIdtCopy);

            ASMSetIDTR(&Idtr);
            Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/, pszMode, uLine++);

            ASMSetIDTR(&Idtr);
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/, pszMode, uLine++);

            rc = Bs3PagingProtect(uCr2Expected, _4K, 0 /*fSet*/, X86_PTE_P /*fClear*/);
            if (RT_SUCCESS(rc))
            {
                ASMSetIDTR(&Idtr);
                Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/, pszMode, uLine++);

                ASMSetIDTR(&Idtr);
                Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, 0 /*uErrCd*/, uCr2Expected, f16BitSys, pszMode, uLine++);

                Bs3PagingProtect(uCr2Expected, _4K, X86_PTE_P /*fSet*/, 0 /*fClear*/);

                /* Check if that the entry type is checked after the whole IDTE has been cleared for #PF. */
                pIdtCopy[0x80 << cIdteShift].Gate.u4Type = 0;
                rc = Bs3PagingProtect(uCr2Expected, _4K, 0 /*fSet*/, X86_PTE_P /*fClear*/);
                if (RT_SUCCESS(rc))
                {
                    ASMSetIDTR(&Idtr);
                    Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
                    bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, 0 /*uErrCd*/, uCr2Expected, f16BitSys, pszMode, uLine++);

                    Bs3PagingProtect(uCr2Expected, _4K, X86_PTE_P /*fSet*/, 0 /*fClear*/);
                }
            }
            else
                Bs3TestPrintf("Bs3PagingProtectPtr: %d\n", i);

            ASMSetIDTR(&IdtrSaved);
        }
    }

    /*
     * The read/write and user/supervisor bits the IDT PTEs are irrelevant.
     */
    uLine = 5300;
    if (BS3_MODE_IS_PAGED(bMode) && pbIdtCopyAlloc)
    {
        Bs3MemCpy(pbIdtCopyAlloc, paIdt, cbIdte * 256);
        Idtr.cbIdt = IdtrSaved.cbIdt;
        Idtr.pIdt  = Bs3SelPtrToFlat(pbIdtCopyAlloc);

        ASMSetIDTR(&Idtr);
        Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/, pszMode, uLine++);

        rc = Bs3PagingProtect(Idtr.pIdt, _4K, 0 /*fSet*/, X86_PTE_RW | X86_PTE_US /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            ASMSetIDTR(&Idtr);
            Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/, pszMode, uLine++);

            Bs3PagingProtect(Idtr.pIdt, _4K, X86_PTE_RW | X86_PTE_US /*fSet*/, 0 /*fClear*/);
        }
        ASMSetIDTR(&IdtrSaved);
    }

    /*
     * Check that CS.u1Accessed is set to 1. Use the test page selector #0 and #3 together
     * with interrupt gates 80h and 83h, respectively.
     */
    uLine = 5400;
    if (BS3_MODE_IS_PAGED(bMode) && pbIdtCopyAlloc)
    {
        BS3_DATA_NM(Bs3GdteTestPage00) = BS3_DATA_NM(Bs3Gdt)[uSysR0Cs >> X86_SEL_SHIFT];
        BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        paIdt[0x80 << cIdteShift].Gate.u16Sel   = BS3_SEL_TEST_PAGE_00;

        BS3_DATA_NM(Bs3GdteTestPage03) = BS3_DATA_NM(Bs3Gdt)[(uSysR0Cs + (3 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        paIdt[0x83 << cIdteShift].Gate.u16Sel   = BS3_SEL_TEST_PAGE_03; /* rpl is ignored, so leave it as zero. */

        /* Check that the CS.A bit is being set on a general basis and that
           the special CS values work with out generic handler code. */
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/, pszMode, uLine);
        if (!(BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
            Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
        uLine++;

        Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, 3);
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/, pszMode, uLine);
        if (!(BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
            Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed!\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
        if (TrapCtx.uHandlerCs != (BS3_SEL_TEST_PAGE_03 | 3))
            Bs3TestFailedF("%u - %s: uHandlerCs=%#x, expected %#x\n", uLine, pszMode, TrapCtx.uHandlerCs, (BS3_SEL_TEST_PAGE_03 | 3));
        uLine++;

        /*
         * Now check that setting CS.u1Access to 1 does __NOT__ trigger a page
         * fault due to the RW bit being zero.
         * (We check both with with and without the WP bit if 80486.)
         */
        if ((BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
            ASMSetCR0(uCr0Saved | X86_CR0_WP);

        BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        rc = Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, 0 /*fSet*/, X86_PTE_RW /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            /* ring-0 handler */
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/, pszMode, uLine);
            if (!(BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed!\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
            uLine++;

            /* ring-3 handler */
            Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, 3);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/, pszMode, uLine);
            if (!(BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed!\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
            uLine++;

            /* clear WP and repeat the above. */
            if ((BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
                ASMSetCR0(uCr0Saved & ~X86_CR0_WP);
            BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* (No need to RW the page - ring-0, WP=0.) */
            BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* (No need to RW the page - ring-0, WP=0.) */

            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/, pszMode, uLine);
            if (!(BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed!\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
            uLine++;

            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/, pszMode, uLine);
            if (!(BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                Bs3TestFailedF("%u - %s: u4Type=%#x, not accessed!\n", uLine, pszMode, BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type);
            uLine++;

            Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, X86_PTE_RW /*fSet*/, 0 /*fClear*/);
        }

        ASMSetCR0(uCr0Saved);

        /*
         * While we're here, check that if the CS GDT entry is a non-present
         * page we do get a #PF with the rigth error code and CR2.
         */
        BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* Just for fun, really a pointless gesture. */
        BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        rc = Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, 0 /*fSet*/, X86_PTE_P /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx80, 0 /*uErrCd*/, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00,
                                      f16BitSys, pszMode, uLine);
            uLine++;

            /* Do it from ring-3 to check ErrCd, which doesn't set X86_TRAP_PF_US it turns out. */
            Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, 3);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);

            bs3CpuBasic2_ComparePfCtx(&TrapCtx, &CtxTmp, 0 /*uErrCd*/, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_03,
                                      f16BitSys, pszMode, uLine);
            uLine++;

            Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, X86_PTE_P /*fSet*/, 0 /*fClear*/);
            if (BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type & X86_SEL_TYPE_ACCESSED)
                Bs3TestFailedF("%u - %s: u4Type=%#x, accessed!\n", uLine - 2, pszMode, BS3_DATA_NM(Bs3GdteTestPage00).Gen.u4Type);
            if (BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type & X86_SEL_TYPE_ACCESSED)
                Bs3TestFailedF("%u - %s: u4Type=%#x, accessed!\n", uLine - 1, pszMode, BS3_DATA_NM(Bs3GdteTestPage03).Gen.u4Type);
        }

        /* restore */
        paIdt[0x80 << cIdteShift].Gate.u16Sel = uSysR0Cs;
        paIdt[0x83 << cIdteShift].Gate.u16Sel = uSysR0Cs;// + (3 << BS3_SEL_RING_SHIFT) + 3;
    }

# endif /* 32 || 64*/

    /*
     * Check broad EFLAGS effects.
     */
    uLine = 5600;
    for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
    {
        for (iRing = 0; iRing < 4; iRing++)
        {
            Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, iRing);

            /* all set */
            CtxTmp.rflags.u32 &= X86_EFL_VM | X86_EFL_1;
            CtxTmp.rflags.u32 |= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF /* | X86_EFL_TF */ /*| X86_EFL_IF*/
                               | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL /* | X86_EFL_NT*/;
            if (f386Plus)
                CtxTmp.rflags.u32 |= /*X86_EFL_VM |*/ X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP;
            if (f386Plus && !f16BitSys)
                CtxTmp.rflags.u32 |= X86_EFL_RF;
            if (BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_F_CPUID)
                CtxTmp.rflags.u32 |= X86_EFL_ID;
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            CtxTmp.rflags.u32 &= ~X86_EFL_RF;

            if (iCtx >= iRing)
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
            else
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                          f16BitSys, pszMode, uLine);
            uExpected = CtxTmp.rflags.u32
                      & (  X86_EFL_1 |  X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_DF
                         | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_VM | X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP
                         | X86_EFL_ID /*| X86_EFL_TF*/ /*| X86_EFL_IF*/ /*| X86_EFL_RF*/ );
            if (TrapCtx.fHandlerRfl != uExpected)
                Bs3TestFailedF("%u - %s: unexpected handler rflags value: %RX64 expected %RX32; CtxTmp.rflags=%RX64 Ctx.rflags=%RX64\n",
                               uLine, pszMode, TrapCtx.fHandlerRfl, uExpected, CtxTmp.rflags.u, TrapCtx.Ctx.rflags.u);
            uLine++;

            /* all cleared */
            if ((BS3_DATA_NM(g_uBs3CpuDetected) & BS3CPU_TYPE_MASK) < BS3CPU_80286)
                CtxTmp.rflags.u32 = apCtx8x[iCtx]->rflags.u32 & (X86_EFL_RA1_MASK | UINT16_C(0xf000));
            else
                CtxTmp.rflags.u32 = apCtx8x[iCtx]->rflags.u32 & (X86_EFL_VM | X86_EFL_RA1_MASK);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            if (iCtx >= iRing)
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/, pszMode, uLine);
            else
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                          f16BitSys, pszMode, uLine);
            uExpected = CtxTmp.rflags.u32;
            if (TrapCtx.fHandlerRfl != uExpected)
                Bs3TestFailedF("%u - %s: unexpected handler rflags value: %RX64 expected %RX32; CtxTmp.rflags=%RX64 Ctx.rflags=%RX64\n",
                               uLine, pszMode, TrapCtx.fHandlerRfl, uExpected, CtxTmp.rflags.u, TrapCtx.Ctx.rflags.u);
            uLine++;
        }
    }

    /*
     * Check invalid gate types.
     */
    uLine = 32000;
    for (iRing = 0; iRing <= 3; iRing++)
    {
        static const uint16_t   s_auCSes[]        = { BS3_SEL_R0_CS16, BS3_SEL_R0_CS32, BS3_SEL_R0_CS64,
                                                      BS3_SEL_TSS16, BS3_SEL_TSS32, BS3_SEL_TSS64, 0, BS3_SEL_SPARE_1f };
        static uint16_t const   s_auInvlTypes64[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
                                                      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                                      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
        static uint16_t const   s_auInvlTypes32[] = { 0, 1, 2, 3, 8, 9, 10, 11, 13,
                                                      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                                      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                                                      /*286:*/ 12, 14, 15 };
        uint16_t const * const  pauInvTypes       = cIdteShift != 0 ? s_auInvlTypes64 : s_auInvlTypes32;
        uint16_t const          cInvTypes         = cIdteShift != 0 ? RT_ELEMENTS(s_auInvlTypes64)
                                                  : f386Plus ? RT_ELEMENTS(s_auInvlTypes32) - 3 : RT_ELEMENTS(s_auInvlTypes32);


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
                uint8_t const bSavedType = paIdt[(0x80 + iCtx) << cIdteShift].Gate.u4Type;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1DescType = pauInvTypes[iType] >> 4;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u4Type     = pauInvTypes[iType] & 0xf;

                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < RT_ELEMENTS(s_auCSes); j++)
                    {
                        uint16_t uCs = (unsigned)(s_auCSes[j] - BS3_SEL_R0_FIRST) < (unsigned)(4 << BS3_SEL_RING_SHIFT)
                                     ? (s_auCSes[j] | i) + (i << BS3_SEL_RING_SHIFT)
                                     : s_auCSes[j] | i;
                        /*Bs3TestPrintf("uLine=%u iCtx=%u iRing=%u i=%u uCs=%04x type=%#x\n", uLine, iCtx, iRing, i, uCs, pauInvTypes[iType]);*/
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        uLine++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);

                        /* Mark it not-present to check that invalid type takes precedence. */
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 0;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        uLine++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT,
                                                  f16BitSys, pszMode, uLine);
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 1;
                    }
                }

                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel     = MY_SYS_SEL_R0_CS;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u4Type     = bSavedType;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1DescType = 0;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present  = 1;
            }
        }
    }
    BS3_ASSERT(uLine < 62000U && uLine > 32000U);


    /** @todo
     *  - Run \#PF and \#GP (and others?) at CPLs other than zero.
     *  - Quickly generate all faults.
     *  - All the peculiarities v8086.
     */

# if TMPL_BITS != 16
    Bs3MemFree(pbIdtCopyAlloc, 12*_1K);
# endif
}

#endif /* once for each bitcount */


#if TMPL_MODE == BS3_MODE_PE16 || TMPL_MODE == BS3_MODE_PE16_32

/**
 * Worker for bs3CpuBasic2_TssGateEsp that tests the INT 80 from outer rings.
 */
#define bs3CpuBasic2_TssGateEsp_AltStackOuterRing BS3_CMN_NM(bs3CpuBasic2_TssGateEsp_AltStackOuterRing)
void bs3CpuBasic2_TssGateEsp_AltStackOuterRing(PCBS3REGCTX pCtx, uint8_t bRing, uint8_t *pbAltStack, size_t cbAltStack,
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

    bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx2, 0x80 /*bXcpt*/, pszMode, uLine);
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

#define bs3CpuBasic2_TssGateEspCommon BS3_CMN_NM(bs3CpuBasic2_TssGateEspCommon)
void bs3CpuBasic2_TssGateEspCommon(uint8_t const bMode, const char * const pszMode, bool const f16BitSys,
                                   PX86DESC const paIdt, unsigned const cIdteShift)
{
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx;
    BS3REGCTX       Ctx2;
# if TMPL_BITS == 16
    uint8_t        *pbTmp;
# endif
    unsigned        uLine;

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&Ctx2, sizeof(Ctx2));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

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
    paIdt[0x80 << cIdteShift].Gate.u2Dpl = 3;
    paIdt[0x81 << cIdteShift].Gate.u2Dpl = 0;

    /*
     * Check that the basic stuff works first.
     */
    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
    bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx, 0x80 /*bXcpt*/, pszMode, __LINE__);

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
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx2, 0x80 /*bXcpt*/, pszMode, uLine);
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
            if (f16BitSys)
            {
                Bs3Tss16.ss0 = BS3_SEL_R0_SS32;
                bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                          false, f16BitSys, f16BitSys, pszMode, __LINE__);
                Bs3Tss16.ss0 = BS3_SEL_R0_SS16;
            }
            else
            {
                Bs3Tss32.ss0 = BS3_SEL_R0_SS16;
                bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                          true,  f16BitSys, f16BitSys, pszMode, __LINE__);
                Bs3Tss32.ss0 = BS3_SEL_R0_SS32;
            }

            Bs3MemFree(pbAltStack, cbAltStack);
        }
        else
            Bs3TestPrintf("%s: Skipping ESP check, alloc failed\n", pszMode);
    }
    else
        Bs3TestPrintf("%s: Skipping ESP check, CPU too old\n", pszMode);
}

#endif


BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t bRet = 0;

#if TMPL_MODE == BS3_MODE_PE16 \
 || TMPL_MODE == BS3_MODE_PE16_32 \
 || TMPL_MODE == BS3_MODE_PP16 \
 || TMPL_MODE == BS3_MODE_PP16_32 \
 || TMPL_MODE == BS3_MODE_PAE16 \
 || TMPL_MODE == BS3_MODE_PAE16_32 \
 || TMPL_MODE == BS3_MODE_PE32
    bs3CpuBasic2_TssGateEspCommon(bMode,
                                  BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)),
                                  BS3_MODE_IS_16BIT_SYS(TMPL_MODE),
                                  (PX86DESC)MyBs3Idt,
                                  BS3_MODE_IS_64BIT_SYS(TMPL_MODE) ? 1 : 0);
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
#if !BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
    uint8_t bRet = 0;

    /*
     * Pass to common worker which is only compiled once per mode.
     */
    bs3CpuBasic2_RaiseXcpt1Common(bMode,
                                  BS3_DATA_NM(TMPL_NM(g_szBs3ModeName)),
                                  BS3_MODE_IS_16BIT_SYS(TMPL_MODE),
                                  MY_SYS_SEL_R0_CS,
                                  MY_SYS_SEL_R0_CS_CNF,
                                  MY_SYS_SEL_R0_SS,
                                  (PX86DESC)MyBs3Idt,
                                  BS3_MODE_IS_64BIT_SYS(TMPL_MODE) ? 1 : 0);

    /*
     * Re-initialize the IDT.
     */
    TMPL_NM(Bs3TrapInit)();
    return bRet;
#elif !BS3_MODE_IS_RM(TMPL_MODE)

    /*
     * Check
     */
    /** @todo check    */
    return BS3TESTDOMODE_SKIPPED;

#else
    return BS3TESTDOMODE_SKIPPED;
#endif
}




BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_iret)(uint8_t bMode)
{
    NOREF(bMode);
    return BS3TESTDOMODE_SKIPPED;
}


#endif /* BS3_INSTANTIATING_MODE */

