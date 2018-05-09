/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-weird-2, C test driver code (16-bit).
 */

/*
 * Copyright (C) 2007-2018 Oracle Corporation
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
#define BS3_USE_X0_TEXT_SEG
#include <bs3kit.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#undef  CHECK_MEMBER
#define CHECK_MEMBER(a_szName, a_szFmt, a_Actual, a_Expected) \
    do \
    { \
        if ((a_Actual) == (a_Expected)) { /* likely */ } \
        else bs3CpuWeird1_FailedF(a_szName "=" a_szFmt " expected " a_szFmt, (a_Actual), (a_Expected)); \
    } while (0)


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_c64;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_int80_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_int80_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt80_int80_c64;

extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_c64;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_int3_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_int3_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedInt3_int3_c64;

extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_c64;
extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_int3_c16;
extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_int3_c32;
extern FNBS3FAR     bs3CpuWeird1_InhibitedBp_int3_c64;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char BS3_FAR  *g_pszTestMode = (const char *)1;
static BS3CPUVENDOR         g_enmCpuVendor = BS3CPUVENDOR_INTEL;
static bool                 g_fVME = false;
//static uint8_t              g_bTestMode = 1;
//static bool                 g_f16BitSys = 1;



/**
 * Sets globals according to the mode.
 *
 * @param   bTestMode   The test mode.
 */
static void bs3CpuWeird1_SetGlobals(uint8_t bTestMode)
{
//    g_bTestMode     = bTestMode;
    g_pszTestMode   = Bs3GetModeName(bTestMode);
//    g_f16BitSys     = BS3_MODE_IS_16BIT_SYS(bTestMode);
    g_usBs3TestStep = 0;
    g_enmCpuVendor  = Bs3GetCpuVendor();
    g_fVME = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486
          && (Bs3RegGetCr4() & X86_CR4_VME);
}


/**
 * Wrapper around Bs3TestFailedF that prefixes the error with g_usBs3TestStep
 * and g_pszTestMode.
 */
static void bs3CpuWeird1_FailedF(const char *pszFormat, ...)
{
    va_list va;

    char szTmp[168];
    va_start(va, pszFormat);
    Bs3StrPrintfV(szTmp, sizeof(szTmp), pszFormat, va);
    va_end(va);

    Bs3TestFailedF("%u - %s: %s", g_usBs3TestStep, g_pszTestMode, szTmp);
}


/**
 * Compares interrupt stuff.
 */
static void bs3CpuWeird1_CompareDbgInhibitRingXfer(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint8_t bXcpt,
                                                   int8_t cbPcAdjust, int8_t cbSpAdjust, uint32_t uDr6Expected,
                                                   uint8_t cbIretFrame, uint64_t uHandlerRsp)
{
    uint32_t uDr6 = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386 ? Bs3RegGetDr6() : X86_DR6_INIT_VAL;
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",       "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",      "%#06RX64", pTrapCtx->uErrCd,       0);
    CHECK_MEMBER("cbIretFrame", "%#04x",    pTrapCtx->cbIretFrame,  cbIretFrame);
    CHECK_MEMBER("uHandlerRsp",  "%#06RX64", pTrapCtx->uHandlerRsp,  uHandlerRsp);
    if (uDr6 != uDr6Expected)
        bs3CpuWeird1_FailedF("dr6=%#010RX32 expected %#010RX32", uDr6, uDr6Expected);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbPcAdjust, cbSpAdjust, 0 /*fExtraEfl*/, g_pszTestMode, g_usBs3TestStep);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
        Bs3TestPrintf("DR6=%#RX32; Handler: CS=%04RX16 SS:ESP=%04RX16:%08RX64 EFL=%RX64 cbIret=%#x\n",
                      uDr6, pTrapCtx->uHandlerCs, pTrapCtx->uHandlerSs, pTrapCtx->uHandlerRsp,
                      pTrapCtx->fHandlerRfl, pTrapCtx->cbIretFrame);
#if 0
        Bs3TestPrintf("Halting in CompareIntCtx: bXcpt=%#x\n", bXcpt);
        ASMHalt();
#endif
    }
}

static uint64_t bs3CpuWeird1_GetTrapHandlerEIP(uint8_t bXcpt, uint8_t bMode, bool fV86)
{
    if (   BS3_MODE_IS_RM_SYS(bMode)
        || (fV86 && BS3_MODE_IS_V86(bMode)))
    {
        PRTFAR16 paIvt = (PRTFAR16)Bs3XptrFlatToCurrent(0);
        return paIvt[bXcpt].off;
    }
    if (BS3_MODE_IS_16BIT_SYS(bMode))
        return Bs3Idt16[bXcpt].Gate.u16OffsetLow;
    if (BS3_MODE_IS_32BIT_SYS(bMode))
        return RT_MAKE_U32(Bs3Idt32[bXcpt].Gate.u16OffsetLow, Bs3Idt32[bXcpt].Gate.u16OffsetHigh);
    return RT_MAKE_U64(RT_MAKE_U32(Bs3Idt64[bXcpt].Gate.u16OffsetLow, Bs3Idt32[bXcpt].Gate.u16OffsetHigh),
                       Bs3Idt64[bXcpt].Gate.u32OffsetTop);
}


static int bs3CpuWeird1_DbgInhibitRingXfer_Worker(uint8_t bTestMode, uint8_t bIntGate, uint8_t cbRingInstr, int8_t cbSpAdjust,
                                                  FPFNBS3FAR pfnTestCode, FPFNBS3FAR pfnTestLabel)
{
    BS3TRAPFRAME            TrapCtx;
    BS3TRAPFRAME            TrapExpect;
    BS3REGCTX               Ctx;
    uint8_t                 bSavedDpl;
    uint8_t const           offTestLabel    = BS3_FP_OFF(pfnTestLabel) - BS3_FP_OFF(pfnTestCode);
    //uint8_t const           cbIretFrameSame = BS3_MODE_IS_RM_SYS(bTestMode)    ? 6
    //                                        : BS3_MODE_IS_16BIT_SYS(bTestMode) ? 12
    //                                        : BS3_MODE_IS_64BIT_SYS(bTestMode) ? 40 : 12;
    uint8_t                 cbIretFrameInt;
    uint8_t                 cbIretFrameIntDb;
    uint8_t const           cbIretFrameSame = BS3_MODE_IS_16BIT_SYS(bTestMode) ? 6
                                            : BS3_MODE_IS_32BIT_SYS(bTestMode) ? 12 : 40;
    uint8_t const           cbSpAdjSame     = BS3_MODE_IS_64BIT_SYS(bTestMode) ? 48 : cbIretFrameSame;
    uint8_t                 bVmeMethod = 0;
    uint64_t                uHandlerRspInt;
    uint64_t                uHandlerRspIntDb;
    BS3_XPTR_AUTO(uint32_t, StackXptr);

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));
    Bs3MemZero(&TrapExpect, sizeof(TrapExpect));

    /*
     * Make INT xx accessible from DPL 3 and create a ring-3 context that we can work with.
     */
    bSavedDpl = Bs3TrapSetDpl(bIntGate, 3);

    Bs3RegCtxSaveEx(&Ctx, bTestMode, 1024);
    Bs3RegCtxSetRipCsFromLnkPtr(&Ctx, pfnTestCode);
    if (BS3_MODE_IS_16BIT_SYS(bTestMode))
        g_uBs3TrapEipHint = Ctx.rip.u32;
    Ctx.rflags.u32 &= ~X86_EFL_RF;

    /* Raw-mode enablers. */
    Ctx.rflags.u32 |= X86_EFL_IF;
    if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
        Ctx.cr0.u32 |= X86_CR0_WP;

    /* We put the SS value on the stack so we can easily set breakpoints there. */
    Ctx.rsp.u32 -= 8;
    BS3_XPTR_SET_FLAT(uint32_t, StackXptr, Ctx.rsp.u32); /* ASSUMES SS.BASE == 0!! */

    /* ring-3 */
    if (!BS3_MODE_IS_RM_OR_V86(bTestMode))
        Bs3RegCtxConvertToRingX(&Ctx, 3);

    /* V8086: Set IOPL to 3. */
    if (BS3_MODE_IS_V86(bTestMode))
    {
        Ctx.rflags.u32 |= X86_EFL_IOPL;
        if (g_fVME)
        {
            Bs3RegSetTr(BS3_SEL_TSS32_IRB);
#if 0
            /* SDMv3b, 20.3.3 method 5: */
            ASMBitClear(&Bs3SharedIntRedirBm, bIntGate);
            bVmeMethod = 5;
#else
            /* SDMv3b, 20.3.3 method 4 (similar to non-VME): */
            ASMBitSet(&Bs3SharedIntRedirBm, bIntGate);
            bVmeMethod = 4;
        }
#endif
    }

    /*
     * Test #0: Test run.  Calc expected delayed #DB from it.
     */
    if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        Bs3RegSetDr7(0);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
    }
    *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;
    Bs3TrapSetJmpAndRestore(&Ctx, &TrapExpect);
    if (TrapExpect.bXcpt != bIntGate)
    {

        Bs3TestFailedF("%u: bXcpt is %#x, expected %#x!\n", g_usBs3TestStep, TrapExpect.bXcpt, bIntGate);
        Bs3TrapPrintFrame(&TrapExpect);
        return 1;
    }

    cbIretFrameInt   = TrapExpect.cbIretFrame;
    cbIretFrameIntDb = cbIretFrameInt + cbIretFrameSame;
    uHandlerRspInt   = TrapExpect.uHandlerRsp;
    uHandlerRspIntDb = uHandlerRspInt - cbSpAdjSame;

    TrapExpect.Ctx.bCpl         = 0;
    TrapExpect.Ctx.cs           = TrapExpect.uHandlerCs;
    TrapExpect.Ctx.ss           = TrapExpect.uHandlerSs;
    TrapExpect.Ctx.rsp.u64      = TrapExpect.uHandlerRsp;
    TrapExpect.Ctx.rflags.u64   = TrapExpect.fHandlerRfl;
    if (BS3_MODE_IS_V86(bTestMode))
    {
        if (bVmeMethod >= 5)
        {
            TrapExpect.Ctx.rflags.u32 |= X86_EFL_VM;
            TrapExpect.Ctx.bCpl = 3;
            TrapExpect.Ctx.rip.u64 = bs3CpuWeird1_GetTrapHandlerEIP(bIntGate, bTestMode, true);
            cbIretFrameIntDb = 36;
            if (BS3_MODE_IS_16BIT_SYS(bTestMode))
                uHandlerRspIntDb = Bs3Tss16.sp0  - cbIretFrameIntDb;
            else
                uHandlerRspIntDb = Bs3Tss32.esp0 - cbIretFrameIntDb;
        }
        else
        {
            TrapExpect.Ctx.ds   = 0;
            TrapExpect.Ctx.es   = 0;
            TrapExpect.Ctx.fs   = 0;
            TrapExpect.Ctx.gs   = 0;
        }
    }

    /*
     * Test #1: Single stepping ring-3.  Ignored except for V8086 w/ VME.
     */
    g_usBs3TestStep++;
    if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        Bs3RegSetDr7(0);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
    }
    *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;
    Ctx.rflags.u32 |= X86_EFL_TF;

    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
    if (   !BS3_MODE_IS_V86(bTestMode)
        || bVmeMethod < 5)
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, bIntGate, offTestLabel + cbRingInstr, cbSpAdjust,
                                               X86_DR6_INIT_VAL, cbIretFrameInt, uHandlerRspInt);
    else
    {
        TrapExpect.Ctx.rflags.u32 |= X86_EFL_TF;
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &TrapExpect.Ctx, X86_XCPT_DB, offTestLabel, -2,
                                               X86_DR6_INIT_VAL | X86_DR6_BS, cbIretFrameIntDb, uHandlerRspIntDb);
        TrapExpect.Ctx.rflags.u32 &= ~X86_EFL_TF;
    }

    Ctx.rflags.u32 &= ~X86_EFL_TF;
    if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        uint32_t uDr6Expect;

        /*
         * Test #2: Execution breakpoint on ring transition instruction.
         *          This hits on AMD-V (threadripper) but not on VT-x (skylake).
         */
        g_usBs3TestStep++;
        Bs3RegSetDr0(Bs3SelRealModeCodeToFlat(pfnTestLabel));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_EO) | X86_DR7_LEN(0, X86_DR7_LEN_BYTE));
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        Bs3RegSetDr7(0);
        if (g_enmCpuVendor == BS3CPUVENDOR_AMD)
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, X86_XCPT_DB, offTestLabel, cbSpAdjust,
                                                   X86_DR6_INIT_VAL | X86_DR6_B0, cbIretFrameInt, uHandlerRspInt);
        else
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, bIntGate, offTestLabel + cbRingInstr, cbSpAdjust,
                                                   X86_DR6_INIT_VAL, cbIretFrameInt, uHandlerRspInt);

        /*
         * Test #3: Same as above, but with the LE and GE flags set.
         */
        g_usBs3TestStep++;
        Bs3RegSetDr0(Bs3SelRealModeCodeToFlat(pfnTestLabel));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_EO) | X86_DR7_LEN(0, X86_DR7_LEN_BYTE) | X86_DR7_LE | X86_DR7_GE);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        if (g_enmCpuVendor == BS3CPUVENDOR_AMD)
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, X86_XCPT_DB, offTestLabel, cbSpAdjust,
                                                   X86_DR6_INIT_VAL | X86_DR6_B0, cbIretFrameInt, uHandlerRspInt);
        else
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, bIntGate, offTestLabel + cbRingInstr, cbSpAdjust,
                                                   X86_DR6_INIT_VAL, cbIretFrameInt, uHandlerRspInt);

        /*
         * Test #4: Execution breakpoint on pop ss / mov ss.  Hits.
         * Note! In real mode AMD-V updates the stack pointer, or something else is busted. Totally weird!
         */
        g_usBs3TestStep++;
        Bs3RegSetDr0(Bs3SelRealModeCodeToFlat(pfnTestCode));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_EO) | X86_DR7_LEN(0, X86_DR7_LEN_BYTE));
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, X86_XCPT_DB, 0, 0, X86_DR6_INIT_VAL | X86_DR6_B0,
                                               cbIretFrameInt,
                                               uHandlerRspInt - (BS3_MODE_IS_RM_SYS(bTestMode) ? 2 : 0) );

        /*
         * Test #5: Same as above, but with the LE and GE flags set.
         */
        g_usBs3TestStep++;
        Bs3RegSetDr0(Bs3SelRealModeCodeToFlat(pfnTestCode));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_EO) | X86_DR7_LEN(0, X86_DR7_LEN_BYTE) | X86_DR7_LE | X86_DR7_GE);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &Ctx, X86_XCPT_DB, 0, 0, X86_DR6_INIT_VAL | X86_DR6_B0,
                                               cbIretFrameInt,
                                               uHandlerRspInt - (BS3_MODE_IS_RM_SYS(bTestMode) ? 2 : 0) );

        /*
         * Test #6: Data breakpoint on SS load.  The #DB is delivered after ring transition.  Weird!
         *
         * Note! Intel loses the B0 status, probably for reasons similar to Pentium Pro errata 3.  Similar
         *       erratum is seen with virtually every march since, e.g. skylake SKL009 & SKL111.
         *       Weirdly enougth, they seem to get this right in real mode.  Go figure.
         */
        g_usBs3TestStep++;
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;
        Bs3RegSetDr0(BS3_XPTR_GET_FLAT(uint32_t, StackXptr));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_RW) | X86_DR7_LEN(0, X86_DR7_LEN_WORD));
        Bs3RegSetDr6(X86_DR6_INIT_VAL);

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        TrapExpect.Ctx.rip = TrapCtx.Ctx.rip; /// @todo fixme
        Bs3RegSetDr7(0);
        uDr6Expect = X86_DR6_INIT_VAL | X86_DR6_B0;
        if (g_enmCpuVendor == BS3CPUVENDOR_INTEL && bTestMode != BS3_MODE_RM)
            uDr6Expect = X86_DR6_INIT_VAL;
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &TrapExpect.Ctx, X86_XCPT_DB, 0, 0, uDr6Expect,
                                               cbIretFrameSame, uHandlerRspIntDb);

        /*
         * Test #7: Same as above, but with the LE and GE flags set.
         */
        g_usBs3TestStep++;
        *BS3_XPTR_GET(uint32_t, StackXptr) = Ctx.ss;
        Bs3RegSetDr0(BS3_XPTR_GET_FLAT(uint32_t, StackXptr));
        Bs3RegSetDr7(X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW(0, X86_DR7_RW_RW) | X86_DR7_LEN(0, X86_DR7_LEN_WORD) | X86_DR7_LE | X86_DR7_GE);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);

        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        TrapExpect.Ctx.rip = TrapCtx.Ctx.rip; /// @todo fixme
        Bs3RegSetDr7(0);
        uDr6Expect = X86_DR6_INIT_VAL | X86_DR6_B0;
        if (g_enmCpuVendor == BS3CPUVENDOR_INTEL && bTestMode != BS3_MODE_RM)
            uDr6Expect = X86_DR6_INIT_VAL;
        bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &TrapExpect.Ctx, X86_XCPT_DB, 0, 0, uDr6Expect,
                                               cbIretFrameSame, uHandlerRspIntDb);

        if (!BS3_MODE_IS_RM_OR_V86(bTestMode))
        {
            /*
             * Test #8: Data breakpoint on SS GDT entry.  Half weird!
             * Note! Intel loses the B1 status, see test #6.
             */
            g_usBs3TestStep++;
            *BS3_XPTR_GET(uint32_t, StackXptr) = (Ctx.ss & X86_SEL_RPL) | BS3_SEL_SPARE_00;
            Bs3GdteSpare00 = Bs3Gdt[Ctx.ss / sizeof(Bs3Gdt[0])];

            Bs3RegSetDr1(Bs3SelPtrToFlat(&Bs3GdteSpare00));
            Bs3RegSetDr7(X86_DR7_L1 | X86_DR7_G1 | X86_DR7_RW(1, X86_DR7_RW_RW) | X86_DR7_LEN(1, X86_DR7_LEN_DWORD));
            Bs3RegSetDr6(X86_DR6_INIT_VAL);

            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            TrapExpect.Ctx.rip = TrapCtx.Ctx.rip; /// @todo fixme
            Bs3RegSetDr7(0);
            uDr6Expect = g_enmCpuVendor == BS3CPUVENDOR_INTEL ? X86_DR6_INIT_VAL : X86_DR6_INIT_VAL | X86_DR6_B1;
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &TrapExpect.Ctx, X86_XCPT_DB, 0, 0, uDr6Expect,
                                                   cbIretFrameSame, uHandlerRspIntDb);

            /*
             * Test #9: Same as above, but with the LE and GE flags set.
             */
            g_usBs3TestStep++;
            *BS3_XPTR_GET(uint32_t, StackXptr) = (Ctx.ss & X86_SEL_RPL) | BS3_SEL_SPARE_00;
            Bs3GdteSpare00 = Bs3Gdt[Ctx.ss / sizeof(Bs3Gdt[0])];

            Bs3RegSetDr1(Bs3SelPtrToFlat(&Bs3GdteSpare00));
            Bs3RegSetDr7(X86_DR7_L1 | X86_DR7_G1 | X86_DR7_RW(1, X86_DR7_RW_RW) | X86_DR7_LEN(1, X86_DR7_LEN_DWORD) | X86_DR7_LE | X86_DR7_GE);
            Bs3RegSetDr6(X86_DR6_INIT_VAL);

            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            TrapExpect.Ctx.rip = TrapCtx.Ctx.rip; /// @todo fixme
            Bs3RegSetDr7(0);
            uDr6Expect = g_enmCpuVendor == BS3CPUVENDOR_INTEL ? X86_DR6_INIT_VAL : X86_DR6_INIT_VAL | X86_DR6_B1;
            bs3CpuWeird1_CompareDbgInhibitRingXfer(&TrapCtx, &TrapExpect.Ctx, X86_XCPT_DB, 0, 0, uDr6Expect,
                                                   cbIretFrameSame, uHandlerRspIntDb);
        }

        /*
         * Cleanup.
         */
        Bs3RegSetDr0(0);
        Bs3RegSetDr1(0);
        Bs3RegSetDr2(0);
        Bs3RegSetDr3(0);
        Bs3RegSetDr6(X86_DR6_INIT_VAL);
        Bs3RegSetDr7(0);
    }
    Bs3TrapSetDpl(bIntGate, bSavedDpl);
    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_FAR_NM(bs3CpuWeird1_DbgInhibitRingXfer)(uint8_t bMode)
{
    if (BS3_MODE_IS_V86(bMode))
        switch (bMode)
        {
            /** @todo some busted stack stuff with the 16-bit guys.  Also, if VME is
             *        enabled, we're probably not able to do any sensible testing. */
            case BS3_MODE_PP16_V86:
            case BS3_MODE_PE16_V86:
            case BS3_MODE_PAE16_V86:
                return BS3TESTDOMODE_SKIPPED;
        }
    //if (bMode != BS3_MODE_PE16_V86) return BS3TESTDOMODE_SKIPPED;
    //if (bMode != BS3_MODE_PAEV86) return BS3TESTDOMODE_SKIPPED;

    bs3CpuWeird1_SetGlobals(bMode);

    /** @todo test sysenter and syscall too. */
    /** @todo test INTO. */
    /** @todo test all V8086 software INT delivery modes (currently only 4 and 1). */

    /* Note! Both ICEBP and BOUND has be checked cursorily and found not to be affected. */
    if (BS3_MODE_IS_16BIT_CODE(bMode))
    {
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x80, 2, 2, bs3CpuWeird1_InhibitedInt80_c16, bs3CpuWeird1_InhibitedInt80_int80_c16);
        if (!BS3_MODE_IS_V86(bMode) || !g_fVME)
        {
            /** @todo explain why these GURU     */
            bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 2, 2, bs3CpuWeird1_InhibitedInt3_c16,  bs3CpuWeird1_InhibitedInt3_int3_c16);
            bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 1, 2, bs3CpuWeird1_InhibitedBp_c16,    bs3CpuWeird1_InhibitedBp_int3_c16);
        }
    }
    else if (BS3_MODE_IS_32BIT_CODE(bMode))
    {
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x80, 2, 4, bs3CpuWeird1_InhibitedInt80_c32, bs3CpuWeird1_InhibitedInt80_int80_c32);
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 2, 4, bs3CpuWeird1_InhibitedInt3_c32,  bs3CpuWeird1_InhibitedInt3_int3_c32);
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 1, 4, bs3CpuWeird1_InhibitedBp_c32,    bs3CpuWeird1_InhibitedBp_int3_c32);
    }
    else
    {
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x80, 2, 0, bs3CpuWeird1_InhibitedInt80_c64, bs3CpuWeird1_InhibitedInt80_int80_c64);
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 2, 0, bs3CpuWeird1_InhibitedInt3_c64,  bs3CpuWeird1_InhibitedInt3_int3_c64);
        bs3CpuWeird1_DbgInhibitRingXfer_Worker(bMode, 0x03, 1, 0, bs3CpuWeird1_InhibitedBp_c64,    bs3CpuWeird1_InhibitedBp_int3_c64);
    }

    return 0;
}


