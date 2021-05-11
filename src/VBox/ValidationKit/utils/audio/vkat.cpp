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
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <package-generated.h>
#include "product-generated.h"

#include <VBox/version.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include "../../../Devices/Audio/AudioHlp.h"
#include "../../../Devices/Audio/AudioTest.h"
#include "../../../Devices/Audio/VBoxDDVKAT.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
struct AUDIOTESTENV;
struct AUDIOTESTDESC;
struct AUDIOTESTPARMS;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Audio test request data.
 */
typedef struct AUDIOTESTPARMS
{
    /** Specifies the test to run. */
    uint32_t                idxTest;
    /** How many iterations the test should be executed. */
    uint32_t                cIterations;
    /** Audio device to use. */
    PDMAUDIOHOSTDEV         Dev;
    /** How much to delay (wait, in ms) the test being executed. */
    RTMSINTERVAL            msDelay;
    /** The test type. */
    PDMAUDIODIR             enmDir;
    union
    {
        AUDIOTESTTONEPARMS  ToneParms;
    };
} AUDIOTESTPARMS;
/** Pointer to a test parameter structure. */
typedef AUDIOTESTPARMS *PAUDIOTESTPARMS;

/**
 * Callback to set up the test parameters for a specific test.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS    if setting the parameters up succeeded. Any other error code
 *                          otherwise indicating the kind of error.
 * @param   pszTest         Test name.
 * @param   pTstParmsAcq    The audio test parameters to set up.
 */
typedef DECLCALLBACKTYPE(int, FNAUDIOTESTSETUP,(AUDIOTESTENV *pTstEnv, AUDIOTESTDESC *pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx));
/** Pointer to an audio test setup callback. */
typedef FNAUDIOTESTSETUP *PFNAUDIOTESTSETUP;

typedef DECLCALLBACKTYPE(int, FNAUDIOTESTEXEC,(AUDIOTESTENV *pTstEnv, void *pvCtx, PAUDIOTESTPARMS pTstParms));
/** Pointer to an audio test exec callback. */
typedef FNAUDIOTESTEXEC *PFNAUDIOTESTEXEC;

typedef DECLCALLBACKTYPE(int, FNAUDIOTESTDESTROY,(AUDIOTESTENV *pTstEnv, void *pvCtx));
/** Pointer to an audio test destroy callback. */
typedef FNAUDIOTESTDESTROY *PFNAUDIOTESTDESTROY;

/**
 * Audio test environment parameters.
 * Not necessarily bound to a specific test (can be reused).
 */
typedef struct AUDIOTESTENV
{
    /** The host (backend) driver to use. */
    PPDMIHOSTAUDIO   pDrvAudio;
    /** The current (last) audio device enumeration to use. */
    PDMAUDIOHOSTENUM DevEnm;
    /** The audio test set to use. */
    AUDIOTESTSET     Set;
} AUDIOTESTENV;
/** Pointer a audio test environment. */
typedef AUDIOTESTENV *PAUDIOTESTENV;

/**
 * Audio test descriptor.
 */
typedef struct AUDIOTESTDESC
{
    /** (Sort of) Descriptive test name. */
    const char             *pszName;
    /** Flag whether the test is excluded. */
    bool                    fExcluded;
    /** The setup callback. */
    PFNAUDIOTESTSETUP       pfnSetup;
    /** The exec callback. */
    PFNAUDIOTESTEXEC        pfnExec;
    /** The destruction callback. */
    PFNAUDIOTESTDESTROY     pfnDestroy;
} AUDIOTESTDESC;
/** Pointer a audio test descriptor. */
typedef AUDIOTESTDESC *PAUDIOTESTDESC;


/*********************************************************************************************************************************
*   Forward declarations                                                                                                         *
*********************************************************************************************************************************/
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Enumeration of test ("test") command line parameters.
 */
enum
{
    VKAT_TEST_OPT_COUNT = 900,
    VKAT_TEST_OPT_DEV,
    VKAT_TEST_OPT_OUTDIR,
    VKAT_TEST_OPT_PAUSE,
    VKAT_TEST_OPT_PCM_HZ,
    VKAT_TEST_OPT_PCM_BIT,
    VKAT_TEST_OPT_PCM_CHAN,
    VKAT_TEST_OPT_PCM_SIGNED,
    VKAT_TEST_OPT_TAG,
    VKAT_TEST_OPT_VOL
};

/**
 * Enumeration of verification ("verify") command line parameters.
 */
enum
{
    VKAT_VERIFY_OPT_TAG = 900
};

/**
 * Common command line parameters.
 */
static const RTGETOPTDEF g_aCmdCommonOptions[] =
{
    { "--help",             'h',                          RTGETOPT_REQ_NOTHING },
    { "--verbose",          'v',                          RTGETOPT_REQ_NOTHING }
};

/**
 * Command line parameters for test mode.
 */
static const RTGETOPTDEF g_aCmdTestOptions[] =
{
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--exclude",          'e',                          RTGETOPT_REQ_UINT32  },
    { "--exclude-all",      'a',                          RTGETOPT_REQ_NOTHING },
    { "--include",          'i',                          RTGETOPT_REQ_UINT32  },
    { "--outdir",           VKAT_TEST_OPT_OUTDIR,         RTGETOPT_REQ_STRING  },
    { "--count",            VKAT_TEST_OPT_COUNT,          RTGETOPT_REQ_UINT32  },
    { "--device",           VKAT_TEST_OPT_DEV,            RTGETOPT_REQ_STRING  },
    { "--pause",            VKAT_TEST_OPT_PAUSE,          RTGETOPT_REQ_UINT32  },
    { "--pcm-bit",          VKAT_TEST_OPT_PCM_BIT,        RTGETOPT_REQ_UINT8   },
    { "--pcm-chan",         VKAT_TEST_OPT_PCM_CHAN,       RTGETOPT_REQ_UINT8   },
    { "--pcm-hz",           VKAT_TEST_OPT_PCM_HZ,         RTGETOPT_REQ_UINT16  },
    { "--pcm-signed",       VKAT_TEST_OPT_PCM_SIGNED,     RTGETOPT_REQ_BOOL    },
    { "--tag",              VKAT_TEST_OPT_TAG,            RTGETOPT_REQ_STRING  },
    { "--volume",           VKAT_TEST_OPT_VOL,            RTGETOPT_REQ_UINT8   }
};

/**
 * Command line parameters for verification mode.
 */
static const RTGETOPTDEF g_aCmdVerifyOptions[] =
{
    { "--tag",              VKAT_VERIFY_OPT_TAG,          RTGETOPT_REQ_STRING  }
};

/** The test handle. */
static RTTEST g_hTest;
/** The driver instance data. */
PDMDRVINS g_DrvIns;
/** The current verbosity level. */
unsigned  g_uVerbosity = 0;

/*********************************************************************************************************************************
*   Test callbacks                                                                                                               *
*********************************************************************************************************************************/

/**
 * Setup callback for playing an output tone.
 *
 * @copydoc FNAUDIOTESTSETUP
 */
static DECLCALLBACK(int) audioTestPlayToneSetup(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx)
{
    RT_NOREF(pTstEnv, pTstDesc, pTstParmsAcq, ppvCtx);

//    PDMAudioPropsInit(&Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

    //AudioTestToneParamsInitRandom(&pTstParms->ToneParms, &pTstParms->ToneParms.Props);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) audioTestPlayToneExec(PAUDIOTESTENV pTstEnv, void *pvCtx, PAUDIOTESTPARMS pTstParms)
{
    RT_NOREF(pTstEnv, pvCtx, pTstParms);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) audioTestPlayToneDestroy(PAUDIOTESTENV pTstEnv, void *pvCtx)
{
    RT_NOREF(pTstEnv, pvCtx);

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Initializes an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvAudio           Audio driver to use.
 * @param   pszPathOut          Output path to use. If NULL, the system's temp directory will be used.
 * @þaram   pszTag              Tag name to use. If NULL, a generated UUID will be used.
 */
static int audioTestEnvInit(PAUDIOTESTENV pTstEnv, PPDMIHOSTAUDIO pDrvAudio, const char *pszPathOut, const char *pszTag)
{
    RT_BZERO(pTstEnv, sizeof(AUDIOTESTENV));

    pTstEnv->pDrvAudio = pDrvAudio;
    PDMAudioHostEnumInit(&pTstEnv->DevEnm);

    return AudioTestSetCreate(&pTstEnv->Set, pszPathOut, pszTag);
}

/**
 * Destroys an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to destroy.
 */
static void audioTestEnvDestroy(PAUDIOTESTENV pTstEnv)
{
    if (!pTstEnv)
        return;

    PDMAudioHostEnumDelete(&pTstEnv->DevEnm);

    AudioTestSetDestroy(&pTstEnv->Set);
}

/**
 * Initializes an audio test parameters set.
 *
 * @param   pTstParms           Test parameters set to initialize.
 */
static void audioTestParmsInit(PAUDIOTESTPARMS pTstParms)
{
    RT_BZERO(pTstParms, sizeof(AUDIOTESTPARMS));

    return;
}

/**
 * Destroys an audio test parameters set.
 *
 * @param   pTstParms           Test parameters set to destroy.
 */
static void audioTestParmsDestroy(PAUDIOTESTPARMS pTstParms)
{
    if (!pTstParms)
        return;

    return;
}

static AUDIOTESTDESC g_aTests[] =
{
    /* pszTest      fExcluded      pfnSetup */
    { "PlayTone",   false,         audioTestPlayToneSetup,       audioTestPlayToneExec,      audioTestPlayToneDestroy }
};


/**
 * Shows the application logo.
 *
 * @param   pStream             Output stream to show logo on.
 */
void showLogo(PRTSTREAM pStream)
{
    static bool s_fLogoShown = false; /* Show logo only once. */

    if (!s_fLogoShown)
    {
        RTStrmPrintf(pStream, VBOX_PRODUCT " VKAT (Validation Kit Audio Test) "
                     VBOX_VERSION_STRING " - r%s\n"
                     "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                     "All rights reserved.\n\n", RTBldCfgRevisionStr());
        s_fLogoShown = true;
    }
}

/**
 * Shows tool usage text.
 */
static void audioTestUsage(PRTSTREAM pStrm)
{
    char szExec[RTPATH_MAX];
    RTStrmPrintf(pStrm, "usage: %s [options]\n",
                 RTPathFilename(RTProcGetExecutablePath(szExec, sizeof(szExec))));
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "options: \n");

    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdTestOptions); i++)
    {
        const char *pszHelp;
        switch (g_aCmdTestOptions[i].iShort)
        {
            case 'h':
                pszHelp = "Displays this help and exit";
                break;
            case 'd':
                pszHelp = "Use the specified audio device";
                break;
            case 'e':
                pszHelp = "Exclude the given test id from the list";
                break;
            case 'a':
                pszHelp = "Exclude all tests from the list (useful to enable single tests later with --include)";
                break;
            case 'i':
                pszHelp = "Include the given test id in the list";
                break;
            default:
                pszHelp = "Option undocumented";
                break;
        }
        char szOpt[256];
        RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdTestOptions[i].pszLong, g_aCmdTestOptions[i].iShort);
        RTStrmPrintf(pStrm, "  %-30s%s\n", szOpt, pszHelp);
    }

    /** @todo Add all other options. */
}

/**
 * Constructs a PDM audio driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvReg             PDM driver registration record to use for construction.
 * @param   pDrvIns             Driver instance to use for construction.
 * @param   ppDrvAudio          Where to return the audio driver interface of type IHOSTAUDIO.
 */
static int audioTestDrvConstruct(const PDMDRVREG *pDrvReg, PPDMDRVINS pDrvIns, PPDMIHOSTAUDIO *ppDrvAudio)
{
    AssertReturn(pDrvReg->cbInstance, VERR_INVALID_PARAMETER); /** @todo Very crude; improve. */

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Initializing backend '%s' ...\n", pDrvReg->szName);

    pDrvIns->pvInstanceData = RTMemAllocZ(pDrvReg->cbInstance);
    AssertPtrReturn(pDrvIns->pvInstanceData, VERR_NO_MEMORY);

    int rc = pDrvReg->pfnConstruct(pDrvIns, NULL /* PCFGMNODE */, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        PPDMIHOSTAUDIO pDrvAudio = (PPDMIHOSTAUDIO)pDrvIns->IBase.pfnQueryInterface(&pDrvIns->IBase, PDMIHOSTAUDIO_IID);

        pDrvAudio->pfnGetStatus(pDrvAudio, PDMAUDIODIR_OUT);

        *ppDrvAudio = pDrvAudio;
    }

    return rc;
}

/**
 * Destructs a PDM audio driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvReg             PDM driver registration record to destruct.
 * @param   pDrvIns             Driver instance to destruct.
 */
static int audioTestDrvDestruct(const PDMDRVREG *pDrvReg, PPDMDRVINS pDrvIns)
{
    if (!pDrvIns)
        return VINF_SUCCESS;

    if (pDrvReg->pfnDestruct)
        pDrvReg->pfnDestruct(pDrvIns);

    if (pDrvIns->pvInstanceData)
    {
        RTMemFree(pDrvIns->pvInstanceData);
        pDrvIns->pvInstanceData = NULL;
    }

    return VINF_SUCCESS;
}

/**
 * Enumerates audio devices and optionally searches for a specific device.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test env to use for enumeration.
 * @param   pszDev              Device name to search for. Can be NULL if the default device shall be used.
 * @param   ppDev               Where to return the pointer of the device enumeration of \a pTstEnv when a
 *                              specific device was found.
 */
static int audioTestDevicesEnumerateAndCheck(PAUDIOTESTENV pTstEnv, const char *pszDev, PPDMAUDIOHOSTDEV *ppDev)
{
    RTTestSubF(g_hTest, "Enumerating audio devices and checking for device '%s'", pszDev ? pszDev : "<Default>");

    if (!pTstEnv->pDrvAudio->pfnGetDevices)
    {
        RTTestSkipped(g_hTest, "Backend does not support device enumeration, skipping");
        return VINF_NOT_SUPPORTED;
    }

    Assert(pszDev == NULL || ppDev);

    if (ppDev)
        *ppDev = NULL;

    int rc = pTstEnv->pDrvAudio->pfnGetDevices(pTstEnv->pDrvAudio, &pTstEnv->DevEnm);
    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOHOSTDEV pDev;
        RTListForEach(&pTstEnv->DevEnm.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
        {
            char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s':\n", pDev->szName);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Usage           = %s\n",   PDMAudioDirGetName(pDev->enmUsage));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Flags           = %s\n",   PDMAudioHostDevFlagsToString(szFlags, pDev->fFlags));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Input channels  = %RU8\n", pDev->cMaxInputChannels);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Output channels = %RU8\n", pDev->cMaxOutputChannels);

            if (   pszDev
                && !RTStrCmp(pDev->szName, pszDev))
            {
                *ppDev = pDev;
            }
        }
    }
    else
        RTTestFailed(g_hTest, "Enumerating audio devices failed with %Rrc", rc);

    RTTestSubDone(g_hTest);

    if (   pszDev
        && *ppDev == NULL)
    {
        RTTestFailed(g_hTest, "Audio device '%s' not found", pszDev);
        return VERR_NOT_FOUND;
    }

    return VINF_SUCCESS;
}

/**
 * Opens an audio device.
 *
 * @returns VBox status code.
 * @param   pDev                Audio device to open.
 */
static int audioTestDeviceOpen(PPDMAUDIOHOSTDEV pDev)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(g_hTest, "Opening audio device '%s' ...", pDev->szName);

    /** @todo Detect + open device here. */

    RTTestSubDone(g_hTest);

    return rc;
}

/**
 * Closes an audio device.
 *
 * @returns VBox status code.
 * @param   pDev                Audio device to close.
 */
static int audioTestDeviceClose(PPDMAUDIOHOSTDEV pDev)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(g_hTest, "Closing audio device '%s' ...", pDev->szName);

    /** @todo Close device here. */

    RTTestSubDone(g_hTest);

    return rc;
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
            return rc;
    }

    audioTestCombineParms(&TstParms, pOverrideParms);

    if (strlen(TstParms.Dev.szName)) /** @todo Refine this check. */
        rc = audioTestDeviceOpen(&TstParms.Dev);

    AssertPtr(pTstDesc->pfnExec);
    rc = pTstDesc->pfnExec(pTstEnv, pvCtx, &TstParms);

    RTTestSubDone(g_hTest);

    if (pTstDesc->pfnDestroy)
    {
        int rc2 = pTstDesc->pfnDestroy(pTstEnv, pvCtx);
        if (RT_SUCCESS(rc))
            rc = rc2;
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
static int audioTestWorker(PAUDIOTESTENV pTstEnv, PAUDIOTESTPARMS pOverrideParms)
{
    int rc = VINF_SUCCESS;

    unsigned uSeq = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
    {
        int rc2 = audioTestOne(pTstEnv, &g_aTests[i], uSeq, pOverrideParms);
        if (RT_SUCCESS(rc))
            rc = rc2;

        if (!g_aTests[i].fExcluded)
            uSeq++;
    }

    return rc;
}

/**
 * Main (entry) function for the testing functionality of VKAT.
 *
 * @returns RTEXITCODE
 * @param   argc                Number of argv arguments.
 * @param   argv                argv arguments.
 */
int audioTestMain(int argc, char **argv)
{
    int rc;

    AUDIOTESTPARMS TstCust;
    audioTestParmsInit(&TstCust);

    char *pszDevice  = NULL; /* Custom device to use. Can be NULL if not being used. */
    char *pszPathOut = NULL; /* Custom output path to use. Can be NULL if not being used. */
    char *pszTag     = NULL; /* Custom tag to use. Can be NULL if not being used. */

    RT_ZERO(g_DrvIns);
    const PDMDRVREG *pDrvReg = NULL;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, g_aCmdTestOptions, RT_ELEMENTS(g_aCmdTestOptions), 0, 0 /* fFlags */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
            {
                audioTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            }

            case 'e':
            {
                if (ValueUnion.u32 < RT_ELEMENTS(g_aTests))
                    g_aTests[ValueUnion.u32].fExcluded = true;
                else
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_FAILURE, "Invalid test number passed to --exclude\n");
                    RTTestErrorInc(g_hTest);
                    return RTGetOptPrintError(VERR_INVALID_PARAMETER, &ValueUnion);
                }
                break;
            }

            case 'a':
            {
                for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
                    g_aTests[i].fExcluded = true;
                break;
            }

            case 'b':
            {
#ifdef VBOX_WITH_AUDIO_PULSE
                if (   !RTStrICmp(ValueUnion.psz, "pulseaudio")
                    || !RTStrICmp(ValueUnion.psz, "pa"))
                    pDrvReg = &g_DrvHostPulseAudio;
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
                if (   !RTStrICmp(ValueUnion.psz, "alsa"))
                    pDrvReg = &g_DrvHostALSAAudio;
#endif
#ifdef VBOX_WITH_AUDIO_OSS
                if (   !RTStrICmp(ValueUnion.psz, "oss"))
                    pDrvReg = &g_DrvHostOSSAudio;
#endif
#if defined(RT_OS_DARWIN)
                if (   !RTStrICmp(ValueUnion.psz, "coreaudio"))
                    pDrvReg = &g_DrvHostCoreAudio;
#endif
#if defined(RT_OS_WINDOWS)
                if (        !RTStrICmp(ValueUnion.psz, "wasapi"))
                    pDrvReg = &g_DrvHostAudioWas;
                else if (   !RTStrICmp(ValueUnion.psz, "directsound")
                         || !RTStrICmp(ValueUnion.psz, "dsound")
                    pDrvReg = &g_DrvHostDSound;
#endif
                if (pDrvReg == NULL)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Invalid / unsupported backend '%s' specified\n", ValueUnion.psz);
                break;
            }

            case 'i':
            {
                if (ValueUnion.u32 < RT_ELEMENTS(g_aTests))
                    g_aTests[ValueUnion.u32].fExcluded = false;
                else
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_FAILURE, "Invalid test number passed to --include\n");
                    RTTestErrorInc(g_hTest);
                    return RTGetOptPrintError(VERR_INVALID_PARAMETER, &ValueUnion);
                }
                break;
            }

            case VKAT_TEST_OPT_COUNT:
            {
                break;
            }

            case VKAT_TEST_OPT_DEV:
            {
                pszDevice = RTStrDup(ValueUnion.psz);
                break;
            }

            case VKAT_TEST_OPT_PAUSE:
            {
                break;
            }

            case VKAT_TEST_OPT_OUTDIR:
            {
                pszPathOut = RTStrDup(ValueUnion.psz);
                break;
            }

            case VKAT_TEST_OPT_PCM_BIT:
            {
                TstCust.ToneParms.Props.cbSampleX = ValueUnion.u8 / 8 /* bit */;
                break;
            }

            case VKAT_TEST_OPT_PCM_CHAN:
            {
                TstCust.ToneParms.Props.cChannelsX = ValueUnion.u8;
                break;
            }

            case VKAT_TEST_OPT_PCM_HZ:
            {
                TstCust.ToneParms.Props.uHz = ValueUnion.u32;
                break;
            }

            case VKAT_TEST_OPT_PCM_SIGNED:
            {
                TstCust.ToneParms.Props.fSigned = ValueUnion.f;
                break;
            }

            case VKAT_TEST_OPT_TAG:
            {
                pszTag = RTStrDup(ValueUnion.psz);
                break;
            }

            case VKAT_TEST_OPT_VOL:
            {
                TstCust.ToneParms.uVolumePercent = ValueUnion.u8;
                break;
            }

            default:
                break;
        }
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    /* If no backend is specified, go with the default backend for that OS. */
    if (pDrvReg == NULL)
#if defined(RT_OS_WINDOWS)
        pDrvReg = &g_DrvHostAudioWas;
#elif defined(RT_OS_DARWIN)
        pDrvReg = &g_DrvHostCoreAudio;
#elif defined(RT_OS_SOLARIS)
        pDrvReg = &g_DrvHostOSSAudio;
#else
        pDrvReg = &g_DrvHostALSAAudio;
#endif

    PPDMIHOSTAUDIO pDrvAudio;
    rc = audioTestDrvConstruct(pDrvReg, &g_DrvIns, &pDrvAudio);
    if (RT_SUCCESS(rc))
    {
        /* For now all tests have the same test environment. */
        AUDIOTESTENV TstEnv;
        rc = audioTestEnvInit(&TstEnv, pDrvAudio, pszPathOut, pszTag);
        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", TstEnv.Set.szPathOutAbs);

            PPDMAUDIOHOSTDEV pDev;
            rc = audioTestDevicesEnumerateAndCheck(&TstEnv, pszDevice, &pDev);
            if (RT_SUCCESS(rc))
                audioTestWorker(&TstEnv, &TstCust);

            audioTestEnvDestroy(&TstEnv);
        }
        audioTestDrvDestruct(pDrvReg, &g_DrvIns);
    }

    audioTestParmsDestroy(&TstCust);

    RTStrFree(pszDevice);
    RTStrFree(pszPathOut);
    RTStrFree(pszTag);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}

/**
 * Verifies one single test set.
 *
 * @returns VBox status code.
 * @param   pszPath             Absolute path to test set.
 * @param   pszTag              Tag of test set to verify. Optional and can be NULL.
 */
int audioVerifyOne(const char *pszPath, const char *pszTag)
{
    RTTestSubF(g_hTest, "Verifying test set (tag '%s') ...", pszTag ? pszTag : "<Default>");

    AUDIOTESTSET tstSet;
    int rc = AudioTestSetOpen(&tstSet, pszPath);
    if (RT_SUCCESS(rc))
    {
        rc = AudioTestSetVerify(&tstSet, pszTag);
        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Verification successful");
        }
        else
            RTTestFailed(g_hTest, "Verification failed with %Rrc", rc);

        AudioTestSetClose(&tstSet);
    }
    else
        RTTestFailed(g_hTest, "Opening test set '%s' (tag '%s') failed, rc=%Rrc\n", pszPath, pszTag, rc);

    RTTestSubDone(g_hTest);

    return rc;
}

/**
 * Main (entry) function for the verification functionality of VKAT.
 *
 * @returns RTEXITCODE
 * @param   argc                Number of argv arguments.
 * @param   argv                argv arguments.
 */
int audioVerifyMain(int argc, char **argv)
{
    char *pszTag = NULL; /* Custom tag to use. Can be NULL if not being used. */

    char szDirCur[RTPATH_MAX];
    int rc = RTPathGetCurrent(szDirCur, sizeof(szDirCur));
    if (RT_FAILURE(rc))
    {
        RTMsgError("Getting current directory failed, rc=%Rrc\n", rc);
        return RTEXITCODE_FAILURE;
    }

    /*
     * Process common options.
     */
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, g_aCmdVerifyOptions, RT_ELEMENTS(g_aCmdVerifyOptions),
                      0, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case VKAT_VERIFY_OPT_TAG:
            {
                pszTag = RTStrDup(ValueUnion.psz);
                break;
            }

            case VINF_GETOPT_NOT_OPTION:
            {
                Assert(GetState.iNext);
                GetState.iNext--;
                break;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }

        /* All flags / options processed? Bail out here.
         * Processing the file / directory list comes down below. */
        if (rc == VINF_GETOPT_NOT_OPTION)
            break;
    }

    if (pszTag)
        RTMsgInfo("Using tag '%s'\n", pszTag);

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    /*
     * Deal with test sets.
     */
    rc = RTGetOpt(&GetState, &ValueUnion);
    do
    {
        char const *pszPath;

        if (rc == 0) /* Use current directory if no element specified. */
            pszPath = szDirCur;
        else
            pszPath = ValueUnion.psz;

        RTFSOBJINFO objInfo;
        rc = RTPathQueryInfoEx(pszPath, &objInfo,
                               RTFSOBJATTRADD_UNIX, RTPATH_F_FOLLOW_LINK);
        if (RT_SUCCESS(rc))
        {
            rc = audioVerifyOne(pszPath, pszTag);
        }
        else
            RTTestFailed(g_hTest, "Cannot access path '%s', rc=%Rrc\n", pszPath, rc);

    } while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0);

    RTStrFree(pszTag);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}

int main(int argc, char **argv)
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("AudioTest", &g_hTest);
    if (rc)
        return rc;

    AssertReturn(argc > 0, false);

    /* At least the operation mode must be there. */
    if (argc < 2)
    {
        audioTestUsage(g_pStdOut);
        return RTEXITCODE_SYNTAX;
    }

    /*
     * Process common options.
     */
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, g_aCmdCommonOptions, RT_ELEMENTS(g_aCmdCommonOptions), 1 /* idxFirst */, 0 /* fFlags */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
            {
                showLogo(g_pStdOut);
                audioTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            }

            case 'v':
            {
                g_uVerbosity++;
                break;
            }

            case VINF_GETOPT_NOT_OPTION:
            {
                Assert(GetState.iNext);
                GetState.iNext--;
            }

            default:
                /* Ignore everything else here. */
                break;
        }

        /* All flags / options processed? Bail out here.
         * Processing the file / directory list comes down below. */
        if (rc == VINF_GETOPT_NOT_OPTION)
            break;
    }

    showLogo(g_pStdOut);

    /* Get operation mode. */
    const char *pszMode = argv[GetState.iNext++]; /** @todo Also do it busybox-like? */

    argv += GetState.iNext;
    Assert(argc >= GetState.iNext);
    argc -= GetState.iNext;

    if (!RTStrICmp(pszMode, "test"))
    {
        return audioTestMain(argc, argv);
    }
    else if (!RTStrICmp(pszMode, "verify"))
    {
        return audioVerifyMain(argc, argv);
    }

    RTStrmPrintf(g_pStdOut, "Must specify a mode first, either 'test' or 'verify'\n\n");

    audioTestUsage(g_pStdOut);
    return RTEXITCODE_SYNTAX;
}

