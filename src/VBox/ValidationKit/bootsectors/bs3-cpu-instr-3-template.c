/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-3 - MMX, SSE and AVX instructions, C code template.
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
/** Instruction set type and operand width. */
typedef enum
{
    T_INVALID,
    T_MMX,
    T_AXMMX,
    T_SSE,
    T_128BITS = T_SSE,
    T_SSE2,
    T_SSE3,
    T_SSSE3,
    T_SSE4_1,
    T_SSE4_2,
    T_SSE4A,
    T_AVX_128,
    T_AVX2_128,
    T_AVX_256,
    T_256BITS = T_AVX_256,
    T_AVX2_256,
    T_MAX
} INPUT_TYPE_T;

/** Memory or register rm variant. */
enum { RM_REG, RM_MEM };

/**
 * Execution environment configuration.
 */
typedef struct BS3CPUINSTR3_CONFIG_T
{
    uint16_t    fCr0Mp      : 1;
    uint16_t    fCr0Em      : 1;
    uint16_t    fCr0Ts      : 1;
    uint16_t    fCr4OsFxSR  : 1;
    uint16_t    fCr4OsXSave : 1;
    uint16_t    fXcr0Sse    : 1;
    uint16_t    fXcr0Avx    : 1;
    uint16_t    fAligned    : 1; /**< Aligned memory operands. If zero, they will be misaligned and tests w/o memory ops skipped. */
    uint16_t    fAlignCheck : 1;
    uint16_t    fMxCsrMM    : 1; /**< AMD only */
    uint8_t     bXcptMmx;
    uint8_t     bXcptSse;
    uint8_t     bXcptAvx;
} BS3CPUINSTR3_CONFIG_T;
/** Pointer to an execution environment configuration. */
typedef BS3CPUINSTR3_CONFIG_T const BS3_FAR *PCBS3CPUINSTR3_CONFIG_T;

/** State saved by bs3CpuInstr3ConfigReconfigure. */
typedef struct BS3CPUINSTR3_CONFIG_SAVED_T
{
    uint32_t uCr0;
    uint32_t uCr4;
    uint32_t uEfl;
    uint32_t uMxCsr;
} BS3CPUINSTR3_CONFIG_SAVED_T;
typedef BS3CPUINSTR3_CONFIG_SAVED_T BS3_FAR *PBS3CPUINSTR3_CONFIG_SAVED_T;
typedef BS3CPUINSTR3_CONFIG_SAVED_T const BS3_FAR *PCBS3CPUINSTR3_CONFIG_SAVED_T;

#endif


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN

# define BS3_FNBS3FAR_PROTOTYPES_CMN(a_BaseNm) \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c16); \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c32); \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c64)

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorps_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorps_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp);
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
static bool g_fGlobalInitialized    = false;
static bool g_fAmdMisalignedSse     = false;
static bool g_afTypeSupports[T_MAX] = { false, false, false, false, false, false, false, false, false };

/** Exception type #4 test configurations. */
static const BS3CPUINSTR3_CONFIG_T g_aXcptConfig4[] =
{
/*
 *   X87 SSE SSE SSE     AVX      AVX  AVX   SSE+AVX   AVX+AMD/SSE  AMD/SSE   <-- applies to
 *
 *   CR0 CR0 CR0 CR4     CR4      XCR0 XCR0
 *   MP, EM, TS, OSFXSR, OSXSAVE, SSE, AVX,  fAligned, fAlignCheck, fMxCsrMM, bXcptMmx,    bXcptSse,    bXcptAvx */
    { 0, 0,  0,  1,      1,       1,   1,    1,        0,           0,        X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_DB }, /* #0 */
    { 1, 0,  0,  1,      1,       1,   1,    1,        0,           0,        X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_DB }, /* #1 */
    { 0, 1,  0,  1,      1,       1,   1,    1,        0,           0,        X86_XCPT_UD, X86_XCPT_UD, X86_XCPT_DB }, /* #2 */
    { 0, 0,  1,  1,      1,       1,   1,    1,        0,           0,        X86_XCPT_NM, X86_XCPT_NM, X86_XCPT_NM }, /* #3 */
    { 0, 1,  1,  1,      1,       1,   1,    1,        0,           0,        X86_XCPT_UD, X86_XCPT_UD, X86_XCPT_NM }, /* #4 */
    { 0, 0,  0,  0,      1,       1,   1,    1,        0,           0,        X86_XCPT_UD, X86_XCPT_UD, X86_XCPT_DB }, /* #5 */
    { 0, 0,  0,  1,      0,       1,   1,    1,        0,           0,        X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #6 */
    { 0, 0,  0,  1,      1,       1,   0,    1,        0,           0,        X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #7 */
    { 0, 0,  0,  1,      1,       0,   0,    1,        0,           0,        X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #8 */
    /* Memory misalignment: */
    { 0, 0,  0,  1,      1,       1,   1,    0,        0,           0,        X86_XCPT_GP, X86_XCPT_GP, X86_XCPT_DB }, /* #9 */
    { 0, 0,  0,  1,      1,       1,   1,    0,        1,           0,        X86_XCPT_GP, X86_XCPT_GP, X86_XCPT_AC }, /* #10 */
    /* AMD only: */
    { 0, 0,  0,  1,      1,       1,   1,    0,        0,           1,        X86_XCPT_GP, X86_XCPT_DB, X86_XCPT_DB }, /* #11 */
    { 0, 0,  0,  1,      1,       1,   1,    0,        1,           1,        X86_XCPT_GP, X86_XCPT_AC, X86_XCPT_AC }, /* #12 */
};
#endif


/*
 * Common code.
 * Common code.
 * Common code.
 */
#ifdef BS3_INSTANTIATING_CMN

/** Initializes global variables. */
static void bs3CpuInstr3InitGlobals(void)
{
    if (!g_fGlobalInitialized)
    {
        if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
        {
            uint32_t fEbx, fEcx, fEdx;
            ASMCpuIdExSlow(1, 0, 0, 0, NULL, NULL, &fEcx, &fEdx);
            g_afTypeSupports[T_MMX]         = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_MMX);
            g_afTypeSupports[T_SSE]         = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_SSE);
            g_afTypeSupports[T_SSE2]        = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_SSE2);
            g_afTypeSupports[T_SSE3]        = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE3);
            g_afTypeSupports[T_SSSE3]       = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSSE3);
            g_afTypeSupports[T_SSE4_1]      = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE4_1);
            g_afTypeSupports[T_SSE4_2]      = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE4_2);
            g_afTypeSupports[T_AVX_128]     = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_AVX);
            g_afTypeSupports[T_AVX_256]     = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_AVX);

            if (ASMCpuId_EAX(0) >= 7)
            {
                ASMCpuIdExSlow(7, 0, 0, 0, NULL, &fEbx, NULL, NULL);
                g_afTypeSupports[T_AVX2_128] = RT_BOOL(fEbx & X86_CPUID_STEXT_FEATURE_EBX_AVX2);
                g_afTypeSupports[T_AVX2_256] = RT_BOOL(fEbx & X86_CPUID_STEXT_FEATURE_EBX_AVX2);
            }

            if (g_uBs3CpuDetected & BS3CPU_F_CPUID_EXT_LEAVES)
            {
                ASMCpuIdExSlow(UINT32_C(0x80000001), 0, 0, 0, NULL, NULL, &fEcx, &fEdx);
                g_afTypeSupports[T_AXMMX]   = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_EDX_AXMMX);
                g_afTypeSupports[T_SSE4A]   = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_ECX_SSE4A);
                g_fAmdMisalignedSse         = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);
            }
        }

        g_fGlobalInitialized = true;
    }
}


/**
 * Reconfigures the execution environment according to @a pConfig.
 *
 * Call bs3CpuInstr3ConfigRestore to undo the changes.
 *
 * @returns true on success, false if the configuration cannot be applied. In
 *          the latter case, no context changes are made.
 * @param   pSavedCfg   Where to save state we modify.
 * @param   pCtx        The register context to modify.
 * @param   pExtCtx     The extended register context to modify.
 * @param   pConfig     The configuration to apply.
 */
static bool bs3CpuInstr3ConfigReconfigure(PBS3CPUINSTR3_CONFIG_SAVED_T pSavedCfg, PBS3REGCTX pCtx, PBS3EXTCTX pExtCtx,
                                          PCBS3CPUINSTR3_CONFIG_T pConfig)
{
    /*
     * Save context bits we may change here
     */
    pSavedCfg->uCr0   = pCtx->cr0.u32;
    pSavedCfg->uCr4   = pCtx->cr4.u32;
    pSavedCfg->uEfl   = pCtx->rflags.u32;
    pSavedCfg->uMxCsr = Bs3ExtCtxGetMxCsr(pExtCtx);

    /*
     * Can we make these changes?
     */
    if (pConfig->fMxCsrMM && !g_fAmdMisalignedSse)
        return false;

    /*
     * Modify the test context.
     */
    if (pConfig->fCr0Mp)
        pCtx->cr0.u32 |= X86_CR0_MP;
    else
        pCtx->cr0.u32 &= ~X86_CR0_MP;
    if (pConfig->fCr0Em)
        pCtx->cr0.u32 |= X86_CR0_EM;
    else
        pCtx->cr0.u32 &= ~X86_CR0_EM;
    if (pConfig->fCr0Ts)
        pCtx->cr0.u32 |= X86_CR0_TS;
    else
        pCtx->cr0.u32 &= ~X86_CR0_TS;

    if (pConfig->fCr4OsFxSR)
        pCtx->cr4.u32 |= X86_CR4_OSFXSR;
    else
        pCtx->cr4.u32 &= ~X86_CR4_OSFXSR;
    /** @todo X86_CR4_OSXMMEEXCPT? */
    if (pConfig->fCr4OsXSave)
        pCtx->cr4.u32 |= X86_CR4_OSXSAVE;
    else
        pCtx->cr4.u32 &= ~X86_CR4_OSXSAVE;

    if (pConfig->fXcr0Sse)
        pExtCtx->fXcr0Saved |= XSAVE_C_SSE;
    else
        pExtCtx->fXcr0Saved &= ~XSAVE_C_SSE;
    if (pConfig->fXcr0Avx)
        pExtCtx->fXcr0Saved |= XSAVE_C_YMM;
    else
        pExtCtx->fXcr0Saved &= ~XSAVE_C_YMM;

    if (pConfig->fAlignCheck)
    {
        pCtx->rflags.u32 |= X86_EFL_AC;
        pCtx->cr0.u32    |= X86_CR0_AM;
    }
    else
    {
        pCtx->rflags.u32 &= ~X86_EFL_AC;
        pCtx->cr0.u32    &= ~X86_CR0_AM;
    }

    if (pConfig->fMxCsrMM)
        Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr | X86_MXCSR_MM);
    else
        Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr & ~X86_MXCSR_MM);
    return true;
}


/**
 * Undoes changes made by bs3CpuInstr3ConfigReconfigure.
 */
static void bs3CpuInstr3ConfigRestore(PCBS3CPUINSTR3_CONFIG_SAVED_T pSavedCfg, PBS3REGCTX pCtx, PBS3EXTCTX pExtCtx)
{
    pCtx->cr0.u32       = pSavedCfg->uCr0;
    pCtx->cr4.u32       = pSavedCfg->uCr4;
    pCtx->rflags.u32    = pSavedCfg->uEfl;
    pExtCtx->fXcr0Saved = pExtCtx->fXcr0Nominal;
    Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr);
}


/**
 * Allocates two extended CPU contexts and initializes the first one
 * with random data.
 * @returns First extended context, initialized with randomish data. NULL on
 *          failure (complained).
 * @param   ppExtCtx2   Where to return the 2nd context.
 */
static PBS3EXTCTX bs3CpuInstr3AllocExtCtxs(PBS3EXTCTX BS3_FAR *ppExtCtx2)
{
    /* Allocate extended context structures. */
    uint64_t   fFlags;
    uint16_t   cb       = Bs3ExtCtxGetSize(&fFlags);
    PBS3EXTCTX pExtCtx1 = Bs3MemAlloc(BS3MEMKIND_TILED, cb * 2);
    PBS3EXTCTX pExtCtx2 = (PBS3EXTCTX)((uint8_t BS3_FAR *)pExtCtx1 + cb);
    if (pExtCtx1)
    {
        Bs3ExtCtxInit(pExtCtx1, cb, fFlags);
        /** @todo populate with semi-random stuff. */

        Bs3ExtCtxInit(pExtCtx2, cb, fFlags);
        *ppExtCtx2 = pExtCtx2;
        return pExtCtx1;
    }
    Bs3TestFailedF("Bs3MemAlloc(tiled,%#x)", cb * 2);
    *ppExtCtx2 = NULL;
    return NULL;
}

static void bs3CpuInstr3FreeExtCtxs(PBS3EXTCTX pExtCtx1, PBS3EXTCTX BS3_FAR pExtCtx2)
{
    RT_NOREF_PV(pExtCtx2);
    Bs3MemFree(pExtCtx1, pExtCtx1->cb * 2);
}

/**
 * Sets up SSE and maybe AVX.
 */
static void bs3CpuInstr3SetupSseAndAvx(PBS3REGCTX pCtx, PCBS3EXTCTX pExtCtx)
{
    uint32_t cr0 =  Bs3RegGetCr0();
    cr0 &= ~(X86_CR0_TS | X86_CR0_MP | X86_CR0_EM);
    cr0 |= X86_CR0_NE;
    pCtx->cr0.u32 = cr0;
    Bs3RegSetCr0(cr0);

    if (pExtCtx->enmMethod != BS3EXTCTXMETHOD_ANCIENT)
    {
        uint32_t cr4 = Bs3RegGetCr4();
        if (pExtCtx->enmMethod == BS3EXTCTXMETHOD_XSAVE)
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT | X86_CR4_OSXSAVE;
            Bs3RegSetCr4(cr4);
            Bs3RegSetXcr0(pExtCtx->fXcr0Nominal);
        }
        else if (pExtCtx->enmMethod == BS3EXTCTXMETHOD_FXSAVE)
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT;
            Bs3RegSetCr4(cr4);
        }
        pCtx->cr4.u32 = cr4;
    }
}


/*
 * Test type #1.
 */

typedef struct BS3CPUINSTR3_TEST1_VALUES_T
{
    RTUINT256U      uSrc2;
    RTUINT256U      uSrc1; /**< uDstIn for MMX & SSE */
    RTUINT256U      uDstOut;
} BS3CPUINSTR3_TEST1_VALUES_T;

typedef struct BS3CPUINSTR3_TEST1_T
{
    FPFNBS3FAR      pfnWorker;
    uint8_t         bAvxMisalignXcpt;
    uint8_t         enmRm;
    uint8_t         enmType;
    uint8_t         iRegDst;
    uint8_t         iRegSrc1;
    uint8_t         iRegSrc2;
    uint8_t         cValues;
    BS3CPUINSTR3_TEST1_VALUES_T const BS3_FAR *paValues;
} BS3CPUINSTR3_TEST1_T;

typedef struct BS3CPUINSTR3_TEST1_MODE_T
{
    BS3CPUINSTR3_TEST1_T const BS3_FAR *paTests;
    unsigned                            cTests;
} BS3CPUINSTR3_TEST1_MODE_T;

/** Initializer for a BS3CPUINSTR3_TEST1_MODE_T array (three entries). */
#if ARCH_BITS == 16
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { NULL, 0 }, { NULL, 0 } }
#elif ARCH_BITS == 32
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { NULL, 0 } }
#else
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { a_aTests64, RT_ELEMENTS(a_aTests64) } }
#endif

/** Converts an execution mode (BS3_MODE_XXX) into an index into an array
 *  initialized by BS3CPUINSTR3_TEST1_MODES_INIT. */
#define BS3CPUINSTR3_TEST1_MODES_INDEX(a_bMode) \
    (BS3_MODE_IS_16BIT_CODE(bMode) ? 0 : BS3_MODE_IS_32BIT_CODE(bMode) ? 1 : 2)


/**
 * Test type #1 worker.
 */
static uint8_t bs3CpuInstr3_WorkerTestType1(uint8_t bMode, BS3CPUINSTR3_TEST1_T const BS3_FAR *paTests, unsigned cTests,
                                            PCBS3CPUINSTR3_CONFIG_T paConfigs, unsigned cConfigs)
{
    const char BS3_FAR * const  pszMode = Bs3GetModeName(bMode);
    BS3REGCTX                   Ctx;
    BS3TRAPFRAME                TrapFrame;
    uint8_t                     bRing = BS3_MODE_IS_V86(bMode) ? 3 : 0;
    PBS3EXTCTX                  pExtCtxOut;
    PBS3EXTCTX                  pExtCtx = bs3CpuInstr3AllocExtCtxs(&pExtCtxOut);
    if (!pExtCtx)
        return 0;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /* Ensure that the globals we use here have been initialized. */
    bs3CpuInstr3InitGlobals();

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 1024);
    bs3CpuInstr3SetupSseAndAvx(&Ctx, pExtCtx);

    /*
     * Run the tests in all rings since alignment issues may behave
     * differently in ring-3 compared to ring-0.
     */
    for (;;)
    {
        unsigned iCfg;
        for (iCfg = 0; iCfg < cConfigs; iCfg++)
        {
            unsigned                    iTest;
            BS3CPUINSTR3_CONFIG_SAVED_T SavedCfg;
            if (!bs3CpuInstr3ConfigReconfigure(&SavedCfg, &Ctx, pExtCtx, &paConfigs[iCfg]))
                continue; /* unsupported config */

            /*
             * Iterate the tests.
             */
            for (iTest = 0; iTest < cTests; iTest++)
            {
                BS3CPUINSTR3_TEST1_VALUES_T const BS3_FAR *paValues = paTests[iTest].paValues;
                uint8_t const   cbInstr     = ((uint8_t const BS3_FAR *)(uintptr_t)paTests[iTest].pfnWorker)[-1];
                unsigned const  cValues     = paTests[iTest].cValues;
                bool const      fMmxInstr   = paTests[iTest].enmType < T_SSE;
                bool const      fSseInstr   = paTests[iTest].enmType >= T_SSE && paTests[iTest].enmType < T_AVX_128;
                bool const      fAvxInstr   = paTests[iTest].enmType >= T_AVX_128;
                uint8_t const   cbOperand   = paTests[iTest].enmType < T_128BITS ? 64/8
                                            : paTests[iTest].enmType < T_256BITS ? 128/8 : 256/8;
                uint8_t const   cbAlign     = RT_MIN(cbOperand, 16);
                uint8_t         bXcptExpect = !g_afTypeSupports[paTests[iTest].enmType] ? X86_XCPT_UD
                                            : fMmxInstr ? paConfigs[iCfg].bXcptMmx
                                            : fSseInstr ? paConfigs[iCfg].bXcptSse
                                            : BS3_MODE_IS_RM_OR_V86(bMode) ? X86_XCPT_UD : paConfigs[iCfg].bXcptAvx;
                uint16_t        idTestStep  = bRing * 10000 + iCfg * 100 + iTest * 10;
                unsigned        iVal;
                uint8_t         abPadding[sizeof(RTUINT256U) * 2];
                unsigned const  offPadding  = (BS3_FP_OFF(&abPadding[sizeof(RTUINT256U)]) & ~(size_t)(cbAlign - 1))
                                            - BS3_FP_OFF(&abPadding[0]);
                PRTUINT256U     puMemOp     = (PRTUINT256U)&abPadding[offPadding - !paConfigs[iCfg].fAligned];
                BS3_ASSERT((uint8_t BS3_FAR *)puMemOp - &abPadding[0] <= sizeof(RTUINT256U));

                /* If testing unaligned memory accesses, skip register-only tests.  This allows
                   setting bXcptMmx, bXcptSse and bXcptAvx to reflect the misaligned exceptions.  */
                if (!paConfigs[iCfg].fAligned && paTests[iTest].enmRm != RM_MEM)
                    continue;

                /* #AC is only raised in ring-3.: */
                if (bXcptExpect == X86_XCPT_AC)
                {
                    if (bRing != 3)
                        bXcptExpect = X86_XCPT_DB;
                    else if (fAvxInstr)
                        bXcptExpect = paTests[iTest].bAvxMisalignXcpt; /* they generally don't raise #AC */
                }

                Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paTests[iTest].pfnWorker);

                /*
                 * Iterate the test values and do the actual testing.
                 */
                for (iVal = 0; iVal < cValues; iVal++, idTestStep++)
                {
                    uint16_t   cErrors;
                    RTUINT256U uMemOpExpect;

                    /*
                     * Set up the context and some expectations.
                     */
                    /* dest */
                    if (paTests[iTest].iRegDst == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        Bs3MemSet(puMemOp, sizeof(*puMemOp), 0xcc);
                        if (bXcptExpect == X86_XCPT_DB)
                            uMemOpExpect = paValues[iVal].uDstOut;
                        else
                            uMemOpExpect = *puMemOp;
                    }

                    /* source #1 (/ destination for MMX and SSE) */
                    if (paTests[iTest].iRegSrc1 == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        *puMemOp = paValues[iVal].uSrc1;
                        if (paTests[iTest].iRegDst == UINT8_MAX)
                            BS3_ASSERT(fSseInstr);
                        else
                            uMemOpExpect = paValues[iVal].uSrc1;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc1, paValues[iVal].uSrc1.QWords.qw0);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc1, &paValues[iVal].uSrc1.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc1, &paValues[iVal].uSrc1, 32);

                    /* source #2 */
                    if (paTests[iTest].iRegSrc2 == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        BS3_ASSERT(paTests[iTest].iRegDst != UINT8_MAX && paTests[iTest].iRegSrc1 != UINT8_MAX);
                        *puMemOp = uMemOpExpect = paValues[iVal].uSrc2;
                        uMemOpExpect = paValues[iVal].uSrc2;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc2, paValues[iVal].uSrc2.QWords.qw0);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc2, &paValues[iVal].uSrc2.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc2, &paValues[iVal].uSrc2, 32);

                    /* Memory pointer. */
                    if (paTests[iTest].enmRm == RM_MEM)
                    {
                        BS3_ASSERT(   paTests[iTest].iRegDst == UINT8_MAX
                                   || paTests[iTest].iRegSrc1 == UINT8_MAX
                                   || paTests[iTest].iRegSrc2 == UINT8_MAX);
                        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, puMemOp);
                    }

                    /*
                     * Execute.
                     */
                    Bs3ExtCtxRestore(pExtCtx);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                    Bs3ExtCtxSave(pExtCtxOut);

                    /*
                     * Check the result:
                     */
                    cErrors = Bs3TestSubErrorCount();

                    if (bXcptExpect == X86_XCPT_DB && paTests[iTest].iRegDst != UINT8_MAX)
                    {
                        if (fMmxInstr)
                            Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegDst, paValues[iVal].uDstOut.QWords.qw0);
                        else if (fSseInstr)
                            Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut.DQWords.dqw0);
                        else
                            Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut, cbOperand);
                    }
                    Bs3TestCheckExtCtx(pExtCtxOut, pExtCtx, 0 /*fFlags*/, pszMode, idTestStep);

                    if (TrapFrame.bXcpt != bXcptExpect)
                        Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bXcptExpect, TrapFrame.bXcpt);

                    /* Kludge! Looks like EFLAGS.AC is cleared when raising #GP in real mode on the 10980XE. WEIRD! */
                    if (bMode == BS3_MODE_RM && (Ctx.rflags.u32 & X86_EFL_AC))
                    {
                        if (TrapFrame.Ctx.rflags.u32 & X86_EFL_AC)
                            Bs3TestFailedF("Expected EFLAGS.AC to be cleared (bXcpt=%d)", TrapFrame.bXcpt);
                        TrapFrame.Ctx.rflags.u32 |= X86_EFL_AC;
                    }
                    Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &Ctx, bXcptExpect == X86_XCPT_DB ? cbInstr + 1 : 0, 0,
                                         bXcptExpect == X86_XCPT_DB || BS3_MODE_IS_16BIT_SYS(bMode) ? 0 : X86_EFL_RF,
                                         pszMode, idTestStep);

                    if (   paTests[iTest].enmRm == RM_MEM
                        && Bs3MemCmp(puMemOp, &uMemOpExpect, cbOperand) != 0)
                        Bs3TestFailedF("Expected uMemOp %*.Rhxs, got %*.Rhxs", cbOperand, &uMemOpExpect, cbOperand, puMemOp);

                    if (cErrors != Bs3TestSubErrorCount())
                    {
                        if (paConfigs[iCfg].fAligned)
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect);
                        else
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x, puMemOp=%p, EFLAGS=%#RX32, CR0=%#RX32)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect, puMemOp, TrapFrame.Ctx.rflags.u32, TrapFrame.Ctx.cr0);
                        Bs3TestPrintf("\n");
                    }
                }
            }

            bs3CpuInstr3ConfigRestore(&SavedCfg, &Ctx, pExtCtx);
        }

        /*
         * Next ring.
         */
        bRing++;
        if (bRing > 3 || bMode == BS3_MODE_RM)
            break;
        Bs3RegCtxConvertToRingX(&Ctx, bRing);
    }

    /*
     * Cleanup.
     */
    bs3CpuInstr3FreeExtCtxs(pExtCtx, pExtCtxOut);
    return 0;
}

/*
 * XORPS, 128-bit VXORPS
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_xorps)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValues[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ^ */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ^ */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888888888, 0x8888888888888888, 0x8888888888888888, 0x8888888888888888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x53d42d8665bf4141, 0xc7a89924261969d5, 0x3c21c6f3e9d5f961, 0xdf8f2dd3b08d0f46) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c16,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c16,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c32,          255, RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c32,         255, RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c32,    255, RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c32,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c64,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c64,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


#endif /* BS3_INSTANTIATING_CMN */



/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE


#endif /* BS3_INSTANTIATING_MODE */

