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

#include "../../../Devices/Audio/AudioHlp.h"
#include "../../../Devices/Audio/AudioTest.h"
#include "../../../Devices/Audio/AudioTestService.h"
#include "../../../Devices/Audio/AudioTestServiceClient.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** For use in the option switch to handle common options. */
#define AUDIO_TEST_COMMON_OPTION_CASES(a_ValueUnion) \
            case 'q': \
                g_uVerbosity = 0; \
                break; \
            \
            case 'v': \
                g_uVerbosity++; \
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
 * Audio driver stack.
 *
 * This can be just be backend driver alone or DrvAudio with a backend.
 * @todo add automatic resampling via mixer so we can test more of the audio
 *       stack used by the device emulations.
 */
typedef struct AUDIOTESTDRVSTACK
{
    /** The device registration record for the backend. */
    PCPDMDRVREG             pDrvReg;
    /** The backend driver instance. */
    PPDMDRVINS              pDrvBackendIns;
    /** The backend's audio interface. */
    PPDMIHOSTAUDIO          pIHostAudio;

    /** The DrvAudio instance. */
    PPDMDRVINS              pDrvAudioIns;
    /** This is NULL if we don't use DrvAudio. */
    PPDMIAUDIOCONNECTOR     pIAudioConnector;
} AUDIOTESTDRVSTACK;
/** Pointer to an audio driver stack. */
typedef AUDIOTESTDRVSTACK *PAUDIOTESTDRVSTACK;

/**
 * Backend-only stream structure.
 */
typedef struct AUDIOTESTDRVSTACKSTREAM
{
    /** The public stream data. */
    PDMAUDIOSTREAM          Core;
    /** The acquired config. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** The backend data (variable size). */
    PDMAUDIOBACKENDSTREAM   Backend;
} AUDIOTESTDRVSTACKSTREAM;
/** Pointer to a backend-only stream structure. */
typedef AUDIOTESTDRVSTACKSTREAM *PAUDIOTESTDRVSTACKSTREAM;

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
    /** The audio test driver stack. */
    AUDIOTESTDRVSTACK     DrvStack;
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

static int audioTestDrvConstruct(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg,
                                 PPDMDRVINS pParentDrvIns, PPPDMDRVINS ppDrvIns);

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

/** The test handle. */
static RTTEST       g_hTest;
/** The current verbosity level. */
static unsigned     g_uVerbosity = 0;
/** DrvAudio: Enable debug (or not). */
static bool         g_fDrvAudioDebug = 0;
/** DrvAudio: The debug output path. */
static const char  *g_pszDrvAudioDebug = NULL;


/*********************************************************************************************************************************
*   Fake PDM Driver Handling.                                                                                                    *
*********************************************************************************************************************************/

/** @name Driver Fakes/Stubs
 *
 * @note The VMM functions defined here will turn into driver helpers before
 *       long, as the drivers aren't supposed to import directly from the VMM in
 *       the future.
 *
 * @{  */

VMMR3DECL(PCFGMNODE) CFGMR3GetChild(PCFGMNODE pNode, const char *pszPath)
{
    RT_NOREF(pNode, pszPath);
    return NULL;
}

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

VMMR3DECL(int) CFGMR3QueryStringAlloc(PCFGMNODE pNode, const char *pszName, char **ppszString)
{
    char szStr[128];
    int rc = CFGMR3QueryString(pNode, pszName, szStr, sizeof(szStr));
    if (RT_SUCCESS(rc))
        *ppszString = RTStrDup(szStr);

    return rc;
}

VMMR3DECL(int) CFGMR3QueryStringDef(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString, const char *pszDef)
{
    PCPDMDRVREG pDrvReg = (PCPDMDRVREG)pNode;
    if (RT_VALID_PTR(pDrvReg))
    {
        const char *pszRet = pszDef;
        if (   g_pszDrvAudioDebug
            && strcmp(pDrvReg->szName, "AUDIO") == 0
            && strcmp(pszName, "DebugPathOut") == 0)
            pszRet = g_pszDrvAudioDebug;

        int rc = RTStrCopy(pszString, cchString, pszRet);

        if (g_uVerbosity > 2)
            RTPrintf("debug: CFGMR3QueryStringDef([%s], %s, %p, %#x, %s) -> '%s' + %Rrc\n",
                     pDrvReg->szName, pszName, pszString, cchString, pszDef, pszRet, rc);
        return rc;
    }

    if (g_uVerbosity > 2)
        RTPrintf("debug: CFGMR3QueryStringDef(%p, %s, %p, %#x, %s)\n", pNode, pszName, pszString, cchString, pszDef);
    return RTStrCopy(pszString, cchString, pszDef);
}

VMMR3DECL(int) CFGMR3QueryBoolDef(PCFGMNODE pNode, const char *pszName, bool *pf, bool fDef)
{
    PCPDMDRVREG pDrvReg = (PCPDMDRVREG)pNode;
    if (RT_VALID_PTR(pDrvReg))
    {
        *pf = fDef;
        if (   strcmp(pDrvReg->szName, "AUDIO") == 0
            && strcmp(pszName, "DebugEnabled") == 0)
            *pf = g_fDrvAudioDebug;

        if (g_uVerbosity > 2)
            RTPrintf("debug: CFGMR3QueryBoolDef([%s], %s, %p, %RTbool) -> %RTbool\n", pDrvReg->szName, pszName, pf, fDef, *pf);
        return VINF_SUCCESS;
    }
    *pf = fDef;
    return VINF_SUCCESS;
}

VMMR3DECL(int) CFGMR3QueryU8(PCFGMNODE pNode, const char *pszName, uint8_t *pu8)
{
    RT_NOREF(pNode, pszName, pu8);
    return VERR_CFGM_VALUE_NOT_FOUND;
}

VMMR3DECL(int) CFGMR3QueryU32(PCFGMNODE pNode, const char *pszName, uint32_t *pu32)
{
    RT_NOREF(pNode, pszName, pu32);
    return VERR_CFGM_VALUE_NOT_FOUND;
}

VMMR3DECL(int) CFGMR3ValidateConfig(PCFGMNODE pNode, const char *pszNode,
                                    const char *pszValidValues, const char *pszValidNodes,
                                    const char *pszWho, uint32_t uInstance)
{
    RT_NOREF(pNode, pszNode, pszValidValues, pszValidNodes, pszWho, uInstance);
    return VINF_SUCCESS;
}

/** @} */

/** @name Driver Helper Fakes
 * @{ */

static DECLCALLBACK(int) audioTestDrvHlp_Attach(PPDMDRVINS pDrvIns, uint32_t fFlags, PPDMIBASE *ppBaseInterface)
{
    /* DrvAudio must be allowed to attach the backend driver (paranoid
       backend drivers may call us to check that nothing is attached). */
    if (strcmp(pDrvIns->pReg->szName, "AUDIO") == 0)
    {
        PAUDIOTESTDRVSTACK pDrvStack = pDrvIns->Internal.s.pStack;
        AssertReturn(pDrvStack->pDrvBackendIns == NULL, VERR_PDM_DRIVER_ALREADY_ATTACHED);

        if (g_uVerbosity > 1)
            RTMsgInfo("Attaching backend '%s' to DrvAudio...\n", pDrvStack->pDrvReg->szName);
        int rc = audioTestDrvConstruct(pDrvStack, pDrvStack->pDrvReg, pDrvIns, &pDrvStack->pDrvBackendIns);
        if (RT_SUCCESS(rc))
        {
            if (ppBaseInterface)
                *ppBaseInterface = &pDrvStack->pDrvBackendIns->IBase;
        }
        else
            RTMsgError("Failed to attach backend: %Rrc", rc);
        return rc;
    }
    RT_NOREF(fFlags);
    return VERR_PDM_NO_ATTACHED_DRIVER;
}

static DECLCALLBACK(void) audioTestDrvHlp_STAMRegisterF(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType,
                                                        STAMVISIBILITY enmVisibility, STAMUNIT enmUnit, const char *pszDesc,
                                                        const char *pszName, ...)
{
    RT_NOREF(pDrvIns, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName);
}

static DECLCALLBACK(void) audioTestDrvHlp_STAMRegisterV(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType,
                                                        STAMVISIBILITY enmVisibility, STAMUNIT enmUnit, const char *pszDesc,
                                                        const char *pszName, va_list args)
{
    RT_NOREF(pDrvIns, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
}

static DECLCALLBACK(int) audioTestDrvHlp_STAMDeregister(PPDMDRVINS pDrvIns, void *pvSample)
{
    RT_NOREF(pDrvIns, pvSample);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) audioTestDrvHlp_STAMDeregisterByPrefix(PPDMDRVINS pDrvIns, const char *pszPrefix)
{
    RT_NOREF(pDrvIns, pszPrefix);
    return VINF_SUCCESS;
}

/**
 * Get the driver helpers.
 */
static const PDMDRVHLPR3 *audioTestFakeGetDrvHlp(void)
{
    /*
     * Note! No initializer for s_DrvHlp (also why it's not a file global).
     *       We do not want to have to update this code every time PDMDRVHLPR3
     *       grows new entries or are otherwise modified.  Only when the
     *       entries used by the audio driver changes do we want to change
     *       our code.
     */
    static PDMDRVHLPR3 s_DrvHlp;
    if (s_DrvHlp.u32Version != PDM_DRVHLPR3_VERSION)
    {
        s_DrvHlp.u32Version                     = PDM_DRVHLPR3_VERSION;
        s_DrvHlp.u32TheEnd                      = PDM_DRVHLPR3_VERSION;
        s_DrvHlp.pfnAttach                      = audioTestDrvHlp_Attach;
        s_DrvHlp.pfnSTAMRegisterF               = audioTestDrvHlp_STAMRegisterF;
        s_DrvHlp.pfnSTAMRegisterV               = audioTestDrvHlp_STAMRegisterV;
        s_DrvHlp.pfnSTAMDeregister              = audioTestDrvHlp_STAMDeregister;
        s_DrvHlp.pfnSTAMDeregisterByPrefix      = audioTestDrvHlp_STAMDeregisterByPrefix;
    }
    return &s_DrvHlp;
}

/** @} */


/**
 * Implementation of PDMIBASE::pfnQueryInterface for a fake device above
 * DrvAudio.
 */
static DECLCALLBACK(void *) audioTestFakeDeviceIBaseQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, pInterface);
    RTMsgWarning("audioTestFakeDeviceIBaseQueryInterface: Unknown interface: %s\n", pszIID);
    return NULL;
}

/** IBase interface for a fake device above DrvAudio. */
static PDMIBASE g_AudioTestFakeDeviceIBase =  { audioTestFakeDeviceIBaseQueryInterface };


static DECLCALLBACK(int) audioTestIHostAudioPort_DoOnWorkerThread(PPDMIHOSTAUDIOPORT pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                                  uintptr_t uUser, void *pvUser)
{
    RT_NOREF(pInterface, pStream, uUser, pvUser);
    RTMsgWarning("audioTestIHostAudioPort_DoOnWorkerThread was called\n");
    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(void) audioTestIHostAudioPort_NotifyDeviceChanged(PPDMIHOSTAUDIOPORT pInterface, PDMAUDIODIR enmDir, void *pvUser)
{
    RT_NOREF(pInterface, enmDir, pvUser);
    RTMsgWarning("audioTestIHostAudioPort_NotifyDeviceChanged was called\n");
}

static DECLCALLBACK(void) audioTestIHostAudioPort_StreamNotifyPreparingDeviceSwitch(PPDMIHOSTAUDIOPORT pInterface,
                                                                                    PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    RTMsgWarning("audioTestIHostAudioPort_StreamNotifyPreparingDeviceSwitch was called\n");
}

static DECLCALLBACK(void) audioTestIHostAudioPort_StreamNotifyDeviceChanged(PPDMIHOSTAUDIOPORT pInterface,
                                                                            PPDMAUDIOBACKENDSTREAM pStream, bool fReInit)
{
    RT_NOREF(pInterface, pStream, fReInit);
    RTMsgWarning("audioTestIHostAudioPort_StreamNotifyDeviceChanged was called\n");
}

static DECLCALLBACK(void) audioTestIHostAudioPort_NotifyDevicesChanged(PPDMIHOSTAUDIOPORT pInterface)
{
    RT_NOREF(pInterface);
    RTMsgWarning("audioTestIHostAudioPort_NotifyDevicesChanged was called\n");
}

static PDMIHOSTAUDIOPORT g_AudioTestIHostAudioPort =
{
    audioTestIHostAudioPort_DoOnWorkerThread,
    audioTestIHostAudioPort_NotifyDeviceChanged,
    audioTestIHostAudioPort_StreamNotifyPreparingDeviceSwitch,
    audioTestIHostAudioPort_StreamNotifyDeviceChanged,
    audioTestIHostAudioPort_NotifyDevicesChanged,
};

/**
 * Implementation of PDMIBASE::pfnQueryInterface for a fake DrvAudio above a
 * backend.
 */
static DECLCALLBACK(void *) audioTestFakeDrvAudioIBaseQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, pInterface);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIOPORT, &g_AudioTestIHostAudioPort);
    RTMsgWarning("audioTestFakeDrvAudioIBaseQueryInterface: Unknown interface: %s\n", pszIID);
    return NULL;
}

/** IBase interface for a fake DrvAudio above a lonesome backend. */
static PDMIBASE g_AudioTestFakeDrvAudioIBase =  { audioTestFakeDrvAudioIBaseQueryInterface };



/**
 * Constructs a PDM audio driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvStack       The stack this is associated with.
 * @param   pDrvReg         PDM driver registration record to use for construction.
 * @param   pParentDrvIns   The parent driver (if any).
 * @param   ppDrvIns        Where to return the driver instance structure.
 */
static int audioTestDrvConstruct(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, PPDMDRVINS pParentDrvIns,
                                 PPPDMDRVINS ppDrvIns)
{
    /* The destruct function must have valid data to work with. */
    *ppDrvIns   = NULL;

    /*
     * Check registration structure validation (doesn't need to be too
     * thorough, PDM check it in detail on every VM startup).
     */
    AssertPtrReturn(pDrvReg, VERR_INVALID_POINTER);
    RTMsgInfo("Initializing backend '%s' ...\n", pDrvReg->szName);
    AssertPtrReturn(pDrvReg->pfnConstruct, VERR_INVALID_PARAMETER);

    /*
     * Create the instance data structure.
     */
    PPDMDRVINS pDrvIns = (PPDMDRVINS)RTMemAllocZVar(RT_UOFFSETOF_DYN(PDMDRVINS, achInstanceData[pDrvReg->cbInstance]));
    RTTEST_CHECK_RET(g_hTest, pDrvIns, VERR_NO_MEMORY);

    pDrvIns->u32Version         = PDM_DRVINS_VERSION;
    pDrvIns->iInstance          = 0;
    pDrvIns->pHlpR3             = audioTestFakeGetDrvHlp();
    pDrvIns->pvInstanceDataR3   = &pDrvIns->achInstanceData[0];
    pDrvIns->pReg               = pDrvReg;
    pDrvIns->pCfg               = (PCFGMNODE)pDrvReg;
    pDrvIns->Internal.s.pStack  = pDrvStack;
    pDrvIns->pUpBase            = NULL;
    pDrvIns->pDownBase          = NULL;
    if (pParentDrvIns)
    {
        Assert(pParentDrvIns->pDownBase == NULL);
        pParentDrvIns->pDownBase = &pDrvIns->IBase;
        pDrvIns->pUpBase         = &pParentDrvIns->IBase;
    }
    else if (strcmp(pDrvReg->szName, "AUDIO") == 0)
        pDrvIns->pUpBase         = &g_AudioTestFakeDeviceIBase;
    else
        pDrvIns->pUpBase         = &g_AudioTestFakeDrvAudioIBase;

    /*
     * Invoke the constructor.
     */
    int rc = pDrvReg->pfnConstruct(pDrvIns, pDrvIns->pCfg, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        *ppDrvIns   = pDrvIns;
        return VINF_SUCCESS;
    }

    RTTestFailed(g_hTest, "Failed to construct audio driver '%s': %Rrc", pDrvReg->szName, rc);
    if (pDrvReg->pfnDestruct)
        pDrvReg->pfnDestruct(pDrvIns);
    RTMemFree(pDrvIns);
    return rc;
}

/**
 * Destructs a PDM audio driver instance.
 *
 * @param   pDrvIns             Driver instance to destruct.
 */
static void audioTestDrvDestruct(PPDMDRVINS pDrvIns)
{
    if (pDrvIns)
    {
        Assert(pDrvIns->u32Version == PDM_DRVINS_VERSION);

        if (pDrvIns->pReg->pfnDestruct)
            pDrvIns->pReg->pfnDestruct(pDrvIns);

        pDrvIns->u32Version = 0;
        pDrvIns->pReg = NULL;
        RTMemFree(pDrvIns);
    }
}

/**
 * Sends the PDM driver a power off notification.
 *
 * @param   pDrvIns             Driver instance to notify.
 */
static void audioTestDrvNotifyPowerOff(PPDMDRVINS pDrvIns)
{
    if (pDrvIns)
    {
        Assert(pDrvIns->u32Version == PDM_DRVINS_VERSION);
        if (pDrvIns->pReg->pfnPowerOff)
            pDrvIns->pReg->pfnPowerOff(pDrvIns);
    }
}

/**
 * Deletes a driver stack.
 *
 * This will power off and destroy the drivers.
 */
static void audioTestDriverStackDelete(PAUDIOTESTDRVSTACK pDrvStack)
{
    /*
     * Do power off notifications (top to bottom).
     */
    audioTestDrvNotifyPowerOff(pDrvStack->pDrvAudioIns);
    audioTestDrvNotifyPowerOff(pDrvStack->pDrvBackendIns);

    /*
     * Drivers are destroyed from bottom to top (closest to the device).
     */
    audioTestDrvDestruct(pDrvStack->pDrvBackendIns);
    pDrvStack->pDrvBackendIns   = NULL;
    pDrvStack->pIHostAudio      = NULL;

    audioTestDrvDestruct(pDrvStack->pDrvAudioIns);
    pDrvStack->pDrvAudioIns     = NULL;
    pDrvStack->pIAudioConnector = NULL;
}

/**
 * Initializes a driver stack.
 *
 * @returns VBox status code.
 * @param   pDrvStack       The driver stack to initialize.
 * @param   pDrvReg         The backend driver to use.
 * @param   fWithDrvAudio   Whether to include DrvAudio in the stack or not.
 */
static int audioTestDriverStackInit(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, bool fWithDrvAudio)
{
    RT_ZERO(*pDrvStack);
    pDrvStack->pDrvReg = pDrvReg;

    int rc;
    if (!fWithDrvAudio)
        rc = audioTestDrvConstruct(pDrvStack, pDrvReg, NULL /*pParentDrvIns*/, &pDrvStack->pDrvBackendIns);
    else
    {
        rc = audioTestDrvConstruct(pDrvStack, &g_DrvAUDIO, NULL /*pParentDrvIns*/, &pDrvStack->pDrvAudioIns);
        if (RT_SUCCESS(rc))
        {
            Assert(pDrvStack->pDrvAudioIns);
            PPDMIBASE const pIBase = &pDrvStack->pDrvAudioIns->IBase;
            pDrvStack->pIAudioConnector = (PPDMIAUDIOCONNECTOR)pIBase->pfnQueryInterface(pIBase, PDMIAUDIOCONNECTOR_IID);
            if (pDrvStack->pIAudioConnector)
            {
                /* Both input and output is disabled by default. Fix that: */
                rc = pDrvStack->pIAudioConnector->pfnEnable(pDrvStack->pIAudioConnector, PDMAUDIODIR_OUT, true);
                if (RT_SUCCESS(rc))
                    rc = pDrvStack->pIAudioConnector->pfnEnable(pDrvStack->pIAudioConnector, PDMAUDIODIR_IN, true);
                if (RT_FAILURE(rc))
                {
                    RTTestFailed(g_hTest, "Failed to enabled input and output: %Rrc", rc);
                    audioTestDriverStackDelete(pDrvStack);
                }
            }
            else
            {
                RTTestFailed(g_hTest, "Failed to query PDMIAUDIOCONNECTOR");
                audioTestDriverStackDelete(pDrvStack);
                rc = VERR_PDM_MISSING_INTERFACE;
            }
        }
    }

    /*
     * Get the IHostAudio interface and check that the host driver is working.
     */
    if (RT_SUCCESS(rc))
    {
        PPDMIBASE const pIBase = &pDrvStack->pDrvBackendIns->IBase;
        pDrvStack->pIHostAudio = (PPDMIHOSTAUDIO)pIBase->pfnQueryInterface(pIBase, PDMIHOSTAUDIO_IID);
        if (pDrvStack->pIHostAudio)
        {
            PDMAUDIOBACKENDSTS enmStatus = pDrvStack->pIHostAudio->pfnGetStatus(pDrvStack->pIHostAudio, PDMAUDIODIR_OUT);
            if (enmStatus == PDMAUDIOBACKENDSTS_RUNNING)
                return VINF_SUCCESS;

            RTTestFailed(g_hTest, "Expected backend status RUNNING, got %d instead", enmStatus);
        }
        else
            RTTestFailed(g_hTest, "Failed to query PDMIHOSTAUDIO for '%s'", pDrvReg->szName);
        audioTestDriverStackDelete(pDrvStack);
    }

    return rc;
}

/**
 * Creates an output stream.
 *
 * @returns VBox status code.
 * @param   pDrvStack           The audio driver stack to create it via.
 * @param   pProps              The audio properties to use.
 * @param   cMsBufferSize       The buffer size in milliseconds.
 * @param   cMsPreBuffer        The pre-buffering amount in milliseconds.
 * @param   cMsSchedulingHint   The scheduling hint in milliseconds.
 * @param   ppStream            Where to return the stream pointer on success.
 * @param   pCfgAcq             Where to return the actual (well, not
 *                              necessarily when using DrvAudio, but probably
 *                              the same) stream config on success (not used as
 *                              input).
 */
static int audioTestDriverStackStreamCreateOutput(PAUDIOTESTDRVSTACK pDrvStack, PCPDMAUDIOPCMPROPS pProps,
                                                  uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                                  PPDMAUDIOSTREAM *ppStream, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    char szTmp[PDMAUDIOSTRMCFGTOSTRING_MAX + 16];
    *ppStream = NULL;

    /*
     * Calculate the stream config.
     */
    PDMAUDIOSTREAMCFG CfgReq;
    int rc = PDMAudioStrmCfgInitWithProps(&CfgReq, pProps);
    AssertRC(rc);
    CfgReq.enmDir                       = PDMAUDIODIR_OUT;
    CfgReq.enmPath                      = PDMAUDIOPATH_OUT_FRONT;
    CfgReq.enmLayout                    = PDMAUDIOSTREAMLAYOUT_INTERLEAVED;
    CfgReq.Device.cMsSchedulingHint     = cMsSchedulingHint == UINT32_MAX || cMsSchedulingHint == 0
                                        ? 10 : cMsSchedulingHint;
    if (pDrvStack->pIAudioConnector && (cMsBufferSize == UINT32_MAX || cMsBufferSize == 0))
        CfgReq.Backend.cFramesBufferSize = 0; /* DrvAudio picks the default */
    else
        CfgReq.Backend.cFramesBufferSize = PDMAudioPropsMilliToFrames(pProps,
                                                                      cMsBufferSize == UINT32_MAX || cMsBufferSize == 0
                                                                      ? 300 : cMsBufferSize);
    if (cMsPreBuffer == UINT32_MAX)
        CfgReq.Backend.cFramesPreBuffering = pDrvStack->pIAudioConnector ? UINT32_MAX /*DrvAudo picks the default */
                                           : CfgReq.Backend.cFramesBufferSize * 2 / 3;
    else
        CfgReq.Backend.cFramesPreBuffering = PDMAudioPropsMilliToFrames(pProps, cMsPreBuffer);
    if (   CfgReq.Backend.cFramesPreBuffering >= CfgReq.Backend.cFramesBufferSize + 16
        && !pDrvStack->pIAudioConnector /*DrvAudio deals with it*/ )
    {
        RTMsgWarning("Cannot pre-buffer %#x frames with only %#x frames of buffer!",
                     CfgReq.Backend.cFramesPreBuffering, CfgReq.Backend.cFramesBufferSize);
        CfgReq.Backend.cFramesPreBuffering = CfgReq.Backend.cFramesBufferSize > 16
            ? CfgReq.Backend.cFramesBufferSize - 16 : 0;
    }

    static uint32_t s_idxStream = 0;
    uint32_t const idxStream = s_idxStream++;
    RTStrPrintf(CfgReq.szName, sizeof(CfgReq.szName), "out-%u", idxStream);

    if (pDrvStack->pIAudioConnector)
    {
        /*
         * DrvAudio does most of the work here.
         */
        PDMAUDIOSTREAMCFG CfgGst = CfgReq;
        rc = pDrvStack->pIAudioConnector->pfnStreamCreate(pDrvStack->pIAudioConnector, PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF,
                                                          &CfgReq, &CfgGst, ppStream);
        if (RT_SUCCESS(rc))
        {
            *pCfgAcq = CfgReq; /** @todo PDMIAUDIOCONNECTOR::pfnStreamCreate only does one utterly pointless change to the two configs (enmLayout) from what I can tell... */
            pCfgAcq->Props = (*ppStream)->Props;
            RTMsgInfo("Created backend stream: %s\n", PDMAudioStrmCfgToString(&CfgReq, szTmp, sizeof(szTmp)));
            return rc;
        }
        RTTestFailed(g_hTest, "pfnStreamCreate failed: %Rrc", rc);
    }
    else
    {
        /*
         * Get the config so we can see how big the PDMAUDIOBACKENDSTREAM
         * structure actually is for this backend.
         */
        PDMAUDIOBACKENDCFG BackendCfg;
        rc = pDrvStack->pIHostAudio->pfnGetConfig(pDrvStack->pIHostAudio, &BackendCfg);
        if (RT_SUCCESS(rc))
        {
            if (BackendCfg.cbStream >= sizeof(PDMAUDIOBACKENDSTREAM))
            {
                /*
                 * Allocate and initialize the stream.
                 */
                uint32_t const cbStream = sizeof(AUDIOTESTDRVSTACKSTREAM) - sizeof(PDMAUDIOBACKENDSTREAM) + BackendCfg.cbStream;
                PAUDIOTESTDRVSTACKSTREAM pStreamAt = (PAUDIOTESTDRVSTACKSTREAM)RTMemAllocZVar(cbStream);
                if (pStreamAt)
                {
                    pStreamAt->Core.uMagic     = PDMAUDIOSTREAM_MAGIC;
                    pStreamAt->Core.enmDir     = PDMAUDIODIR_OUT;
                    pStreamAt->Core.cbBackend  = cbStream;
                    pStreamAt->Core.Props      = CfgReq.Props;
                    RTStrPrintf(pStreamAt->Core.szName, sizeof(pStreamAt->Core.szName), "out-%u", idxStream);

                    pStreamAt->Backend.uMagic  = PDMAUDIOBACKENDSTREAM_MAGIC;
                    pStreamAt->Backend.pStream = &pStreamAt->Core;

                    /*
                     * Call the backend to create the stream.
                     */
                    pStreamAt->Cfg = CfgReq;

                    rc = pDrvStack->pIHostAudio->pfnStreamCreate(pDrvStack->pIHostAudio, &pStreamAt->Backend,
                                                                 &CfgReq, &pStreamAt->Cfg);
                    if (RT_SUCCESS(rc))
                    {
                        pStreamAt->Core.Props = pStreamAt->Cfg.Props;
                        if (g_uVerbosity > 1)
                            RTMsgInfo("Created backend stream: %s\n",
                                      PDMAudioStrmCfgToString(&pStreamAt->Cfg, szTmp, sizeof(szTmp)));

                        /* Return if stream is ready: */
                        if (rc == VINF_SUCCESS)
                        {
                            *ppStream = &pStreamAt->Core;
                            *pCfgAcq  = pStreamAt->Cfg;
                            return VINF_SUCCESS;
                        }
                        if (rc == VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED)
                        {
                            /*
                             * Do async init right here and now.
                             */
                            rc = pDrvStack->pIHostAudio->pfnStreamInitAsync(pDrvStack->pIHostAudio, &pStreamAt->Backend,
                                                                            false /*fDestroyed*/);
                            if (RT_SUCCESS(rc))
                            {
                                *ppStream = &pStreamAt->Core;
                                *pCfgAcq  = pStreamAt->Cfg;
                                return VINF_SUCCESS;
                            }

                            RTTestFailed(g_hTest, "pfnStreamInitAsync failed: %Rrc\n", rc);
                        }
                        else
                        {
                            RTTestFailed(g_hTest, "pfnStreamCreate returned unexpected info status: %Rrc", rc);
                            rc = VERR_IPE_UNEXPECTED_INFO_STATUS;
                        }
                        pDrvStack->pIHostAudio->pfnStreamDestroy(pDrvStack->pIHostAudio, &pStreamAt->Backend, true /*fImmediate*/);
                    }
                    else
                        RTTestFailed(g_hTest, "pfnStreamCreate failed: %Rrc\n", rc);
                }
                else
                {
                    RTTestFailed(g_hTest, "Out of memory!\n");
                    rc = VERR_NO_MEMORY;
                }
            }
            else
            {
                RTTestFailed(g_hTest, "cbStream=%#x is too small, min %#zx!\n", BackendCfg.cbStream, sizeof(PDMAUDIOBACKENDSTREAM));
                rc = VERR_OUT_OF_RANGE;
            }
        }
        else
            RTTestFailed(g_hTest, "pfnGetConfig failed: %Rrc\n", rc);
    }
    return rc;
}

/**
 * Destroys a stream.
 */
static void audioTestDriverStackStreamDestroy(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream)
{
    if (pStream)
    {
        if (pDrvStack->pIAudioConnector)
        {
            int rc = pDrvStack->pIAudioConnector->pfnStreamDestroy(pDrvStack->pIAudioConnector, pStream, true /*fImmediate*/);
            if (RT_FAILURE(rc))
                RTTestFailed(g_hTest, "pfnStreamDestroy failed: %Rrc", rc);
        }
        else
        {
            PAUDIOTESTDRVSTACKSTREAM pStreamAt = (PAUDIOTESTDRVSTACKSTREAM)pStream;
            int rc = pDrvStack->pIHostAudio->pfnStreamDestroy(pDrvStack->pIHostAudio, &pStreamAt->Backend, true /*fImmediate*/);
            if (RT_SUCCESS(rc))
            {
                pStreamAt->Core.uMagic    = ~PDMAUDIOSTREAM_MAGIC;
                pStreamAt->Backend.uMagic = ~PDMAUDIOBACKENDSTREAM_MAGIC;
                RTMemFree(pStreamAt);
            }
            else
                RTTestFailed(g_hTest, "PDMIHOSTAUDIO::pfnStreamDestroy failed: %Rrc", rc);
        }
    }
}

/**
 * Enables a stream.
 */
static int audioTestDriverStackStreamEnable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream)
{
    int rc;
    if (pDrvStack->pIAudioConnector)
    {
        rc = pDrvStack->pIAudioConnector->pfnStreamControl(pDrvStack->pIAudioConnector, pStream, PDMAUDIOSTREAMCMD_ENABLE);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "pfnStreamControl/ENABLE failed: %Rrc", rc);
    }
    else
    {
        PAUDIOTESTDRVSTACKSTREAM pStreamAt = (PAUDIOTESTDRVSTACKSTREAM)pStream;
        rc = pDrvStack->pIHostAudio->pfnStreamControl(pDrvStack->pIHostAudio, &pStreamAt->Backend, PDMAUDIOSTREAMCMD_ENABLE);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "PDMIHOSTAUDIO::pfnStreamControl/ENABLE failed: %Rrc", rc);
    }
    return rc;
}

/**
 * Drains an output stream.
 */
static int audioTestDriverStackStreamDrain(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream, bool fSync)
{
    int rc;
    if (pDrvStack->pIAudioConnector)
    {
        /*
         * Issue the drain request.
         */
        rc = pDrvStack->pIAudioConnector->pfnStreamControl(pDrvStack->pIAudioConnector, pStream, PDMAUDIOSTREAMCMD_DRAIN);
        if (RT_SUCCESS(rc) && fSync)
        {
            /*
             * This is a synchronous drain, so wait for the driver to change state to inactive.
             */
            PDMAUDIOSTREAMSTATE enmState;
            while (   (enmState = pDrvStack->pIAudioConnector->pfnStreamGetState(pDrvStack->pIAudioConnector, pStream))
                   >= PDMAUDIOSTREAMSTATE_ENABLED)
            {
                RTThreadSleep(2);
                rc = pDrvStack->pIAudioConnector->pfnStreamIterate(pDrvStack->pIAudioConnector, pStream);
                if (RT_FAILURE(rc))
                {
                    RTTestFailed(g_hTest, "pfnStreamIterate/DRAIN failed: %Rrc", rc);
                    break;
                }
            }
            if (enmState != PDMAUDIOSTREAMSTATE_INACTIVE)
            {
                RTTestFailed(g_hTest, "Stream state not INACTIVE after draining: %s", PDMAudioStreamStateGetName(enmState));
                rc = VERR_AUDIO_STREAM_NOT_READY;
            }
        }
        else if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "pfnStreamControl/ENABLE failed: %Rrc", rc);
    }
    else
    {
        /*
         * Issue the drain request.
         */
        PAUDIOTESTDRVSTACKSTREAM pStreamAt = (PAUDIOTESTDRVSTACKSTREAM)pStream;
        rc = pDrvStack->pIHostAudio->pfnStreamControl(pDrvStack->pIHostAudio, &pStreamAt->Backend, PDMAUDIOSTREAMCMD_DRAIN);
        if (RT_SUCCESS(rc) && fSync)
        {
            /*
             * This is a synchronous drain, so wait for the driver to change state to inactive.
             */
            PDMHOSTAUDIOSTREAMSTATE enmHostState;
            while (   (enmHostState = pDrvStack->pIHostAudio->pfnStreamGetState(pDrvStack->pIHostAudio, &pStreamAt->Backend))
                   == PDMHOSTAUDIOSTREAMSTATE_DRAINING)
            {
                RTThreadSleep(2);
                uint32_t cbWritten = UINT32_MAX;
                rc = pDrvStack->pIHostAudio->pfnStreamPlay(pDrvStack->pIHostAudio, &pStreamAt->Backend,
                                                           NULL /*pvBuf*/, 0 /*cbBuf*/, &cbWritten);
                if (RT_FAILURE(rc))
                {
                    RTTestFailed(g_hTest, "pfnStreamPlay/DRAIN failed: %Rrc", rc);
                    break;
                }
                if (cbWritten != 0)
                {
                    RTTestFailed(g_hTest, "pfnStreamPlay/DRAIN did not set cbWritten to zero: %#x", cbWritten);
                    rc = VERR_MISSING;
                    break;
                }
            }
            if (enmHostState != PDMHOSTAUDIOSTREAMSTATE_OKAY)
            {
                RTTestFailed(g_hTest, "Stream state not OKAY after draining: %s", PDMHostAudioStreamStateGetName(enmHostState));
                rc = VERR_AUDIO_STREAM_NOT_READY;
            }
        }
        else if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "PDMIHOSTAUDIO::pfnStreamControl/ENABLE failed: %Rrc", rc);
    }
    return rc;
}

/**
 * Checks if the stream is okay.
 * @returns true if okay, false if not.
 */
static bool audioTestDriverStackStreamIsOkay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream)
{
    /*
     * Get the stream status and check if it means is okay or not.
     */
    bool fRc = false;
    if (pDrvStack->pIAudioConnector)
    {
        PDMAUDIOSTREAMSTATE enmState = pDrvStack->pIAudioConnector->pfnStreamGetState(pDrvStack->pIAudioConnector, pStream);
        switch (enmState)
        {
            case PDMAUDIOSTREAMSTATE_NOT_WORKING:
            case PDMAUDIOSTREAMSTATE_NEED_REINIT:
                break;
            case PDMAUDIOSTREAMSTATE_INACTIVE:
            case PDMAUDIOSTREAMSTATE_ENABLED:
            case PDMAUDIOSTREAMSTATE_ENABLED_READABLE:
            case PDMAUDIOSTREAMSTATE_ENABLED_WRITABLE:
                fRc = true;
                break;
            /* no default */
            case PDMAUDIOSTREAMSTATE_INVALID:
            case PDMAUDIOSTREAMSTATE_END:
            case PDMAUDIOSTREAMSTATE_32BIT_HACK:
                break;
        }
    }
    else
    {
        PAUDIOTESTDRVSTACKSTREAM pStreamAt    = (PAUDIOTESTDRVSTACKSTREAM)pStream;
        PDMHOSTAUDIOSTREAMSTATE  enmHostState = pDrvStack->pIHostAudio->pfnStreamGetState(pDrvStack->pIHostAudio,
                                                                                          &pStreamAt->Backend);
        switch (enmHostState)
        {
            case PDMHOSTAUDIOSTREAMSTATE_INITIALIZING:
            case PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING:
                break;
            case PDMHOSTAUDIOSTREAMSTATE_OKAY:
            case PDMHOSTAUDIOSTREAMSTATE_DRAINING:
            case PDMHOSTAUDIOSTREAMSTATE_INACTIVE:
                fRc = true;
                break;
            /* no default */
            case PDMHOSTAUDIOSTREAMSTATE_INVALID:
            case PDMHOSTAUDIOSTREAMSTATE_END:
            case PDMHOSTAUDIOSTREAMSTATE_32BIT_HACK:
                break;
        }
    }
    return fRc;
}

/**
 * Gets the number of bytes it's currently possible to write to the stream.
 */
static uint32_t audioTestDriverStackStreamGetWritable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream)
{
    uint32_t cbWritable;
    if (pDrvStack->pIAudioConnector)
        cbWritable = pDrvStack->pIAudioConnector->pfnStreamGetWritable(pDrvStack->pIAudioConnector, pStream);
    else
    {
        PAUDIOTESTDRVSTACKSTREAM pStreamAt    = (PAUDIOTESTDRVSTACKSTREAM)pStream;
        cbWritable = pDrvStack->pIHostAudio->pfnStreamGetWritable(pDrvStack->pIHostAudio, &pStreamAt->Backend);
    }
    return cbWritable;
}

/**
 * Tries to play the @a cbBuf bytes of samples in @a pvBuf.
 */
static int audioTestDriverStackStreamPlay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream,
                                          void const *pvBuf, uint32_t cbBuf, uint32_t *pcbPlayed)
{
    int rc;
    if (pDrvStack->pIAudioConnector)
    {
        rc = pDrvStack->pIAudioConnector->pfnStreamPlay(pDrvStack->pIAudioConnector, pStream, pvBuf, cbBuf, pcbPlayed);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "pfnStreamPlay(,,,%#x) failed: %Rrc", cbBuf, rc);
    }
    else
    {
        PAUDIOTESTDRVSTACKSTREAM pStreamAt = (PAUDIOTESTDRVSTACKSTREAM)pStream;
        rc = pDrvStack->pIHostAudio->pfnStreamPlay(pDrvStack->pIHostAudio, &pStreamAt->Backend, pvBuf, cbBuf, pcbPlayed);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "PDMIHOSTAUDIO::pfnStreamPlay(,,,%#x) failed: %Rrc", cbBuf, rc);
    }
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
 */
static int audioTestEnvInit(PAUDIOTESTENV pTstEnv, PCPDMDRVREG pDrvReg, bool fWithDrvAudio, const char *pszTag)
{
    PDMAudioHostEnumInit(&pTstEnv->DevEnum);

    int rc = audioTestDriverStackInit(&pTstEnv->DrvStack, pDrvReg, fWithDrvAudio);
    if (RT_FAILURE(rc))
        return rc;

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

    if (RT_FAILURE(rc))
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

    AudioTestSetDestroy(&pTstEnv->Set);
    audioTestDriverStackDelete(&pTstEnv->DrvStack);
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
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s' (ID '%s'):\n", pDev->szName, pDev->pszId);
            else
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

    rc = pTstEnv->DrvStack.pIHostAudio->pfnStreamCreate(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend, pCfg, &CfgAcq);
    if (RT_FAILURE(rc))
        return rc;

    /* Do the async init in a synchronous way for now here. */
    if (rc == VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED)
        rc = pTstEnv->DrvStack.pIHostAudio->pfnStreamInitAsync(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend, false /* fDestroyed */);

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

    int rc = pTstEnv->DrvStack.pIHostAudio->pfnStreamDestroy(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend, true /*fImmediate*/);
    if (RT_SUCCESS(rc))
        RT_BZERO(pStream, sizeof(PDMAUDIOBACKENDSTREAM));

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
    PDMAUDIOSTREAMCFG Cfg;
    int rc = PDMAudioStrmCfgInitWithProps(&Cfg, pProps);
    AssertRC(rc); /* Cannot fail. */

    Cfg.enmDir      = PDMAUDIODIR_IN;
    Cfg.enmPath     = PDMAUDIOPATH_IN_LINE; /* Note: HDA does not have a separate Mic-In enabled yet, so go for Line-In here. */
    Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

    Cfg.Backend.cFramesBufferSize   = PDMAudioPropsMilliToFrames(pProps, 300);
    Cfg.Backend.cFramesPreBuffering = PDMAudioPropsMilliToFrames(pProps, 200);
    Cfg.Backend.cFramesPeriod       = PDMAudioPropsMilliToFrames(pProps, 10);
    Cfg.Device.cMsSchedulingHint    = 10;

    return audioTestStreamCreate(pTstEnv, pStream, &Cfg);
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
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "tone.pcm", &pObj);
    AssertRCReturn(rc, rc);

    PDMHOSTAUDIOSTREAMSTATE enmState = pTstEnv->DrvStack.pIHostAudio->pfnStreamGetState(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend);
    if (enmState == PDMHOSTAUDIOSTREAMSTATE_OKAY)
    {
        uint8_t  abBuf[_4K];

        const uint64_t tsStartMs     = RTTimeMilliTS();
        const uint16_t cSchedulingMs = RTRandU32Ex(10, 80); /* Chose a random scheduling (in ms). */

        do
        {
            uint32_t cbRead = 0;
            rc = pTstEnv->DrvStack.pIHostAudio->pfnStreamCapture(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend, abBuf, sizeof(abBuf), &cbRead);
            if (RT_SUCCESS(rc))
                rc = AudioTestSetObjWrite(pObj, abBuf, cbRead);

            if (RT_FAILURE(rc))
                break;

            if (RTTimeMilliTS() - tsStartMs >= pParms->msDuration)
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
    PDMAUDIOSTREAMCFG Cfg;
    int rc = PDMAudioStrmCfgInitWithProps(&Cfg, pProps);
    AssertRC(rc); /* Cannot fail. */

    Cfg.enmDir      = PDMAUDIODIR_OUT;
    Cfg.enmPath     = PDMAUDIOPATH_OUT_FRONT;
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

    PDMHOSTAUDIOSTREAMSTATE enmState = pTstEnv->DrvStack.pIHostAudio->pfnStreamGetState(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend);
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
                    rc = pTstEnv->DrvStack.pIHostAudio->pfnStreamPlay(pTstEnv->DrvStack.pIHostAudio, &pStream->Backend, abBuf, cbBuf, &cbWritten);
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

    pTstParmsAcq->enmType     = AUDIOTESTTYPE_TESTTONE;

    PDMAudioPropsInit(&pTstParmsAcq->TestTone.Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

    pTstParmsAcq->enmDir      = PDMAUDIODIR_OUT;
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
        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Playing test tone", pTstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            AudioTestToneParamsInitRandom(&pTstParms->TestTone, &pTstParms->TestTone.Props);
            rc = audioTestCreateStreamDefaultOut(pTstEnv, pStream, &pTstParms->TestTone.Props);
            if (RT_SUCCESS(rc))
            {
                rc = audioTestPlayTone(pTstEnv, pStream, &pTstParms->TestTone);
            }

            int rc2 = audioTestStreamDestroy(pTstEnv, pStream);
            if (RT_SUCCESS(rc))
                rc = rc2;

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

    pTstParmsAcq->enmType     = AUDIOTESTTYPE_TESTTONE;

    PDMAudioPropsInit(&pTstParmsAcq->TestTone.Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

    pTstParmsAcq->enmDir      = PDMAUDIODIR_IN;
    pTstParmsAcq->cIterations = RTRandU32Ex(1, 10);
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

    PAUDIOTESTSTREAM pStream = &pTstEnv->aStreams[0];

    for (uint32_t i = 0; i < pTstParms->cIterations; i++)
    {
        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Recording test tone", pTstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            /** @todo  For now we're (re-)creating the recording stream for each iteration. Change that to be random. */
            rc = audioTestCreateStreamDefaultIn(pTstEnv, pStream, &pTstParms->TestTone.Props);
            if (RT_SUCCESS(rc))
            {
                pTstParms->TestTone.msDuration = RTRandU32Ex(50 /* ms */, RT_MS_10SEC); /** @todo Record even longer? */

                rc = audioTestRecordTone(pTstEnv, pStream, &pTstParms->TestTone);
            }

            int rc2 = audioTestStreamDestroy(pTstEnv, pStream);
            if (RT_SUCCESS(rc))
                rc = rc2;

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

/** Option help for the 'test' command.   */
static DECLCALLBACK(const char *) audioTestCmdTestHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'd':                 return "Go via DrvAudio instead of directly interfacing with the backend.";
        case VKAT_TEST_OPT_DEV:   return "Use the specified audio device";
        case 'e':                 return "Exclude the given test id from the list";
        case 'a':                 return "Exclude all tests from the list (useful to enable single tests later with --include)";
        case 'i':                 return "Include the given test id in the list";
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

            case 'd':
                fWithDrvAudio = true;
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

    /* For now all tests have the same test environment. */
    rc = audioTestEnvInit(&TstEnv, pDrvReg, fWithDrvAudio, pszTag);
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

#ifndef DEBUG_andy
        /* Clean up. */
        int rc2 = AudioTestSetWipe(&TstEnv.Set);
        AssertRC(rc2); /* Annoying, but not test-critical. */
#endif
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

/**
 * Verifies one single test set.
 *
 * @returns VBox status code.
 * @param   pszPath             Absolute path to test set.
 * @param   pszTag              Tag of test set to verify. Optional and can be NULL.
 */
static int audioVerifyOne(const char *pszPath, const char *pszTag)
{
    RTTestSubF(g_hTest, "Verifying test set ...");

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pszTag ? pszTag : "default");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Opening archive '%s'\n", pszPath);

    int rc = VINF_SUCCESS;

    char szPathExtracted[RTPATH_MAX];
    const bool fPacked = AudioTestSetIsPacked(pszPath);
    if (fPacked)
    {
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
                    rc = AudioTestSetUnpack(pszPath, szPathExtracted);
                    if (RT_SUCCESS(rc))
                        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Archive successfully unpacked\n");
                }
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "Unable to open / unpack test set archive: %Rrc", rc);
        return rc;
    }

    AUDIOTESTSET tstSet;
    rc = AudioTestSetOpen(&tstSet, fPacked ? szPathExtracted : pszPath);
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
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) audioVerifyMain(PRTGETOPTSTATE pGetState)
{
    /*
     * Parse options and process arguments.
     */
    const char   *pszTag   = NULL; /* Custom tag to use. Can be NULL if not being used. */
    unsigned      iTestSet = 0;

    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)))
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

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

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
        int rc2 = RTPathGetCurrent(szDirCur, sizeof(szDirCur));
        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Failed to retrieve current directory: %Rrc", rc2);
        rc = audioVerifyOne(RT_SUCCESS(rc2) ? szDirCur : ".", pszTag);
    }

    /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
}


/*********************************************************************************************************************************
*   Command: play                                                                                                                *
*********************************************************************************************************************************/

/**
 * Worker for audioTestCmdPlayHandler that plays one file.
 */
static RTEXITCODE audioTestPlayOne(const char *pszFile, PCPDMDRVREG pDrvReg, uint32_t cMsBufferSize,
                                   uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint, bool fWithDrvAudio)
{
    /*
     * First we must open the file and determin the format.
     */
    AUDIOTESTWAVEFILE WaveFile;
    int rc = AudioTestWaveFileOpen(pszFile, &WaveFile);
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("Failed to open '%s': %Rrc", pszFile, rc);

    if (g_uVerbosity > 0)
    {
        char szTmp[128];
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
         * Open a stream for the output.
         */
        PDMAUDIOSTREAMCFG CfgAcq;
        PPDMAUDIOSTREAM   pStream = NULL;
        rc = audioTestDriverStackStreamCreateOutput(&DrvStack, &WaveFile.Props, cMsBufferSize,
                                                    cMsPreBuffer, cMsSchedulingHint, &pStream, &CfgAcq);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestDriverStackStreamEnable(&DrvStack, pStream);
            if (RT_SUCCESS(rc))
            {
                uint64_t const nsStarted = RTTimeNanoTS();

                /*
                 * Transfer data as quickly as we're allowed.
                 */
                for (;;)
                {
                    /* Read a chunk from the wave file. */
                    uint8_t  abSamples[16384];
                    size_t   cbSamples = 0;
                    rc = AudioTestWaveFileRead(&WaveFile, abSamples, sizeof(abSamples), &cbSamples);
                    if (RT_SUCCESS(rc) && cbSamples > 0)
                    {
                        /* Transfer the data to the audio stream. */
                        for (uint32_t offSamples = 0; offSamples < cbSamples;)
                        {
                            uint32_t const cbCanWrite = audioTestDriverStackStreamGetWritable(&DrvStack, pStream);
                            if (cbCanWrite > 0)
                            {
                                uint32_t const cbToPlay = RT_MIN(cbCanWrite, (uint32_t)cbSamples - offSamples);
                                uint32_t       cbPlayed = 0;
                                rc = audioTestDriverStackStreamPlay(&DrvStack, pStream, &abSamples[offSamples],
                                                                    cbToPlay, &cbPlayed);
                                if (RT_SUCCESS(rc))
                                {
                                    if (cbPlayed)
                                        offSamples += cbPlayed;
                                    else
                                    {
                                        rcExit = RTMsgErrorExitFailure("Played zero out of %#x bytes - %#x bytes reported playable!\n",
                                                                       cbToPlay, cbCanWrite);
                                        break;
                                    }
                                }
                                else
                                {
                                    rcExit = RTMsgErrorExitFailure("Failed to play %#x bytes: %Rrc\n", cbToPlay, rc);
                                    break;
                                }
                            }
                            else if (audioTestDriverStackStreamIsOkay(&DrvStack, pStream))
                                RTThreadSleep(RT_MIN(RT_MAX(1, CfgAcq.Device.cMsSchedulingHint), 256));
                            else
                            {
                                rcExit = RTMsgErrorExitFailure("Stream is not okay!\n");
                                break;
                            }
                        }
                    }
                    else if (RT_SUCCESS(rc) && cbSamples == 0)
                    {
                        rcExit = RTEXITCODE_SUCCESS;
                        break;
                    }
                    else
                    {
                        rcExit = RTMsgErrorExitFailure("Error reading wav file '%s': %Rrc", pszFile, rc);
                        break;
                    }
                }

                /*
                 * Drain the stream.
                 */
                if (rcExit == RTEXITCODE_SUCCESS)
                {
                    if (g_uVerbosity > 0)
                        RTMsgInfo("%'RU64 ns: Draining...\n", RTTimeNanoTS() - nsStarted);
                    rc = audioTestDriverStackStreamDrain(&DrvStack, pStream, true /*fSync*/);
                    if (RT_SUCCESS(rc))
                    {
                        if (g_uVerbosity > 0)
                            RTMsgInfo("%'RU64 ns: Done\n", RTTimeNanoTS() - nsStarted);
                    }
                    else
                        rcExit = RTMsgErrorExitFailure("Draining failed: %Rrc", rc);
                }
            }
            else
                rcExit = RTMsgErrorExitFailure("Enabling the output stream failed: %Rrc", rc);
            audioTestDriverStackStreamDestroy(&DrvStack, pStream);
        }
        else
            rcExit = RTMsgErrorExitFailure("Creating output stream failed: %Rrc", rc);
        audioTestDriverStackDelete(&DrvStack);
    }
    else
        rcExit = RTMsgErrorExitFailure("Driver stack construction failed: %Rrc", rc);
    AudioTestWaveFileClose(&WaveFile);
    return rcExit;
}

/**
 * Command line parameters for test mode.
 */
static const RTGETOPTDEF g_aCmdPlayOptions[] =
{
    { "--backend",          'b',                          RTGETOPT_REQ_STRING  },
    { "--with-drv-audio",   'd',                          RTGETOPT_REQ_NOTHING },
};

/** the 'play' command option help. */
static DECLCALLBACK(const char *) audioTestCmdPlayHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case 'b': return "The audio backend to use.";
        case 'd': return "Go via DrvAudio instead of directly interfacing with the backend.";
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
    PCPDMDRVREG pDrvReg           = g_aBackends[0].pDrvReg;
    uint32_t    cMsBufferSize     = UINT32_MAX;
    uint32_t    cMsPreBuffer      = UINT32_MAX;
    uint32_t    cMsSchedulingHint = UINT32_MAX;
    bool        fWithDrvAudio     = false;

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
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

            case 'd':
                fWithDrvAudio = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                RTEXITCODE rcExit = audioTestPlayOne(ValueUnion.psz, pDrvReg, cMsBufferSize, cMsPreBuffer,
                                                     cMsSchedulingHint, fWithDrvAudio);
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

/**
 * Command line parameters for self-test mode.
 */
static const RTGETOPTDEF g_aCmdSelftestOptions[] =
{
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
 * Structure for keeping a user context for the test service callbacks.
 */
typedef struct ATSCALLBACKCTX
{
    /** Driver stack to use. */
    PAUDIOTESTDRVSTACK pDrvStack;
    /** Audio stream to use. */
    PPDMAUDIOSTREAM    pStream;
} ATSCALLBACKCTX;
typedef ATSCALLBACKCTX *PATSCALLBACKCTX;

/**
 * Note: Called within server (client serving) thread.
 */
static DECLCALLBACK(int) audioTestSvcTonePlayCallback(void const *pvUser, PPDMAUDIOSTREAMCFG pStreamCfg, PAUDIOTESTTONEPARMS pToneParms)
{
    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    AUDIOTESTTONE TstTone;
    AudioTestToneInitRandom(&TstTone, &pStreamCfg->Props);

    int rc;

    if (audioTestDriverStackStreamIsOkay(pCtx->pDrvStack, pCtx->pStream))
    {
        uint32_t cbBuf;
        uint8_t  abBuf[_4K];

        const uint64_t tsStartMs     = RTTimeMilliTS();
        const uint16_t cSchedulingMs = RTRandU32Ex(10, 80); /* Chose a random scheduling (in ms). */
        const uint32_t cbPerMs       = PDMAudioPropsMilliToBytes(&pCtx->pStream->Props, cSchedulingMs);

        do
        {
            rc = AudioTestToneGenerate(&TstTone, abBuf, RT_MIN(cbPerMs, sizeof(abBuf)), &cbBuf);
            if (RT_SUCCESS(rc))
            {
                uint32_t cbWritten;
                rc = audioTestDriverStackStreamPlay(pCtx->pDrvStack, pCtx->pStream, abBuf, cbBuf, &cbWritten);
            }

            if (RTTimeMilliTS() - tsStartMs >= pToneParms->msDuration)
                break;

            if (RT_FAILURE(rc))
                break;

            RTThreadSleep(cSchedulingMs);

        } while (RT_SUCCESS(rc));
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    return rc;
}

/**
 * Tests the Audio Test Service (ATS).
 *
 * @param   pDrvReg             Backend driver to use.
 * @returns VBox status code.
 */
static int audioTestDoSelftestSvc(PCPDMDRVREG pDrvReg)
{
    AUDIOTESTDRVSTACK DrvStack;
    int rc = audioTestDriverStackInit(&DrvStack, pDrvReg, true /* fWithDrvAudio */);
    if (RT_SUCCESS(rc))
    {
        PDMAUDIOPCMPROPS  Props;
        PDMAudioPropsInit(&Props, 16 /* bit */ / 8, true /* fSigned */, 2 /* Channels */, 44100 /* Hz */);

        PDMAUDIOSTREAMCFG CfgAcq;
        PPDMAUDIOSTREAM   pStream = NULL;
        rc = audioTestDriverStackStreamCreateOutput(&DrvStack, &Props,
                                                    UINT32_MAX /* cMsBufferSize */,
                                                    UINT32_MAX /* cMsPreBuffer */,
                                                    UINT32_MAX /* cMsSchedulingHint */, &pStream, &CfgAcq);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestDriverStackStreamEnable(&DrvStack, pStream);
            if (RT_SUCCESS(rc))
            {
                ATSCALLBACKCTX Ctx;
                Ctx.pDrvStack = &DrvStack;
                Ctx.pStream   = pStream;

                ATSCALLBACKS Callbacks;
                Callbacks.pfnTonePlay = audioTestSvcTonePlayCallback;
                Callbacks.pvUser      = &Ctx;

                ATSSERVER Srv;
                rc = AudioTestSvcInit(&Srv, &Callbacks);
                if (RT_SUCCESS(rc))
                {
                    rc = AudioTestSvcStart(&Srv);
                    if (RT_SUCCESS(rc))
                    {
                        ATSCLIENT Conn;
                        rc = AudioTestSvcClientConnect(&Conn, NULL);
                        if (RT_SUCCESS(rc))
                        {
                            /* Do the bare minimum here to get a test tone out. */
                            AUDIOTESTTONEPARMS ToneParms;
                            RT_ZERO(ToneParms);
                            ToneParms.msDuration = 2000;
                            memcpy(&ToneParms.Props, &CfgAcq.Props, sizeof(PDMAUDIOPCMPROPS));

                            rc = AudioTestSvcClientTonePlay(&Conn, &CfgAcq, &ToneParms);

                            int rc2 = AudioTestSvcClientClose(&Conn);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                        }

                        int rc2 = AudioTestSvcShutdown(&Srv);
                        if (RT_SUCCESS(rc))
                            rc = rc2;
                    }

                    int rc2 = AudioTestSvcDestroy(&Srv);
                    if (RT_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }
    }

    return rc;
}

/**
 * Main function for performing the self-tests.
 *
 * @returns VBox status code.
 * @param   pDrvReg             Backend driver to use.
 */
static int audioTestDoSelftest(PCPDMDRVREG pDrvReg)
{
    int rc = audioTestDoSelftestSvc(pDrvReg);
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

    /* Argument processing loop: */
    int           rc;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(pGetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
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

            case 'd':
                fWithDrvAudio = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                break;
            }

            AUDIO_TEST_COMMON_OPTION_CASES(ValueUnion);

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    audioTestDoSelftest(pDrvReg);
        /*
     * Print summary and exit.
     */
    return RTTestSummaryAndDestroy(g_hTest);
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
        "play",     audioTestCmdPlayHandler,
        "Plays one or more wave files.",
        g_aCmdPlayOptions,      RT_ELEMENTS(g_aCmdPlayOptions),     audioTestCmdPlayHelp,
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

    /*
     * Process common options.
     */
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, g_aCmdCommonOptions,
                          RT_ELEMENTS(g_aCmdCommonOptions), 1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    RTGETOPTUNION ValueUnion;
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

