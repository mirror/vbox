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
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Enumeration specifying the current audio test mode.
 */
typedef enum AUDIOTESTMODE
{
    /** Unknown mode. */
    AUDIOTESTMODE_UNKNOWN = 0,
    /** VKAT is running on the guest side. */
    AUDIOTESTMODE_GUEST,
    /** VKAT is running on the host side. */
    AUDIOTESTMODE_HOST
} AUDIOTESTMODE;

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

/**
 * Structure for keeping an audio test audio stream.
 */
typedef struct AUDIOTESTSTREAM
{
    /** The PDM stream. */
    PPDMAUDIOSTREAM         pStream;
    /** The backend stream. */
    PPDMAUDIOBACKENDSTREAM  pBackend;
    /** The stream config. */
    PDMAUDIOSTREAMCFG       Cfg;
} AUDIOTESTSTREAM;
/** Pointer to audio test stream. */
typedef AUDIOTESTSTREAM *PAUDIOTESTSTREAM;

/** Maximum audio streams a test environment can handle. */
#define AUDIOTESTENV_MAX_STREAMS 8

/**
 * Audio test environment parameters.
 * Not necessarily bound to a specific test (can be reused).
 */
typedef struct AUDIOTESTENV
{
    /** Audio testing mode. */
    AUDIOTESTMODE           enmMode;
    /** Output path for storing the test environment's final test files. */
    char                    szTag[AUDIOTEST_TAG_MAX];
    /** Output path for storing the test environment's final test files. */
    char                    szPathOut[RTPATH_MAX];
    /** Temporary path for this test environment. */
    char                    szPathTemp[RTPATH_MAX];
    /** Buffer size (in ms). */
    RTMSINTERVAL            cMsBufferSize;
    /** Pre-buffering time (in ms). */
    RTMSINTERVAL            cMsPreBuffer;
    /** Scheduling hint (in ms). */
    RTMSINTERVAL            cMsSchedulingHint;
    /** The audio test driver stack. */
    AUDIOTESTDRVSTACK       DrvStack;
    /** The current (last) audio device enumeration to use. */
    PDMAUDIOHOSTENUM        DevEnum;
    /** Audio stream. */
    AUDIOTESTSTREAM         aStreams[AUDIOTESTENV_MAX_STREAMS];
    /** The audio test set to use. */
    AUDIOTESTSET            Set;
    union
    {
        struct
        {
            /** ATS instance to use. */
            ATSSERVER       Srv;
        } Guest;
        struct
        {
            ATSCLIENT       Client;
        } Host;
    } u;
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

/**
 * Structure for keeping a user context for the test service callbacks.
 */
typedef struct ATSCALLBACKCTX
{
    PAUDIOTESTENV pTstEnv;
} ATSCALLBACKCTX;
typedef ATSCALLBACKCTX *PATSCALLBACKCTX;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int audioTestCombineParms(PAUDIOTESTPARMS pBaseParms, PAUDIOTESTPARMS pOverrideParms);
static int audioTestCreateStreamDefaultIn(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps);
static int audioTestCreateStreamDefaultOut(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps);
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream);
static int audioTestDevicesEnumerateAndCheck(PAUDIOTESTENV pTstEnv, const char *pszDev, PPDMAUDIOHOSTDEV *ppDev);
static int audioTestEnvPrologue(PAUDIOTESTENV pTstEnv);

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
    VKAT_TEST_OPT_ATS_ADDR,
    VKAT_TEST_OPT_ATS_PORT,
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
    VKAT_SELFTEST_OPT_ATS_HOST = 900
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
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--drvaudio",         'd',                          RTGETOPT_REQ_NOTHING },
    { "--exclude",          'e',                          RTGETOPT_REQ_UINT32  },
    { "--exclude-all",      'a',                          RTGETOPT_REQ_NOTHING },
    { "--mode",             VKAT_TEST_OPT_MODE,           RTGETOPT_REQ_STRING  },
    { "--ats-address",      VKAT_TEST_OPT_ATS_ADDR,       RTGETOPT_REQ_STRING  },
    { "--ats-port",         VKAT_TEST_OPT_ATS_PORT,       RTGETOPT_REQ_UINT32  },
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
    {   &g_DrvHostALSAAudio,          "alsa" },
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
    {   &g_DrvHostPulseAudio,         "pulseaudio" },
    {   &g_DrvHostPulseAudio,         "pulse" },
    {   &g_DrvHostPulseAudio,         "pa" },
#endif
#ifdef VBOX_WITH_AUDIO_OSS
    {   &g_DrvHostOSSAudio,           "oss" },
#endif
#if defined(RT_OS_DARWIN)
    {   &g_DrvHostCoreAudio,          "coreaudio" },
    {   &g_DrvHostCoreAudio,          "core" },
    {   &g_DrvHostCoreAudio,          "ca" },
#endif
#if defined(RT_OS_WINDOWS)
    {   &g_DrvHostAudioWas,           "wasapi" },
    {   &g_DrvHostAudioWas,           "was" },
    {   &g_DrvHostDSound,             "directsound" },
    {   &g_DrvHostDSound,             "dsound" },
    {   &g_DrvHostDSound,             "ds" },
#endif
    {   &g_DrvHostValidationKitAudio, "valkit" }
};
AssertCompile(sizeof(g_aBackends) > 0 /* port me */);


/** Terminate ASAP if set.  Set on Ctrl-C. */
static bool volatile    g_fTerminate = false;
/** The release logger. */
static PRTLOGGER        g_pRelLogger = NULL;


/** The test handle. */
RTTEST        g_hTest;
/** The current verbosity level. */
unsigned      g_uVerbosity = 0;
/** DrvAudio: Enable debug (or not). */
bool          g_fDrvAudioDebug = 0;
/** DrvAudio: The debug output path. */
const char   *g_pszDrvAudioDebug = NULL;


/**
 * Helper for handling --backend options.
 *
 * @returns Pointer to the specified backend, NULL if not found (error
 *          displayed).
 * @param   pszBackend      The backend option value.
 */
static PCPDMDRVREG audioTestFindBackendOpt(const char *pszBackend)
{
    for (uintptr_t i = 0; i < RT_ELEMENTS(g_aBackends); i++)
        if (   strcmp(pszBackend, g_aBackends[i].pszName) == 0
            || strcmp(pszBackend, g_aBackends[i].pDrvReg->szName) == 0)
            return g_aBackends[i].pDrvReg;
    RTMsgError("Unknown backend: '%s'", pszBackend);
    return NULL;
}


/*********************************************************************************************************************************
*   Test Primitives                                                                                                              *
*********************************************************************************************************************************/

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
    AudioTestToneInit(&TstTone, &pParms->Props, pParms->dbFreqHz);

    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Playing test tone (tone frequency is %RU16Hz, %RU32ms)\n", (uint16_t)pParms->dbFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "tone-play.pcm", &pObj);
    AssertRCReturn(rc, rc);

    if (audioTestDriverStackStreamIsOkay(&pTstEnv->DrvStack, pStream->pStream))
    {
        uint32_t cbBuf;
        uint8_t  abBuf[_4K];

        const uint32_t cbPerSched = PDMAudioPropsMilliToBytes(&pParms->Props, pTstEnv->cMsSchedulingHint);
        AssertStmt(cbPerSched, rc = VERR_INVALID_PARAMETER);
              uint32_t cbToWrite  = PDMAudioPropsMilliToBytes(&pParms->Props, pParms->msDuration);
        AssertStmt(cbToWrite,  rc = VERR_INVALID_PARAMETER);

        if (RT_SUCCESS(rc))
        {
            AudioTestSetObjAddMetadataStr(pObj, "buffer_size_ms=%RU32\n", pTstEnv->cMsBufferSize);
            AudioTestSetObjAddMetadataStr(pObj, "prebuf_size_ms=%RU32\n", pTstEnv->cMsPreBuffer);
            AudioTestSetObjAddMetadataStr(pObj, "scheduling_hint_ms=%RU32\n", pTstEnv->cMsSchedulingHint);

            while (cbToWrite)
            {
                uint32_t cbWritten    = 0;
                uint32_t cbToGenerate = RT_MIN(cbToWrite, RT_MIN(cbPerSched, sizeof(abBuf)));
                Assert(cbToGenerate);

                rc = AudioTestToneGenerate(&TstTone, abBuf, cbToGenerate, &cbBuf);
                if (RT_SUCCESS(rc))
                {
                    /* Write stuff to disk before trying to play it. Help analysis later. */
                    rc = AudioTestSetObjWrite(pObj, abBuf, cbBuf);
                    if (RT_SUCCESS(rc))
                        rc = audioTestDriverStackStreamPlay(&pTstEnv->DrvStack, pStream->pStream,
                                                            abBuf, cbBuf, &cbWritten);
                }

                if (RT_FAILURE(rc))
                    break;

                RTThreadSleep(pTstEnv->cMsSchedulingHint);

                RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Written %RU32 bytes\n", cbWritten);

                Assert(cbToWrite >= cbWritten);
                cbToWrite -= cbWritten;
            }
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Playing tone done failed with %Rrc\n", rc);

    return rc;
}

/**
 * Records a test tone from a specific audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running the test.
 * @param   pStream             Stream to use for recording the tone.
 * @param   pParms              Tone parameters to use.
 *
 * @note    Blocking function.
 */
static int audioTestRecordTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms)
{
    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Recording test tone (for %RU32ms)\n", pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "tone-rec.pcm", &pObj);
    AssertRCReturn(rc, rc);

    if (audioTestDriverStackStreamIsOkay(&pTstEnv->DrvStack, pStream->pStream))
    {
        const uint32_t cbPerSched = PDMAudioPropsMilliToBytes(&pParms->Props, pTstEnv->cMsSchedulingHint);
        AssertStmt(cbPerSched, rc = VERR_INVALID_PARAMETER);
              uint32_t cbToRead   = PDMAudioPropsMilliToBytes(&pParms->Props, pParms->msDuration);
        AssertStmt(cbToRead,   rc = VERR_INVALID_PARAMETER);

        if (RT_SUCCESS(rc))
        {
            AudioTestSetObjAddMetadataStr(pObj, "buffer_size_ms=%RU32\n", pTstEnv->cMsBufferSize);
            AudioTestSetObjAddMetadataStr(pObj, "prebuf_size_ms=%RU32\n", pTstEnv->cMsPreBuffer);
            AudioTestSetObjAddMetadataStr(pObj, "scheduling_hint_ms=%RU32\n", pTstEnv->cMsSchedulingHint);

            uint8_t abBuf[_4K];

            while (cbToRead)
            {
                const uint32_t cbChunk = RT_MIN(cbToRead, RT_MIN(cbPerSched, sizeof(abBuf)));

                uint32_t cbRead = 0;
                rc = audioTestDriverStackStreamCapture(&pTstEnv->DrvStack, pStream->pStream, (void *)abBuf, cbChunk, &cbRead);
                if (RT_SUCCESS(rc))
                    rc = AudioTestSetObjWrite(pObj, abBuf, cbRead);

                if (RT_FAILURE(rc))
                    break;

                RTThreadSleep(pTstEnv->cMsSchedulingHint);

                RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Read %RU32 bytes\n", cbRead);

                Assert(cbToRead >= cbRead);
                cbToRead -= cbRead;
            }
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Recording tone done failed with %Rrc\n", rc);

    return rc;
}


/*********************************************************************************************************************************
*   ATS Callback Implementations                                                                                                 *
*********************************************************************************************************************************/

/** @copydoc ATSCALLBACKS::pfnTestSetBegin */
static DECLCALLBACK(int) audioTestSvcTestSetBeginCallback(void const *pvUser, const char *pszTag)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Beginning test set '%s'\n", pszTag);

    return AudioTestSetCreate(&pTstEnv->Set, pTstEnv->szPathTemp, pszTag);
}

/** @copydoc ATSCALLBACKS::pfnTestSetEnd */
static DECLCALLBACK(int) audioTestSvcTestSetEndCallback(void const *pvUser, const char *pszTag)
{
    RT_NOREF(pszTag);

    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Ending test set '%s'\n", pszTag);

    return audioTestEnvPrologue(pTstEnv);
}

/** @copydoc ATSCALLBACKS::pfnTonePlay */
static DECLCALLBACK(int) audioTestSvcTonePlayCallback(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    AUDIOTESTTONE TstTone;
    AudioTestToneInitRandom(&TstTone, &pToneParms->Props);

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestCreateStreamDefaultOut(pTstEnv, pTstStream, &pToneParms->Props);
    if (RT_SUCCESS(rc))
    {
        AUDIOTESTPARMS TstParms;
        RT_ZERO(TstParms);
        TstParms.enmType  = AUDIOTESTTYPE_TESTTONE_PLAY;
        TstParms.enmDir   = PDMAUDIODIR_OUT;
        TstParms.TestTone = *pToneParms;

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Playing test tone", &TstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestPlayTone(pTstEnv, pTstStream, pToneParms);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Playing tone failed");
        }

        int rc2 = audioTestStreamDestroy(pTstEnv, pTstStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    else
        RTTestFailed(g_hTest, "Error creating output stream, rc=%Rrc\n", rc);

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnToneRecord */
static DECLCALLBACK(int) audioTestSvcToneRecordCallback(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestCreateStreamDefaultIn(pTstEnv, pTstStream, &pToneParms->Props);
    if (RT_SUCCESS(rc))
    {
        AUDIOTESTPARMS TstParms;
        RT_ZERO(TstParms);
        TstParms.enmType  = AUDIOTESTTYPE_TESTTONE_RECORD;
        TstParms.enmDir   = PDMAUDIODIR_IN;
        TstParms.Props    = pToneParms->Props;
        TstParms.TestTone = *pToneParms;

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Recording test tone", &TstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestRecordTone(pTstEnv, pTstStream, pToneParms);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Recording tone failed");
        }

        int rc2 = audioTestStreamDestroy(pTstEnv, pTstStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    else
        RTTestFailed(g_hTest, "Error creating input stream, rc=%Rrc\n", rc);

    return rc;
}


/*********************************************************************************************************************************
*   Implementation of audio test environment handling                                                                            *
*********************************************************************************************************************************/

/**
 * Initializes an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvReg             Audio driver to use.
 * @param   fWithDrvAudio       Whether to include DrvAudio in the stack or not.
 * @param   pszTag              Tag name to use. If NULL, a generated UUID will be used.
 * @param   pszTcpAddr          TCP/IP address to connect to.
 * @param   uTcpPort            TCP/IP port to connect to.
 */
static int audioTestEnvInit(PAUDIOTESTENV pTstEnv,
                            PCPDMDRVREG pDrvReg, bool fWithDrvAudio, const char *pszTag,
                            const char *pszTcpAddr, uint32_t uTcpPort)
{
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test mode is '%s'\n", pTstEnv->enmMode == AUDIOTESTMODE_HOST ? "host" : "guest");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pszTag);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", pTstEnv->szPathOut);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Temp directory is '%s'\n", pTstEnv->szPathTemp);

    int rc = VINF_SUCCESS;

    PDMAudioHostEnumInit(&pTstEnv->DevEnum);

    pTstEnv->cMsBufferSize     = 300; /* ms */ /** @todo Randomize this also? */
    pTstEnv->cMsPreBuffer      = 150; /* ms */ /** @todo Ditto. */
    pTstEnv->cMsSchedulingHint = RTRandU32Ex(10, 80); /* Choose a random scheduling (in ms). */

    /* Only the guest mode needs initializing the driver stack. */
    const bool fUseDriverStack = pTstEnv->enmMode == AUDIOTESTMODE_GUEST;
    if (fUseDriverStack)
    {
        rc = audioTestDriverStackInit(&pTstEnv->DrvStack, pDrvReg, fWithDrvAudio);
        if (RT_FAILURE(rc))
            return rc;

        PPDMAUDIOHOSTDEV pDev;
        rc = audioTestDevicesEnumerateAndCheck(pTstEnv, NULL /* pszDevice */, &pDev); /** @todo Implement device checking. */
        if (RT_FAILURE(rc))
            return rc;
    }

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

    if (RT_FAILURE(rc))
        return rc;

    /** @todo Implement NAT mode like we do for TxS later? */
    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        ATSCALLBACKCTX Ctx;
        Ctx.pTstEnv = pTstEnv;

        ATSCALLBACKS Callbacks;
        Callbacks.pfnTestSetBegin = audioTestSvcTestSetBeginCallback;
        Callbacks.pfnTestSetEnd   = audioTestSvcTestSetEndCallback;
        Callbacks.pfnTonePlay     = audioTestSvcTonePlayCallback;
        Callbacks.pfnToneRecord   = audioTestSvcToneRecordCallback;
        Callbacks.pvUser          = &Ctx;

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Starting ATS ...\n");
        rc = AudioTestSvcInit(&pTstEnv->u.Guest.Srv, &Callbacks);
        if (RT_SUCCESS(rc))
            rc = AudioTestSvcStart(&pTstEnv->u.Guest.Srv);

        if (RT_FAILURE(rc))
        {
            RTTestFailed(g_hTest, "Starting ATS failed with %Rrc\n", rc);
            return rc;
        }
    }
    else /* Host mode */
    {
        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Connecting to ATS at %s:%RU32 ...\n", pszTcpAddr, uTcpPort);

        rc = AudioTestSvcClientConnect(&pTstEnv->u.Host.Client, pszTcpAddr, uTcpPort);
        if (RT_FAILURE(rc))
        {
            RTTestFailed(g_hTest, "Connecting to ATS failed with %Rrc\n", rc);
            return rc;
        }

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connected to ATS\n");
    }

    if (   RT_FAILURE(rc)
        && fUseDriverStack)
        audioTestDriverStackDelete(&pTstEnv->DrvStack);

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

    audioTestDriverStackDelete(&pTstEnv->DrvStack);
}

/**
 * Closes, packs up and destroys a test environment.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to handle.
 */
static int audioTestEnvPrologue(PAUDIOTESTENV pTstEnv)
{
    /* Close the test set first. */
    AudioTestSetClose(&pTstEnv->Set);

    /* Before destroying the test environment, pack up the test set so
     * that it's ready for transmission. */
    char szFileOut[RTPATH_MAX];
    int rc = AudioTestSetPack(&pTstEnv->Set, pTstEnv->szPathOut, szFileOut, sizeof(szFileOut));
    if (RT_SUCCESS(rc))
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test set packed up to '%s'\n", szFileOut);

    int rc2 = AudioTestSetWipe(&pTstEnv->Set);
    if (RT_SUCCESS(rc))
        rc = rc2;

    AudioTestSetDestroy(&pTstEnv->Set);

    return rc;
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


/*********************************************************************************************************************************
*   Device enumeration + handling.                                                                                               *
*********************************************************************************************************************************/

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

    if (!pTstEnv->DrvStack.pIHostAudio->pfnGetDevices)
    {
        RTTestSkipped(g_hTest, "Backend does not support device enumeration, skipping");
        return VINF_NOT_SUPPORTED;
    }

    Assert(pszDev == NULL || ppDev);

    if (ppDev)
        *ppDev = NULL;

    int rc = pTstEnv->DrvStack.pIHostAudio->pfnGetDevices(pTstEnv->DrvStack.pIHostAudio, &pTstEnv->DevEnum);
    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOHOSTDEV pDev;
        RTListForEach(&pTstEnv->DevEnum.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
        {
            char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
            if (pDev->pszId)
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s' (ID '%s'):\n", pDev->pszName, pDev->pszId);
            else
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s':\n", pDev->pszName);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Usage           = %s\n",   PDMAudioDirGetName(pDev->enmUsage));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Flags           = %s\n",   PDMAudioHostDevFlagsToString(szFlags, pDev->fFlags));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Input channels  = %RU8\n", pDev->cMaxInputChannels);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Output channels = %RU8\n", pDev->cMaxOutputChannels);

            if (   pszDev
                && !RTStrCmp(pDev->pszName, pszDev))
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

    RTTestSubF(g_hTest, "Opening audio device '%s' ...", pDev->pszName);

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

    RTTestSubF(g_hTest, "Closing audio device '%s' ...", pDev->pszName);

    /** @todo Close device here. */

    RTTestSubDone(g_hTest);

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
    int rc = VINF_SUCCESS;
    if (pStream && pStream->pStream)
    {
        /** @todo Anything else to do here, e.g. test if there are left over samples or some such? */

        audioTestDriverStackStreamDestroy(&pTstEnv->DrvStack, pStream->pStream);
        pStream->pStream  = NULL;
        pStream->pBackend = NULL;
    }

    return rc;
}

/**
 * Creates an audio default input (recording) test stream.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pProps              PCM properties to use for creation.
 */
static int audioTestCreateStreamDefaultIn(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps)
{
    pStream->pBackend = NULL;
    int rc = audioTestDriverStackStreamCreateInput(&pTstEnv->DrvStack, pProps, pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer,
                                                   pTstEnv->cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    if (RT_SUCCESS(rc) && !pTstEnv->DrvStack.pIAudioConnector)
        pStream->pBackend = &((PAUDIOTESTDRVSTACKSTREAM)pStream->pStream)->Backend;
    return rc;
}

/**
 * Creates an audio default output (playback) test stream.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pProps              PCM properties to use for creation.
 */
static int audioTestCreateStreamDefaultOut(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps)
{
    pStream->pBackend = NULL;
    int rc = audioTestDriverStackStreamCreateOutput(&pTstEnv->DrvStack, pProps, pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer,
                                                    pTstEnv->cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    if (RT_SUCCESS(rc) && !pTstEnv->DrvStack.pIAudioConnector)
        pStream->pBackend = &((PAUDIOTESTDRVSTACKSTREAM)pStream->pStream)->Backend;
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

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Playing test tone", pTstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            PDMAUDIOSTREAMCFG Cfg;
            RT_ZERO(Cfg);
            /** @todo Add more parameters here? */
            Cfg.Props = pTstParms->Props;

            rc = AudioTestSvcClientTonePlay(&pTstEnv->u.Host.Client, &pTstParms->TestTone);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Playing test tone failed");
        }

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
        pTstParms->TestTone.msDuration = RTRandU32Ex(50 /* ms */, RT_MS_10SEC); /** @todo Record even longer? */

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Recording test tone", pTstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            rc = AudioTestSvcClientToneRecord(&pTstEnv->u.Host.Client, &pTstParms->TestTone);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Recording test tone failed");
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
            RTTestFailed(g_hTest, "Test setup failed\n");
            return rc;
        }
    }

    audioTestCombineParms(&TstParms, pOverrideParms);

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test #%u: %RU32 iterations\n", uSeq, TstParms.cIterations);

    if (TstParms.Dev.pszName && strlen(TstParms.Dev.pszName)) /** @todo Refine this check. Use pszId for starters! */
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

    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "ATS running\n");

        while (!g_fTerminate) /** @todo Implement signal handling. */
        {
            RTThreadSleep(100);
        }

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Shutting down ATS ...\n");

        int rc2 = AudioTestSvcShutdown(&pTstEnv->u.Guest.Srv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    else if (pTstEnv->enmMode == AUDIOTESTMODE_HOST)
    {
        /* We have one single test set for all executed tests for now. */
        rc = AudioTestSetCreate(&pTstEnv->Set, pTstEnv->szPathTemp, pTstEnv->szTag);
        if (RT_SUCCESS(rc))
        {
            /* Copy back the (eventually generated) tag to the test environment. */
            rc = RTStrCopy(pTstEnv->szTag, sizeof(pTstEnv->szTag), AudioTestSetGetTag(&pTstEnv->Set));
            AssertRC(rc);

            rc = AudioTestSvcClientTestSetBegin(&pTstEnv->u.Host.Client, pTstEnv->szTag);
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
                }

                int rc2 = AudioTestSvcClientTestSetEnd(&pTstEnv->u.Host.Client, pTstEnv->szTag);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }

            audioTestEnvPrologue(pTstEnv);
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
        case 'd':                    return "Go via DrvAudio instead of directly interfacing with the backend";
        case VKAT_TEST_OPT_DEV:      return "Use the specified audio device";
        case VKAT_TEST_OPT_ATS_ADDR: return "ATS address (hostname or IP) to connect to";
        case VKAT_TEST_OPT_ATS_PORT: return "ATS port to connect to. Defaults to 6052 if not set";
        case VKAT_TEST_OPT_MODE:     return "Specifies the mode this program runs at";
        case 'e':                    return "Exclude the given test id from the list";
        case 'a':                    return "Exclude all tests from the list (useful to enable single tests later with --include)";
        case 'i':                    return "Include the given test id in the list";
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
    const char *pszTcpAddr    = NULL;
    uint16_t    uTcpPort      = 0;

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

            case VKAT_TEST_OPT_ATS_ADDR:
                if (TstEnv.enmMode == AUDIOTESTMODE_UNKNOWN)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Must specify a test mode first!");
                pszTcpAddr = ValueUnion.psz;
                break;

            case VKAT_TEST_OPT_ATS_PORT:
                if (TstEnv.enmMode == AUDIOTESTMODE_UNKNOWN)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Must specify a test mode first!");
                uTcpPort = ValueUnion.u32;
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
        /* Use the default port is none is specified. */
        if (!uTcpPort)
            uTcpPort = ATS_DEFAULT_PORT;

        if (!pszTcpAddr)
            return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--ats-address missing\n");
    }

    /* For now all tests have the same test environment. */
    rc = audioTestEnvInit(&TstEnv, pDrvReg, fWithDrvAudio, pszTag, pszTcpAddr, uTcpPort);
    if (RT_SUCCESS(rc))
    {
        audioTestWorker(&TstEnv, &TstCust);
        audioTestEnvDestroy(&TstEnv);
    }

    audioTestParmsDestroy(&TstCust);

    if (RT_FAILURE(rc)) /* Let us know that something went wrong in case we forgot to mention it. */
        RTTestFailed(g_hTest, "Tested failed with %Rrc\n", rc);

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


/*********************************************************************************************************************************
*   Command: play                                                                                                                *
*********************************************************************************************************************************/

/**
 * Worker for audioTestPlayOne implementing the play loop.
 */
static RTEXITCODE audioTestPlayOneInner(PAUDIOTESTDRVMIXSTREAM pMix, PAUDIOTESTWAVEFILE pWaveFile,
                                        PCPDMAUDIOSTREAMCFG pCfgAcq, const char *pszFile)
{
    uint32_t const  cbPreBuffer = PDMAudioPropsFramesToBytes(pMix->pProps, pCfgAcq->Backend.cFramesPreBuffering);
    uint64_t const  nsStarted   = RTTimeNanoTS();

    /*
     * Transfer data as quickly as we're allowed.
     */
    uint8_t         abSamples[16384];
    uint32_t const  cbSamplesAligned = PDMAudioPropsFloorBytesToFrame(pMix->pProps, sizeof(abSamples));
    uint64_t        offStream        = 0;
    while (!g_fTerminate)
    {
        /* Read a chunk from the wave file. */
        size_t      cbSamples = 0;
        int rc = AudioTestWaveFileRead(pWaveFile, abSamples, cbSamplesAligned, &cbSamples);
        if (RT_SUCCESS(rc) && cbSamples > 0)
        {
            /* Pace ourselves a little. */
            if (offStream >= cbPreBuffer)
            {
                uint64_t const cNsWritten = PDMAudioPropsBytesToNano64(pMix->pProps, offStream);
                uint64_t const cNsElapsed = RTTimeNanoTS() - nsStarted;
                if (cNsWritten + RT_NS_10MS > cNsElapsed)
                    RTThreadSleep((cNsWritten - cNsElapsed - RT_NS_10MS / 2) / RT_NS_1MS);
            }

            /* Transfer the data to the audio stream. */
            for (uint32_t offSamples = 0; offSamples < cbSamples;)
            {
                uint32_t const cbCanWrite = AudioTestMixStreamGetWritable(pMix);
                if (cbCanWrite > 0)
                {
                    uint32_t const cbToPlay = RT_MIN(cbCanWrite, (uint32_t)cbSamples - offSamples);
                    uint32_t       cbPlayed = 0;
                    rc = AudioTestMixStreamPlay(pMix, &abSamples[offSamples], cbToPlay, &cbPlayed);
                    if (RT_SUCCESS(rc))
                    {
                        if (cbPlayed)
                        {
                            offSamples += cbPlayed;
                            offStream  += cbPlayed;
                        }
                        else
                            return RTMsgErrorExitFailure("Played zero bytes - %#x bytes reported playable!\n", cbCanWrite);
                    }
                    else
                        return RTMsgErrorExitFailure("Failed to play %#x bytes: %Rrc\n", cbToPlay, rc);
                }
                else if (AudioTestMixStreamIsOkay(pMix))
                    RTThreadSleep(RT_MIN(RT_MAX(1, pCfgAcq->Device.cMsSchedulingHint), 256));
                else
                    return RTMsgErrorExitFailure("Stream is not okay!\n");
            }
        }
        else if (RT_SUCCESS(rc) && cbSamples == 0)
            break;
        else
            return RTMsgErrorExitFailure("Error reading wav file '%s': %Rrc", pszFile, rc);
    }

    /*
     * Drain the stream.
     */
    if (g_uVerbosity > 0)
        RTMsgInfo("%'RU64 ns: Draining...\n", RTTimeNanoTS() - nsStarted);
    int rc = AudioTestMixStreamDrain(pMix, true /*fSync*/);
    if (RT_SUCCESS(rc))
    {
        if (g_uVerbosity > 0)
            RTMsgInfo("%'RU64 ns: Done\n", RTTimeNanoTS() - nsStarted);
    }
    else
        return RTMsgErrorExitFailure("Draining failed: %Rrc", rc);

    return RTEXITCODE_SUCCESS;
}


/**
 * Worker for audioTestCmdPlayHandler that plays one file.
 */
static RTEXITCODE audioTestPlayOne(const char *pszFile, PCPDMDRVREG pDrvReg, const char *pszDevId, uint32_t cMsBufferSize,
                                   uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                   uint8_t cChannels, uint8_t cbSample, uint32_t uHz,
                                   bool fWithDrvAudio, bool fWithMixer)
{
    char szTmp[128];

    /*
     * First we must open the file and determin the format.
     */
    RTERRINFOSTATIC ErrInfo;
    AUDIOTESTWAVEFILE WaveFile;
    int rc = AudioTestWaveFileOpen(pszFile, &WaveFile, RTErrInfoInitStatic(&ErrInfo));
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("Failed to open '%s': %Rrc%#RTeim", pszFile, rc, &ErrInfo.Core);

    if (g_uVerbosity > 0)
    {
        RTMsgInfo("Opened '%s' for playing\n", pszFile);
        RTMsgInfo("Format: %s\n", PDMAudioPropsToString(&WaveFile.Props, szTmp, sizeof(szTmp)));
        RTMsgInfo("Size:   %'RU32 bytes / %#RX32 / %'RU32 frames / %'RU64 ns\n",
                  WaveFile.cbSamples, WaveFile.cbSamples,
                  PDMAudioPropsBytesToFrames(&WaveFile.Props, WaveFile.cbSamples),
                  PDMAudioPropsBytesToNano(&WaveFile.Props, WaveFile.cbSamples));
    }

    /*
     * Construct the driver stack.
     */
    RTEXITCODE          rcExit = RTEXITCODE_FAILURE;
    AUDIOTESTDRVSTACK   DrvStack;
    rc = audioTestDriverStackInit(&DrvStack, pDrvReg, fWithDrvAudio);
    if (RT_SUCCESS(rc))
    {
        /*
         * Set the output device if one is specified.
         */
        rc = audioTestDriverStackSetDevice(&DrvStack, PDMAUDIODIR_OUT, pszDevId);
        if (RT_SUCCESS(rc))
        {
            /*
             * Open a stream for the output.
             */
            PDMAUDIOPCMPROPS ReqProps = WaveFile.Props;
            if (cChannels != 0 && PDMAudioPropsChannels(&ReqProps) != cChannels)
                PDMAudioPropsSetChannels(&ReqProps, cChannels);
            if (cbSample != 0)
                PDMAudioPropsSetSampleSize(&ReqProps, cbSample);
            if (uHz != 0)
                ReqProps.uHz = uHz;

            PDMAUDIOSTREAMCFG CfgAcq;
            PPDMAUDIOSTREAM   pStream  = NULL;
            rc = audioTestDriverStackStreamCreateOutput(&DrvStack, &ReqProps, cMsBufferSize,
                                                        cMsPreBuffer, cMsSchedulingHint, &pStream, &CfgAcq);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Automatically enable the mixer if the wave file and the
                 * output parameters doesn't match.
                 */
                if (   !fWithMixer
                    && !PDMAudioPropsAreEqual(&WaveFile.Props, &pStream->Cfg.Props))
                {
                    RTMsgInfo("Enabling the mixer buffer.\n");
                    fWithMixer = true;
                }

                /*
                 * Create a mixer wrapper.  This is just a thin wrapper if fWithMixer
                 * is false, otherwise it's doing mixing, resampling and recoding.
                 */
                AUDIOTESTDRVMIXSTREAM Mix;
                rc = AudioTestMixStreamInit(&Mix, &DrvStack, pStream, fWithMixer ? &WaveFile.Props : NULL, 100 /*ms*/);
                if (RT_SUCCESS(rc))
                {
                    if (g_uVerbosity > 0)
                        RTMsgInfo("Stream: %s cbBackend=%#RX32%s\n",
                                  PDMAudioPropsToString(&pStream->Cfg.Props, szTmp, sizeof(szTmp)),
                                  pStream->cbBackend, fWithMixer ? " mixed" : "");

                    /*
                     * Enable the stream and start playing.
                     */
                    rc = AudioTestMixStreamEnable(&Mix);
                    if (RT_SUCCESS(rc))
                        rcExit = audioTestPlayOneInner(&Mix, &WaveFile, &CfgAcq, pszFile);
                    else
                        rcExit = RTMsgErrorExitFailure("Enabling the output stream failed: %Rrc", rc);

                    /*
                     * Clean up.
                     */
                    AudioTestMixStreamTerm(&Mix);
                }
                audioTestDriverStackStreamDestroy(&DrvStack, pStream);
            }
            else
                rcExit = RTMsgErrorExitFailure("Creating output stream failed: %Rrc", rc);
        }
        else
            rcExit = RTMsgErrorExitFailure("Failed to set output device to '%s': %Rrc", pszDevId, rc);
        audioTestDriverStackDelete(&DrvStack);
    }
    else
        rcExit = RTMsgErrorExitFailure("Driver stack construction failed: %Rrc", rc);
    AudioTestWaveFileClose(&WaveFile);
    return rcExit;
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
 * Worker for audioTestRecOne implementing the recording loop.
 */
static RTEXITCODE audioTestRecOneInner(PAUDIOTESTDRVMIXSTREAM pMix, PAUDIOTESTWAVEFILE pWaveFile,
                                       PCPDMAUDIOSTREAMCFG pCfgAcq, uint64_t cMaxFrames, const char *pszFile)
{
    int             rc;
    uint64_t const  nsStarted   = RTTimeNanoTS();

    /*
     * Transfer data as quickly as we're allowed.
     */
    uint8_t         abSamples[16384];
    uint32_t const  cbSamplesAligned     = PDMAudioPropsFloorBytesToFrame(pMix->pProps, sizeof(abSamples));
    uint64_t        cFramesCapturedTotal = 0;
    while (!g_fTerminate && cFramesCapturedTotal < cMaxFrames)
    {
        /*
         * Anything we can read?
         */
        uint32_t const cbCanRead = AudioTestMixStreamGetReadable(pMix);
        if (cbCanRead)
        {
            uint32_t const cbToRead   = RT_MIN(cbCanRead, cbSamplesAligned);
            uint32_t       cbCaptured = 0;
            rc = AudioTestMixStreamCapture(pMix, abSamples, cbToRead, &cbCaptured);
            if (RT_SUCCESS(rc))
            {
                if (cbCaptured)
                {
                    uint32_t cFramesCaptured = PDMAudioPropsBytesToFrames(pMix->pProps, cbCaptured);
                    if (cFramesCaptured + cFramesCaptured < cMaxFrames)
                    { /* likely */ }
                    else
                    {
                        cFramesCaptured = cMaxFrames - cFramesCaptured;
                        cbCaptured      = PDMAudioPropsFramesToBytes(pMix->pProps, cFramesCaptured);
                    }

                    rc = AudioTestWaveFileWrite(pWaveFile, abSamples, cbCaptured);
                    if (RT_SUCCESS(rc))
                        cFramesCapturedTotal += cFramesCaptured;
                    else
                        return RTMsgErrorExitFailure("Error writing to '%s': %Rrc", pszFile, rc);
                }
                else
                    return RTMsgErrorExitFailure("Captured zero bytes - %#x bytes reported readable!\n", cbCanRead);
            }
            else
                return RTMsgErrorExitFailure("Failed to capture %#x bytes: %Rrc (%#x available)\n", cbToRead, rc, cbCanRead);
        }
        else if (AudioTestMixStreamIsOkay(pMix))
            RTThreadSleep(RT_MIN(RT_MAX(1, pCfgAcq->Device.cMsSchedulingHint), 256));
        else
            return RTMsgErrorExitFailure("Stream is not okay!\n");
    }

    /*
     * Disable the stream.
     */
    rc = AudioTestMixStreamDisable(pMix);
    if (RT_SUCCESS(rc) && g_uVerbosity > 0)
        RTMsgInfo("%'RU64 ns: Stopped after recording %RU64 frames%s\n", RTTimeNanoTS() - nsStarted, cFramesCapturedTotal,
                  g_fTerminate ? " - Ctrl-C" : ".");
    else if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("Disabling stream failed: %Rrc", rc);

    return RTEXITCODE_SUCCESS;
}


/**
 * Worker for audioTestCmdRecHandler that recs one file.
 */
static RTEXITCODE audioTestRecOne(const char *pszFile, uint8_t cWaveChannels, uint8_t cbWaveSample, uint32_t uWaveHz,
                                  PCPDMDRVREG pDrvReg, const char *pszDevId, uint32_t cMsBufferSize,
                                  uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                  uint8_t cChannels, uint8_t cbSample, uint32_t uHz, bool fWithDrvAudio, bool fWithMixer,
                                  uint64_t cMaxFrames, uint64_t cNsMaxDuration)
{
    /*
     * Construct the driver stack.
     */
    RTEXITCODE          rcExit = RTEXITCODE_FAILURE;
    AUDIOTESTDRVSTACK   DrvStack;
    int rc = audioTestDriverStackInit(&DrvStack, pDrvReg, fWithDrvAudio);
    if (RT_SUCCESS(rc))
    {
        /*
         * Set the input device if one is specified.
         */
        rc = audioTestDriverStackSetDevice(&DrvStack, PDMAUDIODIR_IN, pszDevId);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create an input stream.
             */
            PDMAUDIOPCMPROPS  ReqProps;
            PDMAudioPropsInit(&ReqProps,
                              cbSample ? cbSample : cbWaveSample ? cbWaveSample : 2,
                              true /*fSigned*/,
                              cChannels ? cChannels : cWaveChannels ? cWaveChannels : 2,
                              uHz ? uHz : uWaveHz ? uWaveHz : 44100);
            PDMAUDIOSTREAMCFG CfgAcq;
            PPDMAUDIOSTREAM   pStream  = NULL;
            rc = audioTestDriverStackStreamCreateInput(&DrvStack, &ReqProps, cMsBufferSize,
                                                       cMsPreBuffer, cMsSchedulingHint, &pStream, &CfgAcq);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Determine the wave file properties.  If it differs from the stream
                 * properties, make sure the mixer is enabled.
                 */
                PDMAUDIOPCMPROPS WaveProps;
                PDMAudioPropsInit(&WaveProps,
                                  cbWaveSample ? cbWaveSample : PDMAudioPropsSampleSize(&CfgAcq.Props),
                                  true /*fSigned*/,
                                  cWaveChannels ? cWaveChannels : PDMAudioPropsChannels(&CfgAcq.Props),
                                  uWaveHz ? uWaveHz : PDMAudioPropsHz(&CfgAcq.Props));
                if (!fWithMixer && !PDMAudioPropsAreEqual(&WaveProps, &CfgAcq.Props))
                {
                    RTMsgInfo("Enabling the mixer buffer.\n");
                    fWithMixer = true;
                }

                /* Console the max duration into frames now that we've got the wave file format. */
                if (cMaxFrames != UINT64_MAX && cNsMaxDuration != UINT64_MAX)
                {
                    uint64_t cMaxFrames2 = PDMAudioPropsNanoToBytes64(&WaveProps, cNsMaxDuration);
                    cMaxFrames = RT_MAX(cMaxFrames, cMaxFrames2);
                }
                else if (cNsMaxDuration != UINT64_MAX)
                    cMaxFrames = PDMAudioPropsNanoToBytes64(&WaveProps, cNsMaxDuration);

                /*
                 * Create a mixer wrapper.  This is just a thin wrapper if fWithMixer
                 * is false, otherwise it's doing mixing, resampling and recoding.
                 */
                AUDIOTESTDRVMIXSTREAM Mix;
                rc = AudioTestMixStreamInit(&Mix, &DrvStack, pStream, fWithMixer ? &WaveProps : NULL, 100 /*ms*/);
                if (RT_SUCCESS(rc))
                {
                    char szTmp[128];
                    if (g_uVerbosity > 0)
                        RTMsgInfo("Stream: %s cbBackend=%#RX32%s\n",
                                  PDMAudioPropsToString(&pStream->Cfg.Props, szTmp, sizeof(szTmp)),
                                  pStream->cbBackend, fWithMixer ? " mixed" : "");

                    /*
                     * Open the wave output file.
                     */
                    AUDIOTESTWAVEFILE WaveFile;
                    RTERRINFOSTATIC ErrInfo;
                    rc = AudioTestWaveFileCreate(pszFile, &WaveProps, &WaveFile, RTErrInfoInitStatic(&ErrInfo));
                    if (RT_SUCCESS(rc))
                    {
                        if (g_uVerbosity > 0)
                        {
                            RTMsgInfo("Opened '%s' for playing\n", pszFile);
                            RTMsgInfo("Format: %s\n", PDMAudioPropsToString(&WaveFile.Props, szTmp, sizeof(szTmp)));
                        }

                        /*
                         * Enable the stream and start recording.
                         */
                        rc = AudioTestMixStreamEnable(&Mix);
                        if (RT_SUCCESS(rc))
                            rcExit = audioTestRecOneInner(&Mix, &WaveFile, &CfgAcq, cMaxFrames, pszFile);
                        else
                            rcExit = RTMsgErrorExitFailure("Enabling the input stream failed: %Rrc", rc);
                        if (rcExit != RTEXITCODE_SUCCESS)
                            AudioTestMixStreamDisable(&Mix);

                        /*
                         * Clean up.
                         */
                        rc = AudioTestWaveFileClose(&WaveFile);
                        if (RT_FAILURE(rc))
                            rcExit = RTMsgErrorExitFailure("Error closing '%s': %Rrc", pszFile, rc);
                    }
                    else
                        rcExit = RTMsgErrorExitFailure("Failed to open '%s': %Rrc%#RTeim", pszFile, rc, &ErrInfo.Core.pszMsg);

                    AudioTestMixStreamTerm(&Mix);
                }
                audioTestDriverStackStreamDestroy(&DrvStack, pStream);
            }
            else
                rcExit = RTMsgErrorExitFailure("Creating output stream failed: %Rrc", rc);
        }
        else
            rcExit = RTMsgErrorExitFailure("Failed to set output device to '%s': %Rrc", pszDevId, rc);
        audioTestDriverStackDelete(&DrvStack);
    }
    else
        rcExit = RTMsgErrorExitFailure("Driver stack construction failed: %Rrc", rc);
    return rcExit;
}

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
    { "--ats-host",         VKAT_SELFTEST_OPT_ATS_HOST,   RTGETOPT_REQ_STRING },
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--with-drv-audio",   'd',                          RTGETOPT_REQ_NOTHING },
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
 * Tests the Audio Test Service (ATS).
 *
 * @returns VBox status code.
 * @param   pDrvReg             Backend driver to use.
 * @param   pszAdr              Address of ATS server to connect to.
 *                              If NULL, an own (local) ATS server will be created.
 */
static int audioTestDoSelftestAts(PCPDMDRVREG pDrvReg, const char *pszAdr)
{
    AUDIOTESTENV TstEnv;
    int rc = audioTestDriverStackInit(&TstEnv.DrvStack, pDrvReg, true /* fWithDrvAudio */);
    if (RT_SUCCESS(rc))
    {
        /** @todo Make stream parameters configurable. */
        PDMAUDIOPCMPROPS  Props;
        PDMAudioPropsInit(&Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

        PDMAUDIOSTREAMCFG CfgAcq;
        PPDMAUDIOSTREAM   pStream = NULL;
        rc = audioTestDriverStackStreamCreateOutput(&TstEnv.DrvStack, &Props,
                                                    UINT32_MAX /* cMsBufferSize */,
                                                    UINT32_MAX /* cMsPreBuffer */,
                                                    UINT32_MAX /* cMsSchedulingHint */, &pStream, &CfgAcq);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestDriverStackStreamEnable(&TstEnv.DrvStack, pStream);
            if (RT_SUCCESS(rc))
            {
                ATSCALLBACKCTX Ctx;
                Ctx.pTstEnv = &TstEnv;

                ATSCALLBACKS Callbacks;
                Callbacks.pfnTonePlay = audioTestSvcTonePlayCallback;
                Callbacks.pvUser      = &Ctx;

                /* Start an own ATS instance if no address to connect was specified. */
                ATSSERVER Srv;
                if (pszAdr == NULL)
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Starting ATS ...\n");

                    rc = AudioTestSvcInit(&Srv, &Callbacks);
                    if (RT_SUCCESS(rc))
                        rc = AudioTestSvcStart(&Srv);
                }
                else
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting to ATS at '%s' ...\n", pszAdr);

                if (RT_SUCCESS(rc))
                {
                    ATSCLIENT Conn;
                    rc = AudioTestSvcClientConnect(&Conn, NULL, 0);
                    if (RT_SUCCESS(rc))
                    {
                        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connected to ATS, testing ...\n");

                        /* Do the bare minimum here to get a test tone out. */
                        AUDIOTESTTONEPARMS ToneParms;
                        RT_ZERO(ToneParms);
                        ToneParms.msDuration = RTRandU32Ex(250, 1000 * 5);
                        memcpy(&ToneParms.Props, &CfgAcq.Props, sizeof(PDMAUDIOPCMPROPS));

                        rc = AudioTestSvcClientTonePlay(&Conn, &ToneParms);

                        int rc2 = AudioTestSvcClientClose(&Conn);
                        if (RT_SUCCESS(rc))
                            rc = rc2;
                    }
                    else
                        RTTestFailed(g_hTest, "Connecting to ATS failed, rc=%Rrc\n", rc);

                    int rc2 = AudioTestSvcShutdown(&Srv);
                    if (RT_SUCCESS(rc))
                        rc = rc2;
                }

                if (pszAdr == NULL)
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Shutting down ATS ...\n");

                    int rc2 = AudioTestSvcDestroy(&Srv);
                        if (RT_SUCCESS(rc))
                            rc = rc2;
                }
            }
        }
    }

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Testing ATS failed with %Rrc\n", rc);

    return rc;
}

/**
 * Main function for performing the self-tests.
 *
 * @returns VBox status code.
 * @param   pDrvReg             Backend driver to use.
 * @param   pszAtsAdr           Address of ATS server to connect to.
 *                              If NULL, an own (local) ATS server will be created.
 */
static int audioTestDoSelftest(PCPDMDRVREG pDrvReg, const char *pszAtsAdr)
{
    int rc = audioTestDoSelftestAts(pDrvReg, pszAtsAdr);
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Self-test failed with: %Rrc", rc);

    return rc;
}

/**
 * The 'selftest' command handler.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioTestCmdSelftestHandler(PRTGETOPTSTATE pGetState)
{
    /* Option values: */
    PCPDMDRVREG pDrvReg           = g_aBackends[0].pDrvReg;
    bool        fWithDrvAudio     = false;
    const char *pszAtsAddr        = NULL;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case VKAT_SELFTEST_OPT_ATS_HOST:
                pszAtsAddr = ValueUnion.psz;
                break;

            case 'b':
                pDrvReg = audioTestFindBackendOpt(ValueUnion.psz);
                if (pDrvReg == NULL)
                    return RTEXITCODE_SYNTAX;
                break;

            case 'd':
                fWithDrvAudio = true;
                break;

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    audioTestDoSelftest(pDrvReg, pszAtsAddr);

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
                         RT_ELEMENTS(g_apszLogGroups), g_apszLogGroups, RTLOGDEST_STDERR, "vkat-release.log");
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

