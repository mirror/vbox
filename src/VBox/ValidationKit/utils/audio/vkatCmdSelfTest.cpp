/* $Id$ */
/** @file
 * Validation Kit Audio Test (VKAT) - Self test code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/test.h>

#include "Audio/AudioHlp.h"
#include "Audio/AudioTest.h"
#include "Audio/AudioTestService.h"
#include "Audio/AudioTestServiceClient.h"

#include "vkatInternal.h"


/**
 * Thread callback for mocking the guest (VM) side of things.
 *
 * @returns VBox status code.
 * @param   hThread             Thread handle.
 * @param   pvUser              Pointer to user-supplied data.
 */
static DECLCALLBACK(int) audioTestSelftestGuestAtsThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PSELFTESTCTX pCtx = (PSELFTESTCTX)pvUser;

    PAUDIOTESTENV pTstEnvGst = &pCtx->Guest.TstEnv;

    /* Flag the environment for self test mode. */
    pTstEnvGst->fSelftest = true;

    /* Generate tag for guest side. */
    int rc = RTStrCopy(pTstEnvGst->szTag, sizeof(pTstEnvGst->szTag), pCtx->szTag);
    AssertRCReturn(rc, rc);

    rc = AudioTestPathCreateTemp(pTstEnvGst->szPathTemp, sizeof(pTstEnvGst->szPathTemp), "selftest-guest");
    AssertRCReturn(rc, rc);

    rc = AudioTestPathCreateTemp(pTstEnvGst->szPathOut, sizeof(pTstEnvGst->szPathOut), "selftest-out");
    AssertRCReturn(rc, rc);

    pTstEnvGst->enmMode = AUDIOTESTMODE_GUEST;

    /** @todo Make this customizable. */
    PDMAudioPropsInit(&pTstEnvGst->Props,
                      2 /* 16-bit */, true  /* fSigned */, 2 /* cChannels */, 44100 /* uHz */);

    rc = audioTestEnvInit(pTstEnvGst, pTstEnvGst->DrvStack.pDrvReg, pCtx->fWithDrvAudio);
    if (RT_SUCCESS(rc))
    {
        RTThreadUserSignal(hThread);

        audioTestWorker(pTstEnvGst);
        audioTestEnvDestroy(pTstEnvGst);
    }

    return rc;
}

/**
 * Main function for performing the self test.
 *
 * @returns RTEXITCODE
 * @param   pCtx                Self test context to use.
 */
RTEXITCODE audioTestDoSelftest(PSELFTESTCTX pCtx)
{
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS,  "Running self test ...\n");

    /*
     * The self-test does the following:
     * - 1. a) Creates an ATS instance to emulate the guest mode ("--mode guest")
     *         at port 6042 (ATS_TCP_GUEST_DEFAULT_PORT).
     *      or
     *      b) Connect to an already existing guest ATS instance if "--guest-ats-address" is specified.
     *      This makes it more flexible in terms of testing / debugging.
     * - 2. Uses the Validation Kit audio backend, which in turn creates an ATS instance
     *      at port 6052 (ATS_TCP_HOST_DEFAULT_PORT).
     * - 3. Executes a complete test run locally (e.g. without any guest (VM) involved).
     */

    /* Generate a common tag for guest and host side. */
    int rc = AudioTestGenTag(pCtx->szTag, sizeof(pCtx->szTag));
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    PAUDIOTESTENV pTstEnvHst = &pCtx->Host.TstEnv;

    /* Flag the environment for self test mode. */
    pTstEnvHst->fSelftest = true;

    /* Generate tag for host side. */
    rc = RTStrCopy(pTstEnvHst->szTag, sizeof(pTstEnvHst->szTag), pCtx->szTag);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    rc = AudioTestPathCreateTemp(pTstEnvHst->szPathTemp, sizeof(pTstEnvHst->szPathTemp), "selftest-tmp");
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    rc = AudioTestPathCreateTemp(pTstEnvHst->szPathOut, sizeof(pTstEnvHst->szPathOut), "selftest-out");
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    /* Initialize the PCM properties to some sane values. */
    PDMAudioPropsInit(&pTstEnvHst->Props,
                      2 /* 16-bit */, true /* fPcmSigned */, 2 /* cPcmChannels */, 44100 /* uPcmHz */);

    /*
     * Step 1.
     */
    RTTHREAD hThreadGstAts = NIL_RTTHREAD;

    bool const fStartGuestAts = RTStrNLen(pCtx->Host.szGuestAtsAddr, sizeof(pCtx->Host.szGuestAtsAddr)) == 0;
    if (fStartGuestAts)
    {
        /* Step 1b. */
        rc = RTThreadCreate(&hThreadGstAts, audioTestSelftestGuestAtsThread, pCtx, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE,
                            "VKATGstAts");
        if (RT_SUCCESS(rc))
            rc = RTThreadUserWait(hThreadGstAts, RT_MS_30SEC);
    }
    /* else Step 1a later. */

    if (RT_SUCCESS(rc))
    {
        /*
         * Steps 2 + 3.
         */
        pTstEnvHst->enmMode = AUDIOTESTMODE_HOST;

        rc = audioTestEnvInit(pTstEnvHst, &g_DrvHostValidationKitAudio, true /* fWithDrvAudio */);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestWorker(pTstEnvHst);
            if (RT_SUCCESS(rc))
            {

            }

            audioTestEnvDestroy(pTstEnvHst);
        }
    }

    /*
     * Shutting down.
     */
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS,  "Shutting down self test\n");

    ASMAtomicWriteBool(&g_fTerminate, true);

    if (fStartGuestAts)
    {
        int rcThread;
        int rc2 = RTThreadWait(hThreadGstAts, RT_MS_30SEC, &rcThread);
        if (RT_SUCCESS(rc2))
            rc2 = rcThread;
        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Shutting down guest ATS failed with %Rrc\n", rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Self test failed with %Rrc\n", rc);

    return RT_SUCCESS(rc) ?  RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/*********************************************************************************************************************************
*   Command: selftest                                                                                                            *
*********************************************************************************************************************************/

/**
 * Long option values for the 'selftest' command.
 */
enum
{
    VKAT_SELFTEST_OPT_GUEST_ATS_ADDR = 900,
    VKAT_SELFTEST_OPT_GUEST_ATS_PORT,
    VKAT_SELFTEST_OPT_HOST_ATS_ADDR,
    VKAT_SELFTEST_OPT_HOST_ATS_PORT
};

/**
 * Command line parameters for self-test mode.
 */
static const RTGETOPTDEF s_aCmdSelftestOptions[] =
{
    { "--guest-ats-addr",   VKAT_SELFTEST_OPT_GUEST_ATS_ADDR,   RTGETOPT_REQ_STRING  },
    { "--guest-ats-port",   VKAT_SELFTEST_OPT_GUEST_ATS_PORT,   RTGETOPT_REQ_UINT32  },
    { "--host-ats-addr",    VKAT_SELFTEST_OPT_HOST_ATS_ADDR,    RTGETOPT_REQ_STRING  },
    { "--host-ats-port",    VKAT_SELFTEST_OPT_HOST_ATS_PORT,    RTGETOPT_REQ_UINT32  },
    { "--exclude-all",      'a',                                RTGETOPT_REQ_NOTHING },
    { "--backend",          'b',                                RTGETOPT_REQ_STRING  },
    { "--with-drv-audio",   'd',                                RTGETOPT_REQ_NOTHING },
    { "--exclude",          'e',                                RTGETOPT_REQ_UINT32  },
    { "--include",          'i',                                RTGETOPT_REQ_UINT32  }
};

/** the 'selftest' command option help. */
static DECLCALLBACK(const char *) audioTestCmdSelftestHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'a':                              return "Exclude all tests from the list (useful to enable single tests later with --include)";
        case 'b':                              return "The audio backend to use";
        case 'd':                              return "Go via DrvAudio instead of directly interfacing with the backend";
        case 'e':                              return "Exclude the given test id from the list";
        case 'i':                              return "Include the given test id in the list";
        case VKAT_SELFTEST_OPT_GUEST_ATS_ADDR: return "Address of guest ATS to connect to";
        case VKAT_SELFTEST_OPT_GUEST_ATS_PORT: return "Port of guest ATS to connect to [6042]";
        case VKAT_SELFTEST_OPT_HOST_ATS_ADDR:  return "Address of host ATS to connect to";
        case VKAT_SELFTEST_OPT_HOST_ATS_PORT:  return "Port of host ATS to connect to [6052]";
        default:  return NULL;
    }
}

/**
 * The 'selftest' command handler.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
DECLCALLBACK(RTEXITCODE) audioTestCmdSelftestHandler(PRTGETOPTSTATE pGetState)
{
    SELFTESTCTX Ctx;
    RT_ZERO(Ctx);

    /* Go with the platform's default backend if nothing else is specified. */
    Ctx.Guest.pDrvReg = AudioTestGetDefaultBackend();

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case VKAT_SELFTEST_OPT_GUEST_ATS_ADDR:
                rc = RTStrCopy(Ctx.Host.szGuestAtsAddr, sizeof(Ctx.Host.szGuestAtsAddr), ValueUnion.psz);
                break;

            case VKAT_SELFTEST_OPT_GUEST_ATS_PORT:
                Ctx.Host.uGuestAtsPort = ValueUnion.u32;
                break;

            case VKAT_SELFTEST_OPT_HOST_ATS_ADDR:
                rc = RTStrCopy(Ctx.Host.szValKitAtsAddr, sizeof(Ctx.Host.szValKitAtsAddr), ValueUnion.psz);
                break;

            case VKAT_SELFTEST_OPT_HOST_ATS_PORT:
                Ctx.Host.uValKitAtsPort = ValueUnion.u32;
                break;

            case 'a':
                for (unsigned i = 0; i < g_cTests; i++)
                    g_aTests[i].fExcluded = true;
                break;

            case 'b':
                Ctx.Guest.pDrvReg = AudioTestFindBackendOpt(ValueUnion.psz);
                if (Ctx.Guest.pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'd':
                Ctx.fWithDrvAudio = true;
                break;

            case 'e':
                if (ValueUnion.u32 >= g_cTests)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid test number %u passed to --exclude", ValueUnion.u32);
                g_aTests[ValueUnion.u32].fExcluded = true;
                break;

            case 'i':
                if (ValueUnion.u32 >= g_cTests)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid test number %u passed to --include", ValueUnion.u32);
                g_aTests[ValueUnion.u32].fExcluded = false;
                break;

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    int rc2 = audioTestDoSelftest(&Ctx);
    if (RT_FAILURE(rc2))
        RTTestFailed(g_hTest, "Self test failed with rc=%Rrc", rc2);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}

/**
 * Command table entry for 'selftest'.
 */
const VKATCMD g_CmdSelfTest =
{
    "selftest",
    audioTestCmdSelftestHandler,
    "Performs self-tests.",
    s_aCmdSelftestOptions,
    RT_ELEMENTS(s_aCmdSelftestOptions),
    audioTestCmdSelftestHelp,
    true /* fNeedsTransport */
};

