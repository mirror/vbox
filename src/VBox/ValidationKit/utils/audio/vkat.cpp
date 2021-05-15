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
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/rand.h>
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
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
struct AUDIOTESTENV;
/** Pointer a audio test environment. */
typedef AUDIOTESTENV *PAUDIOTESTENV;

struct AUDIOTESTDESC;
/** Pointer a audio test descriptor. */
typedef AUDIOTESTDESC *PAUDIOTESTDESC;

/**
 * Callback to set up the test parameters for a specific test.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS    if setting the parameters up succeeded. Any other error code
 *                          otherwise indicating the kind of error.
 * @param   pszTest         Test name.
 * @param   pTstParmsAcq    The audio test parameters to set up.
 */
typedef DECLCALLBACKTYPE(int, FNAUDIOTESTSETUP,(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx));
/** Pointer to an audio test setup callback. */
typedef FNAUDIOTESTSETUP *PFNAUDIOTESTSETUP;

typedef DECLCALLBACKTYPE(int, FNAUDIOTESTEXEC,(PAUDIOTESTENV pTstEnv, void *pvCtx, PAUDIOTESTPARMS pTstParms));
/** Pointer to an audio test exec callback. */
typedef FNAUDIOTESTEXEC *PFNAUDIOTESTEXEC;

typedef DECLCALLBACKTYPE(int, FNAUDIOTESTDESTROY,(PAUDIOTESTENV pTstEnv, void *pvCtx));
/** Pointer to an audio test destroy callback. */
typedef FNAUDIOTESTDESTROY *PFNAUDIOTESTDESTROY;

/** Maximum audio streams a test environment can handle. */
#define AUDIOTESTENV_MAX_STREAMS 8

/**
 * Audio test environment parameters.
 * Not necessarily bound to a specific test (can be reused).
 */
typedef struct AUDIOTESTENV
{
    /** Output path for storing the test environment's final test files. */
    char                  szPathOut[RTPATH_MAX];
    /** Temporary path for this test environment. */
    char                  szPathTemp[RTPATH_MAX];
    /** The host (backend) driver to use. */
    PPDMIHOSTAUDIO        pDrvAudio;
    /** The current (last) audio device enumeration to use. */
    PDMAUDIOHOSTENUM      DevEnum;
    /** Audio stream. */
    AUDIOTESTSTREAM       aStreams[AUDIOTESTENV_MAX_STREAMS];
    /** The audio test set to use. */
    AUDIOTESTSET          Set;
} AUDIOTESTENV;

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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms);
static int audioTestPlayTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms);
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream);
static DECLCALLBACK(RTEXITCODE) audioTestMain(int argc, char **argv);
static DECLCALLBACK(RTEXITCODE) audioVerifyMain(int argc, char **argv);


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
    VKAT_TEST_OPT_TEMPDIR,
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
    { "--quiet",            'q',                          RTGETOPT_REQ_NOTHING },
    { "--verbose",          'v',                          RTGETOPT_REQ_NOTHING },
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
    { "--tempdir",          VKAT_TEST_OPT_TEMPDIR,        RTGETOPT_REQ_STRING  },
    { "--volume",           VKAT_TEST_OPT_VOL,            RTGETOPT_REQ_UINT8   }
};

/**
 * Command line parameters for verification mode.
 */
static const RTGETOPTDEF g_aCmdVerifyOptions[] =
{
    { "--tag",              VKAT_VERIFY_OPT_TAG,          RTGETOPT_REQ_STRING  }
};

/**
 * Commands.
 */
static struct
{
    const char     *pszCommand;
    DECLCALLBACKMEMBER(RTEXITCODE, pfnHandler,(int argc, char **argv));
    PCRTGETOPTDEF   paOptions;
    size_t          cOptions;
    const char     *pszDesc;
} const g_aCommands[] =
{
    {
        "test",   audioTestMain,      g_aCmdTestOptions,      RT_ELEMENTS(g_aCmdTestOptions),
        "Does some kind of testing, I guess..."
    },
    {
        "verify", audioVerifyMain,    g_aCmdVerifyOptions,    RT_ELEMENTS(g_aCmdVerifyOptions),
        "Verfies something, I guess..."
    },
};

/**
 * Backends.
 *
 * @note The first backend in the array is the default one for the platform.
 */
static struct
{
    /** The driver registration structure. */
    PCPDMDRVREG pDrvReg;
    /** The backend name.
     * Aliases are implemented by having multiple entries for the same backend.  */
    const char *pszName;
} const g_aBackends[] =
{
#if defined(VBOX_WITH_AUDIO_ALSA) && defined(RT_OS_LINUX)
    {   &g_DrvHostALSAAudio,    "alsa" },
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
    {   &g_DrvHostPulseAudio,   "pulseaudio" },
    {   &g_DrvHostPulseAudio,   "pulse" },
    {   &g_DrvHostPulseAudio,   "pa" },
#endif
#ifdef VBOX_WITH_AUDIO_OSS
    {   &g_DrvHostOSSAudio,     "oss" },
#endif
#if defined(RT_OS_DARWIN)
    {   &g_DrvHostCoreAudio,    "coreaudio" },
    {   &g_DrvHostCoreAudio,    "core" },
    {   &g_DrvHostCoreAudio,    "ca" },
#endif
#if defined(RT_OS_WINDOWS)
    {   &g_DrvHostAudioWas,     "wasapi" },
    {   &g_DrvHostAudioWas,     "was" },
    {   &g_DrvHostDSound,       "directsound" },
    {   &g_DrvHostDSound,       "dsound" },
    {   &g_DrvHostDSound,       "ds" },
#endif
};
AssertCompile(sizeof(g_aBackends) > 0 /* port me */);

/** The test handle. */
static RTTEST       g_hTest;
/** The current verbosity level. */
static unsigned     g_uVerbosity = 0;


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Initializes an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvAudio           Audio driver to use.
 * @param   pszPathOut          Output path to use. If NULL, the system's temp directory will be used.
 * @param   pszPathTemp         Temporary path to use. If NULL, the system's temp directory will be used.
 * @param   pszTag              Tag name to use. If NULL, a generated UUID will be used.
 */
static int audioTestEnvInit(PAUDIOTESTENV pTstEnv, PPDMIHOSTAUDIO pDrvAudio, const char *pszTag)
{
    pTstEnv->pDrvAudio = pDrvAudio;
    PDMAudioHostEnumInit(&pTstEnv->DevEnum);

    int rc = VINF_SUCCESS;

    char szPathTemp[RTPATH_MAX];
    if (   !strlen(pTstEnv->szPathTemp)
        || !strlen(pTstEnv->szPathOut))
        rc = RTPathTemp(szPathTemp, sizeof(szPathTemp));

    if (   RT_SUCCESS(rc)
        && !strlen(pTstEnv->szPathTemp))
        rc = RTPathJoin(pTstEnv->szPathTemp, sizeof(pTstEnv->szPathTemp), szPathTemp, "vkat-temp");

    if (   RT_SUCCESS(rc)
        && !strlen(pTstEnv->szPathOut))
        rc = RTPathJoin(pTstEnv->szPathOut, sizeof(pTstEnv->szPathOut), szPathTemp, "vkat");

    if (RT_SUCCESS(rc))
        rc = AudioTestSetCreate(&pTstEnv->Set, pTstEnv->szPathTemp, pszTag);

    if (RT_SUCCESS(rc))
    {
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pszTag);
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", pTstEnv->szPathOut);
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Temp directory is '%s'\n", pTstEnv->szPathTemp);
    }

    return rc;
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

    PDMAudioHostEnumDelete(&pTstEnv->DevEnum);

    for (unsigned i = 0; i < RT_ELEMENTS(pTstEnv->aStreams); i++)
    {
        int rc2 = audioTestStreamDestroy(pTstEnv, &pTstEnv->aStreams[i]);
        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Stream destruction for stream #%u failed with %Rrc\n", i, rc2);
    }

    AudioTestSetDestroy(&pTstEnv->Set);
}

/**
 * Initializes an audio test parameters set.
 *
 * @param   pTstParms           Test parameters set to initialize.
 */
static void audioTestParmsInit(PAUDIOTESTPARMS pTstParms)
{
    RT_ZERO(*pTstParms);
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

/**
 * Shows the logo.
 *
 * @param   pStream             Output stream to show logo on.
 */
static void audioTestShowLogo(PRTSTREAM pStream)
{
    RTStrmPrintf(pStream, VBOX_PRODUCT " VKAT (Validation Kit Audio Test) "
                 VBOX_VERSION_STRING " - r%s\n"
                 "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                 "All rights reserved.\n\n", RTBldCfgRevisionStr());
}

/**
 * Shows tool usage text.
 */
static void audioTestUsage(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm, "usage: %s [global options] <command> [command-options]\n",
                 RTPathFilename(RTProcExecutablePath()));
    RTStrmPrintf(pStrm,
                 "\n"
                 "Global Options:\n"
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
                RTStrmPrintf(pStrm, "  -%c, %s\n", g_aCmdTestOptions[i].iShort, g_aCmdTestOptions[i].pszLong);
            else
                RTStrmPrintf(pStrm, "  %s\n", g_aCmdTestOptions[i].pszLong);

            const char *pszHelp = NULL;
            if (paOptions == g_aCmdTestOptions)
            {
                switch (g_aCmdTestOptions[i].iShort)
                {
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
                }
            }
            /** @todo Add help text for all options. */
            if (pszHelp)
                RTStrmPrintf(pStrm, "    %s\n", pszHelp);
        }
    }

}

/** @name Driver Helper Fakes / Stubs
 * @{  */

VMMR3DECL(int) CFGMR3QueryString(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString)
{
    if (pNode != NULL)
    {
        PCPDMDRVREG pDrvReg = (PCPDMDRVREG)pNode;
        if (g_uVerbosity > 2)
            RTPrintf("debug: CFGMR3QueryString([%s], %s, %p, %#x)\n", pDrvReg->szName, pszName, pszString, cchString);

        if (   (   strcmp(pDrvReg->szName, "PulseAudio") == 0
                || strcmp(pDrvReg->szName, "HostAudioWas") == 0)
            && strcmp(pszName, "VmName") == 0)
            return RTStrCopy(pszString, cchString, "vkat");

        if (   strcmp(pDrvReg->szName, "HostAudioWas") == 0
            && strcmp(pszName, "VmUuid") == 0)
            return RTStrCopy(pszString, cchString, "794c9192-d045-4f28-91ed-46253ac9998e");
    }
    else if (g_uVerbosity > 2)
        RTPrintf("debug: CFGMR3QueryString(%p, %s, %p, %#x)\n", pNode, pszName, pszString, cchString);

    return VERR_CFGM_VALUE_NOT_FOUND;
}

/* Fake stub. Will be removed when this moves into the driver helpers. */
VMMR3DECL(int) CFGMR3QueryStringDef(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString, const char *pszDef)
{
    if (g_uVerbosity > 2)
        RTPrintf("debug: CFGMR3QueryStringDef(%p, %s, %p, %#x, %s)\n", pNode, pszName, pszString, cchString, pszDef);
    return RTStrCopy(pszString, cchString, pszDef);
}

/* Fake stub. Will be removed when this moves into the driver helpers. */
VMMR3DECL(int) CFGMR3ValidateConfig(PCFGMNODE pNode, const char *pszNode,
                                    const char *pszValidValues, const char *pszValidNodes,
                                    const char *pszWho, uint32_t uInstance)
{
    RT_NOREF(pNode, pszNode, pszValidValues, pszValidNodes, pszWho, uInstance);
    return VINF_SUCCESS;
}

/** @} */


/**
 * Constructs a PDM audio driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvReg     PDM driver registration record to use for construction.
 * @param   ppDrvIns    Where to return the driver instance structure.
 * @param   ppDrvAudio  Where to return the audio driver interface of type IHOSTAUDIO.
 */
static int audioTestDrvConstruct(PCPDMDRVREG pDrvReg, PPPDMDRVINS ppDrvIns, PPDMIHOSTAUDIO *ppDrvAudio)
{
    /* The destruct function must have valid data to work with. */
    *ppDrvIns   = NULL;
    *ppDrvAudio = NULL;

    /*
     * Check registration structure validation (doesn't need to be too
     * thorough, PDM check it in detail on every VM startup).
     */
    AssertPtrReturn(pDrvReg, VERR_INVALID_POINTER);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Initializing backend '%s' ...\n", pDrvReg->szName);
    AssertPtrReturn(pDrvReg->pfnConstruct, VERR_INVALID_PARAMETER);

    /*
     * Initialize the driver helper the first time thru.
     */
    static PDMDRVHLPR3 s_DrvHlp;
    if (s_DrvHlp.u32Version == 0)
    {
        s_DrvHlp.u32Version = PDM_DRVHLPR3_VERSION;
        s_DrvHlp.u32TheEnd  = PDM_DRVHLPR3_VERSION;
    }

    /*
     * Create the instance data structure.
     */
    PPDMDRVINS pDrvIns = (PPDMDRVINS)RTMemAllocZVar(RT_UOFFSETOF_DYN(PDMDRVINS, achInstanceData[pDrvReg->cbInstance]));
    RTTEST_CHECK_RET(g_hTest, pDrvIns, VERR_NO_MEMORY);

    pDrvIns->u32Version       = PDM_DRVINS_VERSION;
    pDrvIns->iInstance        = 0;
    pDrvIns->pHlpR3           = &s_DrvHlp;
    pDrvIns->pvInstanceDataR3 = &pDrvIns->achInstanceData[0];
    pDrvIns->pReg             = pDrvReg;
    pDrvIns->pCfg             = (PCFGMNODE)pDrvReg;
    //pDrvIns->pUpBase          = NULL;
    //pDrvIns->pDownBase        = NULL;

    /*
     * Invoke the constructor.
     */
    int rc = pDrvReg->pfnConstruct(pDrvIns, pDrvIns->pCfg, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        PPDMIHOSTAUDIO pDrvAudio = (PPDMIHOSTAUDIO)pDrvIns->IBase.pfnQueryInterface(&pDrvIns->IBase, PDMIHOSTAUDIO_IID);
        if (pDrvAudio)
        {
            PDMAUDIOBACKENDSTS enmStatus = pDrvAudio->pfnGetStatus(pDrvAudio, PDMAUDIODIR_OUT);
            if (enmStatus == PDMAUDIOBACKENDSTS_RUNNING)
            {
                *ppDrvAudio = pDrvAudio;
                *ppDrvIns   = pDrvIns;
                return VINF_SUCCESS;
            }
            RTTestFailed(g_hTest, "Expected backend status RUNNING, got %d instead", enmStatus);
        }
        else
            RTTestFailed(g_hTest, "Failed to query PDMIHOSTAUDIO for '%s'", pDrvReg->szName);
    }
    else
        RTTestFailed(g_hTest, "Failed to construct audio driver '%s': %Rrc", pDrvReg->szName, rc);
    if (pDrvReg->pfnDestruct)
        pDrvReg->pfnDestruct(pDrvIns);
    RTMemFree(pDrvIns);
    return rc;
}

/**
 * Destructs a PDM audio driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvReg             PDM driver registration record to destruct.
 * @param   pDrvIns             Driver instance to destruct.
 */
static int audioTestDrvDestruct(PCPDMDRVREG pDrvReg, PPDMDRVINS pDrvIns)
{
    if (!pDrvIns)
        return VINF_SUCCESS;

    if (pDrvReg->pfnDestruct)
        pDrvReg->pfnDestruct(pDrvIns);

    pDrvIns->u32Version = 0;
    RTMemFree(pDrvIns);

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

    int rc = pTstEnv->pDrvAudio->pfnGetDevices(pTstEnv->pDrvAudio, &pTstEnv->DevEnum);
    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOHOSTDEV pDev;
        RTListForEach(&pTstEnv->DevEnum.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
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
 * Creates an audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pCfg                Stream configuration to use for creation.
 */
static int audioTestStreamCreate(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg)
{
    PDMAUDIOSTREAMCFG CfgAcq;

    int rc = PDMAudioStrmCfgCopy(&CfgAcq, pCfg);
    AssertRC(rc); /* Cannot fail. */

    rc = pTstEnv->pDrvAudio->pfnStreamCreate(pTstEnv->pDrvAudio, &pStream->Backend, pCfg, &CfgAcq);
    if (RT_FAILURE(rc))
        return rc;

    /* Do the async init in a synchronous way for now here. */
    if (rc == VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED)
        rc = pTstEnv->pDrvAudio->pfnStreamInitAsync(pTstEnv->pDrvAudio, &pStream->Backend, false /* fDestroyed */);

    if (RT_SUCCESS(rc))
        pStream->fCreated = true;

    return rc;
}

/**
 * Destroys an audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment the stream to destroy contains.
 * @param   pStream             Audio stream to destroy.
 */
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream)
{
    if (!pStream)
        return VINF_SUCCESS;

    if (!pStream->fCreated)
        return VINF_SUCCESS;

    /** @todo Anything else to do here, e.g. test if there are left over samples or some such? */

    int rc = pTstEnv->pDrvAudio->pfnStreamDestroy(pTstEnv->pDrvAudio, &pStream->Backend);
    if (RT_SUCCESS(rc))
        RT_BZERO(pStream, sizeof(PDMAUDIOBACKENDSTREAM));

    return rc;
}

/**
 * Creates an audio default output test stream.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pProps              PCM properties to use for creation.
 */
static int audioTestCreateStreamDefaultOut(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps)
{
    PDMAUDIOSTREAMCFG Cfg;
    int rc = PDMAudioStrmCfgInitWithProps(&Cfg, pProps);
    AssertRC(rc); /* Cannot fail. */

    Cfg.enmDir      = PDMAUDIODIR_OUT;
    Cfg.u.enmDst    = PDMAUDIOPLAYBACKDST_FRONT;
    Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

    Cfg.Backend.cFramesBufferSize   = PDMAudioPropsMilliToFrames(pProps, 300);
    Cfg.Backend.cFramesPreBuffering = PDMAudioPropsMilliToFrames(pProps, 200);
    Cfg.Backend.cFramesPeriod       = PDMAudioPropsMilliToFrames(pProps, 10);
    Cfg.Device.cMsSchedulingHint    = 10;

    return audioTestStreamCreate(pTstEnv, pStream, &Cfg);
}

/**
 * Plays a test tone on a specific audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running the test.
 * @param   pStream             Stream to use for playing the tone.
 * @param   pParms              Tone parameters to use.
 *
 * @note    Blocking function.
 */
static int audioTestPlayTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms)
{
    AUDIOTESTTONE TstTone;
    AudioTestToneInitRandom(&TstTone, &pParms->Props);

    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Playing test tone (tone frequency is %RU16Hz, %RU32ms)\n", TstTone.rdFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "tone.pcm", &pObj);
    AssertRCReturn(rc, rc);

    PDMHOSTAUDIOSTREAMSTATE enmState = pTstEnv->pDrvAudio->pfnStreamGetState(pTstEnv->pDrvAudio, &pStream->Backend);
    if (enmState == PDMHOSTAUDIOSTREAMSTATE_OKAY)
    {
        uint32_t cbBuf;
        uint8_t  abBuf[_4K];

        const uint64_t tsStartMs     = RTTimeMilliTS();
        const uint16_t cSchedulingMs = RTRandU32Ex(10, 80); /* Chose a random scheduling (in ms). */
        const uint32_t cbPerMs       = PDMAudioPropsMilliToBytes(&pParms->Props, cSchedulingMs);

        do
        {
            rc = AudioTestToneGenerate(&TstTone, abBuf, RT_MIN(cbPerMs, sizeof(abBuf)), &cbBuf);
            if (RT_SUCCESS(rc))
            {
                /* Write stuff to disk before trying to play it. Help analysis later. */
                rc = AudioTestSetObjWrite(pObj, abBuf, cbBuf);
                if (RT_SUCCESS(rc))
                {
                    uint32_t cbWritten;
                    rc = pTstEnv->pDrvAudio->pfnStreamPlay(pTstEnv->pDrvAudio, &pStream->Backend, abBuf, cbBuf, &cbWritten);
                }
            }

            if (RTTimeMilliTS() - tsStartMs >= pParms->msDuration)
                break;

            if (RT_FAILURE(rc))
                break;

            RTThreadSleep(cSchedulingMs);

        } while (RT_SUCCESS(rc));
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

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


/*********************************************************************************************************************************
*   Test callbacks                                                                                                               *
*********************************************************************************************************************************/

/**
 * @copydoc FNAUDIOTESTSETUP
 */
static DECLCALLBACK(int) audioTestPlayToneSetup(PAUDIOTESTENV pTstEnv, PAUDIOTESTDESC pTstDesc, PAUDIOTESTPARMS pTstParmsAcq, void **ppvCtx)
{
    RT_NOREF(pTstEnv, pTstDesc, ppvCtx);

    PDMAudioPropsInit(&pTstParmsAcq->TestTone.Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

#ifdef DEBUG_andy
    pTstParmsAcq->cIterations = RTRandU32Ex(1, 1);
#endif
    pTstParmsAcq->cIterations = RTRandU32Ex(1, 10);
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

    PAUDIOTESTSTREAM pStream = &pTstEnv->aStreams[0];

    for (uint32_t i = 0; i < pTstParms->cIterations; i++)
    {
        AudioTestToneParamsInitRandom(&pTstParms->TestTone, &pTstParms->TestTone.Props);
        rc = audioTestCreateStreamDefaultOut(pTstEnv, pStream, &pTstParms->TestTone.Props);
        if (RT_SUCCESS(rc))
            rc = audioTestPlayTone(pTstEnv, pStream, &pTstParms->TestTone);

        int rc2 = audioTestStreamDestroy(pTstEnv, pStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
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


/*********************************************************************************************************************************
*   Test execution                                                                                                               *
*********************************************************************************************************************************/

static AUDIOTESTDESC g_aTests[] =
{
    /* pszTest      fExcluded      pfnSetup */
    { "PlayTone",   false,         audioTestPlayToneSetup,       audioTestPlayToneExec,      audioTestPlayToneDestroy }
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
            RTTestFailed(g_hTest, "Test setup failed\n");
            return rc;
        }
    }

    audioTestCombineParms(&TstParms, pOverrideParms);

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test #%u: %RU32 iterations\n", uSeq, TstParms.cIterations);

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

        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Test destruction failed\n");
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
static DECLCALLBACK(RTEXITCODE) audioTestMain(int argc, char **argv)
{
    AUDIOTESTENV TstEnv;
    RT_ZERO(TstEnv);

    AUDIOTESTPARMS TstCust;
    audioTestParmsInit(&TstCust);

    const char *pszDevice  = NULL; /* Custom device to use. Can be NULL if not being used. */
    const char *pszTag     = NULL; /* Custom tag to use. Can be NULL if not being used. */
    PCPDMDRVREG pDrvReg    = g_aBackends[0].pDrvReg;

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, g_aCmdTestOptions, RT_ELEMENTS(g_aCmdTestOptions), 1, 0 /*fFlags*/);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
                audioTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case 'a':
                for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
                    g_aTests[i].fExcluded = true;
                break;

            case 'b':
                pDrvReg = NULL;
                for (uintptr_t i = 0; i < RT_ELEMENTS(g_aBackends); i++)
                    if (   strcmp(ValueUnion.psz, g_aBackends[i].pszName) == 0
                        || strcmp(ValueUnion.psz, g_aBackends[i].pDrvReg->szName) == 0)
                    {
                        pDrvReg = g_aBackends[i].pDrvReg;
                        break;
                    }
                if (pDrvReg == NULL)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Unknown backend: '%s'", ValueUnion.psz);
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

            case VKAT_TEST_OPT_COUNT:
                break;

            case VKAT_TEST_OPT_DEV:
                pszDevice = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_PAUSE:
                break;

            case VKAT_TEST_OPT_OUTDIR:
                rc = RTStrCopy(TstEnv.szPathOut, sizeof(TstEnv.szPathOut), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExitFailure("Failed to copy out directory: %Rrc", rc);
                break;

            case VKAT_TEST_OPT_PCM_BIT:
                /** @todo r=bird: the X suffix means: "fingers off!"   */
                TstCust.TestTone.Props.cbSampleX = ValueUnion.u8 / 8 /* bit */;
                break;

            case VKAT_TEST_OPT_PCM_CHAN:
                /** @todo r=bird: the X suffix means: "fingers off!"   */
                TstCust.TestTone.Props.cChannelsX = ValueUnion.u8;
                break;

            case VKAT_TEST_OPT_PCM_HZ:
                TstCust.TestTone.Props.uHz = ValueUnion.u32;
                break;

            case VKAT_TEST_OPT_PCM_SIGNED:
                TstCust.TestTone.Props.fSigned = ValueUnion.f;
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

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    PPDMIHOSTAUDIO pDrvAudio = NULL;
    PPDMDRVINS     pDrvIns   = NULL;
    rc = audioTestDrvConstruct(pDrvReg, &pDrvIns, &pDrvAudio);
    if (RT_SUCCESS(rc))
    {
        /* For now all tests have the same test environment. */
        rc = audioTestEnvInit(&TstEnv, pDrvAudio, pszTag);
        if (RT_SUCCESS(rc))
        {
            PPDMAUDIOHOSTDEV pDev;
            rc = audioTestDevicesEnumerateAndCheck(&TstEnv, pszDevice, &pDev);
            if (RT_SUCCESS(rc))
                audioTestWorker(&TstEnv, &TstCust);

            /* Before destroying the test environment, pack up the test set so
             * that it's ready for transmission. */
            char szFileOut[RTPATH_MAX];
            rc = AudioTestSetPack(&TstEnv.Set, TstEnv.szPathOut, szFileOut, sizeof(szFileOut));
            if (RT_SUCCESS(rc))
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test set packed up to '%s'\n", szFileOut);

            /* Clean up. */
            AudioTestSetWipe(&TstEnv.Set);

            audioTestEnvDestroy(&TstEnv);
        }
    }
    audioTestDrvDestruct(pDrvReg, pDrvIns);

    audioTestParmsDestroy(&TstCust);

    if (RT_FAILURE(rc)) /* Let us know that something went wrong in case we forgot to mention it. */
        RTTestFailed(g_hTest, "Tested failed with %Rrc\n", rc);

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
static int audioVerifyOne(const char *pszPath, const char *pszTag)
{
    RTTestSubF(g_hTest, "Verifying test set (tag '%s') ...", pszTag ? pszTag : "default");

    AUDIOTESTSET tstSet;
    int rc = AudioTestSetOpen(&tstSet, pszPath);
    if (RT_SUCCESS(rc))
    {
        AUDIOTESTERRORDESC errDesc;
        rc = AudioTestSetVerify(&tstSet, pszTag ? pszTag : "default", &errDesc);
        if (RT_SUCCESS(rc))
        {
            if (AudioTestErrorDescFailed(&errDesc))
            {
                /** @todo Use some AudioTestErrorXXX API for enumeration here later. */
                PAUDIOTESTERRORENTRY pErrEntry;
                RTListForEach(&errDesc.List, pErrEntry, AUDIOTESTERRORENTRY, Node)
                {
                    RTTestFailed(g_hTest, pErrEntry->szDesc);
                }
            }
            else
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Verification successful");

            AudioTestErrorDescDestroy(&errDesc);
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
static DECLCALLBACK(RTEXITCODE) audioVerifyMain(int argc, char **argv)
{
    /*
     * Parse options and process arguments.
     */
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, g_aCmdVerifyOptions, RT_ELEMENTS(g_aCmdVerifyOptions),
                          1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    const char   *pszTag   = NULL; /* Custom tag to use. Can be NULL if not being used. */
    unsigned      iTestSet = 0;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case VKAT_VERIFY_OPT_TAG:
                pszTag = ValueUnion.psz;
                if (g_uVerbosity > 0)
                    RTMsgInfo("Using tag '%s'\n", pszTag);
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (iTestSet == 0)
                    RTTestBanner(g_hTest);
                audioVerifyOne(ValueUnion.psz, pszTag);
                iTestSet++;
                break;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * If no paths given, default to the current directory.
     */
    if (iTestSet == 0)
    {
        if (iTestSet == 0)
            RTTestBanner(g_hTest);
        char szDirCur[RTPATH_MAX];
        rc = RTPathGetCurrent(szDirCur, sizeof(szDirCur));
        audioVerifyOne(RT_SUCCESS(rc) ? szDirCur : ".", pszTag);
    }

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
    RTEXITCODE rcExit = RTTestInitAndCreate("AudioTest", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /*
     * Process common options.
     */
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, g_aCmdCommonOptions,
                          RT_ELEMENTS(g_aCmdCommonOptions), 1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'q':
                g_uVerbosity = 0;
                break;

            case 'v':
                g_uVerbosity++;
                break;

            case 'V':
                RTPrintf("v0.0.1\n");
                return RTEXITCODE_SUCCESS;

            case 'h':
                audioTestShowLogo(g_pStdOut);
                audioTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
            {
                for (uintptr_t i = 0; i < RT_ELEMENTS(g_aCommands); i++)
                    if (strcmp(ValueUnion.psz, g_aCommands[i].pszCommand) == 0)
                    {
                        audioTestShowLogo(g_pStdOut);
                        int32_t iCurArg = GetState.iNext - 1;
                        return g_aCommands[i].pfnHandler(argc - iCurArg, argv + iCurArg);
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

