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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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
        else bs3CpuBasic2_FailedF(a_szName "=" a_szFmt " expected " a_szFmt, (a_Actual), (a_Expected)); \
    } while (0)

#ifdef BS3_INSTANTIATING_MODE
# undef MyBs3Idt
# undef MY_SYS_SEL_R0_CS
# undef MY_SYS_SEL_R0_CS_CNF
# undef MY_SYS_SEL_R0_DS
# undef MY_SYS_SEL_R0_SS
# if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              Bs3Idt16
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS16
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS16_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS16
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_SS16
# elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              Bs3Idt32
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS32
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS32_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS32
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_SS32
# elif BS3_MODE_IS_64BIT_SYS(TMPL_MODE)
#  define MyBs3Idt              Bs3Idt64
#  define MY_SYS_SEL_R0_CS      BS3_SEL_R0_CS64
#  define MY_SYS_SEL_R0_CS_CNF  BS3_SEL_R0_CS64_CNF
#  define MY_SYS_SEL_R0_DS      BS3_SEL_R0_DS64
#  define MY_SYS_SEL_R0_SS      BS3_SEL_R0_DS64
# else
#  error "TMPL_MODE"
# endif
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
typedef struct BS3CB2INVLDESCTYPE
{
    uint8_t u4Type;
    uint8_t u1DescType;
} BS3CB2INVLDESCTYPE;

typedef struct BS3CB2SIDTSGDT
{
    FPFNBS3FAR  fpfnWorker;
    uint8_t     cbInstr;
    bool        fSs;
    uint8_t     bMode;
} BS3CB2SIDTSGDT;
#endif


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
extern FNBS3FAR     bs3CpuBasic2_Int80;
extern FNBS3FAR     bs3CpuBasic2_Int81;
extern FNBS3FAR     bs3CpuBasic2_Int82;
extern FNBS3FAR     bs3CpuBasic2_Int83;
extern FNBS3FAR     bs3CpuBasic2_ud2;
# define            g_bs3CpuBasic2_ud2_FlatAddr BS3_DATA_NM(g_bs3CpuBasic2_ud2_FlatAddr)
extern uint32_t     g_bs3CpuBasic2_ud2_FlatAddr;

extern FNBS3FAR     bs3CpuBasic2_sidt_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sidt_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sidt_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sidt_ss_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sidt_ss_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sidt_rexw_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_ss_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_ss_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sidt_opsize_rexw_bx_ud2_c64;

extern FNBS3FAR     bs3CpuBasic2_sgdt_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sgdt_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sgdt_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sgdt_ss_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sgdt_ss_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sgdt_rexw_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_bx_ud2_c64;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_ss_bx_ud2_c16;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_ss_bx_ud2_c32;
extern FNBS3FAR     bs3CpuBasic2_sgdt_opsize_rexw_bx_ud2_c64;
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
# define                    g_pszTestMode   BS3_CMN_NM(g_pszTestMode)
static const char BS3_FAR  *g_pszTestMode = (const char *)1;
# define                    g_bTestMode     BS3_CMN_NM(g_bTestMode)
static uint8_t              g_bTestMode = 1;
# define                    g_f16BitSys     BS3_CMN_NM(g_f16BitSys)
static bool                 g_f16BitSys = 1;


static BS3CB2SIDTSGDT const g_aSidtWorkers[] =
{
    { bs3CpuBasic2_sidt_bx_ud2_c16,             3, false,   BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sidt_ss_bx_ud2_c16,          4, true,    BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sidt_opsize_bx_ud2_c16,      4, false,   BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sidt_opsize_ss_bx_ud2_c16,   5, true,    BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sidt_bx_ud2_c32,             3, false,   BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sidt_ss_bx_ud2_c32,          4, true,    BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sidt_opsize_bx_ud2_c32,      4, false,   BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sidt_opsize_ss_bx_ud2_c32,   5, true,    BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sidt_bx_ud2_c64,             3, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sidt_rexw_bx_ud2_c64,        4, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sidt_opsize_bx_ud2_c64,      4, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sidt_opsize_rexw_bx_ud2_c64, 5, false,   BS3_MODE_CODE_64 },
};


static BS3CB2SIDTSGDT const g_aSgdtWorkers[] =
{
    { bs3CpuBasic2_sgdt_bx_ud2_c16,             3, false,   BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sgdt_ss_bx_ud2_c16,          4, true,    BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sgdt_opsize_bx_ud2_c16,      4, false,   BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sgdt_opsize_ss_bx_ud2_c16,   5, true,    BS3_MODE_CODE_16 | BS3_MODE_CODE_V86 },
    { bs3CpuBasic2_sgdt_bx_ud2_c32,             3, false,   BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sgdt_ss_bx_ud2_c32,          4, true,    BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sgdt_opsize_bx_ud2_c32,      4, false,   BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sgdt_opsize_ss_bx_ud2_c32,   5, true,    BS3_MODE_CODE_32 },
    { bs3CpuBasic2_sgdt_bx_ud2_c64,             3, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sgdt_rexw_bx_ud2_c64,        4, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sgdt_opsize_bx_ud2_c64,      4, false,   BS3_MODE_CODE_64 },
    { bs3CpuBasic2_sgdt_opsize_rexw_bx_ud2_c64, 5, false,   BS3_MODE_CODE_64 },
};


/** Table containing invalid CS selector types. */
static const BS3CB2INVLDESCTYPE g_aInvalidCsTypes[] =
{
    {   X86_SEL_TYPE_RO,            1 },
    {   X86_SEL_TYPE_RO_ACC,        1 },
    {   X86_SEL_TYPE_RW,            1 },
    {   X86_SEL_TYPE_RW_ACC,        1 },
    {   X86_SEL_TYPE_RO_DOWN,       1 },
    {   X86_SEL_TYPE_RO_DOWN_ACC,   1 },
    {   X86_SEL_TYPE_RW_DOWN,       1 },
    {   X86_SEL_TYPE_RW_DOWN_ACC,   1 },
    {   0,                          0 },
    {   1,                          0 },
    {   2,                          0 },
    {   3,                          0 },
    {   4,                          0 },
    {   5,                          0 },
    {   6,                          0 },
    {   7,                          0 },
    {   8,                          0 },
    {   9,                          0 },
    {   10,                         0 },
    {   11,                         0 },
    {   12,                         0 },
    {   13,                         0 },
    {   14,                         0 },
    {   15,                         0 },
};

/** Table containing invalid SS selector types. */
static const BS3CB2INVLDESCTYPE g_aInvalidSsTypes[] =
{
    {   X86_SEL_TYPE_EO,            1 },
    {   X86_SEL_TYPE_EO_ACC,        1 },
    {   X86_SEL_TYPE_ER,            1 },
    {   X86_SEL_TYPE_ER_ACC,        1 },
    {   X86_SEL_TYPE_EO_CONF,       1 },
    {   X86_SEL_TYPE_EO_CONF_ACC,   1 },
    {   X86_SEL_TYPE_ER_CONF,       1 },
    {   X86_SEL_TYPE_ER_CONF_ACC,   1 },
    {   0,                          0 },
    {   1,                          0 },
    {   2,                          0 },
    {   3,                          0 },
    {   4,                          0 },
    {   5,                          0 },
    {   6,                          0 },
    {   7,                          0 },
    {   8,                          0 },
    {   9,                          0 },
    {   10,                         0 },
    {   11,                         0 },
    {   12,                         0 },
    {   13,                         0 },
    {   14,                         0 },
    {   15,                         0 },
};

#endif /* BS3_INSTANTIATING_CMN - global */

#ifdef BS3_INSTANTIATING_CMN

/**
 * Wrapper around Bs3TestFailedF that prefixes the error with g_usBs3TestStep
 * and g_pszTestMode.
 */
# define bs3CpuBasic2_FailedF BS3_CMN_NM(bs3CpuBasic2_FailedF)
BS3_DECL_NEAR(void) bs3CpuBasic2_FailedF(const char *pszFormat, ...)
{
    va_list va;

    char szTmp[168];
    va_start(va, pszFormat);
    Bs3StrPrintfV(szTmp, sizeof(szTmp), pszFormat, va);
    va_end(va);

    Bs3TestFailedF("%u - %s: %s", g_usBs3TestStep, g_pszTestMode, szTmp);
}


/**
 * Compares trap stuff.
 */
# define bs3CpuBasic2_CompareIntCtx1 BS3_CMN_NM(bs3CpuBasic2_CompareIntCtx1)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareIntCtx1(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint8_t bXcpt)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 2 /*int xx*/, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, g_pszTestMode, g_usBs3TestStep);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
#if 1
        Bs3TestPrintf("Halting: g_uBs3CpuDetected=%#x\n", g_uBs3CpuDetected);
        Bs3TestPrintf("Halting in CompareTrapCtx1: bXcpt=%#x\n", bXcpt);
        ASMHalt();
#endif
    }
}


/**
 * Compares trap stuff.
 */
# define bs3CpuBasic2_CompareTrapCtx2 BS3_CMN_NM(bs3CpuBasic2_CompareTrapCtx2)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareTrapCtx2(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t cbIpAdjust,
                                                 uint8_t bXcpt, uint16_t uHandlerCs)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX64", pTrapCtx->uErrCd,       0);
    CHECK_MEMBER("uHandlerCs", "%#06x", pTrapCtx->uHandlerCs,   uHandlerCs);
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, cbIpAdjust, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/, g_pszTestMode, g_usBs3TestStep);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
#if 1
        Bs3TestPrintf("Halting: g_uBs3CpuDetected=%#x\n", g_uBs3CpuDetected);
        Bs3TestPrintf("Halting in CompareTrapCtx2: bXcpt=%#x\n", bXcpt);
        ASMHalt();
#endif
    }
}

/**
 * Compares a CPU trap.
 */
# define bs3CpuBasic2_CompareCpuTrapCtx BS3_CMN_NM(bs3CpuBasic2_CompareCpuTrapCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareCpuTrapCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd,
                                                   uint8_t bXcpt, bool f486ResumeFlagHint)
{
    uint16_t const cErrorsBefore = Bs3TestSubErrorCount();
    uint32_t fExtraEfl;

    CHECK_MEMBER("bXcpt",   "%#04x",    pTrapCtx->bXcpt,        bXcpt);
    CHECK_MEMBER("bErrCd",  "%#06RX16", (uint16_t)pTrapCtx->uErrCd, (uint16_t)uErrCd); /* 486 only writes a word */

    fExtraEfl = X86_EFL_RF;
    if (   g_f16BitSys
        || (   !f486ResumeFlagHint
            && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) <= BS3CPU_80486 ) )
        fExtraEfl = 0;
    else
        fExtraEfl = X86_EFL_RF;
#if 0 /** @todo Running on an AMD Phenom II X6 1100T under AMD-V I'm not getting good X86_EFL_RF results.  Enable this to get on with other work.  */
    fExtraEfl = pTrapCtx->Ctx.rflags.u32 & X86_EFL_RF;
#endif
    Bs3TestCheckRegCtxEx(&pTrapCtx->Ctx, pStartCtx, 0 /*cbIpAdjust*/, 0 /*cbSpAdjust*/, fExtraEfl, g_pszTestMode, g_usBs3TestStep);
    if (Bs3TestSubErrorCount() != cErrorsBefore)
    {
        Bs3TrapPrintFrame(pTrapCtx);
#if 1
        Bs3TestPrintf("Halting: g_uBs3CpuDetected=%#x\n", g_uBs3CpuDetected);
        Bs3TestPrintf("Halting: bXcpt=%#x uErrCd=%#x\n", bXcpt, uErrCd);
        ASMHalt();
#endif
    }
}


/**
 * Compares \#GP trap.
 */
# define bs3CpuBasic2_CompareGpCtx BS3_CMN_NM(bs3CpuBasic2_CompareGpCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareGpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd)
{
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, uErrCd, X86_XCPT_GP, true /*f486ResumeFlagHint*/);
}

/**
 * Compares \#NP trap.
 */
# define bs3CpuBasic2_CompareNpCtx BS3_CMN_NM(bs3CpuBasic2_CompareNpCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareNpCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd)
{
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, uErrCd, X86_XCPT_NP, true /*f486ResumeFlagHint*/);
}

/**
 * Compares \#SS trap.
 */
# define bs3CpuBasic2_CompareSsCtx BS3_CMN_NM(bs3CpuBasic2_CompareSsCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareSsCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd, bool f486ResumeFlagHint)
{
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, uErrCd, X86_XCPT_SS, f486ResumeFlagHint);
}

/**
 * Compares \#TS trap.
 */
# define bs3CpuBasic2_CompareTsCtx BS3_CMN_NM(bs3CpuBasic2_CompareTsCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareTsCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx, uint16_t uErrCd)
{
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, uErrCd, X86_XCPT_TS, false /*f486ResumeFlagHint*/);
}

/**
 * Compares \#PF trap.
 */
# define bs3CpuBasic2_ComparePfCtx BS3_CMN_NM(bs3CpuBasic2_ComparePfCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_ComparePfCtx(PCBS3TRAPFRAME pTrapCtx, PBS3REGCTX pStartCtx, uint16_t uErrCd, uint64_t uCr2Expected)
{
    uint64_t const uCr2Saved     = pStartCtx->cr2.u;
    pStartCtx->cr2.u = uCr2Expected;
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, uErrCd, X86_XCPT_PF, true /*f486ResumeFlagHint*/);
    pStartCtx->cr2.u = uCr2Saved;
}

/**
 * Compares \#UD trap.
 */
# define bs3CpuBasic2_CompareUdCtx BS3_CMN_NM(bs3CpuBasic2_CompareUdCtx)
BS3_DECL_NEAR(void) bs3CpuBasic2_CompareUdCtx(PCBS3TRAPFRAME pTrapCtx, PCBS3REGCTX pStartCtx)
{
    bs3CpuBasic2_CompareCpuTrapCtx(pTrapCtx, pStartCtx, 0 /*no error code*/, X86_XCPT_UD, true /*f486ResumeFlagHint*/);
}


# define bs3CpuBasic2_RaiseXcpt1Common BS3_CMN_NM(bs3CpuBasic2_RaiseXcpt1Common)
BS3_DECL_NEAR(void) bs3CpuBasic2_RaiseXcpt1Common(uint16_t const uSysR0Cs, uint16_t const uSysR0CsConf, uint16_t const uSysR0Ss,
                                                  PX86DESC const paIdt, unsigned const cIdteShift)
{
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx80;
    BS3REGCTX       Ctx81;
    BS3REGCTX       Ctx82;
    BS3REGCTX       Ctx83;
    BS3REGCTX       CtxTmp;
    BS3REGCTX       CtxTmp2;
    PBS3REGCTX      apCtx8x[4];
    unsigned        iCtx;
    unsigned        iRing;
    unsigned        iDpl;
    unsigned        iRpl;
    unsigned        i, j, k;
    uint32_t        uExpected;
    bool const      f486Plus = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486;
# if TMPL_BITS == 16
    bool const      f386Plus = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386;
    bool const      f286     = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) == BS3CPU_80286;
# else
    bool const      f286     = false;
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
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));
    Bs3MemZero(&Ctx80, sizeof(Ctx80));
    Bs3MemZero(&Ctx81, sizeof(Ctx81));
    Bs3MemZero(&Ctx82, sizeof(Ctx82));
    Bs3MemZero(&Ctx83, sizeof(Ctx83));
    Bs3MemZero(&CtxTmp, sizeof(CtxTmp));
    Bs3MemZero(&CtxTmp2, sizeof(CtxTmp2));

    /* Context array. */
    apCtx8x[0] = &Ctx80;
    apCtx8x[1] = &Ctx81;
    apCtx8x[2] = &Ctx82;
    apCtx8x[3] = &Ctx83;

# if TMPL_BITS != 16
    /* Allocate memory for playing around with the IDT. */
    pbIdtCopyAlloc = NULL;
    if (BS3_MODE_IS_PAGED(g_bTestMode))
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
    Ctx80.rip.u  = (uintptr_t)BS3_FP_OFF(&bs3CpuBasic2_Int80);
# if TMPL_BITS == 16
    Ctx80.cs = BS3_MODE_IS_RM_OR_V86(g_bTestMode) ? BS3_SEL_TEXT16 : BS3_SEL_R0_CS16;
# elif TMPL_BITS == 32
    g_uBs3TrapEipHint = Ctx80.rip.u32;
# endif
    Bs3MemCpy(&Ctx81, &Ctx80, sizeof(Ctx80));
    Ctx81.rip.u  = (uintptr_t)BS3_FP_OFF(&bs3CpuBasic2_Int81);
    Bs3MemCpy(&Ctx82, &Ctx80, sizeof(Ctx80));
    Ctx82.rip.u  = (uintptr_t)BS3_FP_OFF(&bs3CpuBasic2_Int82);
    Bs3MemCpy(&Ctx83, &Ctx80, sizeof(Ctx80));
    Ctx83.rip.u  = (uintptr_t)BS3_FP_OFF(&bs3CpuBasic2_Int83);

    /*
     * Check that all the above gates work from ring-0.
     */
    for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
    {
        g_usBs3TestStep = iCtx;
# if TMPL_BITS == 32
        g_uBs3TrapEipHint = apCtx8x[iCtx]->rip.u32;
# endif
        Bs3TrapSetJmpAndRestore(apCtx8x[iCtx], &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, apCtx8x[iCtx], 0x80+iCtx /*bXcpt*/);
    }

    /*
     * Check that the gate DPL checks works.
     */
    g_usBs3TestStep = 100;
    for (iRing = 0; iRing <= 3; iRing++)
    {
        for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
        {
            Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
# if TMPL_BITS == 32
            g_uBs3TrapEipHint = CtxTmp.rip.u32;
# endif
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            if (iCtx < iRing)
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
            else
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/);
            g_usBs3TestStep++;
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
    g_usBs3TestStep = 1000;
    for (i = 0; i <= 3; i++)
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
# if TMPL_BITS == 32
                g_uBs3TrapEipHint = apCtx8x[iCtx]->rip.u32;
# endif
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);

                for (j = 0; j <= 3; j++)
                {
                    uint16_t const uCs = (uSysR0Cs | j) + (i << BS3_SEL_RING_SHIFT);
                    for (k = 0; k < 2; k++)
                    {
                        g_usBs3TestStep++;
                        /*Bs3TestPrintf("g_usBs3TestStep=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", g_usBs3TestStep,  iCtx,  iRing, i, uCs);*/
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = k;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        /*Bs3TrapPrintFrame(&TrapCtx);*/
                        if (iCtx < iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                        else if (k == 0)
                            bs3CpuBasic2_CompareNpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                        else if (i > iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL);
                        else
                        {
                            uint16_t uExpectedCs = uCs & X86_SEL_MASK_OFF_RPL;
                            if (i <= iCtx && i <= iRing)
                                uExpectedCs |= i;
                            bs3CpuBasic2_CompareTrapCtx2(&TrapCtx, &CtxTmp, 2 /*int 8xh*/, 0x80 + iCtx /*bXcpt*/, uExpectedCs);
                        }
                    }
                }

                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 1;
            }
        }
    }
    BS3_ASSERT(g_usBs3TestStep < 1600);

    /*
     * Various CS and SS related faults
     *
     * We temporarily reconfigure gate 80 and 83 with new CS selectors, the
     * latter have a CS.DPL of 2 for testing ring transisions and SS loading
     * without making it impossible to handle faults.
     */
    g_usBs3TestStep = 1600;
    Bs3GdteTestPage00 = Bs3Gdt[uSysR0Cs >> X86_SEL_SHIFT];
    Bs3GdteTestPage00.Gen.u1Present = 0;
    Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
    paIdt[0x80 << cIdteShift].Gate.u16Sel = BS3_SEL_TEST_PAGE_00;

    /* CS.PRESENT = 0 */
    Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00);
    if (Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
        bs3CpuBasic2_FailedF("selector was accessed");
    g_usBs3TestStep++;

    /* Check that GATE.DPL is checked before CS.PRESENT. */
    for (iRing = 1; iRing < 4; iRing++)
    {
        Bs3MemCpy(&CtxTmp, &Ctx80, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, (0x80 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
        if (Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
            bs3CpuBasic2_FailedF("selector was accessed");
        g_usBs3TestStep++;
    }

    /* CS.DPL mismatch takes precedence over CS.PRESENT = 0. */
    Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
    Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00);
    if (Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
        bs3CpuBasic2_FailedF("CS selector was accessed");
    g_usBs3TestStep++;
    for (iDpl = 1; iDpl < 4; iDpl++)
    {
        Bs3GdteTestPage00.Gen.u2Dpl = iDpl;
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00);
        if (Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
            bs3CpuBasic2_FailedF("CS selector was accessed");
        g_usBs3TestStep++;
    }

    /* 1608: Check all the invalid CS selector types alone. */
    Bs3GdteTestPage00 = Bs3Gdt[uSysR0Cs >> X86_SEL_SHIFT];
    for (i = 0; i < RT_ELEMENTS(g_aInvalidCsTypes); i++)
    {
        Bs3GdteTestPage00.Gen.u4Type     = g_aInvalidCsTypes[i].u4Type;
        Bs3GdteTestPage00.Gen.u1DescType = g_aInvalidCsTypes[i].u1DescType;
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00);
        if (Bs3GdteTestPage00.Gen.u4Type != g_aInvalidCsTypes[i].u4Type)
            bs3CpuBasic2_FailedF("Invalid CS type %#x/%u -> %#x/%u\n",
                                 g_aInvalidCsTypes[i].u4Type, g_aInvalidCsTypes[i].u1DescType,
                                 Bs3GdteTestPage00.Gen.u4Type, Bs3GdteTestPage00.Gen.u1DescType);
        g_usBs3TestStep++;

        /* Incorrect CS.TYPE takes precedence over CS.PRESENT = 0. */
        Bs3GdteTestPage00.Gen.u1Present = 0;
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx80, BS3_SEL_TEST_PAGE_00);
        Bs3GdteTestPage00.Gen.u1Present = 1;
        g_usBs3TestStep++;
    }

    /* Fix CS again. */
    Bs3GdteTestPage00 = Bs3Gdt[uSysR0Cs >> X86_SEL_SHIFT];

    /* 1632: Test SS. */
    if (!BS3_MODE_IS_64BIT_SYS(g_bTestMode))
    {
        uint16_t BS3_FAR *puTssSs2    = BS3_MODE_IS_16BIT_SYS(g_bTestMode) ? &Bs3Tss16.ss2 : &Bs3Tss32.ss2;
        uint16_t const    uSavedSs2   = *puTssSs2;
        X86DESC const     SavedGate83 = paIdt[0x83 << cIdteShift];

        /* Make the handler execute in ring-2. */
        Bs3GdteTestPage02 = Bs3Gdt[(uSysR0Cs + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        Bs3GdteTestPage02.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        paIdt[0x83 << cIdteShift].Gate.u16Sel = BS3_SEL_TEST_PAGE_02 | 2;

        Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, 3); /* yeah, from 3 so SS:xSP is reloaded. */
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83);
        if (!(Bs3GdteTestPage02.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
            bs3CpuBasic2_FailedF("CS selector was not access");
        g_usBs3TestStep++;

        /* Create a SS.DPL=2 stack segment and check that SS2.RPL matters and
           that we get #SS if the selector isn't present. */
        i = 0; /* used for cycling thru invalid CS types */
        for (k = 0; k < 10; k++)
        {
            /* k=0: present,
               k=1: not-present,
               k=2: present but very low limit,
               k=3: not-present, low limit.
               k=4: present, read-only.
               k=5: not-present, read-only.
               k=6: present, code-selector.
               k=7: not-present, code-selector.
               k=8: present, read-write / no access + system (=LDT).
               k=9: not-present, read-write / no access + system (=LDT).
               */
            Bs3GdteTestPage03 = Bs3Gdt[(uSysR0Ss + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
            Bs3GdteTestPage03.Gen.u1Present  = !(k & 1);
            if (k >= 8)
            {
                Bs3GdteTestPage03.Gen.u1DescType = 0; /* system */
                Bs3GdteTestPage03.Gen.u4Type = X86_SEL_TYPE_RW; /* = LDT */
            }
            else if (k >= 6)
                Bs3GdteTestPage03.Gen.u4Type = X86_SEL_TYPE_ER;
            else if (k >= 4)
                Bs3GdteTestPage03.Gen.u4Type = X86_SEL_TYPE_RO;
            else if (k >= 2)
            {
                Bs3GdteTestPage03.Gen.u16LimitLow   = 0x400;
                Bs3GdteTestPage03.Gen.u4LimitHigh   = 0;
                Bs3GdteTestPage03.Gen.u1Granularity = 0;
            }

            for (iDpl = 0; iDpl < 4; iDpl++)
            {
                Bs3GdteTestPage03.Gen.u2Dpl = iDpl;

                for (iRpl = 0; iRpl < 4; iRpl++)
                {
                    *puTssSs2 = BS3_SEL_TEST_PAGE_03 | iRpl;
                    //Bs3TestPrintf("k=%u iDpl=%u iRpl=%u step=%u\n", k, iDpl, iRpl, g_usBs3TestStep);
                    Bs3GdteTestPage02.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
                    Bs3GdteTestPage03.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    if (iRpl != 2 || iRpl != iDpl || k >= 4)
                        bs3CpuBasic2_CompareTsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03);
                    else if (k != 0)
                        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03,
                                                  k == 2 /*f486ResumeFlagHint*/);
                    else
                    {
                        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83);
                        if (TrapCtx.uHandlerSs != (BS3_SEL_TEST_PAGE_03 | 2))
                            bs3CpuBasic2_FailedF("uHandlerSs=%#x expected %#x\n", TrapCtx.uHandlerSs, BS3_SEL_TEST_PAGE_03 | 2);
                    }
                    if (!(Bs3GdteTestPage02.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                        bs3CpuBasic2_FailedF("CS selector was not access");
                    if (   TrapCtx.bXcpt == 0x83
                        || (TrapCtx.bXcpt == X86_XCPT_SS && k == 2) )
                    {
                        if (!(Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                            bs3CpuBasic2_FailedF("SS selector was not accessed");
                    }
                    else if (Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
                        bs3CpuBasic2_FailedF("SS selector was accessed");
                    g_usBs3TestStep++;

                    /* +1: Modify the gate DPL to check that this is checked before SS.DPL and SS.PRESENT. */
                    paIdt[0x83 << cIdteShift].Gate.u2Dpl = 2;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, (0x83 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                    paIdt[0x83 << cIdteShift].Gate.u2Dpl = 3;
                    g_usBs3TestStep++;

                    /* +2: Check the the CS.DPL check is done before the SS ones. Restoring the
                           ring-0 INT 83 context triggers the CS.DPL < CPL check. */
                    Bs3TrapSetJmpAndRestore(&Ctx83, &TrapCtx);
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx83, BS3_SEL_TEST_PAGE_02);
                    g_usBs3TestStep++;

                    /* +3: Now mark the CS selector not present and check that that also triggers before SS stuff. */
                    Bs3GdteTestPage02.Gen.u1Present = 0;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    bs3CpuBasic2_CompareNpCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_02);
                    Bs3GdteTestPage02.Gen.u1Present = 1;
                    g_usBs3TestStep++;

                    /* +4: Make the CS selector some invalid type and check it triggers before SS stuff. */
                    Bs3GdteTestPage02.Gen.u4Type = g_aInvalidCsTypes[i].u4Type;
                    Bs3GdteTestPage02.Gen.u1DescType = g_aInvalidCsTypes[i].u1DescType;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_02);
                    Bs3GdteTestPage02.Gen.u4Type = X86_SEL_TYPE_ER_ACC;
                    Bs3GdteTestPage02.Gen.u1DescType = 1;
                    g_usBs3TestStep++;

                    /* +5: Now, make the CS selector limit too small and that it triggers after SS trouble.
                           The 286 had a simpler approach to these GP(0). */
                    Bs3GdteTestPage02.Gen.u16LimitLow = 0;
                    Bs3GdteTestPage02.Gen.u4LimitHigh = 0;
                    Bs3GdteTestPage02.Gen.u1Granularity = 0;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    if (f286)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, 0 /*uErrCd*/);
                    else if (iRpl != 2 || iRpl != iDpl || k >= 4)
                        bs3CpuBasic2_CompareTsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03);
                    else if (k != 0)
                        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03, k == 2 /*f486ResumeFlagHint*/);
                    else
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, 0 /*uErrCd*/);
                    Bs3GdteTestPage02 = Bs3Gdt[(uSysR0Cs + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
                    g_usBs3TestStep++;
                }
            }
        }

        /* Check all the invalid SS selector types alone. */
        Bs3GdteTestPage02 = Bs3Gdt[(uSysR0Cs + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        Bs3GdteTestPage03 = Bs3Gdt[(uSysR0Ss + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        *puTssSs2 = BS3_SEL_TEST_PAGE_03 | 2;
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83);
        g_usBs3TestStep++;
        for (i = 0; i < RT_ELEMENTS(g_aInvalidSsTypes); i++)
        {
            Bs3GdteTestPage03.Gen.u4Type     = g_aInvalidSsTypes[i].u4Type;
            Bs3GdteTestPage03.Gen.u1DescType = g_aInvalidSsTypes[i].u1DescType;
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            bs3CpuBasic2_CompareTsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03);
            if (Bs3GdteTestPage03.Gen.u4Type != g_aInvalidSsTypes[i].u4Type)
                bs3CpuBasic2_FailedF("Invalid SS type %#x/%u -> %#x/%u\n",
                                     g_aInvalidSsTypes[i].u4Type, g_aInvalidSsTypes[i].u1DescType,
                                     Bs3GdteTestPage03.Gen.u4Type, Bs3GdteTestPage03.Gen.u1DescType);
            g_usBs3TestStep++;
        }

        /*
         * Continue the SS experiments with a expand down segment.  We'll use
         * the same setup as we already have with gate 83h being DPL and
         * having CS.DPL=2.
         *
         * Expand down segments are weird. The valid area is practically speaking
         * reversed.  So, a 16-bit segment with a limit of 0x6000 will have valid
         * addresses from 0xffff thru 0x6001.
         *
         * So, with expand down segments we can more easily cut partially into the
         * pushing of the iret frame and trigger more interesting behavior than
         * with regular "expand up" segments where the whole pushing area is either
         * all fine or not not fine.
         */
        Bs3GdteTestPage02 = Bs3Gdt[(uSysR0Cs + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        Bs3GdteTestPage03 = Bs3Gdt[(uSysR0Ss + (2 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        Bs3GdteTestPage03.Gen.u2Dpl = 2;
        Bs3GdteTestPage03.Gen.u4Type = X86_SEL_TYPE_RW_DOWN;
        *puTssSs2 = BS3_SEL_TEST_PAGE_03 | 2;

        /* First test, limit = max --> no bytes accessible --> #GP */
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03, true /*f486ResumeFlagHint*/);

        /* Second test, limit = 0 --> all by zero byte accessible --> works */
        Bs3GdteTestPage03.Gen.u16LimitLow = 0;
        Bs3GdteTestPage03.Gen.u4LimitHigh = 0;
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83);

        /* Modify the gate handler to be a dummy that immediately does UD2
           and triggers #UD, then advance the limit down till we get the #UD. */
        Bs3GdteTestPage03.Gen.u1Granularity = 0;

        Bs3MemCpy(&CtxTmp2, &CtxTmp, sizeof(CtxTmp2));  /* #UD result context */
        if (g_f16BitSys)
        {
            CtxTmp2.rip.u = g_bs3CpuBasic2_ud2_FlatAddr - BS3_ADDR_BS3TEXT16;
            Bs3Trap16SetGate(0x83, X86_SEL_TYPE_SYS_286_INT_GATE, 3, BS3_SEL_TEST_PAGE_02, CtxTmp2.rip.u16, 0 /*cParams*/);
            CtxTmp2.rsp.u = Bs3Tss16.sp2 - 2*5;
        }
        else
        {
            CtxTmp2.rip.u = g_bs3CpuBasic2_ud2_FlatAddr;
            Bs3Trap32SetGate(0x83, X86_SEL_TYPE_SYS_386_INT_GATE, 3, BS3_SEL_TEST_PAGE_02, CtxTmp2.rip.u32, 0 /*cParams*/);
            CtxTmp2.rsp.u = Bs3Tss32.esp2 - 4*5;
        }
        CtxTmp2.bMode = g_bTestMode; /* g_bBs3CurrentMode not changed by the UD2 handler. */
        CtxTmp2.cs = BS3_SEL_TEST_PAGE_02 | 2;
        CtxTmp2.ss = BS3_SEL_TEST_PAGE_03 | 2;
        CtxTmp2.bCpl = 2;

        /* test run. */
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxTmp2);
        g_usBs3TestStep++;

        /* Real run. */
        i = (g_f16BitSys ? 2 : 4) * 6 + 1;
        while (i-- > 0)
        {
            Bs3GdteTestPage03.Gen.u16LimitLow = CtxTmp2.rsp.u16 + i - 1;
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            if (i > 0)
                bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, BS3_SEL_TEST_PAGE_03, true /*f486ResumeFlagHint*/);
            else
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxTmp2);
            g_usBs3TestStep++;
        }

        /* Do a run where we do the same-ring kind of access.  */
        Bs3RegCtxConvertToRingX(&CtxTmp, 2);
        if (g_f16BitSys)
        {
            CtxTmp2.rsp.u32 = CtxTmp.rsp.u32 - 2*3;
            i = 2*3 - 1;
        }
        else
        {
            CtxTmp2.rsp.u32 = CtxTmp.rsp.u32 - 4*3;
            i = 4*3 - 1;
        }
        CtxTmp.ss = BS3_SEL_TEST_PAGE_03 | 2;
        CtxTmp2.ds = CtxTmp.ds;
        CtxTmp2.es = CtxTmp.es;
        CtxTmp2.fs = CtxTmp.fs;
        CtxTmp2.gs = CtxTmp.gs;
        while (i-- > 0)
        {
            Bs3GdteTestPage03.Gen.u16LimitLow = CtxTmp2.rsp.u16 + i - 1;
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            if (i > 0)
                bs3CpuBasic2_CompareSsCtx(&TrapCtx, &CtxTmp, 0 /*BS3_SEL_TEST_PAGE_03*/, true /*f486ResumeFlagHint*/);
            else
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxTmp2);
            g_usBs3TestStep++;
        }

        *puTssSs2 = uSavedSs2;
        paIdt[0x83 << cIdteShift] = SavedGate83;
    }
    paIdt[0x80 << cIdteShift].Gate.u16Sel = uSysR0Cs;
    BS3_ASSERT(g_usBs3TestStep < 3000);

    /*
     * Modify the gate CS value with a conforming segment.
     */
    g_usBs3TestStep = 3000;
    for (i = 0; i <= 3; i++) /* cs.dpl */
    {
        for (iRing = 0; iRing <= 3; iRing++)
        {
            for (iCtx = 0; iCtx < RT_ELEMENTS(apCtx8x); iCtx++)
            {
                Bs3MemCpy(&CtxTmp, apCtx8x[iCtx], sizeof(CtxTmp));
                Bs3RegCtxConvertToRingX(&CtxTmp, iRing);
# if TMPL_BITS == 32
                g_uBs3TrapEipHint = CtxTmp.rip.u32;
# endif

                for (j = 0; j <= 3; j++) /* rpl */
                {
                    uint16_t const uCs = (uSysR0CsConf | j) + (i << BS3_SEL_RING_SHIFT);
                    /*Bs3TestPrintf("g_usBs3TestStep=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", g_usBs3TestStep,  iCtx,  iRing, i, uCs);*/
                    paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                    Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                    //Bs3TestPrintf("%u/%u/%u/%u: cs=%04x hcs=%04x xcpt=%02x\n", i, iRing, iCtx, j, uCs, TrapCtx.uHandlerCs, TrapCtx.bXcpt);
                    /*Bs3TrapPrintFrame(&TrapCtx);*/
                    g_usBs3TestStep++;
                    if (iCtx < iRing)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                    else if (i > iRing)
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL);
                    else
                        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/);
                }
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
            }
        }
    }
    BS3_ASSERT(g_usBs3TestStep < 3500);

    /*
     * The gates must be 64-bit in long mode.
     */
    if (cIdteShift != 0)
    {
        g_usBs3TestStep = 3500;
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
                        g_usBs3TestStep++;
                        /*Bs3TestPrintf("g_usBs3TestStep=%u iCtx=%u iRing=%u i=%u uCs=%04x\n", g_usBs3TestStep,  iCtx,  iRing, i, uCs);*/
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        /*Bs3TrapPrintFrame(&TrapCtx);*/
                        if (iCtx < iRing)
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                        else
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, uCs & X86_SEL_MASK_OFF_RPL);
                    }
                    paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uSysR0Cs;
                }
            }
        }
        BS3_ASSERT(g_usBs3TestStep < 4000);
    }

    /*
     * IDT limit check.  The 286 does not access X86DESCGATE::u16OffsetHigh.
     */
    g_usBs3TestStep = 5000;
    i = (0x80 << (cIdteShift + 3)) - 1;
    j = (0x82 << (cIdteShift + 3)) - (!f286 ? 1 : 3);
    k = (0x83 << (cIdteShift + 3)) - 1;
    for (; i <= k; i++, g_usBs3TestStep++)
    {
        Idtr = IdtrSaved;
        Idtr.cbIdt  = i;
        ASMSetIDTR(&Idtr);
        Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
        if (i < j)
            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx81, (0x81 << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
        else
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/);
    }
    ASMSetIDTR(&IdtrSaved);
    BS3_ASSERT(g_usBs3TestStep < 5100);

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
    g_usBs3TestStep = 5200;
    if (BS3_MODE_IS_PAGED(g_bTestMode) && pbIdtCopyAlloc)
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
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/);
            g_usBs3TestStep++;

            ASMSetIDTR(&Idtr);
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/);
            g_usBs3TestStep++;

            rc = Bs3PagingProtect(uCr2Expected, _4K, 0 /*fSet*/, X86_PTE_P /*fClear*/);
            if (RT_SUCCESS(rc))
            {
                ASMSetIDTR(&Idtr);
                Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/);
                g_usBs3TestStep++;

                ASMSetIDTR(&Idtr);
                Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
                if (f486Plus)
                    bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, 0 /*uErrCd*/, uCr2Expected);
                else
                    bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, X86_TRAP_PF_RW /*uErrCd*/, uCr2Expected + 4 - RT_MIN(j, 4));
                g_usBs3TestStep++;

                Bs3PagingProtect(uCr2Expected, _4K, X86_PTE_P /*fSet*/, 0 /*fClear*/);

                /* Check if that the entry type is checked after the whole IDTE has been cleared for #PF. */
                pIdtCopy[0x80 << cIdteShift].Gate.u4Type = 0;
                rc = Bs3PagingProtect(uCr2Expected, _4K, 0 /*fSet*/, X86_PTE_P /*fClear*/);
                if (RT_SUCCESS(rc))
                {
                    ASMSetIDTR(&Idtr);
                    Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
                    if (f486Plus)
                        bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, 0 /*uErrCd*/, uCr2Expected);
                    else
                        bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx81, X86_TRAP_PF_RW /*uErrCd*/, uCr2Expected + 4 - RT_MIN(j, 4));
                    g_usBs3TestStep++;

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
    g_usBs3TestStep = 5300;
    if (BS3_MODE_IS_PAGED(g_bTestMode) && pbIdtCopyAlloc)
    {
        Bs3MemCpy(pbIdtCopyAlloc, paIdt, cbIdte * 256);
        Idtr.cbIdt = IdtrSaved.cbIdt;
        Idtr.pIdt  = Bs3SelPtrToFlat(pbIdtCopyAlloc);

        ASMSetIDTR(&Idtr);
        Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/);
        g_usBs3TestStep++;

        rc = Bs3PagingProtect(Idtr.pIdt, _4K, 0 /*fSet*/, X86_PTE_RW | X86_PTE_US /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            ASMSetIDTR(&Idtr);
            Bs3TrapSetJmpAndRestore(&Ctx81, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx81, 0x81 /*bXcpt*/);
            g_usBs3TestStep++;

            Bs3PagingProtect(Idtr.pIdt, _4K, X86_PTE_RW | X86_PTE_US /*fSet*/, 0 /*fClear*/);
        }
        ASMSetIDTR(&IdtrSaved);
    }

    /*
     * Check that CS.u1Accessed is set to 1. Use the test page selector #0 and #3 together
     * with interrupt gates 80h and 83h, respectively.
     */
/** @todo Throw in SS.u1Accessed too. */
    g_usBs3TestStep = 5400;
    if (BS3_MODE_IS_PAGED(g_bTestMode) && pbIdtCopyAlloc)
    {
        Bs3GdteTestPage00 = Bs3Gdt[uSysR0Cs >> X86_SEL_SHIFT];
        Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        paIdt[0x80 << cIdteShift].Gate.u16Sel   = BS3_SEL_TEST_PAGE_00;

        Bs3GdteTestPage03 = Bs3Gdt[(uSysR0Cs + (3 << BS3_SEL_RING_SHIFT)) >> X86_SEL_SHIFT];
        Bs3GdteTestPage03.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        paIdt[0x83 << cIdteShift].Gate.u16Sel   = BS3_SEL_TEST_PAGE_03; /* rpl is ignored, so leave it as zero. */

        /* Check that the CS.A bit is being set on a general basis and that
           the special CS values work with out generic handler code. */
        Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/);
        if (!(Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
            bs3CpuBasic2_FailedF("u4Type=%#x, not accessed", Bs3GdteTestPage00.Gen.u4Type);
        g_usBs3TestStep++;

        Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
        Bs3RegCtxConvertToRingX(&CtxTmp, 3);
        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
        bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/);
        if (!(Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
            bs3CpuBasic2_FailedF("u4Type=%#x, not accessed!", Bs3GdteTestPage00.Gen.u4Type);
        if (TrapCtx.uHandlerCs != (BS3_SEL_TEST_PAGE_03 | 3))
            bs3CpuBasic2_FailedF("uHandlerCs=%#x, expected %#x", TrapCtx.uHandlerCs, (BS3_SEL_TEST_PAGE_03 | 3));
        g_usBs3TestStep++;

        /*
         * Now check that setting CS.u1Access to 1 does __NOT__ trigger a page
         * fault due to the RW bit being zero.
         * (We check both with with and without the WP bit if 80486.)
         */
        if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
            ASMSetCR0(uCr0Saved | X86_CR0_WP);

        Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        Bs3GdteTestPage03.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        rc = Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, 0 /*fSet*/, X86_PTE_RW /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            /* ring-0 handler */
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/);
            if (!(Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                bs3CpuBasic2_FailedF("u4Type=%#x, not accessed!", Bs3GdteTestPage00.Gen.u4Type);
            g_usBs3TestStep++;

            /* ring-3 handler */
            Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, 3);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/);
            if (!(Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                bs3CpuBasic2_FailedF("u4Type=%#x, not accessed!", Bs3GdteTestPage00.Gen.u4Type);
            g_usBs3TestStep++;

            /* clear WP and repeat the above. */
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
                ASMSetCR0(uCr0Saved & ~X86_CR0_WP);
            Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* (No need to RW the page - ring-0, WP=0.) */
            Bs3GdteTestPage03.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* (No need to RW the page - ring-0, WP=0.) */

            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx80, 0x80 /*bXcpt*/);
            if (!(Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                bs3CpuBasic2_FailedF("u4Type=%#x, not accessed!", Bs3GdteTestPage00.Gen.u4Type);
            g_usBs3TestStep++;

            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x83 /*bXcpt*/);
            if (!(Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                bs3CpuBasic2_FailedF("u4Type=%#x, not accessed!n", Bs3GdteTestPage03.Gen.u4Type);
            g_usBs3TestStep++;

            Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, X86_PTE_RW /*fSet*/, 0 /*fClear*/);
        }

        ASMSetCR0(uCr0Saved);

        /*
         * While we're here, check that if the CS GDT entry is a non-present
         * page we do get a #PF with the rigth error code and CR2.
         */
        Bs3GdteTestPage00.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED; /* Just for fun, really a pointless gesture. */
        Bs3GdteTestPage03.Gen.u4Type &= ~X86_SEL_TYPE_ACCESSED;
        rc = Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, 0 /*fSet*/, X86_PTE_P /*fClear*/);
        if (RT_SUCCESS(rc))
        {
            Bs3TrapSetJmpAndRestore(&Ctx80, &TrapCtx);
            if (f486Plus)
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx80, 0 /*uErrCd*/, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00);
            else
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx80, X86_TRAP_PF_RW, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00 + 4);
            g_usBs3TestStep++;

            /* Do it from ring-3 to check ErrCd, which doesn't set X86_TRAP_PF_US it turns out. */
            Bs3MemCpy(&CtxTmp, &Ctx83, sizeof(CtxTmp));
            Bs3RegCtxConvertToRingX(&CtxTmp, 3);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);

            if (f486Plus)
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &CtxTmp, 0 /*uErrCd*/, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_03);
            else
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &CtxTmp, X86_TRAP_PF_RW, GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_03 + 4);
            g_usBs3TestStep++;

            Bs3PagingProtect(GdtrSaved.pGdt + BS3_SEL_TEST_PAGE_00, 8, X86_PTE_P /*fSet*/, 0 /*fClear*/);
            if (Bs3GdteTestPage00.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
                bs3CpuBasic2_FailedF("u4Type=%#x, accessed! #1", Bs3GdteTestPage00.Gen.u4Type);
            if (Bs3GdteTestPage03.Gen.u4Type & X86_SEL_TYPE_ACCESSED)
                bs3CpuBasic2_FailedF("u4Type=%#x, accessed! #2", Bs3GdteTestPage03.Gen.u4Type);
        }

        /* restore */
        paIdt[0x80 << cIdteShift].Gate.u16Sel = uSysR0Cs;
        paIdt[0x83 << cIdteShift].Gate.u16Sel = uSysR0Cs;// + (3 << BS3_SEL_RING_SHIFT) + 3;
    }

# endif /* 32 || 64*/

    /*
     * Check broad EFLAGS effects.
     */
    g_usBs3TestStep = 5600;
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
            if (f486Plus)
                CtxTmp.rflags.u32 |= X86_EFL_AC;
            if (f486Plus && !g_f16BitSys)
                CtxTmp.rflags.u32 |= X86_EFL_RF;
            if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
                CtxTmp.rflags.u32 |= X86_EFL_VIF | X86_EFL_VIP;
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            CtxTmp.rflags.u32 &= ~X86_EFL_RF;

            if (iCtx >= iRing)
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/);
            else
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
            uExpected = CtxTmp.rflags.u32
                      & (  X86_EFL_1 |  X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_DF
                         | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_VM | X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP
                         | X86_EFL_ID /*| X86_EFL_TF*/ /*| X86_EFL_IF*/ /*| X86_EFL_RF*/ );
            if (TrapCtx.fHandlerRfl != uExpected)
                bs3CpuBasic2_FailedF("unexpected handler rflags value: %RX64 expected %RX32; CtxTmp.rflags=%RX64 Ctx.rflags=%RX64\n",
                                     TrapCtx.fHandlerRfl, uExpected, CtxTmp.rflags.u, TrapCtx.Ctx.rflags.u);
            g_usBs3TestStep++;

            /* all cleared */
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) < BS3CPU_80286)
                CtxTmp.rflags.u32 = apCtx8x[iCtx]->rflags.u32 & (X86_EFL_RA1_MASK | UINT16_C(0xf000));
            else
                CtxTmp.rflags.u32 = apCtx8x[iCtx]->rflags.u32 & (X86_EFL_VM | X86_EFL_RA1_MASK);
            Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
            if (iCtx >= iRing)
                bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &CtxTmp, 0x80 + iCtx /*bXcpt*/);
            else
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
            uExpected = CtxTmp.rflags.u32;
            if (TrapCtx.fHandlerRfl != uExpected)
                bs3CpuBasic2_FailedF("unexpected handler rflags value: %RX64 expected %RX32; CtxTmp.rflags=%RX64 Ctx.rflags=%RX64\n",
                                     TrapCtx.fHandlerRfl, uExpected, CtxTmp.rflags.u, TrapCtx.Ctx.rflags.u);
            g_usBs3TestStep++;
        }
    }

/** @todo CS.LIMIT / canonical(CS)  */


    /*
     * Check invalid gate types.
     */
    g_usBs3TestStep = 32000;
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
            g_uBs3TrapEipHint = CtxTmp.rip.u32;
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
                        /*Bs3TestPrintf("g_usBs3TestStep=%u iCtx=%u iRing=%u i=%u uCs=%04x type=%#x\n", g_usBs3TestStep, iCtx, iRing, i, uCs, pauInvTypes[iType]);*/
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel = uCs;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        g_usBs3TestStep++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);

                        /* Mark it not-present to check that invalid type takes precedence. */
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 0;
                        Bs3TrapSetJmpAndRestore(&CtxTmp, &TrapCtx);
                        g_usBs3TestStep++;
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &CtxTmp, ((0x80 + iCtx) << X86_TRAP_ERR_SEL_SHIFT) | X86_TRAP_ERR_IDT);
                        paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present = 1;
                    }
                }

                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u16Sel     = uSysR0Cs;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u4Type     = bSavedType;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1DescType = 0;
                paIdt[(0x80 + iCtx) << cIdteShift].Gate.u1Present  = 1;
            }
        }
    }
    BS3_ASSERT(g_usBs3TestStep < 62000U && g_usBs3TestStep > 32000U);


    /** @todo
     *  - Run \#PF and \#GP (and others?) at CPLs other than zero.
     *  - Quickly generate all faults.
     *  - All the peculiarities v8086.
     */

# if TMPL_BITS != 16
    Bs3MemFree(pbIdtCopyAlloc, 12*_1K);
# endif
}


/**
 * Executes one round of SIDT and SGDT tests using one assembly worker.
 *
 * This is written with driving everything from the 16-bit or 32-bit worker in
 * mind, i.e. not assuming the test bitcount is the same as the current.
 */
# define bs3CpuBasic2_sidt_sgdt_One BS3_CMN_NM(bs3CpuBasic2_sidt_sgdt_One)
BS3_DECL_NEAR(void) bs3CpuBasic2_sidt_sgdt_One(BS3CB2SIDTSGDT const BS3_FAR *pWorker, uint8_t bTestMode, uint8_t bRing,
                                               uint8_t const *pabExpected)
{
    BS3TRAPFRAME        TrapCtx;
    BS3REGCTX           Ctx;
    BS3REGCTX           CtxUdExpected;
    BS3REGCTX           TmpCtx;
    uint8_t const       cbBuf = 8*2;         /* test buffer area  */
    uint8_t             abBuf[8*2 + 8 + 8];  /* test buffer w/ misalignment test space and some extra guard. */
    uint8_t BS3_FAR    *pbBuf  = abBuf;
    uint8_t const       cbIdtr = BS3_MODE_IS_64BIT_CODE(bTestMode) ? 2+8 : 2+4;
    bool const          f286   = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) == BS3CPU_80286;
    uint8_t             bFiller;
    int                 off;
    int                 off2;
    unsigned            cb;
    uint8_t BS3_FAR    *pbTest;

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&CtxUdExpected, sizeof(CtxUdExpected));
    Bs3MemZero(&TmpCtx, sizeof(TmpCtx));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));
    Bs3MemZero(&abBuf, sizeof(abBuf));

    /* Create a context, give this routine some more stack space, point the context
       at our SIDT [xBX] + UD2 combo, and point DS:xBX at abBuf. */
    Bs3RegCtxSaveEx(&Ctx, bTestMode, 256 /*cbExtraStack*/);
    Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, abBuf);
    Bs3RegCtxSetRipCsFromLnkPtr(&Ctx, pWorker->fpfnWorker);
    if (BS3_MODE_IS_16BIT_SYS(bTestMode))
        g_uBs3TrapEipHint = Ctx.rip.u32;
    if (!BS3_MODE_IS_RM_OR_V86(bTestMode))
        Bs3RegCtxConvertToRingX(&Ctx, bRing);

    /* For successful SIDT attempts, we'll stop at the UD2. */
    Bs3MemCpy(&CtxUdExpected, &Ctx, sizeof(Ctx));
    CtxUdExpected.rip.u += pWorker->cbInstr;

    /*
     * Check that it works at all and that only bytes we expect gets written to.
     */
    /* First with zero buffer. */
    Bs3MemZero(abBuf, sizeof(abBuf));
    if (!ASMMemIsAllU8(abBuf, sizeof(abBuf), 0))
        Bs3TestFailedF("ASMMemIsAllU8 or Bs3MemZero is busted: abBuf=%.*Rhxs\n", sizeof(abBuf), pbBuf);
    if (!ASMMemIsZero(abBuf, sizeof(abBuf)))
        Bs3TestFailedF("ASMMemIsZero or Bs3MemZero is busted: abBuf=%.*Rhxs\n", sizeof(abBuf), pbBuf);
    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
    bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
    if (f286 && abBuf[cbIdtr - 1] != 0xff)
        Bs3TestFailedF("286: Top base byte isn't 0xff (#1): %#x\n", abBuf[cbIdtr - 1]);
    if (!ASMMemIsZero(&abBuf[cbIdtr], cbBuf - cbIdtr))
        Bs3TestFailedF("Unexpected buffer bytes set (#1): cbIdtr=%u abBuf=%.*Rhxs\n", cbIdtr, cbBuf, pbBuf);
    if (Bs3MemCmp(abBuf, pabExpected, cbIdtr) != 0)
        Bs3TestFailedF("Mismatch (#1): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, abBuf);
    g_usBs3TestStep++;

    /* Again with buffer filled a byte not occuring in the previous result. */
    bFiller = 0x55;
    while (Bs3MemChr(abBuf, bFiller, cbBuf) != NULL)
        bFiller++;
    Bs3MemSet(abBuf, bFiller, sizeof(abBuf));
    if (!ASMMemIsAllU8(abBuf, sizeof(abBuf), bFiller))
        Bs3TestFailedF("ASMMemIsAllU8 or Bs3MemSet is busted: bFiller=%#x abBuf=%.*Rhxs\n", bFiller, sizeof(abBuf), pbBuf);

    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
    bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
    if (f286 && abBuf[cbIdtr - 1] != 0xff)
        Bs3TestFailedF("286: Top base byte isn't 0xff (#2): %#x\n", abBuf[cbIdtr - 1]);
    if (!ASMMemIsAllU8(&abBuf[cbIdtr], cbBuf - cbIdtr, bFiller))
        Bs3TestFailedF("Unexpected buffer bytes set (#2): cbIdtr=%u bFiller=%#x abBuf=%.*Rhxs\n", cbIdtr, bFiller, cbBuf, pbBuf);
    if (Bs3MemChr(abBuf, bFiller, cbIdtr) != NULL)
        Bs3TestFailedF("Not all bytes touched: cbIdtr=%u bFiller=%#x abBuf=%.*Rhxs\n", cbIdtr, bFiller, cbBuf, pbBuf);
    if (Bs3MemCmp(abBuf, pabExpected, cbIdtr) != 0)
        Bs3TestFailedF("Mismatch (#2): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, abBuf);
    g_usBs3TestStep++;

    /*
     * Slide the buffer along 8 bytes to cover misalignment.
     */
    for (off = 0; off < 8; off++)
    {
        pbBuf = &abBuf[off];
        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, &abBuf[off]);
        CtxUdExpected.rbx.u = Ctx.rbx.u;

        /* First with zero buffer. */
        Bs3MemZero(abBuf, sizeof(abBuf));
        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
        if (off > 0 && !ASMMemIsZero(abBuf, off))
            Bs3TestFailedF("Unexpected buffer bytes set before (#3): cbIdtr=%u off=%u abBuf=%.*Rhxs\n",
                           cbIdtr, off, off + cbBuf, abBuf);
        if (!ASMMemIsZero(&abBuf[off + cbIdtr], sizeof(abBuf) - cbIdtr - off))
            Bs3TestFailedF("Unexpected buffer bytes set after (#3): cbIdtr=%u off=%u abBuf=%.*Rhxs\n",
                           cbIdtr, off, off + cbBuf, abBuf);
        if (f286 && abBuf[off + cbIdtr - 1] != 0xff)
            Bs3TestFailedF("286: Top base byte isn't 0xff (#3): %#x\n", abBuf[off + cbIdtr - 1]);
        if (Bs3MemCmp(&abBuf[off], pabExpected, cbIdtr) != 0)
            Bs3TestFailedF("Mismatch (#3): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &abBuf[off]);
        g_usBs3TestStep++;

        /* Again with buffer filled a byte not occuring in the previous result. */
        Bs3MemSet(abBuf, bFiller, sizeof(abBuf));
        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
        bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
        if (off > 0 && !ASMMemIsAllU8(abBuf, off, bFiller))
            Bs3TestFailedF("Unexpected buffer bytes set before (#4): cbIdtr=%u off=%u bFiller=%#x abBuf=%.*Rhxs\n",
                           cbIdtr, off, bFiller, off + cbBuf, abBuf);
        if (!ASMMemIsAllU8(&abBuf[off + cbIdtr], sizeof(abBuf) - cbIdtr - off, bFiller))
            Bs3TestFailedF("Unexpected buffer bytes set after (#4): cbIdtr=%u off=%u bFiller=%#x abBuf=%.*Rhxs\n",
                           cbIdtr, off, bFiller, off + cbBuf, abBuf);
        if (Bs3MemChr(&abBuf[off], bFiller, cbIdtr) != NULL)
            Bs3TestFailedF("Not all bytes touched (#4): cbIdtr=%u off=%u bFiller=%#x abBuf=%.*Rhxs\n",
                           cbIdtr, off, bFiller, off + cbBuf, abBuf);
        if (f286 && abBuf[off + cbIdtr - 1] != 0xff)
            Bs3TestFailedF("286: Top base byte isn't 0xff (#4): %#x\n", abBuf[off + cbIdtr - 1]);
        if (Bs3MemCmp(&abBuf[off], pabExpected, cbIdtr) != 0)
            Bs3TestFailedF("Mismatch (#4): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &abBuf[off]);
        g_usBs3TestStep++;
    }
    pbBuf = abBuf;
    Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, abBuf);
    CtxUdExpected.rbx.u = Ctx.rbx.u;

    /*
     * Play with the selector limit if the target mode supports limit checking
     * We use BS3_SEL_TEST_PAGE_00 for this
     */
    if (   !BS3_MODE_IS_RM_OR_V86(bTestMode)
        && !BS3_MODE_IS_64BIT_CODE(bTestMode))
    {
        uint16_t cbLimit;
        uint32_t uFlatBuf = Bs3SelPtrToFlat(abBuf);
        Bs3GdteTestPage00 = Bs3Gdte_DATA16;
        Bs3GdteTestPage00.Gen.u2Dpl       = bRing;
        Bs3GdteTestPage00.Gen.u16BaseLow  = (uint16_t)uFlatBuf;
        Bs3GdteTestPage00.Gen.u8BaseHigh1 = (uint8_t)(uFlatBuf >> 16);
        Bs3GdteTestPage00.Gen.u8BaseHigh2 = (uint8_t)(uFlatBuf >> 24);

        if (pWorker->fSs)
            CtxUdExpected.ss = Ctx.ss = BS3_SEL_TEST_PAGE_00 | bRing;
        else
            CtxUdExpected.ds = Ctx.ds = BS3_SEL_TEST_PAGE_00 | bRing;

        /* Expand up (normal). */
        for (off = 0; off < 8; off++)
        {
            CtxUdExpected.rbx.u = Ctx.rbx.u = off;
            for (cbLimit = 0; cbLimit < cbIdtr*2; cbLimit++)
            {
                Bs3GdteTestPage00.Gen.u16LimitLow = cbLimit;
                Bs3MemSet(abBuf, bFiller, sizeof(abBuf));
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
                if (off + cbIdtr <= cbLimit + 1)
                {
                    bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                    if (Bs3MemChr(&abBuf[off], bFiller, cbIdtr) != NULL)
                        Bs3TestFailedF("Not all bytes touched (#5): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                       cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                    if (Bs3MemCmp(&abBuf[off], pabExpected, cbIdtr) != 0)
                        Bs3TestFailedF("Mismatch (#5): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &abBuf[off]);
                    if (f286 && abBuf[off + cbIdtr - 1] != 0xff)
                        Bs3TestFailedF("286: Top base byte isn't 0xff (#5): %#x\n", abBuf[off + cbIdtr - 1]);
                }
                else
                {
                    if (pWorker->fSs)
                        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &Ctx, 0, false /*f486ResumeFlagHint*/);
                    else
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                    if (off + 2 <= cbLimit + 1)
                    {
                        if (Bs3MemChr(&abBuf[off], bFiller, 2) != NULL)
                            Bs3TestFailedF("Limit bytes not touched (#6): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                           cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                        if (Bs3MemCmp(&abBuf[off], pabExpected, 2) != 0)
                            Bs3TestFailedF("Mismatch (#6): expected %.2Rhxs, got %.2Rhxs\n", pabExpected, &abBuf[off]);
                        if (!ASMMemIsAllU8(&abBuf[off + 2], cbIdtr - 2, bFiller))
                            Bs3TestFailedF("Base bytes touched on #GP (#6): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                           cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                    }
                    else if (!ASMMemIsAllU8(abBuf, sizeof(abBuf), bFiller))
                        Bs3TestFailedF("Bytes touched on #GP: cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                       cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                }

                if (off > 0 && !ASMMemIsAllU8(abBuf, off, bFiller))
                    Bs3TestFailedF("Leading bytes touched (#7): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                   cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                if (!ASMMemIsAllU8(&abBuf[off + cbIdtr], sizeof(abBuf) - off - cbIdtr, bFiller))
                    Bs3TestFailedF("Trailing bytes touched (#7): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                   cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);

                g_usBs3TestStep++;
            }
        }

        /* Expand down (weird).  Inverted valid area compared to expand up,
           so a limit of zero give us a valid range for 0001..0ffffh (instead of
           a segment with one valid byte at 0000h).  Whereas a limit of 0fffeh
           means one valid byte at 0ffffh, and a limit of 0ffffh means none
           (because in a normal expand up the 0ffffh means all 64KB are
           accessible). */
        Bs3GdteTestPage00.Gen.u4Type = X86_SEL_TYPE_RW_DOWN_ACC;
        for (off = 0; off < 8; off++)
        {
            CtxUdExpected.rbx.u = Ctx.rbx.u = off;
            for (cbLimit = 0; cbLimit < cbIdtr*2; cbLimit++)
            {
                Bs3GdteTestPage00.Gen.u16LimitLow = cbLimit;
                Bs3MemSet(abBuf, bFiller, sizeof(abBuf));
                Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);

                if (off > cbLimit)
                {
                    bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                    if (Bs3MemChr(&abBuf[off], bFiller, cbIdtr) != NULL)
                        Bs3TestFailedF("Not all bytes touched (#8): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                       cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                    if (Bs3MemCmp(&abBuf[off], pabExpected, cbIdtr) != 0)
                        Bs3TestFailedF("Mismatch (#8): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &abBuf[off]);
                    if (f286 && abBuf[off + cbIdtr - 1] != 0xff)
                        Bs3TestFailedF("286: Top base byte isn't 0xff (#8): %#x\n", abBuf[off + cbIdtr - 1]);
                }
                else
                {
                    if (pWorker->fSs)
                        bs3CpuBasic2_CompareSsCtx(&TrapCtx, &Ctx, 0, false /*f486ResumeFlagHint*/);
                    else
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                    if (!ASMMemIsAllU8(abBuf, sizeof(abBuf), bFiller))
                        Bs3TestFailedF("Bytes touched on #GP: cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                       cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                }

                if (off > 0 && !ASMMemIsAllU8(abBuf, off, bFiller))
                    Bs3TestFailedF("Leading bytes touched (#9): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                   cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);
                if (!ASMMemIsAllU8(&abBuf[off + cbIdtr], sizeof(abBuf) - off - cbIdtr, bFiller))
                    Bs3TestFailedF("Trailing bytes touched (#9): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x abBuf=%.*Rhxs\n",
                                   cbIdtr, off, cbLimit, bFiller, off + cbBuf, abBuf);

                g_usBs3TestStep++;
            }
        }

        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, abBuf);
        CtxUdExpected.rbx.u = Ctx.rbx.u;
        CtxUdExpected.ss = Ctx.ss;
        CtxUdExpected.ds = Ctx.ds;
    }

    /*
     * Play with the paging.
     */
    if (   BS3_MODE_IS_PAGED(bTestMode)
        && (!pWorker->fSs || bRing == 3) /* SS.DPL == CPL, we'll get some tiled ring-3 selector here.  */
        && (pbTest = (uint8_t BS3_FAR *)Bs3MemGuardedTestPageAlloc(BS3MEMKIND_TILED)) != NULL)
    {
        RTCCUINTXREG uFlatTest = Bs3SelPtrToFlat(pbTest);

        /*
         * Slide the buffer towards the trailing guard page.  We'll observe the
         * first word being written entirely separately from the 2nd dword/qword.
         */
        for (off = X86_PAGE_SIZE - cbIdtr - 4; off < X86_PAGE_SIZE + 4; off++)
        {
            Bs3MemSet(&pbTest[X86_PAGE_SIZE - cbIdtr * 2], bFiller, cbIdtr * 2);
            Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, &pbTest[off]);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            if (off + cbIdtr <= X86_PAGE_SIZE)
            {
                CtxUdExpected.rbx = Ctx.rbx;
                CtxUdExpected.ss  = Ctx.ss;
                CtxUdExpected.ds  = Ctx.ds;
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                    Bs3TestFailedF("Mismatch (#9): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
            }
            else
            {
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx, X86_TRAP_PF_RW | (Ctx.bCpl == 3 ? X86_TRAP_PF_US : 0),
                                          uFlatTest + RT_MAX(off, X86_PAGE_SIZE));
                if (   off <= X86_PAGE_SIZE - 2
                    && Bs3MemCmp(&pbTest[off], pabExpected, 2) != 0)
                    Bs3TestPrintf("Mismatch (#10): Expected limit %.2Rhxs, got %.2Rhxs; off=%#x\n",
                                  pabExpected, &pbTest[off], off);
                if (   off < X86_PAGE_SIZE - 2
                    && !ASMMemIsAllU8(&pbTest[off + 2], X86_PAGE_SIZE - off - 2, bFiller))
                    Bs3TestPrintf("Wrote partial base on #PF (#10): bFiller=%#x, got %.*Rhxs; off=%#x\n",
                                  bFiller, X86_PAGE_SIZE - off - 2, &pbTest[off + 2], off);
                if (off == X86_PAGE_SIZE - 1 && pbTest[off] != bFiller)
                    Bs3TestPrintf("Wrote partial limit on #PF (#10): Expected %02x, got %02x\n", bFiller, pbTest[off]);
            }
            g_usBs3TestStep++;
        }

        /*
         * Now, do it the other way around. It should look normal now since writing
         * the limit will #PF first and nothing should be written.
         */
        for (off = cbIdtr + 4; off >= -cbIdtr - 4; off--)
        {
            Bs3MemSet(pbTest, bFiller, 48);
            Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, pWorker->fSs ? &Ctx.ss : &Ctx.ds, &pbTest[off]);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            if (off >= 0)
            {
                CtxUdExpected.rbx = Ctx.rbx;
                CtxUdExpected.ss  = Ctx.ss;
                CtxUdExpected.ds  = Ctx.ds;
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                    Bs3TestFailedF("Mismatch (#11): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
            }
            else
            {
                bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx, X86_TRAP_PF_RW | (Ctx.bCpl == 3 ? X86_TRAP_PF_US : 0), uFlatTest + off);
                if (   -off < cbIdtr
                    && !ASMMemIsAllU8(pbTest, cbIdtr + off, bFiller))
                    Bs3TestPrintf("Wrote partial content on #PF (#12): bFiller=%#x, found %.*Rhxs; off=%d\n",
                                  bFiller, cbIdtr + off, pbTest, off);
            }
            if (!ASMMemIsAllU8(&pbTest[RT_MAX(cbIdtr + off, 0)], 16, bFiller))
                Bs3TestPrintf("Wrote beyond expected area (#13): bFiller=%#x, found %.16Rhxs; off=%d\n",
                              bFiller, &pbTest[RT_MAX(cbIdtr + off, 0)], off);
            g_usBs3TestStep++;
        }

        /*
         * Combine paging and segment limit and check ordering.
         * This is kind of interesting here since it the instruction seems to
         * be doing two separate writes.
         */
        if (   !BS3_MODE_IS_RM_OR_V86(bTestMode)
            && !BS3_MODE_IS_64BIT_CODE(bTestMode))
        {
            uint16_t cbLimit;

            Bs3GdteTestPage00 = Bs3Gdte_DATA16;
            Bs3GdteTestPage00.Gen.u2Dpl       = bRing;
            Bs3GdteTestPage00.Gen.u16BaseLow  = (uint16_t)uFlatTest;
            Bs3GdteTestPage00.Gen.u8BaseHigh1 = (uint8_t)(uFlatTest >> 16);
            Bs3GdteTestPage00.Gen.u8BaseHigh2 = (uint8_t)(uFlatTest >> 24);

            if (pWorker->fSs)
                CtxUdExpected.ss = Ctx.ss = BS3_SEL_TEST_PAGE_00 | bRing;
            else
                CtxUdExpected.ds = Ctx.ds = BS3_SEL_TEST_PAGE_00 | bRing;

            /* Expand up (normal), approaching tail guard page. */
            for (off = X86_PAGE_SIZE - cbIdtr - 4; off < X86_PAGE_SIZE + 4; off++)
            {
                CtxUdExpected.rbx.u = Ctx.rbx.u = off;
                for (cbLimit = X86_PAGE_SIZE - cbIdtr*2; cbLimit < X86_PAGE_SIZE + cbIdtr*2; cbLimit++)
                {
                    Bs3GdteTestPage00.Gen.u16LimitLow = cbLimit;
                    Bs3MemSet(&pbTest[X86_PAGE_SIZE - cbIdtr * 2], bFiller, cbIdtr * 2);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
                    if (off + cbIdtr <= cbLimit + 1)
                    {
                        /* No #GP, but maybe #PF. */
                        if (off + cbIdtr <= X86_PAGE_SIZE)
                        {
                            bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                            if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                                Bs3TestFailedF("Mismatch (#14): expected %.*Rhxs, got %.*Rhxs\n",
                                               cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
                        }
                        else
                        {
                            bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx, X86_TRAP_PF_RW | (Ctx.bCpl == 3 ? X86_TRAP_PF_US : 0),
                                                      uFlatTest + RT_MAX(off, X86_PAGE_SIZE));
                            if (   off <= X86_PAGE_SIZE - 2
                                && Bs3MemCmp(&pbTest[off], pabExpected, 2) != 0)
                                Bs3TestPrintf("Mismatch (#15): Expected limit %.2Rhxs, got %.2Rhxs; off=%#x\n",
                                              pabExpected, &pbTest[off], off);
                            cb = X86_PAGE_SIZE - off - 2;
                            if (   off < X86_PAGE_SIZE - 2
                                && !ASMMemIsAllU8(&pbTest[off + 2], cb, bFiller))
                                Bs3TestPrintf("Wrote partial base on #PF (#15): bFiller=%#x, got %.*Rhxs; off=%#x\n",
                                              bFiller, cb, &pbTest[off + 2], off);
                            if (off == X86_PAGE_SIZE - 1 && pbTest[off] != bFiller)
                                Bs3TestPrintf("Wrote partial limit on #PF (#15): Expected %02x, got %02x\n", bFiller, pbTest[off]);
                        }
                    }
                    else if (off + 2 <= cbLimit + 1)
                    {
                        /* [ig]tr.limit writing does not cause #GP, but may cause #PG, if not writing the base causes #GP. */
                        if (off <= X86_PAGE_SIZE - 2)
                        {
                            if (pWorker->fSs)
                                bs3CpuBasic2_CompareSsCtx(&TrapCtx, &Ctx, 0, false /*f486ResumeFlagHint*/);
                            else
                                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                            if (Bs3MemCmp(&pbTest[off], pabExpected, 2) != 0)
                                Bs3TestPrintf("Mismatch (#16): Expected limit %.2Rhxs, got %.2Rhxs; off=%#x\n",
                                              pabExpected, &pbTest[off], off);
                            cb = X86_PAGE_SIZE - off - 2;
                            if (   off < X86_PAGE_SIZE - 2
                                && !ASMMemIsAllU8(&pbTest[off + 2], cb, bFiller))
                                Bs3TestPrintf("Wrote partial base with limit (#16): bFiller=%#x, got %.*Rhxs; off=%#x\n",
                                              bFiller, cb, &pbTest[off + 2], off);
                        }
                        else
                        {
                            bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx, X86_TRAP_PF_RW | (Ctx.bCpl == 3 ? X86_TRAP_PF_US : 0),
                                                      uFlatTest + RT_MAX(off, X86_PAGE_SIZE));
                            if (   off < X86_PAGE_SIZE
                                && !ASMMemIsAllU8(&pbTest[off], X86_PAGE_SIZE - off, bFiller))
                                Bs3TestPrintf("Mismatch (#16): Partial limit write on #PF: bFiller=%#x, got %.*Rhxs\n",
                                              bFiller, X86_PAGE_SIZE - off, &pbTest[off]);
                        }
                    }
                    else
                    {
                        /* #GP/#SS on limit. */
                        if (pWorker->fSs)
                            bs3CpuBasic2_CompareSsCtx(&TrapCtx, &Ctx, 0, false /*f486ResumeFlagHint*/);
                        else
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                        if (   off < X86_PAGE_SIZE
                            && !ASMMemIsAllU8(&pbTest[off], X86_PAGE_SIZE - off, bFiller))
                            Bs3TestPrintf("Mismatch (#17): Partial write on #GP: bFiller=%#x, got %.*Rhxs\n",
                                          bFiller, X86_PAGE_SIZE - off, &pbTest[off]);
                    }

                    cb = RT_MIN(cbIdtr * 2, off - (X86_PAGE_SIZE - cbIdtr*2));
                    if (!ASMMemIsAllU8(&pbTest[X86_PAGE_SIZE - cbIdtr * 2], cb, bFiller))
                        Bs3TestFailedF("Leading bytes touched (#18): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x pbTest=%.*Rhxs\n",
                                       cbIdtr, off, cbLimit, bFiller, cb, pbTest[X86_PAGE_SIZE - cbIdtr * 2]);

                    g_usBs3TestStep++;

                    /* Set DS to 0 and check that we get #GP(0). */
                    if (!pWorker->fSs)
                    {
                        Ctx.ds = 0;
                        Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
                        bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                        Ctx.ds = BS3_SEL_TEST_PAGE_00 | bRing;
                        g_usBs3TestStep++;
                    }
                }
            }

            /* Expand down. */
            pbTest    -= X86_PAGE_SIZE; /* Note! we're backing up a page to simplify things */
            uFlatTest -= X86_PAGE_SIZE;

            Bs3GdteTestPage00.Gen.u4Type = X86_SEL_TYPE_RW_DOWN_ACC;
            Bs3GdteTestPage00.Gen.u16BaseLow  = (uint16_t)uFlatTest;
            Bs3GdteTestPage00.Gen.u8BaseHigh1 = (uint8_t)(uFlatTest >> 16);
            Bs3GdteTestPage00.Gen.u8BaseHigh2 = (uint8_t)(uFlatTest >> 24);

            for (off = X86_PAGE_SIZE - cbIdtr - 4; off < X86_PAGE_SIZE + 4; off++)
            {
                CtxUdExpected.rbx.u = Ctx.rbx.u = off;
                for (cbLimit = X86_PAGE_SIZE - cbIdtr*2; cbLimit < X86_PAGE_SIZE + cbIdtr*2; cbLimit++)
                {
                    Bs3GdteTestPage00.Gen.u16LimitLow = cbLimit;
                    Bs3MemSet(&pbTest[X86_PAGE_SIZE], bFiller, cbIdtr * 2);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
                    if (cbLimit < off && off >= X86_PAGE_SIZE)
                    {
                        bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                        if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                            Bs3TestFailedF("Mismatch (#19): expected %.*Rhxs, got %.*Rhxs\n",
                                           cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
                        cb = X86_PAGE_SIZE + cbIdtr*2 - off;
                        if (!ASMMemIsAllU8(&pbTest[off + cbIdtr], cb, bFiller))
                            Bs3TestFailedF("Trailing bytes touched (#20): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x pbTest=%.*Rhxs\n",
                                           cbIdtr, off, cbLimit, bFiller, cb, pbTest[off + cbIdtr]);
                    }
                    else
                    {
                        if (cbLimit < off && off < X86_PAGE_SIZE)
                            bs3CpuBasic2_ComparePfCtx(&TrapCtx, &Ctx, X86_TRAP_PF_RW | (Ctx.bCpl == 3 ? X86_TRAP_PF_US : 0),
                                                      uFlatTest + off);
                        else if (pWorker->fSs)
                            bs3CpuBasic2_CompareSsCtx(&TrapCtx, &Ctx, 0, false /*f486ResumeFlagHint*/);
                        else
                            bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                        cb = cbIdtr*2;
                        if (!ASMMemIsAllU8(&pbTest[X86_PAGE_SIZE], cb, bFiller))
                            Bs3TestFailedF("Trailing bytes touched (#20): cbIdtr=%u off=%u cbLimit=%u bFiller=%#x pbTest=%.*Rhxs\n",
                                           cbIdtr, off, cbLimit, bFiller, cb, pbTest[X86_PAGE_SIZE]);
                    }
                    g_usBs3TestStep++;
                }
            }

            pbTest    += X86_PAGE_SIZE;
            uFlatTest += X86_PAGE_SIZE;
        }

        Bs3MemGuardedTestPageFree(pbTest);
    }

    /*
     * Check non-canonical 64-bit space.
     */
    if (   BS3_MODE_IS_64BIT_CODE(bTestMode)
        && (pbTest = (uint8_t BS3_FAR *)Bs3PagingSetupCanonicalTraps()) != NULL)
    {
        /* Make our references relative to the gap. */
        pbTest += g_cbBs3PagingOneCanonicalTrap;

        /* Hit it from below. */
        for (off = -cbIdtr - 8; off < cbIdtr + 8; off++)
        {
            Ctx.rbx.u = CtxUdExpected.rbx.u = UINT64_C(0x0000800000000000) + off;
            Bs3MemSet(&pbTest[-64], bFiller, 64*2);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            if (off + cbIdtr <= 0)
            {
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                    Bs3TestFailedF("Mismatch (#21): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
            }
            else
            {
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                if (off <= -2 && Bs3MemCmp(&pbTest[off], pabExpected, 2) != 0)
                    Bs3TestFailedF("Mismatch (#21): expected limit %.2Rhxs, got %.2Rhxs\n", pabExpected, &pbTest[off]);
                off2 = off <= -2 ? 2 : 0;
                cb   = cbIdtr - off2;
                if (!ASMMemIsAllU8(&pbTest[off + off2], cb, bFiller))
                    Bs3TestFailedF("Mismatch (#21): touched base %.*Rhxs, got %.*Rhxs\n",
                                   cb, &pabExpected[off], cb, &pbTest[off + off2]);
            }
            if (!ASMMemIsAllU8(&pbTest[off - 16], 16, bFiller))
                Bs3TestFailedF("Leading bytes touched (#21): bFiller=%#x, got %.16Rhxs\n", bFiller, &pbTest[off]);
            if (!ASMMemIsAllU8(&pbTest[off + cbIdtr], 16, bFiller))
                Bs3TestFailedF("Trailing bytes touched (#21): bFiller=%#x, got %.16Rhxs\n", bFiller, &pbTest[off + cbIdtr]);
        }

        /* Hit it from above. */
        for (off = -cbIdtr - 8; off < cbIdtr + 8; off++)
        {
            Ctx.rbx.u = CtxUdExpected.rbx.u = UINT64_C(0xffff800000000000) + off;
            Bs3MemSet(&pbTest[-64], bFiller, 64*2);
            Bs3TrapSetJmpAndRestore(&Ctx, &TrapCtx);
            if (off >= 0)
            {
                bs3CpuBasic2_CompareUdCtx(&TrapCtx, &CtxUdExpected);
                if (Bs3MemCmp(&pbTest[off], pabExpected, cbIdtr) != 0)
                    Bs3TestFailedF("Mismatch (#22): expected %.*Rhxs, got %.*Rhxs\n", cbIdtr, pabExpected, cbIdtr, &pbTest[off]);
            }
            else
            {
                bs3CpuBasic2_CompareGpCtx(&TrapCtx, &Ctx, 0);
                if (!ASMMemIsAllU8(&pbTest[off], cbIdtr, bFiller))
                    Bs3TestFailedF("Mismatch (#22): touched base %.*Rhxs, got %.*Rhxs\n",
                                   cbIdtr, &pabExpected[off], cbIdtr, &pbTest[off]);
            }
            if (!ASMMemIsAllU8(&pbTest[off - 16], 16, bFiller))
                Bs3TestFailedF("Leading bytes touched (#22): bFiller=%#x, got %.16Rhxs\n", bFiller, &pbTest[off]);
            if (!ASMMemIsAllU8(&pbTest[off + cbIdtr], 16, bFiller))
                Bs3TestFailedF("Trailing bytes touched (#22): bFiller=%#x, got %.16Rhxs\n", bFiller, &pbTest[off + cbIdtr]);
        }

    }

    /*
     * Move the table by setting up alias pages.  Aim at areas above 2GB for 32-bit and in
     * the 'negative' space of 64-bit addresses.
     */
    if (BS3_MODE_IS_PAGED(bTestMode))
    {

    }


}

# define bs3CpuBasic2_sidt_sgdt_Common BS3_CMN_NM(bs3CpuBasic2_sidt_sgdt_Common)
BS3_DECL_NEAR(void) bs3CpuBasic2_sidt_sgdt_Common(uint8_t bTestMode, BS3CB2SIDTSGDT const BS3_FAR *paWorkers, unsigned cWorkers,
                                                  uint8_t const *pabExpected)
{
    unsigned idx;
    unsigned bRing;
    unsigned iStep = 0;

    /* Note! We skip the SS checks for ring-0 since we badly mess up SS in the
             test and don't want to bother with double faults. */
    for (bRing = 0; bRing <= 3; bRing++)
    {
        for (idx = 0; idx < cWorkers; idx++)
            if (    (paWorkers[idx].bMode & (bTestMode & BS3_MODE_CODE_MASK))
                && (!paWorkers[idx].fSs || bRing != 0 /** @todo || BS3_MODE_IS_64BIT_SYS(bTestMode)*/ ))
            {
                g_usBs3TestStep = iStep;
                bs3CpuBasic2_sidt_sgdt_One(&paWorkers[idx], bTestMode, bRing, pabExpected);
                iStep += 1000;
            }
        if (BS3_MODE_IS_RM_OR_V86(bTestMode))
            break;
    }
}


# if ARCH_BITS != 64

/**
 * Worker for bs3CpuBasic2_TssGateEsp that tests the INT 80 from outer rings.
 */
#  define bs3CpuBasic2_TssGateEsp_AltStackOuterRing BS3_CMN_NM(bs3CpuBasic2_TssGateEsp_AltStackOuterRing)
BS3_DECL_NEAR(void) bs3CpuBasic2_TssGateEsp_AltStackOuterRing(PCBS3REGCTX pCtx, uint8_t bRing, uint8_t *pbAltStack,
                                                              size_t cbAltStack, bool f16BitStack, bool f16BitTss,
                                                              bool f16BitHandler, unsigned uLine)
{
    uint8_t const   cbIretFrame = f16BitHandler ? 5*2 : 5*4;
    BS3REGCTX       Ctx2;
    BS3TRAPFRAME    TrapCtx;
    uint8_t        *pbTmp;
    g_usBs3TestStep = uLine;

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

    bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx2, 0x80 /*bXcpt*/);
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
            bs3CpuBasic2_FailedF("handler SS:ESP=%04x:%08RX64, expected %04x:%08RX16",
                                 TrapCtx.uHandlerSs, TrapCtx.uHandlerRsp, Bs3Tss16.ss0, uExpectedRsp);

        pbTmp = (uint8_t *)ASMMemFirstNonZero(pbAltStack, cbAltStack);
        if ((f16BitStack || TrapCtx.uHandlerRsp <= UINT16_MAX) && pbTmp != NULL)
            bs3CpuBasic2_FailedF("someone touched the alt stack (%p) with SS:ESP=%04x:%#RX32: %p=%02x",
                                 pbAltStack, Ctx2.ss, Ctx2.rsp.u32, pbTmp, *pbTmp);
        else if (!f16BitStack && TrapCtx.uHandlerRsp > UINT16_MAX && pbTmp == NULL)
            bs3CpuBasic2_FailedF("the alt stack (%p) was not used SS:ESP=%04x:%#RX32\n", pbAltStack, Ctx2.ss, Ctx2.rsp.u32);
    }
}

#  define bs3CpuBasic2_TssGateEspCommon BS3_CMN_NM(bs3CpuBasic2_TssGateEspCommon)
BS3_DECL_NEAR(void) bs3CpuBasic2_TssGateEspCommon(bool const g_f16BitSys, PX86DESC const paIdt, unsigned const cIdteShift)
{
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx;
    BS3REGCTX       Ctx2;
#  if TMPL_BITS == 16
    uint8_t        *pbTmp;
#  endif

    /* make sure they're allocated  */
    Bs3MemZero(&Ctx, sizeof(Ctx));
    Bs3MemZero(&Ctx2, sizeof(Ctx2));
    Bs3MemZero(&TrapCtx, sizeof(TrapCtx));

    Bs3RegCtxSave(&Ctx);
    Ctx.rsp.u -= 0x80;
    Ctx.rip.u  = (uintptr_t)BS3_FP_OFF(&bs3CpuBasic2_Int80);
#  if TMPL_BITS == 32
    g_uBs3TrapEipHint = Ctx.rip.u32;
#  endif

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
    g_usBs3TestStep = __LINE__;
    bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx, 0x80 /*bXcpt*/);

    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, NULL, 0, g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);
    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 2, NULL, 0, g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);
    bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 3, NULL, 0, g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);

    /*
     * Check that the upper part of ESP is preserved when doing .
     */
    if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        size_t const cbAltStack = _8K;
        uint8_t *pbAltStack = Bs3MemAllocZ(BS3MEMKIND_TILED, cbAltStack);
        if (pbAltStack)
        {
            /* same ring */
            g_usBs3TestStep = __LINE__;
            Bs3MemCpy(&Ctx2, &Ctx, sizeof(Ctx2));
            Ctx2.rsp.u = Bs3SelPtrToFlat(pbAltStack + 0x1980);
            if (Bs3TrapSetJmp(&TrapCtx))
                Bs3RegCtxRestore(&Ctx2, 0); /* (does not return) */
            bs3CpuBasic2_CompareIntCtx1(&TrapCtx, &Ctx2, 0x80 /*bXcpt*/);
#  if TMPL_BITS == 16
            if ((pbTmp = (uint8_t *)ASMMemFirstNonZero(pbAltStack, cbAltStack)) != NULL)
                bs3CpuBasic2_FailedF("someone touched the alt stack (%p) with SS:ESP=%04x:%#RX32: %p=%02x\n",
                                     pbAltStack, Ctx2.ss, Ctx2.rsp.u32, pbTmp, *pbTmp);
#  else
            if (ASMMemIsZero(pbAltStack, cbAltStack))
                bs3CpuBasic2_FailedF("alt stack wasn't used despite SS:ESP=%04x:%#RX32\n", Ctx2.ss, Ctx2.rsp.u32);
#  endif

            /* Different rings (load SS0:SP0 from TSS). */
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                      g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 2, pbAltStack, cbAltStack,
                                                      g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);
            bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 3, pbAltStack, cbAltStack,
                                                      g_f16BitSys, g_f16BitSys, g_f16BitSys, __LINE__);

            /* Different rings but switch the SS bitness in the TSS. */
            if (g_f16BitSys)
            {
                Bs3Tss16.ss0 = BS3_SEL_R0_SS32;
                bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                          false, g_f16BitSys, g_f16BitSys, __LINE__);
                Bs3Tss16.ss0 = BS3_SEL_R0_SS16;
            }
            else
            {
                Bs3Tss32.ss0 = BS3_SEL_R0_SS16;
                bs3CpuBasic2_TssGateEsp_AltStackOuterRing(&Ctx, 1, pbAltStack, cbAltStack,
                                                          true,  g_f16BitSys, g_f16BitSys, __LINE__);
                Bs3Tss32.ss0 = BS3_SEL_R0_SS32;
            }

            Bs3MemFree(pbAltStack, cbAltStack);
        }
        else
            Bs3TestPrintf("%s: Skipping ESP check, alloc failed\n", g_pszTestMode);
    }
    else
        Bs3TestPrintf("%s: Skipping ESP check, CPU too old\n", g_pszTestMode);
}

# endif /* ARCH_BITS != 64 */
#endif /* BS3_INSTANTIATING_CMN */


/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE

BS3_DECL_FAR(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t bRet = 0;

    g_pszTestMode = TMPL_NM(g_szBs3ModeName);
    g_bTestMode   = bMode;
    g_f16BitSys   = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

# if TMPL_MODE == BS3_MODE_PE16 \
 || TMPL_MODE == BS3_MODE_PE16_32 \
 || TMPL_MODE == BS3_MODE_PP16 \
 || TMPL_MODE == BS3_MODE_PP16_32 \
 || TMPL_MODE == BS3_MODE_PAE16 \
 || TMPL_MODE == BS3_MODE_PAE16_32 \
 || TMPL_MODE == BS3_MODE_PE32
    bs3CpuBasic2_TssGateEspCommon(BS3_MODE_IS_16BIT_SYS(TMPL_MODE),
                                  (PX86DESC)MyBs3Idt,
                                  BS3_MODE_IS_64BIT_SYS(TMPL_MODE) ? 1 : 0);
# else
    bRet = BS3TESTDOMODE_SKIPPED;
# endif

    /*
     * Re-initialize the IDT.
     */
    Bs3TrapInit();
    return bRet;
}


BS3_DECL_FAR(uint8_t) TMPL_NM(bs3CpuBasic2_RaiseXcpt1)(uint8_t bMode)
{
    g_pszTestMode = TMPL_NM(g_szBs3ModeName);
    g_bTestMode   = bMode;
    g_f16BitSys   = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

# if !BS3_MODE_IS_RM_OR_V86(TMPL_MODE)

    /*
     * Pass to common worker which is only compiled once per mode.
     */
    bs3CpuBasic2_RaiseXcpt1Common(MY_SYS_SEL_R0_CS,
                                  MY_SYS_SEL_R0_CS_CNF,
                                  MY_SYS_SEL_R0_SS,
                                  (PX86DESC)MyBs3Idt,
                                  BS3_MODE_IS_64BIT_SYS(TMPL_MODE) ? 1 : 0);

    /*
     * Re-initialize the IDT.
     */
    Bs3TrapInit();
    return 0;
# elif TMPL_MODE == BS3_MODE_RM

    /*
     * Check
     */
    /** @todo check    */
    return BS3TESTDOMODE_SKIPPED;

# else
    return BS3TESTDOMODE_SKIPPED;
# endif
}


BS3_DECL_FAR(uint8_t) TMPL_NM(bs3CpuBasic2_iret)(uint8_t bMode)
{
    g_pszTestMode = TMPL_NM(g_szBs3ModeName);
    g_bTestMode   = bMode;
    g_f16BitSys   = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

    Bs3PrintStrN(RT_STR_TUPLE("Hello world!\n"));
# if !BS3_MODE_IS_V86(TMPL_MODE)
    Bs3TestPrintf(RT_STR_TUPLE("Hi there!\n"));
# endif
    return BS3TESTDOMODE_SKIPPED;
}


BS3_DECL_FAR(uint8_t) TMPL_NM(bs3CpuBasic2_sidt)(uint8_t bMode)
{
//if (bMode == BS3_MODE_LM64)
{
    union
    {
        RTIDTR  Idtr;
        uint8_t ab[16];
    } Expected;

    g_pszTestMode = TMPL_NM(g_szBs3ModeName);
    g_bTestMode   = bMode;
    g_f16BitSys   = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);

    BS3_ASSERT(bMode == TMPL_MODE);

    /*
     * Pass to common worker which is only compiled once per mode.
     */
    Bs3MemZero(&Expected, sizeof(Expected));
    ASMGetIDTR(&Expected.Idtr);
    bs3CpuBasic2_sidt_sgdt_Common(bMode, g_aSidtWorkers, RT_ELEMENTS(g_aSidtWorkers), Expected.ab);

    /*
     * Re-initialize the IDT.
     */
    Bs3TrapInit();
}

    return 0;
}


BS3_DECL_FAR(uint8_t) TMPL_NM(bs3CpuBasic2_sgdt)(uint8_t bMode)
{
//if (bMode >= BS3_MODE_LM16)
{
    uint64_t const uOrgAddr = Bs3Lgdt_Gdt.uAddr;
    uint64_t       uNew     = 0;
    union
    {
        RTGDTR  Gdtr;
        uint8_t ab[16];
    } Expected;

    g_pszTestMode = TMPL_NM(g_szBs3ModeName);
    g_bTestMode   = bMode;
    g_f16BitSys   = BS3_MODE_IS_16BIT_SYS(TMPL_MODE);
    BS3_ASSERT(bMode == TMPL_MODE);

    /*
     * If paged mode, try push the GDT way up.
     */
    if (BS3_MODE_IS_PAGED(bMode))
    {
/** @todo loading non-canonical base addresses.   */
        int rc;
        uNew  = BS3_MODE_IS_64BIT_SYS(bMode) ? UINT64_C(0xffff80fedcb70000) : UINT64_C(0xc2d28000);
        uNew |= uOrgAddr & X86_PAGE_OFFSET_MASK;
        rc = Bs3PagingAlias(uNew, uOrgAddr, Bs3Lgdt_Gdt.cb, X86_PTE_P | X86_PTE_RW | X86_PTE_US | X86_PTE_D | X86_PTE_A);
        if (RT_SUCCESS(rc))
        {
            Bs3Lgdt_Gdt.uAddr = uNew;
            Bs3UtilSetFullGdtr(Bs3Lgdt_Gdt.cb, uNew);
        }
    }

    /*
     * Pass to common worker which is only compiled once per mode.
     */
    Bs3MemZero(&Expected, sizeof(Expected));
    ASMGetGDTR(&Expected.Gdtr);
    bs3CpuBasic2_sidt_sgdt_Common(bMode, g_aSgdtWorkers, RT_ELEMENTS(g_aSgdtWorkers), Expected.ab);

    /*
     * Unalias the GDT.
     */
    if (uNew != 0)
    {
        Bs3Lgdt_Gdt.uAddr = uOrgAddr;
        Bs3UtilSetFullGdtr(Bs3Lgdt_Gdt.cb, uOrgAddr);
        Bs3PagingUnalias(uNew, Bs3Lgdt_Gdt.cb);
    }

    /*
     * Re-initialize the IDT.
     */
    Bs3TrapInit();
}
    return 0;
}

#endif /* BS3_INSTANTIATING_MODE */

