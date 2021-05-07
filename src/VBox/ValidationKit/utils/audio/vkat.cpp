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
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <VBox/vmm/pdmaudioinline.h>

#include "../../../Devices/Audio/AudioHlp.h"
#include "../../../Devices/Audio/AudioTest.h"
#include "../../../Devices/Audio/VBoxDDVKAT.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


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
    /** Name or path of audio device to use, depending on the OS.
     *  If NULL, the default device for this specific test (input / output) will be used. */
    char                   *pszDevice;
    /** Absolute path where to store the test audio data.
     *  If NULL, no test audio data will be written. */
    char                   *pszPathOutAbs;
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

struct AUDIOTESTENV;
struct AUDIOTESTDESC;

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
    PPDMIHOSTAUDIO          pDrvAudio;
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
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

enum
{
    VKAT_TEST_OPT_COUNT = 900,
    VKAT_TEST_OPT_DEV,
    VKAT_TEST_OPT_OUTDIR,
    VKAT_TEST_OPT_PAUSE,
    VKAT_TEST_OPT_HZ,
    VKAT_TEST_OPT_BIT,
    VKAT_TEST_OPT_CHAN,
    VKAT_TEST_OPT_SIGNED,
    VKAT_TEST_OPT_VOL,
};

#if 0
static const RTGETOPTDEF g_aCmdCommonOptions[] =
{
    { "--help",             'h',                 RTGETOPT_REQ_NOTHING }
};
#endif

/** Command line parameters for test mode. */
static const RTGETOPTDEF g_aCmdTestOptions[] =
{
    { "--exclude",          'e',                          RTGETOPT_REQ_UINT32  },
    { "--exclude-all",      'a',                          RTGETOPT_REQ_NOTHING },
    { "--include",          'i',                          RTGETOPT_REQ_UINT32  },
    { "--outdir",           VKAT_TEST_OPT_OUTDIR,         RTGETOPT_REQ_STRING  },
    { "--count",            VKAT_TEST_OPT_COUNT,          RTGETOPT_REQ_UINT32  },
    { "--device",           VKAT_TEST_OPT_DEV,            RTGETOPT_REQ_STRING  },
    { "--pause",            VKAT_TEST_OPT_PAUSE,          RTGETOPT_REQ_UINT32  },
    { "--pcm-bit",          VKAT_TEST_OPT_BIT,            RTGETOPT_REQ_UINT8   },
    { "--pcm-chan",         VKAT_TEST_OPT_CHAN,           RTGETOPT_REQ_UINT8   },
    { "--pcm-hz",           VKAT_TEST_OPT_HZ,             RTGETOPT_REQ_UINT16  },
    { "--pcm-signed",       VKAT_TEST_OPT_SIGNED,         RTGETOPT_REQ_BOOL    },
    { "--volume",           VKAT_TEST_OPT_VOL,            RTGETOPT_REQ_UINT8   }

};

/** The test handle. */
static RTTEST g_hTest;
/** The driver instance data. */
PDMDRVINS g_DrvIns;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms);


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

    //AudioTestToneParamsInitRandom(&pTestParms->ToneParms, &pTestParms->ToneParms.Props);

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

static void audioTestParmsInit(PAUDIOTESTPARMS pTestParms)
{
    RT_BZERO(pTestParms, sizeof(AUDIOTESTPARMS));
    return;
}

static void audioTestParmsDestroy(PAUDIOTESTPARMS pTestParms)
{
    if (!pTestParms)
        return;

    RTStrFree(pTestParms->pszDevice);
    pTestParms->pszDevice = NULL;

    RTStrFree(pTestParms->pszPathOutAbs);
    pTestParms->pszPathOutAbs = NULL;

    return;
}

static AUDIOTESTDESC g_aTests[] =
{
    /* pszTest      fExcluded      pfnSetup */
    { "PlayTone",   false,         audioTestPlayToneSetup,       audioTestPlayToneExec,      audioTestPlayToneDestroy }
};

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

static int audioTestDrvConstruct(const PDMDRVREG *pDrvReg, PPDMDRVINS pDrvIns, PPDMIHOSTAUDIO *ppDrvAudio)
{
    AssertReturn(pDrvReg->cbInstance, VERR_INVALID_PARAMETER); /** @todo Very crude; improve. */

    pDrvIns->pvInstanceData = RTMemAllocZ(pDrvReg->cbInstance);

    int rc = pDrvReg->pfnConstruct(pDrvIns, NULL /* PCFGMNODE */, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        PPDMIHOSTAUDIO pDrvAudio = (PPDMIHOSTAUDIO)pDrvIns->IBase.pfnQueryInterface(&pDrvIns->IBase, PDMIHOSTAUDIO_IID);

        pDrvAudio->pfnGetStatus(pDrvAudio, PDMAUDIODIR_OUT);

        *ppDrvAudio = pDrvAudio;
    }

    return rc;
}

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
 * Searches for the default audio test device and return the device path.
 *
 * @returns Path to the device audio device or NULL if none was found.
 */
static char *audioTestDeviceFindDefault(void)
{
    /** @todo Implement finding default device. */
    return NULL;
}

static int audioTestDeviceOpen(const char *pszDevice)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(g_hTest, "Opening audio device '%s' ...", pszDevice ? "<Default>>" : pszDevice);

    if (!pszDevice)
        audioTestDeviceFindDefault();

    /** @todo Detect + open device here. */

    RTTestSubDone(g_hTest);

    return rc;
}

static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms)
{
    RT_NOREF(pBaseParms, pOverrideParms);
    return 0;
}

static int audioTestOne(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc,
                        unsigned uSeq, PAUDIOTESTPARMS pOverrideParms)
{
    int rc;

    AUDIOTESTPARMS TstParms;
    audioTestParmsInit(&TstParms);

    RTTestSub(g_hTest, pTstDesc->pszName);

    if (pTstDesc->fExcluded)
    {
        RTTestSkipped(g_hTest, "Excluded from list");
        return VINF_SUCCESS;
    }

    void *pvCtx = NULL;

    if (pTstDesc->pfnSetup)
    {
        rc = pTstDesc->pfnSetup(pTstEnv, pTstDesc, &TstParms, &pvCtx);
        if (RT_FAILURE(rc))
            return rc;
    }

    audioTestCombineParms(&TstParms, pOverrideParms);

    /* Open the device on the first test being run. */
    if (   uSeq == 0
        && TstParms.pszDevice
        && strlen(TstParms.pszDevice))
    {
        rc = audioTestDeviceOpen(TstParms.pszDevice);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "Unable to find audio device '%s'", TstParms.pszDevice);
    }

    AssertPtr(pTstDesc->pfnExec);
    rc = pTstDesc->pfnExec(pTstEnv, pvCtx, &TstParms);

    RTTestSubDone(g_hTest);

    if (pTstDesc->pfnDestroy)
    {
        int rc2 = pTstDesc->pfnDestroy(pTstEnv, pvCtx);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    audioTestParmsDestroy(&TstParms);

    return rc;
}

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

int mainTest(int argc, char **argv)
{
    int rc;

    AUDIOTESTPARMS TstCust;
    audioTestParmsInit(&TstCust);

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdTestOptions, RT_ELEMENTS(g_aCmdTestOptions), 1, 0 /* fFlags */);
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
                TstCust.pszDevice = RTStrDup(ValueUnion.psz);
                break;
            }

            case VKAT_TEST_OPT_PAUSE:
            {
                break;
            }

            case VKAT_TEST_OPT_OUTDIR:
            {
                TstCust.pszPathOutAbs = RTStrDup(ValueUnion.psz);
                break;
            }

            case VKAT_TEST_OPT_BIT:
            {
                TstCust.ToneParms.Props.cbSampleX = ValueUnion.u8 / 8 /* bit */;
                break;
            }

            case VKAT_TEST_OPT_CHAN:
            {
                TstCust.ToneParms.Props.cChannelsX = ValueUnion.u8;
                break;
            }

            case VKAT_TEST_OPT_HZ:
            {
                TstCust.ToneParms.Props.uHz = ValueUnion.u32;
                break;
            }

            case VKAT_TEST_OPT_SIGNED:
            {
                TstCust.ToneParms.Props.fSigned = ValueUnion.f;
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

    const PDMDRVREG *pDrvReg = &g_DrvVKATAlsa;

    RT_ZERO(g_DrvIns);

    PPDMIHOSTAUDIO pDrvAudio;
    rc = audioTestDrvConstruct(pDrvReg, &g_DrvIns, &pDrvAudio);
    if (RT_SUCCESS(rc))
    {
        /* For now all tests have the same test environment. */
        AUDIOTESTENV TestEnv;
        TestEnv.pDrvAudio = pDrvAudio;

        audioTestWorker(&TestEnv, &TstCust);

        audioTestDrvDestruct(pDrvReg, &g_DrvIns);
    }

    audioTestParmsDestroy(&TstCust);

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}

int mainVerify(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    return 0;
}

int main(int argc, char **argv)
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("AudioTest", &g_hTest);
    if (rc)
        return rc;

    if (argc < 2)
    {
        audioTestUsage(g_pStdOut);
        return RTEXITCODE_SYNTAX;
    }

    const char *pszMode = argv[1];

    argc -= 2;
    argv += 2;

    if (!RTStrICmp(pszMode, "test"))
    {
        return mainTest(argc, argv);
    }
    else if (!RTStrICmp(pszMode, "verify"))
    {
        return mainVerify(argc, argv);
    }

    RTStrmPrintf(g_pStdOut, "Must specify a mode first, either 'test' or 'verify'\n\n");

    audioTestUsage(g_pStdOut);
    return RTEXITCODE_SYNTAX;
}

