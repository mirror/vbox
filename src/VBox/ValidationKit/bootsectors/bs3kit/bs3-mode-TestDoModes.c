/* $Id$ */
/** @file
 * BS3Kit - Bs3TestDoModeTests
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
#include "bs3-cmn-test.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def CONV_TO_FLAT
 * Get flat address.  In 16-bit the parameter is a real mode far address, while
 * in 32-bit and 64-bit modes it is already flat.
 */
/** @def CONV_TO_BS3TEXT16
 * Get BS3TEXT16 relative address. In 32-bit and 64-bit mode this is a flat
 * address, while in 16-bit it is alreay a BS3TEXT16 relative address.
 */
#if ARCH_BITS == 16
# define CONV_TO_FLAT(a_fpfn)           (((uint32_t)BS3_FP_SEG(a_fpfn) << 4) + BS3_FP_OFF(a_fpfn))
# define CONV_TO_BS3TEXT16(a_pfn)       ((uint16_t)(a_pfn))
#else
# define CONV_TO_FLAT(a_fpfn)           ((uint32_t)(uintptr_t)(a_fpfn))
# define CONV_TO_BS3TEXT16(a_pfn)       ((uint16_t)(uintptr_t)(a_pfn))
#endif


/*********************************************************************************************************************************
*   Assembly Symbols                                                                                                             *
*********************************************************************************************************************************/
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInRM)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16_32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16_V86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE32_16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPEV86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16_32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16_V86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP32_16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPPV86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16_32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16_V86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE32_16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAEV86)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM16)(uint16_t offCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM32)(uint32_t uFlatAddrCallback);
BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM64)(uint32_t uFlatAddrCallback);



/**
 * Warns about CPU modes that must be skipped.
 *
 * It will try not warn about modes for which there are no tests.
 *
 * @param   paEntries       The mode test entries.
 * @param   cEntries        The number of tests.
 * @param   bCpuType        The CPU type byte (see #BS3CPU_TYPE_MASK).
 * @param   fHavePae        Whether the CPU has PAE.
 * @param   fHaveLongMode   Whether the CPU does long mode.
 */
static void bs3TestWarnAboutSkippedModes(PCBS3TESTMODEENTRY paEntries, unsigned cEntries,
                                         uint8_t bCpuType, bool fHavePae, bool fHaveLongMode)
{
    bool           fComplained286   = false;
    bool           fComplained386   = false;
    bool           fComplainedPAE   = false;
    bool           fComplainedAMD64 = false;
    unsigned       i;

    /*
     * Complaint run.
     */
    for (i = 0; i < cEntries; i++)
    {
        if (   !fComplained286
            &&  paEntries[i].pfnDoPE16)
        {
            if (bCpuType < BS3CPU_80286)
            {
                Bs3Printf("Only executing real-mode tests as no 80286+ CPU was detected.\n");
                break;
            }
            fComplained286 = true;
        }

        if (   !fComplained386
            &&  (   paEntries[i].pfnDoPE16_32
                 || paEntries[i].pfnDoPE16_V86
                 || paEntries[i].pfnDoPE32
                 || paEntries[i].pfnDoPE32_16
                 || paEntries[i].pfnDoPEV86
                 || paEntries[i].pfnDoPP16
                 || paEntries[i].pfnDoPP16_32
                 || paEntries[i].pfnDoPP16_V86
                 || paEntries[i].pfnDoPP32
                 || paEntries[i].pfnDoPP32_16
                 || paEntries[i].pfnDoPPV86) )
        {
            if (bCpuType < BS3CPU_80386)
            {
                Bs3Printf("80286 CPU: Only executing 16-bit protected and real mode tests.\n");
                break;
            }
            fComplained386 = true;
        }

        if (   !fComplainedPAE
            &&  (   paEntries[i].pfnDoPAE16
                 || paEntries[i].pfnDoPAE16_32
                 || paEntries[i].pfnDoPAE16_V86
                 || paEntries[i].pfnDoPAE32
                 || paEntries[i].pfnDoPAE32_16
                 || paEntries[i].pfnDoPAEV86) )
        {
            if (!fHavePae)
            {
                Bs3Printf("PAE and long mode tests will be skipped.\n");
                break;
            }
            fComplainedPAE = true;
        }

        if (   !fComplainedAMD64
            &&  (   paEntries[i].pfnDoLM16
                 || paEntries[i].pfnDoLM32
                 || paEntries[i].pfnDoLM64) )
        {
            if (!fHaveLongMode)
            {
                Bs3Printf("Long mode tests will be skipped.\n");
                break;
            }
            fComplainedAMD64 = true;
        }
    }
}

BS3_DECL(void) TMPL_NM(Bs3TestDoModes)(PCBS3TESTMODEENTRY paEntries, size_t cEntries)
{
    bool const      fDoV86Modes      = true;
    bool const      fDoWeirdV86Modes = true;
    uint16_t const  uCpuDetected  = BS3_DATA_NM(g_uBs3CpuDetected);
    uint8_t const   bCpuType      = uCpuDetected & BS3CPU_TYPE_MASK;
    bool const      fHavePae      = RT_BOOL(uCpuDetected & BS3CPU_F_PAE);
    bool const      fHaveLongMode = RT_BOOL(uCpuDetected & BS3CPU_F_LONG_MODE);
    unsigned        i;

#if 1 /* debug. */
    Bs3Printf("Bs3TestDoModes: uCpuDetected=%#x fHavePae=%d fHaveLongMode=%d\n", uCpuDetected, fHavePae, fHaveLongMode);
#endif
    bs3TestWarnAboutSkippedModes(paEntries, cEntries, bCpuType, fHavePae, fHaveLongMode);

    /*
     * The real run.
     */
    for (i = 0; i < cEntries; i++)
    {
        const char *pszFmtStr = "Error #%u (%#x) in %s!\n";
        bool        fSkipped  = true;
        uint8_t     bErrNo;
        Bs3TestSub(paEntries[i].pszSubTest);

#define CHECK_RESULT(a_szModeName) \
            do { \
                if (bErrNo != BS3TESTDOMODE_SKIPPED) \
                { \
                    /*Bs3Printf("bErrNo=%#x %s\n", bErrNo, a_szModeName);*/ \
                    fSkipped = false; \
                    if (bErrNo != 0) \
                        Bs3TestFailedF(pszFmtStr, bErrNo, bErrNo, a_szModeName); \
                } \
            } while (0)

        if (paEntries[i].pfnDoRM)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInRM)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoRM));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_rm));
        }

        if (bCpuType < BS3CPU_80286)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        /*
         * Unpaged prot mode.
         */
        if (paEntries[i].pfnDoPE16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPE16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pe16));
        }
        if (bCpuType < BS3CPU_80386)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].pfnDoPE16_32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPE16_32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pe16_32));
        }

        if (paEntries[i].pfnDoPE16_V86 && fDoWeirdV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16_V86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPE16_V86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pe16_v86));
        }

        if (paEntries[i].pfnDoPE32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE32)(CONV_TO_FLAT(paEntries[i].pfnDoPE32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pe32));
        }

        if (paEntries[i].pfnDoPE32_16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE32_16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPE32_16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pe32_16));
        }

        if (paEntries[i].pfnDoPEV86 && fDoV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPEV86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPEV86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pev86));
        }

        /*
         * Paged protected mode.
         */
        if (paEntries[i].pfnDoPP16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPP16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pp16));
        }

        if (paEntries[i].pfnDoPP16_32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPP16_32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pp16_32));
        }

        if (paEntries[i].pfnDoPP16_V86 && fDoWeirdV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16_V86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPP16_V86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pp16_v86));
        }

        if (paEntries[i].pfnDoPP32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP32)(CONV_TO_FLAT(paEntries[i].pfnDoPP32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pp32));
        }

        if (paEntries[i].pfnDoPP32_16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP32_16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPP32_16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pp32_16));
        }

        if (paEntries[i].pfnDoPPV86 && fDoV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPPV86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPPV86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_ppv86));
        }

        /*
         * Protected mode with PAE paging.
         */
        if (!fHavePae)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].pfnDoPAE16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPAE16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pae16));
        }

        if (paEntries[i].pfnDoPAE16_32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE16_32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pae16_32));
        }

        if (paEntries[i].pfnDoPAE16_V86 && fDoWeirdV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16_V86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPAE16_V86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pae16_v86));
        }

        if (paEntries[i].pfnDoPAE32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pae32));
        }

        if (paEntries[i].pfnDoPAE32_16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE32_16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPAE32_16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_pae32_16));
        }

        if (paEntries[i].pfnDoPAEV86 && fDoV86Modes)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAEV86)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoPAEV86));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_paev86));
        }

        /*
         * Long mode.
         */
        if (!fHaveLongMode)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].pfnDoLM16)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM16)(CONV_TO_BS3TEXT16(paEntries[i].pfnDoLM16));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_lm16));
        }

        if (paEntries[i].pfnDoLM32)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM32)(CONV_TO_FLAT(paEntries[i].pfnDoLM32));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_lm32));
        }

        if (paEntries[i].pfnDoLM64)
        {
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM64)(CONV_TO_FLAT(paEntries[i].pfnDoLM64));
            CHECK_RESULT(BS3_DATA_NM(g_szBs3ModeName_lm64));
        }

        if (fSkipped)
            Bs3TestSkipped("skipped\n");
    }
    Bs3TestSubDone();
}

