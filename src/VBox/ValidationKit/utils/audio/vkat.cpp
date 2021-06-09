/* $Id$ */
/** @file
 * Validation Kit Audio Test (VKAT) utility for testing and validating the audio stack.
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
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/test.h>

#include <package-generated.h>
#include "product-generated.h"

#include <VBox/version.h>
#include <VBox/log.h>

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h> /* for CoInitializeEx */
#endif
#include <signal.h>

/**
 * Internal driver instance data
 * @note This must be put here as it's needed before pdmdrv.h is included.
 */
typedef struct PDMDRVINSINT
{
    /** The stack the drive belongs to. */
    struct AUDIOTESTDRVSTACK *pStack;
} PDMDRVINSINT;
#define PDMDRVINSINT_DECLARED

#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include "Audio/AudioHlp.h"
#include "Audio/AudioTest.h"
#include "Audio/AudioTestService.h"
#include "Audio/AudioTestServiceClient.h"

#include "VBoxDD.h"

#include "vkatInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** For use in the option switch to handle common options. */
#define AUDIO_TEST_COMMON_OPTION_CASES(a_ValueUnion) \
            case 'q': \
                g_uVerbosity = 0; \
                if (g_pRelLogger) \
                    RTLogGroupSettings(g_pRelLogger, "all=0 all.e"); \
                break; \
            \
            case 'v': \
                g_uVerbosity++; \
                if (g_pRelLogger) \
                    RTLogGroupSettings(g_pRelLogger, g_uVerbosity == 1 ? "all.e.l" : g_uVerbosity == 2 ? "all.e.l.f" : "all=~0"); \
                break; \
            \
            case 'V': \
                return audioTestVersion(); \
            \
            case 'h': \
                return audioTestUsage(g_pStdOut); \
            \
            case AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_ENABLE: \
                g_fDrvAudioDebug = true; \
                break; \
            \
            case AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_PATH: \
                g_pszDrvAudioDebug = (a_ValueUnion).psz; \
                break



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms);
static RTEXITCODE audioTestUsage(PRTSTREAM pStrm);
static RTEXITCODE audioTestVersion(void);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Common long options values.
 */
enum
{
    AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_ENABLE = 256,
    AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_PATH
};

/**
 * Long option values for the 'test' command.
 */
enum
{
    VKAT_TEST_OPT_COUNT = 900,
    VKAT_TEST_OPT_DEV,
    VKAT_TEST_OPT_GUEST_ATS_ADDR,
    VKAT_TEST_OPT_GUEST_ATS_PORT,
    VKAT_TEST_OPT_HOST_ATS_ADDR,
    VKAT_TEST_OPT_HOST_ATS_PORT,
    VKAT_TEST_OPT_MODE,
    VKAT_TEST_OPT_OUTDIR,
    VKAT_TEST_OPT_PAUSE,
    VKAT_TEST_OPT_PCM_HZ,
    VKAT_TEST_OPT_PCM_BIT,
    VKAT_TEST_OPT_PCM_CHAN,
    VKAT_TEST_OPT_PCM_SIGNED,
    VKAT_TEST_OPT_TAG,
    VKAT_TEST_OPT_TEMPDIR,
    VKAT_TEST_OPT_VOL
};

/**
 * Long option values for the 'verify' command.
 */
enum
{
    VKAT_VERIFY_OPT_TAG = 900
};

/**
 * Long option values for the 'selftest' command.
 */
enum
{
    VKAT_SELFTEST_OPT_ATS_GUEST_ADDR = 900,
    VKAT_SELFTEST_OPT_ATS_GUEST_PORT,
    VKAT_SELFTEST_OPT_ATS_VALKIT_ADDR,
    VKAT_SELFTEST_OPT_ATS_VALKIT_PORT
};

/**
 * Common command line parameters.
 */
static const RTGETOPTDEF g_aCmdCommonOptions[] =
{
    { "--quiet",            'q',                                        RTGETOPT_REQ_NOTHING },
    { "--verbose",          'v',                                        RTGETOPT_REQ_NOTHING },
    { "--debug-audio",      AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_ENABLE,      RTGETOPT_REQ_NOTHING },
    { "--debug-audio-path", AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_PATH,        RTGETOPT_REQ_STRING  },
};

/**
 * Command line parameters for test mode.
 */
static const RTGETOPTDEF g_aCmdTestOptions[] =
{
    { "--backend",           'b',                          RTGETOPT_REQ_STRING  },
    { "--drvaudio",          'd',                          RTGETOPT_REQ_NOTHING },
    { "--exclude",           'e',                          RTGETOPT_REQ_UINT32  },
    { "--exclude-all",       'a',                          RTGETOPT_REQ_NOTHING },
    { "--mode",              VKAT_TEST_OPT_MODE,           RTGETOPT_REQ_STRING  },
    { "--guest-ats-address", VKAT_TEST_OPT_GUEST_ATS_ADDR, RTGETOPT_REQ_STRING  },
    { "--guest-ats-port",    VKAT_TEST_OPT_GUEST_ATS_PORT, RTGETOPT_REQ_UINT32  },
    { "--host-ats-address",  VKAT_TEST_OPT_HOST_ATS_ADDR,  RTGETOPT_REQ_STRING  },
    { "--host-ats-port",     VKAT_TEST_OPT_HOST_ATS_PORT,  RTGETOPT_REQ_UINT32  },
    { "--include",           'i',                          RTGETOPT_REQ_UINT32  },
    { "--outdir",            VKAT_TEST_OPT_OUTDIR,         RTGETOPT_REQ_STRING  },
    { "--count",             VKAT_TEST_OPT_COUNT,          RTGETOPT_REQ_UINT32  },
    { "--device",            VKAT_TEST_OPT_DEV,            RTGETOPT_REQ_STRING  },
    { "--pause",             VKAT_TEST_OPT_PAUSE,          RTGETOPT_REQ_UINT32  },
    { "--pcm-bit",           VKAT_TEST_OPT_PCM_BIT,        RTGETOPT_REQ_UINT8   },
    { "--pcm-chan",          VKAT_TEST_OPT_PCM_CHAN,       RTGETOPT_REQ_UINT8   },
    { "--pcm-hz",            VKAT_TEST_OPT_PCM_HZ,         RTGETOPT_REQ_UINT16  },
    { "--pcm-signed",        VKAT_TEST_OPT_PCM_SIGNED,     RTGETOPT_REQ_BOOL    },
    { "--tag",               VKAT_TEST_OPT_TAG,            RTGETOPT_REQ_STRING  },
    { "--tempdir",           VKAT_TEST_OPT_TEMPDIR,        RTGETOPT_REQ_STRING  },
    { "--volume",            VKAT_TEST_OPT_VOL,            RTGETOPT_REQ_UINT8   }
};

/**
 * Command line parameters for verification mode.
 */
static const RTGETOPTDEF g_aCmdVerifyOptions[] =
{
    { "--tag",              VKAT_VERIFY_OPT_TAG,          RTGETOPT_REQ_STRING  }
};

/** Terminate ASAP if set.  Set on Ctrl-C. */
bool volatile    g_fTerminate = false;
/** The release logger. */
PRTLOGGER        g_pRelLogger = NULL;
/** The test handle. */
RTTEST           g_hTest;
/** The current verbosity level. */
unsigned         g_uVerbosity = 0;
/** DrvAudio: Enable debug (or not). */
bool             g_fDrvAudioDebug = 0;
/** DrvAudio: The debug output path. */
const char      *g_pszDrvAudioDebug = NULL;


/**
 * Helper for handling --backend options.
 *
 * @returns Pointer to the specified backend, NULL if not found (error
 *          displayed).
 * @param   pszBackend      The backend option value.
 */
PCPDMDRVREG audioTestFindBackendOpt(const char *pszBackend)
{
    for (uintptr_t i = 0; i < RT_ELEMENTS(g_aBackends); i++)
        if (   strcmp(pszBackend, g_aBackends[i].pszName) == 0
            || strcmp(pszBackend, g_aBackends[i].pDrvReg->szName) == 0)
            return g_aBackends[i].pDrvReg;
    RTMsgError("Unknown backend: '%s'", pszBackend);
    return NULL;
}


/**
 * Overrides audio test base parameters with another set.
 *
 * @returns VBox status code.
 * @param   pBaseParms          Base parameters to override.
 * @param   pOverrideParms      Override parameters to use for overriding the base parameters.
 *
 * @note    Overriding a parameter depends on its type / default values.
 */
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms)
{
    RT_NOREF(pBaseParms, pOverrideParms);

    /** @todo Implement parameter overriding. */
    return VERR_NOT_IMPLEMENTED;
}


/*********************************************************************************************************************************
*   Test callbacks                                                                                                               *
*********************************************************************************************************************************/

/**
 * @copydoc FNAUDIOTESTSETUP
 */
static DECLCALLBACK(int) audioTestPlayToneSetup(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx)
{
    RT_NOREF(pTstEnv, pTstDesc, ppvCtx);

    pTstParmsAcq->enmType     = AUDIOTESTTYPE_TESTTONE_PLAY;

    PDMAudioPropsInit(&pTstParmsAcq->Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

    pTstParmsAcq->enmDir      = PDMAUDIODIR_OUT;
#ifdef DEBUG
    pTstParmsAcq->cIterations = 2;
#else
    pTstParmsAcq->cIterations = RTRandU32Ex(1, 10);
#endif
    pTstParmsAcq->idxCurrent  = 0;

    return VINF_SUCCESS;
}

/**
 * @copydoc FNAUDIOTESTEXEC
 */
static DECLCALLBACK(int) audioTestPlayToneExec(PAUDIOTESTENV pTstEnv, void *pvCtx, PAUDIOTESTPARMS pTstParms)
{
    RT_NOREF(pvCtx);

    int rc = VINF_SUCCESS;

    for (uint32_t i = 0; i < pTstParms->cIterations; i++)
    {
        AudioTestToneParamsInitRandom(&pTstParms->TestTone, &pTstParms->Props);

        PAUDIOTESTTONEPARMS const pToneParms = &pTstParms->TestTone;
        rc = AudioTestSvcClientToneRecord(&pTstEnv->u.Host.AtsClValKit, pToneParms);
        if (RT_SUCCESS(rc))
            rc = AudioTestSvcClientTonePlay(&pTstEnv->u.Host.AtsClGuest, pToneParms);

        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "Playing tone failed\n");
    }

    return rc;
}

/**
 * @copydoc FNAUDIOTESTDESTROY
 */
static DECLCALLBACK(int) audioTestPlayToneDestroy(PAUDIOTESTENV pTstEnv, void *pvCtx)
{
    RT_NOREF(pTstEnv, pvCtx);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNAUDIOTESTSETUP
 */
static DECLCALLBACK(int) audioTestRecordToneSetup(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx)
{
    RT_NOREF(pTstEnv, pTstDesc, ppvCtx);

    pTstParmsAcq->enmType     = AUDIOTESTTYPE_TESTTONE_RECORD;

    PDMAudioPropsInit(&pTstParmsAcq->Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

    pTstParmsAcq->enmDir      = PDMAUDIODIR_IN;
#ifdef DEBUG
    pTstParmsAcq->cIterations = 2;
#else
    pTstParmsAcq->cIterations = RTRandU32Ex(1, 10);
#endif
    pTstParmsAcq->idxCurrent  = 0;

    return VINF_SUCCESS;
}

/**
 * @copydoc FNAUDIOTESTEXEC
 */
static DECLCALLBACK(int) audioTestRecordToneExec(PAUDIOTESTENV pTstEnv, void *pvCtx, PAUDIOTESTPARMS pTstParms)
{
    RT_NOREF(pvCtx);

    int rc = VINF_SUCCESS;

    for (uint32_t i = 0; i < pTstParms->cIterations; i++)
    {
        pTstParms->TestTone.Props      = pTstParms->Props;
#ifdef DEBUG_andy
        pTstParms->TestTone.msDuration = RTRandU32Ex(50 /* ms */, 2000);
#else
        pTstParms->TestTone.msDuration = RTRandU32Ex(50 /* ms */, RT_MS_30SEC); /** @todo Record even longer? */
#endif
        /*
         * 1. Arm the ValKit ATS with the recording parameters.
         */
        rc = AudioTestSvcClientTonePlay(&pTstEnv->u.Host.AtsClValKit, &pTstParms->TestTone);
        if (RT_SUCCESS(rc))
        {
            /*
             * 2. Tell the guest ATS to start recording.
             */
            rc = AudioTestSvcClientToneRecord(&pTstEnv->u.Host.AtsClGuest, &pTstParms->TestTone);
        }

        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "Recording tone failed\n");
    }

    return rc;
}

/**
 * @copydoc FNAUDIOTESTDESTROY
 */
static DECLCALLBACK(int) audioTestRecordToneDestroy(PAUDIOTESTENV pTstEnv, void *pvCtx)
{
    RT_NOREF(pTstEnv, pvCtx);

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Test execution                                                                                                               *
*********************************************************************************************************************************/

static AUDIOTESTDESC g_aTests[] =
{
    /* pszTest      fExcluded      pfnSetup */
    { "PlayTone",   false,         audioTestPlayToneSetup,       audioTestPlayToneExec,      audioTestPlayToneDestroy },
    { "RecordTone", false,         audioTestRecordToneSetup,     audioTestRecordToneExec,    audioTestRecordToneDestroy }
};

/**
 * Runs one specific audio test.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running the test.
 * @param   pTstDesc            Test to run.
 * @param   uSeq                Test sequence # in case there are more tests.
 * @param   pOverrideParms      Test parameters for overriding the actual test parameters. Optional.
 */
static int audioTestOne(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc,
                        unsigned uSeq, PAUDIOTESTPARMS pOverrideParms)
{
    RT_NOREF(uSeq);

    int rc;

    AUDIOTESTPARMS TstParms;
    audioTestParmsInit(&TstParms);

    RTTestSub(g_hTest, pTstDesc->pszName);

    if (pTstDesc->fExcluded)
    {
        RTTestSkipped(g_hTest, "Excluded from list");
        return VINF_SUCCESS;
    }

    void *pvCtx = NULL; /* Test-specific opaque context. Optional and can be NULL. */

    if (pTstDesc->pfnSetup)
    {
        rc = pTstDesc->pfnSetup(pTstEnv, pTstDesc, &TstParms, &pvCtx);
        if (RT_FAILURE(rc))
        {
            RTTestFailed(g_hTest, "Test setup failed with %Rrc\n", rc);
            return rc;
        }
    }

    audioTestCombineParms(&TstParms, pOverrideParms);

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test #%u: %RU32 iterations\n", uSeq, TstParms.cIterations);

    if (TstParms.Dev.pszName && strlen(TstParms.Dev.pszName)) /** @todo Refine this check. Use pszId for starters! */
        rc = audioTestDeviceOpen(&TstParms.Dev);

    AssertPtr(pTstDesc->pfnExec);
    rc = pTstDesc->pfnExec(pTstEnv, pvCtx, &TstParms);
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Test failed with %Rrc\n", rc);

    RTTestSubDone(g_hTest);

    if (pTstDesc->pfnDestroy)
    {
        int rc2 = pTstDesc->pfnDestroy(pTstEnv, pvCtx);
        if (RT_SUCCESS(rc))
            rc = rc2;

        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Test destruction failed with %Rrc\n", rc2);
    }

    rc = audioTestDeviceClose(&TstParms.Dev);

    audioTestParmsDestroy(&TstParms);

    return rc;
}

/**
 * Runs all specified tests in a row.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running all tests.
 * @param   pOverrideParms      Test parameters for (some / all) specific test parameters. Optional.
 */
int audioTestWorker(PAUDIOTESTENV pTstEnv, PAUDIOTESTPARMS pOverrideParms)
{
    int rc = VINF_SUCCESS;

    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Guest ATS running\n");

        while (!g_fTerminate)
            RTThreadSleep(100);

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Shutting down guest ATS ...\n");

        int rc2 = AudioTestSvcShutdown(&pTstEnv->u.Guest.Srv);
        if (RT_SUCCESS(rc))
            rc = rc2;

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Guest ATS shutdown complete\n");
    }
    else if (pTstEnv->enmMode == AUDIOTESTMODE_HOST)
    {
        /* Generate tags for the host and guest side. */
        char szTagHost [AUDIOTEST_TAG_MAX];
        char szTagGuest[AUDIOTEST_TAG_MAX];

        rc = RTStrPrintf2(szTagHost, sizeof(szTagHost),   "%s-host",  pTstEnv->szTag);
        AssertRCReturn(rc, rc);
        rc = RTStrPrintf2(szTagGuest, sizeof(szTagGuest), "%s-guest", pTstEnv->szTag);
        AssertRCReturn(rc, rc);

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Guest test set tag is '%s'\n", szTagGuest);
        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Host test set tag is '%s'\n", szTagHost);

        rc = AudioTestSvcClientTestSetBegin(&pTstEnv->u.Host.AtsClValKit, szTagHost);
        if (RT_SUCCESS(rc))
            rc = AudioTestSvcClientTestSetBegin(&pTstEnv->u.Host.AtsClGuest, szTagGuest);

        if (RT_SUCCESS(rc))
        {
            unsigned uSeq = 0;
            for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
            {
                int rc2 = audioTestOne(pTstEnv, &g_aTests[i], uSeq, pOverrideParms);
                if (RT_SUCCESS(rc))
                    rc = rc2;

                if (!g_aTests[i].fExcluded)
                    uSeq++;

                if (g_fTerminate)
                    break;
            }

            int rc2 = AudioTestSvcClientTestSetEnd(&pTstEnv->u.Host.AtsClGuest, szTagGuest);
            if (RT_SUCCESS(rc))
                rc = rc2;
            rc2 = AudioTestSvcClientTestSetEnd(&pTstEnv->u.Host.AtsClValKit, szTagHost);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
        AssertFailed();

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Test worker failed with %Rrc", rc);

    return rc;
}

/** Option help for the 'test' command.   */
static DECLCALLBACK(const char *) audioTestCmdTestHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'd':                          return "Go via DrvAudio instead of directly interfacing with the backend";
        case VKAT_TEST_OPT_DEV:            return "Use the specified audio device";
        case VKAT_TEST_OPT_GUEST_ATS_ADDR: return "Address of guest ATS to connect to.";
        case VKAT_TEST_OPT_GUEST_ATS_PORT: return "Port of guest ATS to connect to [6052].";
        case VKAT_TEST_OPT_HOST_ATS_ADDR:  return "Address of host ATS to connect to.";
        case VKAT_TEST_OPT_HOST_ATS_PORT:  return "Port of host ATS to connect to [6052].";
        case VKAT_TEST_OPT_MODE:           return "Specifies the mode this program runs at";
        case 'e':                          return "Exclude the given test id from the list";
        case 'a':                          return "Exclude all tests from the list (useful to enable single tests later with --include)";
        case 'i':                          return "Include the given test id in the list";
    }
    return NULL;
}

/**
 * Main (entry) function for the testing functionality of VKAT.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioTestMain(PRTGETOPTSTATE pGetState)
{
    AUDIOTESTENV TstEnv;
    RT_ZERO(TstEnv);

    AUDIOTESTPARMS TstCust;
    audioTestParmsInit(&TstCust);

    const char *pszDevice     = NULL; /* Custom device to use. Can be NULL if not being used. */
    const char *pszTag        = NULL; /* Custom tag to use. Can be NULL if not being used. */
    PCPDMDRVREG pDrvReg       = g_aBackends[0].pDrvReg;
    bool        fWithDrvAudio = false;
    uint8_t     cPcmSampleBit = 0;
    uint8_t     cPcmChannels  = 0;
    uint32_t    uPcmHz        = 0;
    bool        fPcmSigned    = true;

    const char *pszGuestTcpAddr  = NULL;
    uint16_t    uGuestTcpPort    = ATS_TCP_GUEST_DEFAULT_PORT;
    const char *pszValKitTcpAddr = NULL;
    uint16_t    uValKitTcpPort   = ATS_TCP_HOST_DEFAULT_PORT;

    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'a':
                for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
                    g_aTests[i].fExcluded = true;
                break;

            case 'b':
                pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'd':
                fWithDrvAudio = true;
                break;

            case 'e':
                if (ValueUnion.u32 >= RT_ELEMENTS(g_aTests))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid test number %u passed to --exclude", ValueUnion.u32);
                g_aTests[ValueUnion.u32].fExcluded = true;
                break;

            case VKAT_TEST_OPT_GUEST_ATS_ADDR:
                pszGuestTcpAddr = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_GUEST_ATS_PORT:
                uGuestTcpPort = ValueUnion.u32;
                break;

            case VKAT_TEST_OPT_HOST_ATS_ADDR:
                pszValKitTcpAddr = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_HOST_ATS_PORT:
                uValKitTcpPort = ValueUnion.u32;
                break;

            case VKAT_TEST_OPT_MODE:
                if (TstEnv.enmMode != AUDIOTESTMODE_UNKNOWN)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Test mode (guest / host) already specified");
                TstEnv.enmMode = RTStrICmp(ValueUnion.psz, "guest") == 0 ? AUDIOTESTMODE_GUEST : AUDIOTESTMODE_HOST;
                break;

            case 'i':
                if (ValueUnion.u32 >= RT_ELEMENTS(g_aTests))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid test number %u passed to --include", ValueUnion.u32);
                g_aTests[ValueUnion.u32].fExcluded = false;
                break;

            case VKAT_TEST_OPT_COUNT:
                return RTMsgErrorExitFailure("Not yet implemented!");

            case VKAT_TEST_OPT_DEV:
                pszDevice = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_PAUSE:
                return RTMsgErrorExitFailure("Not yet implemented!");

            case VKAT_TEST_OPT_OUTDIR:
                rc = RTStrCopy(TstEnv.szPathOut, sizeof(TstEnv.szPathOut), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExitFailure("Failed to copy out directory: %Rrc", rc);
                break;

            case VKAT_TEST_OPT_PCM_BIT:
                cPcmSampleBit = ValueUnion.u8;
                break;

            case VKAT_TEST_OPT_PCM_CHAN:
                cPcmChannels = ValueUnion.u8;
                break;

            case VKAT_TEST_OPT_PCM_HZ:
                uPcmHz = ValueUnion.u32;
                break;

            case VKAT_TEST_OPT_PCM_SIGNED:
                fPcmSigned = ValueUnion.f;
                break;

            case VKAT_TEST_OPT_TAG:
                pszTag = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_TEMPDIR:
                rc = RTStrCopy(TstEnv.szPathTemp, sizeof(TstEnv.szPathTemp), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Temp dir invalid, rc=%Rrc", rc);
                break;

            case VKAT_TEST_OPT_VOL:
                TstCust.TestTone.uVolumePercent = ValueUnion.u8;
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

    /* Initialize the custom test parameters with sensible defaults if nothing else is given. */
    PDMAudioPropsInit(&TstCust.TestTone.Props,
                      cPcmSampleBit ? cPcmSampleBit / 8 : 2 /* 16-bit */, fPcmSigned, cPcmChannels ? cPcmChannels : 2,
                      uPcmHz ? uPcmHz : 44100);

    if (TstEnv.enmMode == AUDIOTESTMODE_UNKNOWN)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No test mode specified!\n");

    if (TstEnv.enmMode == AUDIOTESTMODE_HOST)
    {
        if (!pszGuestTcpAddr)
            return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--guest-ats-address missing\n");
    }

    /* For now all tests have the same test environment. */
    rc = audioTestEnvInit(&TstEnv, pDrvReg, fWithDrvAudio,
                          pszValKitTcpAddr, uValKitTcpPort,
                          pszGuestTcpAddr, uGuestTcpPort);
    if (RT_SUCCESS(rc))
    {
        audioTestWorker(&TstEnv, &TstCust);
        audioTestEnvDestroy(&TstEnv);
    }

    audioTestParmsDestroy(&TstCust);

    if (RT_FAILURE(rc)) /* Let us know that something went wrong in case we forgot to mention it. */
        RTTestFailed(g_hTest, "Testing failed with %Rrc\n", rc);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}


/*********************************************************************************************************************************
*   Command: verify                                                                                                              *
*********************************************************************************************************************************/

static int audioVerifyOpenTestSet(const char *pszPathSet, PAUDIOTESTSET pSet)
{
    int rc;

    char szPathExtracted[RTPATH_MAX];

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Opening test set '%s'\n", pszPathSet);

    const bool fPacked = AudioTestSetIsPacked(pszPathSet);
    if (fPacked)
    {
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test set is an archive and needs to be unpacked\n");

        char szPathTemp[RTPATH_MAX];
        rc = RTPathTemp(szPathTemp, sizeof(szPathTemp));
        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using temporary directory '%s'\n", szPathTemp);

            rc = RTPathJoin(szPathExtracted, sizeof(szPathExtracted), szPathTemp, "vkat-XXXX");
            if (RT_SUCCESS(rc))
            {
                rc = RTDirCreateTemp(szPathExtracted, 0755);
                if (RT_SUCCESS(rc))
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Unpacking archive to '%s'\n", szPathExtracted);
                    rc = AudioTestSetUnpack(pszPathSet, szPathExtracted);
                    if (RT_SUCCESS(rc))
                        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Archive successfully unpacked\n");
                }
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        rc = AudioTestSetOpen(pSet, fPacked ? szPathExtracted : pszPathSet);

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Unable to open / unpack test set archive: %Rrc", rc);

    return rc;
}

/**
 * Verifies one single test set.
 *
 * @returns VBox status code.
 * @param   pszPathSetA         Absolute path to test set A.
 * @param   pszPathSetB         Absolute path to test set B.
 */
static int audioVerifyOne(const char *pszPathSetA, const char *pszPathSetB)
{
    RTTestSubF(g_hTest, "Verifying");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Verifying test set '%s' with test set '%s'\n", pszPathSetA, pszPathSetB);

    AUDIOTESTSET SetA, SetB;
    int rc = audioVerifyOpenTestSet(pszPathSetA, &SetA);
    if (RT_SUCCESS(rc))
        rc = audioVerifyOpenTestSet(pszPathSetB, &SetB);

    if (RT_SUCCESS(rc))
    {
        AUDIOTESTERRORDESC errDesc;
        rc = AudioTestSetVerify(&SetA, &SetB, &errDesc);
        if (RT_SUCCESS(rc))
        {
            if (AudioTestErrorDescFailed(&errDesc))
            {
                /** @todo Use some AudioTestErrorXXX API for enumeration here later. */
                PAUDIOTESTERRORENTRY pErrEntry;
                RTListForEach(&errDesc.List, pErrEntry, AUDIOTESTERRORENTRY, Node)
                    RTTestFailed(g_hTest, pErrEntry->szDesc);
            }
            else
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Verification successful\n");

            AudioTestErrorDescDestroy(&errDesc);
        }
        else
            RTTestFailed(g_hTest, "Verification failed with %Rrc", rc);
    }

    AudioTestSetClose(&SetA);
    AudioTestSetClose(&SetB);

    RTTestSubDone(g_hTest);

    return rc;
}

/**
 * Main (entry) function for the verification functionality of VKAT.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioVerifyMain(PRTGETOPTSTATE pGetState)
{
    /*
     * Parse options and process arguments.
     */
    const char *apszSets[2] = { NULL, NULL };
    unsigned    iTestSet    = 0;

    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (rc)
        {
            case VKAT_VERIFY_OPT_TAG:
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (iTestSet == 0)
                    RTTestBanner(g_hTest);
                if (iTestSet >= RT_ELEMENTS(apszSets))
                    return RTMsgErrorExitFailure("Only two test sets can be verified at one time");
                apszSets[iTestSet++] = ValueUnion.psz;
                break;

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    if (!iTestSet)
        return RTMsgErrorExitFailure("At least one test set must be specified");

    /*
     * If only test set A is given, default to the current directory
     * for test set B.
     */
    char szDirCur[RTPATH_MAX];
    if (iTestSet == 1)
    {
        rc = RTPathGetCurrent(szDirCur, sizeof(szDirCur));
        if (RT_SUCCESS(rc))
            apszSets[1] = szDirCur;
        else
            RTTestFailed(g_hTest, "Failed to retrieve current directory: %Rrc", rc);
    }

    if (RT_SUCCESS(rc))
        audioVerifyOne(apszSets[0], apszSets[1]);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}


/*********************************************************************************************************************************
*   Command: enum                                                                                                                *
*********************************************************************************************************************************/

/**
 * Options for 'enum'.
 */
static const RTGETOPTDEF g_aCmdEnumOptions[] =
{
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
};

/** The 'enum' command option help. */
static DECLCALLBACK(const char *) audioTestCmdEnumHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'b': return "The audio backend to use.";
        default:  return NULL;
    }
}

/**
 * The 'enum' command handler.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioTestCmdEnumHandler(PRTGETOPTSTATE pGetState)
{
    /*
     * Parse options.
     */
    /* Option values: */
    PCPDMDRVREG pDrvReg = g_aBackends[0].pDrvReg;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'b':
                pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Do the enumeration.
     */
    RTEXITCODE          rcExit = RTEXITCODE_FAILURE;
    AUDIOTESTDRVSTACK   DrvStack;
    rc = audioTestDriverStackInit(&DrvStack, pDrvReg, false /*fWithDrvAudio*/);
    if (RT_SUCCESS(rc))
    {
        if (DrvStack.pIHostAudio->pfnGetDevices)
        {
            PDMAUDIOHOSTENUM Enum;
            rc = DrvStack.pIHostAudio->pfnGetDevices(DrvStack.pIHostAudio, &Enum);
            if (RT_SUCCESS(rc))
            {
                RTPrintf("Found %u device%s\n", Enum.cDevices, Enum.cDevices != 1 ? "s" : "");

                PPDMAUDIOHOSTDEV pHostDev;
                RTListForEach(&Enum.LstDevices, pHostDev, PDMAUDIOHOSTDEV, ListEntry)
                {
                    RTPrintf("\nDevice \"%s\":\n", pHostDev->pszName);

                    char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
                    if (pHostDev->cMaxInputChannels && !pHostDev->cMaxOutputChannels && pHostDev->enmUsage == PDMAUDIODIR_IN)
                        RTPrintf("    Input:  max %u channels (%s)\n",
                                 pHostDev->cMaxInputChannels, PDMAudioHostDevFlagsToString(szFlags, pHostDev->fFlags));
                    else if (!pHostDev->cMaxInputChannels && pHostDev->cMaxOutputChannels && pHostDev->enmUsage == PDMAUDIODIR_OUT)
                        RTPrintf("    Output: max %u channels (%s)\n",
                                 pHostDev->cMaxOutputChannels, PDMAudioHostDevFlagsToString(szFlags, pHostDev->fFlags));
                    else
                        RTPrintf("    %s: max %u output channels, max %u input channels (%s)\n",
                                 PDMAudioDirGetName(pHostDev->enmUsage), pHostDev->cMaxOutputChannels,
                                 pHostDev->cMaxInputChannels, PDMAudioHostDevFlagsToString(szFlags, pHostDev->fFlags));

                    if (pHostDev->pszId && *pHostDev->pszId)
                        RTPrintf("    ID:     \"%s\"\n", pHostDev->pszId);
                }

                PDMAudioHostEnumDelete(&Enum);
            }
            else
                rcExit = RTMsgErrorExitFailure("Enumeration failed: %Rrc\n", rc);
        }
        else
            rcExit = RTMsgErrorExitFailure("Enumeration not supported by backend '%s'\n", pDrvReg->szName);
        audioTestDriverStackDelete(&DrvStack);
    }
    else
        rcExit = RTMsgErrorExitFailure("Driver stack construction failed: %Rrc", rc);
    return RTEXITCODE_SUCCESS;
}

/**
 * Options for 'play'.
 */
static const RTGETOPTDEF g_aCmdPlayOptions[] =
{
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--channels",         'c',                          RTGETOPT_REQ_UINT8 },
    { "--hz",               'f',                          RTGETOPT_REQ_UINT32 },
    { "--frequency",        'f',                          RTGETOPT_REQ_UINT32 },
    { "--sample-size",      'z',                          RTGETOPT_REQ_UINT8 },
    { "--output-device",    'o',                          RTGETOPT_REQ_STRING  },
    { "--with-drv-audio",   'd',                          RTGETOPT_REQ_NOTHING },
    { "--with-mixer",       'm',                          RTGETOPT_REQ_NOTHING },
};

/** The 'play' command option help. */
static DECLCALLBACK(const char *) audioTestCmdPlayHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'b': return "The audio backend to use.";
        case 'c': return "Number of backend output channels";
        case 'd': return "Go via DrvAudio instead of directly interfacing with the backend.";
        case 'f': return "Output frequency (Hz)";
        case 'z': return "Output sample size (bits)";
        case 'm': return "Go via the mixer.";
        case 'o': return "The ID of the output device to use.";
        default:  return NULL;
    }
}

/**
 * The 'play' command handler.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioTestCmdPlayHandler(PRTGETOPTSTATE pGetState)
{
    /* Option values: */
    PCPDMDRVREG pDrvReg             = g_aBackends[0].pDrvReg;
    uint32_t    cMsBufferSize       = UINT32_MAX;
    uint32_t    cMsPreBuffer        = UINT32_MAX;
    uint32_t    cMsSchedulingHint   = UINT32_MAX;
    const char *pszDevId            = NULL;
    bool        fWithDrvAudio       = false;
    bool        fWithMixer          = false;
    uint8_t     cbSample            = 0;
    uint8_t     cChannels           = 0;
    uint32_t    uHz                 = 0;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'b':
                pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'c':
                cChannels = ValueUnion.u8;
                break;

            case 'd':
                fWithDrvAudio = true;
                break;

            case 'f':
                uHz = ValueUnion.u32;
                break;

            case 'm':
                fWithMixer = true;
                break;

            case 'o':
                pszDevId = ValueUnion.psz;
                break;

            case 'z':
                cbSample = ValueUnion.u8 / 8;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                RTEXITCODE rcExit = audioTestPlayOne(ValueUnion.psz, pDrvReg, pszDevId, cMsBufferSize, cMsPreBuffer,
                                                     cMsSchedulingHint, cChannels, cbSample, uHz, fWithDrvAudio, fWithMixer);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }
    return RTEXITCODE_SUCCESS;
}


/*********************************************************************************************************************************
*   Command: rec                                                                                                                 *
*********************************************************************************************************************************/

/**
 * Options for 'rec'.
 */
static const RTGETOPTDEF g_aCmdRecOptions[] =
{
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--channels",         'c',                          RTGETOPT_REQ_UINT8 },
    { "--hz",               'f',                          RTGETOPT_REQ_UINT32 },
    { "--frequency",        'f',                          RTGETOPT_REQ_UINT32 },
    { "--sample-size",      'z',                          RTGETOPT_REQ_UINT8 },
    { "--input-device",     'i',                          RTGETOPT_REQ_STRING  },
    { "--wav-channels",     'C',                          RTGETOPT_REQ_UINT8 },
    { "--wav-hz",           'F',                          RTGETOPT_REQ_UINT32 },
    { "--wav-frequency",    'F',                          RTGETOPT_REQ_UINT32 },
    { "--wav-sample-size",  'Z',                          RTGETOPT_REQ_UINT8 },
    { "--with-drv-audio",   'd',                          RTGETOPT_REQ_NOTHING },
    { "--with-mixer",       'm',                          RTGETOPT_REQ_NOTHING },
    { "--max-frames",       'r',                          RTGETOPT_REQ_UINT64 },
    { "--max-sec",          's',                          RTGETOPT_REQ_UINT64 },
    { "--max-seconds",      's',                          RTGETOPT_REQ_UINT64 },
    { "--max-ms",           't',                          RTGETOPT_REQ_UINT64 },
    { "--max-milliseconds", 't',                          RTGETOPT_REQ_UINT64 },
    { "--max-ns",           'T',                          RTGETOPT_REQ_UINT64 },
    { "--max-nanoseconds",  'T',                          RTGETOPT_REQ_UINT64 },
};

/** The 'rec' command option help. */
static DECLCALLBACK(const char *) audioTestCmdRecHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'b': return "The audio backend to use.";
        case 'c': return "Number of backend input channels";
        case 'C': return "Number of wave-file channels";
        case 'd': return "Go via DrvAudio instead of directly interfacing with the backend.";
        case 'f': return "Input frequency (Hz)";
        case 'F': return "Wave-file frequency (Hz)";
        case 'z': return "Input sample size (bits)";
        case 'Z': return "Wave-file sample size (bits)";
        case 'm': return "Go via the mixer.";
        case 'i': return "The ID of the input device to use.";
        case 'r': return "Max recording duration in frames.";
        case 's': return "Max recording duration in seconds.";
        case 't': return "Max recording duration in milliseconds.";
        case 'T': return "Max recording duration in nanoseconds.";
        default:  return NULL;
    }
}

/**
 * The 'play' command handler.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioTestCmdRecHandler(PRTGETOPTSTATE pGetState)
{
    /* Option values: */
    PCPDMDRVREG pDrvReg             = g_aBackends[0].pDrvReg;
    uint32_t    cMsBufferSize       = UINT32_MAX;
    uint32_t    cMsPreBuffer        = UINT32_MAX;
    uint32_t    cMsSchedulingHint   = UINT32_MAX;
    const char *pszDevId            = NULL;
    bool        fWithDrvAudio       = false;
    bool        fWithMixer          = false;
    uint8_t     cbSample            = 0;
    uint8_t     cChannels           = 0;
    uint32_t    uHz                 = 0;
    uint8_t     cbWaveSample        = 0;
    uint8_t     cWaveChannels       = 0;
    uint32_t    uWaveHz             = 0;
    uint64_t    cMaxFrames          = UINT64_MAX;
    uint64_t    cNsMaxDuration      = UINT64_MAX;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'b':
                pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'c':
                cChannels = ValueUnion.u8;
                break;

            case 'C':
                cWaveChannels = ValueUnion.u8;
                break;

            case 'd':
                fWithDrvAudio = true;
                break;

            case 'f':
                uHz = ValueUnion.u32;
                break;

            case 'F':
                uWaveHz = ValueUnion.u32;
                break;

            case 'i':
                pszDevId = ValueUnion.psz;
                break;

            case 'm':
                fWithMixer = true;
                break;

            case 'r':
                cMaxFrames = ValueUnion.u64;
                break;

            case 's':
                cNsMaxDuration = ValueUnion.u64 >= UINT64_MAX / RT_NS_1SEC ? UINT64_MAX : ValueUnion.u64 * RT_NS_1SEC;
                break;

            case 't':
                cNsMaxDuration = ValueUnion.u64 >= UINT64_MAX / RT_NS_1MS  ? UINT64_MAX : ValueUnion.u64 * RT_NS_1MS;
                break;

            case 'T':
                cNsMaxDuration = ValueUnion.u64;
                break;

            case 'z':
                cbSample = ValueUnion.u8 / 8;
                break;

            case 'Z':
                cbWaveSample = ValueUnion.u8 / 8;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                RTEXITCODE rcExit = audioTestRecOne(ValueUnion.psz, cWaveChannels, cbWaveSample, uWaveHz,
                                                    pDrvReg, pszDevId, cMsBufferSize, cMsPreBuffer, cMsSchedulingHint,
                                                    cChannels, cbSample, uHz, fWithDrvAudio, fWithMixer,
                                                    cMaxFrames, cNsMaxDuration);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }
    return RTEXITCODE_SUCCESS;
}


/*********************************************************************************************************************************
*   Command: selftest                                                                                                            *
*********************************************************************************************************************************/

/**
 * Command line parameters for self-test mode.
 */
static const RTGETOPTDEF g_aCmdSelftestOptions[] =
{
    { "--ats-guest-addr",   VKAT_SELFTEST_OPT_ATS_GUEST_ADDR,   RTGETOPT_REQ_STRING  },
    { "--ats-guest-port",   VKAT_SELFTEST_OPT_ATS_GUEST_PORT,   RTGETOPT_REQ_UINT32  },
    { "--ats-valkit-addr",  VKAT_SELFTEST_OPT_ATS_GUEST_ADDR,   RTGETOPT_REQ_STRING  },
    { "--ats-valkit-port",  VKAT_SELFTEST_OPT_ATS_GUEST_PORT,   RTGETOPT_REQ_UINT32  },
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
        case 'b': return "The audio backend to use.";
        case 'd': return "Go via DrvAudio instead of directly interfacing with the backend.";
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

    /* Go with the platform's default bakcend if nothing else is specified. */
    Ctx.Guest.pDrvReg = g_aBackends[0].pDrvReg;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case VKAT_SELFTEST_OPT_ATS_GUEST_ADDR:
                rc = RTStrCopy(Ctx.Host.szGuestAtsAddr, sizeof(Ctx.Host.szGuestAtsAddr), ValueUnion.psz);
                break;

            case VKAT_SELFTEST_OPT_ATS_GUEST_PORT:
                Ctx.Host.uGuestAtsPort = ValueUnion.u32;
                break;

            case VKAT_SELFTEST_OPT_ATS_VALKIT_ADDR:
                rc = RTStrCopy(Ctx.Host.szValKitAtsAddr, sizeof(Ctx.Host.szValKitAtsAddr), ValueUnion.psz);
                break;

            case VKAT_SELFTEST_OPT_ATS_VALKIT_PORT:
                Ctx.Host.uValKitAtsPort = ValueUnion.u32;
                break;

            case 'a':
                for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
                    g_aTests[i].fExcluded = true;
                break;

            case 'b':
                Ctx.Guest.pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (Ctx.Guest.pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'd':
                Ctx.fWithDrvAudio = true;
                break;

            case 'e':
                if (ValueUnion.u32 >= RT_ELEMENTS(g_aTests))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid test number %u passed to --exclude", ValueUnion.u32);
                g_aTests[ValueUnion.u32].fExcluded = true;
                break;

            case 'i':
                if (ValueUnion.u32 >= RT_ELEMENTS(g_aTests))
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
 * Ctrl-C signal handler.
 *
 * This just sets g_fTerminate and hope it will be noticed soon.  It restores
 * the SIGINT action to default, so that a second Ctrl-C will have the normal
 * effect (just in case the code doesn't respond to g_fTerminate).
 */
static void audioTestSignalHandler(int iSig) RT_NOEXCEPT
{
    Assert(iSig == SIGINT); RT_NOREF(iSig);
    RTPrintf("Ctrl-C!\n");
    ASMAtomicWriteBool(&g_fTerminate, true);
    signal(SIGINT, SIG_DFL);
}


/**
 * Commands.
 */
static struct
{
    /** The command name. */
    const char     *pszCommand;
    /** The command handler.   */
    DECLCALLBACKMEMBER(RTEXITCODE, pfnHandler,(PRTGETOPTSTATE pGetState));

    /** Command description.   */
    const char     *pszDesc;
    /** Options array.  */
    PCRTGETOPTDEF   paOptions;
    /** Number of options in the option array. */
    size_t          cOptions;
    /** Gets help for an option. */
    DECLCALLBACKMEMBER(const char *, pfnOptionHelp,(PCRTGETOPTDEF pOpt));
} const g_aCommands[] =
{
    {
        "test",     audioTestMain,
        "Runs audio tests and creates an audio test set.",
        g_aCmdTestOptions,      RT_ELEMENTS(g_aCmdTestOptions),     audioTestCmdTestHelp
    },
    {
        "verify",   audioVerifyMain,
        "Verifies a formerly created audio test set.",
        g_aCmdVerifyOptions,    RT_ELEMENTS(g_aCmdVerifyOptions),   NULL,
    },
    {
        "enum",     audioTestCmdEnumHandler,
        "Enumerates audio devices.",
        g_aCmdEnumOptions,      RT_ELEMENTS(g_aCmdEnumOptions),     audioTestCmdEnumHelp,
    },
    {
        "play",     audioTestCmdPlayHandler,
        "Plays one or more wave files.",
        g_aCmdPlayOptions,      RT_ELEMENTS(g_aCmdPlayOptions),     audioTestCmdPlayHelp,
    },
    {
        "rec",      audioTestCmdRecHandler,
        "Records audio to a wave file.",
        g_aCmdRecOptions,       RT_ELEMENTS(g_aCmdRecOptions),      audioTestCmdRecHelp,
    },
    {
        "selftest", audioTestCmdSelftestHandler,
        "Performs self-tests.",
        g_aCmdSelftestOptions,  RT_ELEMENTS(g_aCmdSelftestOptions), audioTestCmdSelftestHelp,
    }
};

/**
 * Shows tool usage text.
 */
static RTEXITCODE audioTestUsage(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm, "usage: %s [global options] <command> [command-options]\n",
                 RTPathFilename(RTProcExecutablePath()));
    RTStrmPrintf(pStrm,
                 "\n"
                 "Global Options:\n"
                 "  --debug-audio\n"
                 "    Enables DrvAudio debugging.\n"
                 "  --debug-audio-path=<path>\n"
                 "    Tells DrvAudio where to put its debug output (wav-files).\n"
                 "  -q, --quiet\n"
                 "    Sets verbosity to zero.\n"
                 "  -v, --verbose\n"
                 "    Increase verbosity.\n"
                 "  -V, --version\n"
                 "    Displays version.\n"
                 "  -h, -?, --help\n"
                 "    Displays help.\n"
                 );

    for (uintptr_t iCmd = 0; iCmd < RT_ELEMENTS(g_aCommands); iCmd++)
    {
        RTStrmPrintf(pStrm,
                     "\n"
                     "Command '%s':\n"
                     "    %s\n"
                     "Options for '%s':\n",
                     g_aCommands[iCmd].pszCommand, g_aCommands[iCmd].pszDesc, g_aCommands[iCmd].pszCommand);
        PCRTGETOPTDEF const paOptions = g_aCommands[iCmd].paOptions;
        for (unsigned i = 0; i < g_aCommands[iCmd].cOptions; i++)
        {
            if (RT_C_IS_PRINT(paOptions[i].iShort))
                RTStrmPrintf(pStrm, "  -%c, %s\n", paOptions[i].iShort, paOptions[i].pszLong);
            else
                RTStrmPrintf(pStrm, "  %s\n", paOptions[i].pszLong);

            const char *pszHelp = NULL;
            if (g_aCommands[iCmd].pfnOptionHelp)
                pszHelp = g_aCommands[iCmd].pfnOptionHelp(&paOptions[i]);
            if (pszHelp)
                RTStrmPrintf(pStrm, "    %s\n", pszHelp);
        }
    }
    return RTEXITCODE_SUCCESS;
}

/**
 * Shows tool version.
 */
static RTEXITCODE audioTestVersion(void)
{
    RTPrintf("v0.0.1\n");
    return RTEXITCODE_SUCCESS;
}

/**
 * Shows the logo.
 *
 * @param   pStream             Output stream to show logo on.
 */
static void audioTestShowLogo(PRTSTREAM pStream)
{
    RTStrmPrintf(pStream, VBOX_PRODUCT " VKAT (Validation Kit Audio Test) Version " VBOX_VERSION_STRING " - r%s\n"
                 "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                 "All rights reserved.\n\n", RTBldCfgRevisionStr());
}

int main(int argc, char **argv)
{
    /*
     * Init IPRT and globals.
     */
    RTEXITCODE rcExit = RTTestInitAndCreate("AudioTest", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

#ifdef RT_OS_WINDOWS
    HRESULT hrc = CoInitializeEx(NULL /*pReserved*/, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrc))
        RTMsgWarning("CoInitializeEx failed: %#x", hrc);
#endif

    /*
     * Configure release logging to go to stderr.
     */
    static const char * const g_apszLogGroups[] = VBOX_LOGGROUP_NAMES;
    int rc = RTLogCreate(&g_pRelLogger, RTLOGFLAGS_PREFIX_THREAD, "all.e.l", "VKAT_RELEASE_LOG",
                         RT_ELEMENTS(g_apszLogGroups), g_apszLogGroups, RTLOGDEST_STDERR, NULL /*"vkat-release.log"*/);
    if (RT_SUCCESS(rc))
        RTLogRelSetDefaultInstance(g_pRelLogger);
    else
        RTMsgWarning("Failed to create release logger: %Rrc", rc);

    /*
     * Install a Ctrl-C signal handler.
     */
#ifdef RT_OS_WINDOWS
    signal(SIGINT, audioTestSignalHandler);
#else
    struct sigaction sa;
    RT_ZERO(sa);
    sa.sa_handler = audioTestSignalHandler;
    sigaction(SIGINT, &sa, NULL);
#endif

    /*
     * Process common options.
     */
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, g_aCmdCommonOptions,
                      RT_ELEMENTS(g_aCmdCommonOptions), 1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'q':
                g_uVerbosity = 0;
                if (g_pRelLogger)
                    RTLogGroupSettings(g_pRelLogger, "all=0 all.e");
                break;

            case 'v':
                g_uVerbosity++;
                if (g_pRelLogger)
                    RTLogGroupSettings(g_pRelLogger, g_uVerbosity == 1 ? "all.e.l" : g_uVerbosity == 2 ? "all.e.l.f" : "all=~0");
                break;

            case 'V':
                return audioTestVersion();

            case 'h':
                audioTestShowLogo(g_pStdOut);
                return audioTestUsage(g_pStdOut);

            case VINF_GETOPT_NOT_OPTION:
            {
                for (uintptr_t i = 0; i < RT_ELEMENTS(g_aCommands); i++)
                    if (strcmp(ValueUnion.psz, g_aCommands[i].pszCommand) == 0)
                    {
                        size_t const cCombinedOptions  = g_aCommands[i].cOptions + RT_ELEMENTS(g_aCmdCommonOptions);
                        PRTGETOPTDEF paCombinedOptions = (PRTGETOPTDEF)RTMemAlloc(cCombinedOptions * sizeof(RTGETOPTDEF));
                        if (paCombinedOptions)
                        {
                            memcpy(paCombinedOptions, g_aCmdCommonOptions, sizeof(g_aCmdCommonOptions));
                            memcpy(&paCombinedOptions[RT_ELEMENTS(g_aCmdCommonOptions)],
                                   g_aCommands[i].paOptions, g_aCommands[i].cOptions * sizeof(RTGETOPTDEF));

                            rc = RTGetOptInit(&GetState, argc, argv, paCombinedOptions, cCombinedOptions,
                                              GetState.iNext /*idxFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);
                            if (RT_SUCCESS(rc))
                            {

                                rcExit = g_aCommands[i].pfnHandler(&GetState);
                                RTMemFree(paCombinedOptions);
                                return rcExit;
                            }
                            return RTMsgErrorExitFailure("RTGetOptInit failed for '%s': %Rrc", ValueUnion.psz, rc);
                        }
                        return RTMsgErrorExitFailure("Out of memory!");
                    }
                RTMsgError("Unknown command '%s'!\n", ValueUnion.psz);
                audioTestUsage(g_pStdErr);
                return RTEXITCODE_SYNTAX;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    RTMsgError("No command specified!\n");
    audioTestUsage(g_pStdErr);
    return RTEXITCODE_SYNTAX;
}

