/* $Id$ */
/** @file
 * VKAT - Internal header file for common definitions + structs.
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

#ifndef VBOX_INCLUDED_SRC_audio_vkatInternal_h
#define VBOX_INCLUDED_SRC_audio_vkatInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/getopt.h>

#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include "Audio/AudioMixBuffer.h"
#include "Audio/AudioTest.h"
#include "Audio/AudioTestService.h"
#include "Audio/AudioTestServiceClient.h"

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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
    /** The backend data (variable size). */
    PDMAUDIOBACKENDSTREAM   Backend;
} AUDIOTESTDRVSTACKSTREAM;
/** Pointer to a backend-only stream structure. */
typedef AUDIOTESTDRVSTACKSTREAM *PAUDIOTESTDRVSTACKSTREAM;

/**
 * Mixer setup for a stream.
 */
typedef struct AUDIOTESTDRVMIXSTREAM
{
    /** Pointer to the driver stack. */
    PAUDIOTESTDRVSTACK      pDrvStack;
    /** Pointer to the stream. */
    PPDMAUDIOSTREAM         pStream;
    /** Properties to use. */
    PCPDMAUDIOPCMPROPS      pProps;
    /** Set if we're mixing or just passing thru to the driver stack. */
    bool                    fDoMixing;
    /** Mixer buffer. */
    AUDIOMIXBUF             MixBuf;
    /** Write state. */
    AUDIOMIXBUFWRITESTATE   WriteState;
    /** Peek state. */
    AUDIOMIXBUFPEEKSTATE    PeekState;
} AUDIOTESTDRVMIXSTREAM;
/** Pointer to mixer setup for a stream. */
typedef AUDIOTESTDRVMIXSTREAM *PAUDIOTESTDRVMIXSTREAM;

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
    /** Whether self test mode is active or not. */
    bool                    fSelftest;
    /** Whether skip the actual verification or not. */
    bool                    fSkipVerify;
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
            /** Client connected to the ATS on the guest side. */
            ATSCLIENT       AtsClGuest;
            /** Path to the guest's test set downloaded to the host. */
            char            szPathTestSetGuest[RTPATH_MAX];
            /** Client connected to the Validation Kit audio driver ATS. */
            ATSCLIENT       AtsClValKit;
            /** Path to the Validation Kit audio driver's test set downloaded to the host. */
            char            szPathTestSetValKit[RTPATH_MAX];
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
 * Structure for keeping a VKAT self test context.
 */
typedef struct SELFTESTCTX
{
    /** Common tag for guest and host side. */
    char             szTag[AUDIOTEST_TAG_MAX];
    /** Whether to use DrvAudio in the driver stack or not. */
    bool             fWithDrvAudio;
    struct
    {
        AUDIOTESTENV TstEnv;
        /** Audio driver to use.
         *  Defaults to the platform's default driver. */
        PCPDMDRVREG  pDrvReg;
        /** Where to bind the address of the guest ATS instance to.
         *  Defaults to localhost (127.0.0.1) if empty. */
        char         szAtsAddr[64];
        /** Port of the guest ATS instance.
         *  Defaults to ATS_ALT_PORT if not set. */
        uint32_t     uAtsPort;
    } Guest;
    struct
    {
        AUDIOTESTENV TstEnv;
        /** Address of the guest ATS instance.
         *  Defaults to localhost (127.0.0.1) if not set. */
        char         szGuestAtsAddr[64];
        /** Port of the guest ATS instance.
         *  Defaults to ATS_DEFAULT_PORT if not set. */
        uint32_t     uGuestAtsPort;
        /** Address of the Validation Kit audio driver ATS instance.
         *  Defaults to localhost (127.0.0.1) if not set. */
        char         szValKitAtsAddr[64];
        /** Port of the Validation Kit audio driver ATS instance.
         *  Defaults to ATS_ALT_PORT if not set. */
        uint32_t     uValKitAtsPort;
    } Host;
} SELFTESTCTX;
/** Pointer to a VKAT self test context. */
typedef SELFTESTCTX *PSELFTESTCTX;

/**
 * VKAT command table entry.
 */
typedef struct VKATCMD
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
} VKATCMD;
/** Pointer to a const VKAT command entry. */
typedef VKATCMD const *PCVKATCMD;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Terminate ASAP if set.  Set on Ctrl-C. */
extern bool volatile    g_fTerminate;
/** The release logger. */
extern PRTLOGGER        g_pRelLogger;

/** The test handle. */
extern RTTEST           g_hTest;
extern unsigned         g_uVerbosity;
extern bool             g_fDrvAudioDebug;
extern const char      *g_pszDrvAudioDebug;

/** The test handle. */
extern RTTEST           g_hTest;
/** The current verbosity level. */
extern unsigned         g_uVerbosity;
/** DrvAudio: Enable debug (or not). */
extern bool             g_fDrvAudioDebug;
/** DrvAudio: The debug output path. */
extern const char      *g_pszDrvAudioDebug;

extern const VKATCMD    g_CmdEnum;
extern const VKATCMD    g_CmdPlay;
extern const VKATCMD    g_CmdRec;
extern const VKATCMD    g_CmdSelfTest;

extern AUDIOTESTDESC    g_aTests[];
extern unsigned         g_cTests;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/

/** @name Command line handlers
 * @{ */
RTEXITCODE audioTestUsage(PRTSTREAM pStrm);
RTEXITCODE audioTestVersion(void);
/** @}  */

/** @name Driver stack
 * @{ */
void        audioTestDriverStackDelete(PAUDIOTESTDRVSTACK pDrvStack);
int         audioTestDriverStackInitEx(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, bool fEnabledIn, bool fEnabledOut, bool fWithDrvAudio);
int         audioTestDriverStackInit(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, bool fWithDrvAudio);
int         audioTestDriverStackSetDevice(PAUDIOTESTDRVSTACK pDrvStack, PDMAUDIODIR enmDir, const char *pszDevId);
/** @}  */

/** @name Driver
 * @{ */
int         audioTestDrvConstruct(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, PPDMDRVINS pParentDrvIns, PPPDMDRVINS ppDrvIns);
/** @}  */

/** @name Driver stack stream
 * @{ */
int         audioTestDriverStackStreamCreateInput(PAUDIOTESTDRVSTACK pDrvStack, PCPDMAUDIOPCMPROPS pProps,
                                                  uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                                  PPDMAUDIOSTREAM *ppStream, PPDMAUDIOSTREAMCFG pCfgAcq);
int         audioTestDriverStackStreamCreateOutput(PAUDIOTESTDRVSTACK pDrvStack, PCPDMAUDIOPCMPROPS pProps,
                                                   uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                                   PPDMAUDIOSTREAM *ppStream, PPDMAUDIOSTREAMCFG pCfgAcq);
void        audioTestDriverStackStreamDestroy(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int         audioTestDriverStackStreamDrain(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream, bool fSync);
int         audioTestDriverStackStreamEnable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int         AudioTestDriverStackStreamDisable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
bool        audioTestDriverStackStreamIsOkay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
uint32_t    audioTestDriverStackStreamGetWritable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int         audioTestDriverStackStreamPlay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream, void const *pvBuf,
                                           uint32_t cbBuf, uint32_t *pcbPlayed);
uint32_t    audioTestDriverStackStreamGetReadable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int         audioTestDriverStackStreamCapture(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream,
                                              void *pvBuf, uint32_t cbBuf, uint32_t *pcbCaptured);
/** @}  */

/** @name Backend handling
 * @{ */
PCPDMDRVREG AudioTestGetDefaultBackend(void);
PCPDMDRVREG AudioTestFindBackendOpt(const char *pszBackend);
/** @}  */

/** @name Mixing stream
 * @{ */
int         AudioTestMixStreamInit(PAUDIOTESTDRVMIXSTREAM pMix, PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream,
                                   PCPDMAUDIOPCMPROPS pProps, uint32_t cMsBuffer);
void        AudioTestMixStreamTerm(PAUDIOTESTDRVMIXSTREAM pMix);
int         AudioTestMixStreamEnable(PAUDIOTESTDRVMIXSTREAM pMix);
int         AudioTestMixStreamDrain(PAUDIOTESTDRVMIXSTREAM pMix, bool fSync);
int         AudioTestMixStreamDisable(PAUDIOTESTDRVMIXSTREAM pMix);
bool        AudioTestMixStreamIsOkay(PAUDIOTESTDRVMIXSTREAM pMix);
uint32_t    AudioTestMixStreamGetWritable(PAUDIOTESTDRVMIXSTREAM pMix);
int         AudioTestMixStreamPlay(PAUDIOTESTDRVMIXSTREAM pMix, void const *pvBuf, uint32_t cbBuf, uint32_t *pcbPlayed);
uint32_t    AudioTestMixStreamGetReadable(PAUDIOTESTDRVMIXSTREAM pMix);
int         AudioTestMixStreamCapture(PAUDIOTESTDRVMIXSTREAM pMix, void *pvBuf, uint32_t cbBuf, uint32_t *pcbCaptured);
/** @}  */

/** @name Device handling
 * @{ */
int         audioTestDeviceOpen(PPDMAUDIOHOSTDEV pDev);
int         audioTestDeviceClose(PPDMAUDIOHOSTDEV pDev);
/** @}  */

/** @name ATS routines
 * @{ */
int         audioTestEnvConnectToHostAts(PAUDIOTESTENV pTstEnv,
                                         const char *pszHostTcpAddr, uint32_t uHostTcpPort);
/** @}  */

/** @name Test environment handling
 * @{ */
int         audioTestEnvInit(PAUDIOTESTENV pTstEnv, PCPDMDRVREG pDrvReg, bool fWithDrvAudio, const char *pszHostTcpAddr, uint32_t uHostTcpPort, const char *pszGuestTcpAddr, uint32_t uGuestTcpPort);
void        audioTestEnvDestroy(PAUDIOTESTENV pTstEnv);
int         audioTestEnvPrologue(PAUDIOTESTENV pTstEnv, bool fPack, char *pszPackFile, size_t cbPackFile);

void        audioTestParmsInit(PAUDIOTESTPARMS pTstParms);
void        audioTestParmsDestroy(PAUDIOTESTPARMS pTstParms);
/** @}  */

int         audioTestWorker(PAUDIOTESTENV pTstEnv, PAUDIOTESTPARMS pOverrideParms);

/** @name Command handlers
 * @{ */
RTEXITCODE   audioTestDoSelftest(PSELFTESTCTX pCtx);
/** @}  */


/*********************************************************************************************************************************
*   Common command line stuff                                                                                                    *
*********************************************************************************************************************************/

/**
 * Common long options values.
 */
enum
{
    AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_ENABLE = 256,
    AUDIO_TEST_OPT_CMN_DEBUG_AUDIO_PATH
};

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

#endif /* !VBOX_INCLUDED_SRC_audio_vkatInternal_h */

