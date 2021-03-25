/* $Id$ */
/** @file
 * Intermediate audio driver - Connects the audio device emulation with the host backend.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include <iprt/alloc.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/circbuf.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#include <ctype.h>
#include <stdlib.h>

#include "AudioHlp.h"
#include "AudioMixBuffer.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef enum
{
    AUD_OPT_INT,
    AUD_OPT_FMT,
    AUD_OPT_STR,
    AUD_OPT_BOOL
} audio_option_tag_e;

typedef struct audio_option
{
    const char *name;
    audio_option_tag_e tag;
    void *valp;
    const char *descr;
    int *overridenp;
    int overriden;
} audio_option;

#ifdef VBOX_WITH_STATISTICS
/**
 * Structure for keeping stream statistics for the
 * statistic manager (STAM).
 */
typedef struct DRVAUDIOSTATS
{
    STAMCOUNTER TotalStreamsActive;
    STAMCOUNTER TotalStreamsCreated;
    STAMCOUNTER TotalFramesRead;
    STAMCOUNTER TotalFramesWritten;
    STAMCOUNTER TotalFramesMixedIn;
    STAMCOUNTER TotalFramesMixedOut;
    STAMCOUNTER TotalFramesLostIn;
    STAMCOUNTER TotalFramesLostOut;
    STAMCOUNTER TotalFramesOut;
    STAMCOUNTER TotalFramesIn;
    STAMCOUNTER TotalBytesRead;
    STAMCOUNTER TotalBytesWritten;
    /** How much delay (in ms) for input processing. */
    STAMPROFILEADV DelayIn;
    /** How much delay (in ms) for output processing. */
    STAMPROFILEADV DelayOut;
} DRVAUDIOSTATS, *PDRVAUDIOSTATS;
#endif

/**
 * Audio driver configuration data, tweakable via CFGM.
 */
typedef struct DRVAUDIOCFG
{
    /** PCM properties to use. */
    PDMAUDIOPCMPROPS     Props;
    /** Whether using signed sample data or not.
     *  Needed in order to know whether there is a custom value set in CFGM or not.
     *  By default set to UINT8_MAX if not set to a custom value. */
    uint8_t              uSigned;
    /** Whether swapping endianess of sample data or not.
     *  Needed in order to know whether there is a custom value set in CFGM or not.
     *  By default set to UINT8_MAX if not set to a custom value. */
    uint8_t              uSwapEndian;
    /** Configures the period size (in ms).
     *  This value reflects the time in between each hardware interrupt on the
     *  backend (host) side. */
    uint32_t             uPeriodSizeMs;
    /** Configures the (ring) buffer size (in ms). Often is a multiple of uPeriodMs. */
    uint32_t             uBufferSizeMs;
    /** Configures the pre-buffering size (in ms).
     *  Time needed in buffer before the stream becomes active (pre buffering).
     *  The bigger this value is, the more latency for the stream will occur.
     *  Set to 0 to disable pre-buffering completely.
     *  By default set to UINT32_MAX if not set to a custom value. */
    uint32_t             uPreBufSizeMs;
    /** The driver's debugging configuration. */
    struct
    {
        /** Whether audio debugging is enabled or not. */
        bool             fEnabled;
        /** Where to store the debugging files. */
        char             szPathOut[RTPATH_MAX];
    } Dbg;
} DRVAUDIOCFG, *PDRVAUDIOCFG;

/**
 * Audio driver instance data.
 *
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVAUDIO
{
    /** Friendly name of the driver. */
    char                    szName[64];
    /** Critical section for serializing access. */
    RTCRITSECT              CritSect;
    /** Shutdown indicator. */
    bool                    fTerminate;
#ifdef VBOX_WITH_AUDIO_ENUM
    /** Flag indicating to perform an (re-)enumeration of the host audio devices. */
    bool                    fEnumerateDevices;
#endif
    /** Our audio connector interface. */
    PDMIAUDIOCONNECTOR      IAudioConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to audio driver below us. */
    PPDMIHOSTAUDIO          pHostDrvAudio;
    /** List of audio streams. */
    RTLISTANCHOR            lstStreams;
    /** Audio configuration settings retrieved from the backend. */
    PDMAUDIOBACKENDCFG      BackendCfg;
    struct
    {
        /** Whether this driver's input streams are enabled or not.
         *  This flag overrides all the attached stream statuses. */
        bool                fEnabled;
        /** Max. number of free input streams.
         *  UINT32_MAX for unlimited streams. */
        uint32_t            cStreamsFree;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
        RTLISTANCHOR        lstCB;
#endif
        /** The driver's input confguration (tweakable via CFGM). */
        DRVAUDIOCFG         Cfg;
    } In;
    struct
    {
        /** Whether this driver's output streams are enabled or not.
         *  This flag overrides all the attached stream statuses. */
        bool                fEnabled;
        /** Max. number of free output streams.
         *  UINT32_MAX for unlimited streams. */
        uint32_t            cStreamsFree;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
        RTLISTANCHOR        lstCB;
#endif
        /** The driver's output confguration (tweakable via CFGM). */
        DRVAUDIOCFG         Cfg;
        /** Number of times underruns triggered re-(pre-)buffering. */
        STAMCOUNTER         StatsReBuffering;
    } Out;
#ifdef VBOX_WITH_STATISTICS
    /** Statistics for the statistics manager (STAM). */
    DRVAUDIOSTATS           Stats;
#endif
} DRVAUDIO;
/** Pointer to the instance data of an audio driver. */
typedef DRVAUDIO *PDRVAUDIO;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_AUDIO_ENUM
static int drvAudioDevicesEnumerateInternal(PDRVAUDIO pThis, bool fLog, PPDMAUDIOHOSTENUM pDevEnum);
#endif

static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamCreateInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq);
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static void drvAudioStreamFree(PPDMAUDIOSTREAM pStream);
static int drvAudioStreamUninitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamReInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static void drvAudioStreamDropInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static void drvAudioStreamResetInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);


#ifndef VBOX_AUDIO_TESTCASE

# if 0 /* unused */

static PDMAUDIOFMT drvAudioGetConfFormat(PCFGMNODE pCfgHandle, const char *pszKey,
                                         PDMAUDIOFMT enmDefault, bool *pfDefault)
{
    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return enmDefault;
    }

    char *pszValue = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, pszKey, &pszValue);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return enmDefault;
    }

    PDMAUDIOFMT fmt = AudioHlpStrToAudFmt(pszValue);
    if (fmt == PDMAUDIOFMT_INVALID)
    {
         *pfDefault = true;
        return enmDefault;
    }

    *pfDefault = false;
    return fmt;
}

static int drvAudioGetConfInt(PCFGMNODE pCfgHandle, const char *pszKey,
                              int iDefault, bool *pfDefault)
{

    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return iDefault;
    }

    uint64_t u64Data = 0;
    int rc = CFGMR3QueryInteger(pCfgHandle, pszKey, &u64Data);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return iDefault;

    }

    *pfDefault = false;
    return u64Data;
}

static const char *drvAudioGetConfStr(PCFGMNODE pCfgHandle, const char *pszKey,
                                      const char *pszDefault, bool *pfDefault)
{
    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return pszDefault;
    }

    char *pszValue = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, pszKey, &pszValue);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return pszDefault;
    }

    *pfDefault = false;
    return pszValue;
}

# endif /* unused */

#ifdef LOG_ENABLED

/** Buffer size for dbgAudioStreamStatusToStr.  */
# define DRVAUDIO_STATUS_STR_MAX sizeof("INITIALIZED ENABLED PAUSED PENDING_DISABLED PENDING_REINIT 0x12345678")

/**
 * Converts an audio stream status to a string.
 *
 * @returns pszDst
 * @param   pszDst      Buffer to convert into, at least minimum size is
 *                      DRVAUDIO_STATUS_STR_MAX.
 * @param   fStatus     Stream status flags to convert.
 */
static const char *dbgAudioStreamStatusToStr(char pszDst[DRVAUDIO_STATUS_STR_MAX], PDMAUDIOSTREAMSTS fStatus)
{
    static const struct
    {
        const char         *pszMnemonic;
        uint32_t            cchMnemnonic;
        PDMAUDIOSTREAMSTS   fFlag;
    } s_aFlags[] =
    {
        { RT_STR_TUPLE("INITIALIZED "),     PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED     },
        { RT_STR_TUPLE("ENABLED "),         PDMAUDIOSTREAMSTS_FLAGS_ENABLED         },
        { RT_STR_TUPLE("PAUSED "),          PDMAUDIOSTREAMSTS_FLAGS_PAUSED          },
        { RT_STR_TUPLE("PENDING_DISABLE "), PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE },
        { RT_STR_TUPLE("PENDING_REINIT "),  PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT  },
    };
    if (!fStatus)
        strcpy(pszDst, "NONE");
    else
    {
        char *psz = pszDst;
        for (size_t i = 0; i < RT_ELEMENTS(s_aFlags); i++)
            if (fStatus & s_aFlags[i].fFlag)
            {
                memcpy(psz, s_aFlags[i].pszMnemonic, s_aFlags[i].cchMnemnonic);
                psz += s_aFlags[i].cchMnemnonic;
                fStatus &= ~s_aFlags[i].fFlag;
                if (!fStatus)
                    break;
            }
        if (fStatus == 0)
            psz[-1] = '\0';
        else
            psz += RTStrPrintf(psz, DRVAUDIO_STATUS_STR_MAX - (psz - pszDst), "%#x", fStatus);
        Assert((uintptr_t)(psz - pszDst) <= DRVAUDIO_STATUS_STR_MAX);
    }
    return pszDst;
}

#endif /* defined(LOG_ENABLED) */

# if 0 /* unused */
static int drvAudioProcessOptions(PCFGMNODE pCfgHandle, const char *pszPrefix, audio_option *paOpts, size_t cOpts)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPrefix,  VERR_INVALID_POINTER);
    /* oaOpts and cOpts are optional. */

    PCFGMNODE pCfgChildHandle = NULL;
    PCFGMNODE pCfgChildChildHandle = NULL;

   /* If pCfgHandle is NULL, let NULL be passed to get int and get string functions..
    * The getter function will return default values.
    */
    if (pCfgHandle != NULL)
    {
       /* If its audio general setting, need to traverse to one child node.
        * /Devices/ichac97/0/LUN#0/Config/Audio
        */
       if(!strncmp(pszPrefix, "AUDIO", 5)) /** @todo Use a \#define */
       {
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if(pCfgChildHandle)
                pCfgHandle = pCfgChildHandle;
        }
        else
        {
            /* If its driver specific configuration , then need to traverse two level deep child
             * child nodes. for eg. in case of DirectSoundConfiguration item
             * /Devices/ichac97/0/LUN#0/Config/Audio/DirectSoundConfig
             */
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if (pCfgChildHandle)
            {
                pCfgChildChildHandle = CFGMR3GetFirstChild(pCfgChildHandle);
                if (pCfgChildChildHandle)
                    pCfgHandle = pCfgChildChildHandle;
            }
        }
    }

    for (size_t i = 0; i < cOpts; i++)
    {
        audio_option *pOpt = &paOpts[i];
        if (!pOpt->valp)
        {
            LogFlowFunc(("Option value pointer for `%s' is not set\n", pOpt->name));
            continue;
        }

        bool fUseDefault;

        switch (pOpt->tag)
        {
            case AUD_OPT_BOOL:
            case AUD_OPT_INT:
            {
                int *intp = (int *)pOpt->valp;
                *intp = drvAudioGetConfInt(pCfgHandle, pOpt->name, *intp, &fUseDefault);

                break;
            }

            case AUD_OPT_FMT:
            {
                PDMAUDIOFMT *fmtp = (PDMAUDIOFMT *)pOpt->valp;
                *fmtp = drvAudioGetConfFormat(pCfgHandle, pOpt->name, *fmtp, &fUseDefault);

                break;
            }

            case AUD_OPT_STR:
            {
                const char **strp = (const char **)pOpt->valp;
                *strp = drvAudioGetConfStr(pCfgHandle, pOpt->name, *strp, &fUseDefault);

                break;
            }

            default:
                LogFlowFunc(("Bad value tag for option `%s' - %d\n", pOpt->name, pOpt->tag));
                fUseDefault = false;
                break;
        }

        if (!pOpt->overridenp)
            pOpt->overridenp = &pOpt->overriden;

        *pOpt->overridenp = !fUseDefault;
    }

    return VINF_SUCCESS;
}
# endif /* unused */
#endif /* !VBOX_AUDIO_TESTCASE */

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioStreamControl(PPDMIAUDIOCONNECTOR pInterface,
                                               PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /** @todo r=bird: why?  It's not documented to ignore NULL streams.   */
    if (!pStream)
        return VINF_SUCCESS;

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    LogFlowFunc(("[%s] enmStreamCmd=%s\n", pStream->szName, PDMAudioStrmCmdGetName(enmStreamCmd)));

    rc = drvAudioStreamControlInternal(pThis, pStream, enmStreamCmd);

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}

/**
 * Controls an audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to control.
 * @param   enmStreamCmd        Control command.
 */
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFunc(("[%s] enmStreamCmd=%s fStatus=%s\n", pStream->szName, PDMAudioStrmCmdGetName(enmStreamCmd),
             dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (!(pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED))
            {
                /* Is a pending disable outstanding? Then disable first. */
                if (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
                    rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);

                if (RT_SUCCESS(rc))
                    rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_ENABLE);

                if (RT_SUCCESS(rc))
                    pStream->fStatus |= PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            if (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED)
            {
                /*
                 * For playback (output) streams first mark the host stream as pending disable,
                 * so that the rest of the remaining audio data will be played first before
                 * closing the stream.
                 */
                if (pStream->enmDir == PDMAUDIODIR_OUT)
                {
                    LogFunc(("[%s] Pending disable/pause\n", pStream->szName));
                    pStream->fStatus |= PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE;
                }

                /* Can we close the host stream as well (not in pending disable mode)? */
                if (!(pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE))
                {
                    rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                        drvAudioStreamResetInternal(pThis, pStream);
                }
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (!(pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED))
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_PAUSE);
                if (RT_SUCCESS(rc))
                    pStream->fStatus |= PDMAUDIOSTREAMSTS_FLAGS_PAUSED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED)
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_RESUME);
                if (RT_SUCCESS(rc))
                    pStream->fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_PAUSED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DROP:
        {
            rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DROP);
            if (RT_SUCCESS(rc))
                drvAudioStreamDropInternal(pThis, pStream);
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStream->szName, rc));

    return rc;
}

/**
 * Controls a stream's backend.
 * If the stream has no backend available, VERR_NOT_FOUND is returned.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to control.
 * @param   enmStreamCmd        Control command.
 */
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFlowFunc(("[%s] enmStreamCmd=%s, fStatus=%s\n", pStream->szName, PDMAudioStrmCmdGetName(enmStreamCmd),
                 dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));

    if (!pThis->pHostDrvAudio) /* If not lower driver is configured, bail out. */
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    /*
     * Whether to propagate commands down to the backend.
     *
     * This is needed for critical operations like recording audio if audio input is disabled on a per-driver level.
     *
     * Note that not all commands will be covered by this, such as operations like stopping, draining and droppping,
     * which are considered uncritical and sometimes even are required for certain backends (like DirectSound on Windows).
     *
     * The actual stream state will be untouched to not make the state machine handling more complicated than
     * it already is.
     *
     * See @bugref{9882}.
     */
    const bool fEnabled =    (   pStream->enmDir == PDMAUDIODIR_IN
                              && pThis->In.fEnabled)
                          || (   pStream->enmDir == PDMAUDIODIR_OUT
                              && pThis->Out.fEnabled);

    LogRel2(("Audio: %s stream '%s' in backend (%s is %s)\n", PDMAudioStrmCmdGetName(enmStreamCmd), pStream->szName,
                                                              PDMAudioDirGetName(pStream->enmDir),
                                                              fEnabled ? "enabled" : "disabled"));
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (fEnabled)
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_ENABLE);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_DISABLE);
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (fEnabled) /* Needed, as resume below also is being checked for. */
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_PAUSE);
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (fEnabled)
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_RESUME);
            break;
        }

        case PDMAUDIOSTREAMCMD_DRAIN:
        {
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_DRAIN);
            break;
        }

        case PDMAUDIOSTREAMCMD_DROP:
        {
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStream->pvBackend, PDMAUDIOSTREAMCMD_DROP);
            break;
        }

        default:
        {
            AssertMsgFailed(("Command %RU32 not implemented\n", enmStreamCmd));
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }

    if (RT_FAILURE(rc))
    {
        if (   rc != VERR_NOT_IMPLEMENTED
            && rc != VERR_NOT_SUPPORTED
            && rc != VERR_AUDIO_STREAM_NOT_READY)
        {
            LogRel(("Audio: %s stream '%s' failed with %Rrc\n", PDMAudioStrmCmdGetName(enmStreamCmd), pStream->szName, rc));
        }

        LogFunc(("[%s] %s failed with %Rrc\n", pStream->szName, PDMAudioStrmCmdGetName(enmStreamCmd), rc));
    }

    return rc;
}

/**
 * Frees an audio stream and its allocated resources.
 *
 * @param   pStream             Audio stream to free.
 *                              After this call the pointer will not be valid anymore.
 */
static void drvAudioStreamFree(PPDMAUDIOSTREAM pStream)
{
    if (pStream)
    {
        LogFunc(("[%s]\n", pStream->szName));
        Assert(pStream->uMagic == PDMAUDIOSTREAM_MAGIC);

        pStream->uMagic    = ~PDMAUDIOSTREAM_MAGIC;
        pStream->pvBackend = NULL;

        RTMemFree(pStream);
    }
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * Schedules a re-initialization of all current audio streams.
 * The actual re-initialization will happen at some later point in time.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 */
static int drvAudioScheduleReInitInternal(PDRVAUDIO pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFunc(("\n"));

    /* Mark all host streams to re-initialize. */
    PPDMAUDIOSTREAM pStream;
    RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, ListEntry)
    {
        pStream->fStatus |= PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT;
        pStream->cTriesReInit   = 0;
        pStream->tsLastReInitNs = 0;
    }

# ifdef VBOX_WITH_AUDIO_ENUM
    /* Re-enumerate all host devices as soon as possible. */
    pThis->fEnumerateDevices = true;
# endif

    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

/**
 * Re-initializes an audio stream with its existing host and guest stream configuration.
 * This might be the case if the backend told us we need to re-initialize because something
 * on the host side has changed.
 *
 * Note: Does not touch the stream's status flags.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to re-initialize.
 */
static int drvAudioStreamReInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFlowFunc(("[%s]\n", pStream->szName));

    /*
     * Gather current stream status.
     */
    const bool fIsEnabled = RT_BOOL(pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED); /* Stream is enabled? */

    /*
     * Destroy and re-create stream on backend side.
     */
    int rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
    {
        rc = drvAudioStreamDestroyInternalBackend(pThis, pStream);
        if (RT_SUCCESS(rc))
        {
            PDMAUDIOSTREAMCFG CfgHostAcq;
            rc = drvAudioStreamCreateInternalBackend(pThis, pStream, &pStream->Host.Cfg, &CfgHostAcq);
            /** @todo Validate (re-)acquired configuration with pStream->Host.Cfg? */
            if (RT_SUCCESS(rc))
            {
#ifdef LOG_ENABLED
                LogFunc(("[%s] Acquired host format:\n",  pStream->szName));
                PDMAudioStrmCfgLog(&CfgHostAcq);
#endif
            }
        }
    }

    /* Drop all old data. */
    drvAudioStreamDropInternal(pThis, pStream);

    /*
     * Restore previous stream state.
     */
    if (fIsEnabled)
        rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_ENABLE);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Re-initializing stream '%s' failed with %Rrc\n", pStream->szName, rc));

    LogFunc(("[%s] Returning %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * Drops all audio data (and associated state) of a stream.
 *
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to drop data for.
 */
static void drvAudioStreamDropInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pThis);

    LogFunc(("[%s]\n", pStream->szName));

    AudioMixBufReset(&pStream->Guest.MixBuf);
    AudioMixBufReset(&pStream->Host.MixBuf);

    pStream->tsLastIteratedNs       = 0;
    pStream->tsLastPlayedCapturedNs = 0;
    pStream->tsLastReadWrittenNs    = 0;

    pStream->fThresholdReached = false;
}

/**
 * Resets a given audio stream.
 *
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to reset.
 */
static void drvAudioStreamResetInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    drvAudioStreamDropInternal(pThis, pStream);

    LogFunc(("[%s]\n", pStream->szName));

    pStream->fStatus        = PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;
    pStream->fWarningsShown = PDMAUDIOSTREAM_WARN_FLAGS_NONE;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Reset statistics.
     */
    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
        STAM_COUNTER_RESET(&pStream->In.Stats.TotalFramesCaptured);
        STAM_COUNTER_RESET(&pStream->In.Stats.TotalFramesRead);
        STAM_COUNTER_RESET(&pStream->In.Stats.TotalTimesCaptured);
        STAM_COUNTER_RESET(&pStream->In.Stats.TotalTimesRead);
    }
    else if (pStream->enmDir == PDMAUDIODIR_OUT)
    {
        STAM_COUNTER_RESET(&pStream->Out.Stats.TotalFramesPlayed);
        STAM_COUNTER_RESET(&pStream->Out.Stats.TotalFramesWritten);
        STAM_COUNTER_RESET(&pStream->Out.Stats.TotalTimesPlayed);
        STAM_COUNTER_RESET(&pStream->Out.Stats.TotalTimesWritten);
    }
    else
        AssertFailed();
#endif
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamWrite}
 */
static DECLCALLBACK(int) drvAudioStreamWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                             const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbWritten, VERR_INVALID_PARAMETER);

    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be written to (direction is '%s')\n",
               pStream->szName, PDMAudioDirGetName(pStream->enmDir)));

    AssertMsg(PDMAudioPropsIsSizeAligned(&pStream->Guest.Cfg.Props, cbBuf),
              ("Stream '%s' got a non-frame-aligned write (%RU32 bytes)\n", pStream->szName, cbBuf));

    uint32_t cbWrittenTotal = 0;

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_STATISTICS
    STAM_PROFILE_ADV_START(&pThis->Stats.DelayOut, out);
#endif

    /* Whether to discard the incoming data or not. */
    bool fToBitBucket = false;

    do
    {
        if (!PDMAudioStrmStatusIsReady(pStream->fStatus))
        {
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        /* If output is disabled on a per-driver level, send data to the bit bucket instead. See @bugref{9882}. */
        if (!pThis->Out.fEnabled)
        {
            fToBitBucket = true;
            break;
        }

        if (pThis->pHostDrvAudio)
        {
            /* If the backend's stream is not writable, all written data goes to /dev/null. */
            if (!PDMAudioStrmStatusCanWrite(
                pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pStream->pvBackend)))
            {
                fToBitBucket = true;
                break;
            }
        }

        const uint32_t cbFree = AudioMixBufFreeBytes(&pStream->Host.MixBuf);
        if (cbFree < cbBuf)
            LogRel2(("Audio: Lost audio output (%RU64ms, %RU32 free but needs %RU32) due to full host stream buffer '%s'\n",
                     PDMAudioPropsBytesToMilli(&pStream->Host.Cfg.Props, cbBuf - cbFree), cbFree, cbBuf, pStream->szName));

        uint32_t cbToWrite = RT_MIN(cbBuf, cbFree);
        if (!cbToWrite)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        /* We use the guest side mixing buffer as an intermediate buffer to do some
         * (first) processing (if needed), so always write the incoming data at offset 0. */
        uint32_t cfGstWritten = 0;
        rc = AudioMixBufWriteAt(&pStream->Guest.MixBuf, 0 /* offFrames */, pvBuf, cbToWrite, &cfGstWritten);
        if (   RT_FAILURE(rc)
            || !cfGstWritten)
        {
            AssertMsgFailed(("[%s] Write failed: cbToWrite=%RU32, cfWritten=%RU32, rc=%Rrc\n",
                             pStream->szName, cbToWrite, cfGstWritten, rc));
            break;
        }

        if (pThis->Out.Cfg.Dbg.fEnabled)
            AudioHlpFileWrite(pStream->Out.Dbg.pFileStreamWrite, pvBuf, cbToWrite, 0 /* fFlags */);

        uint32_t cfGstMixed = 0;
        if (cfGstWritten)
        {
            int rc2 = AudioMixBufMixToParentEx(&pStream->Guest.MixBuf, 0 /* cSrcOffset */, cfGstWritten /* cSrcFrames */,
                                               &cfGstMixed /* pcSrcMixed */);
            if (RT_FAILURE(rc2))
            {
                AssertMsgFailed(("[%s] Mixing failed: cbToWrite=%RU32, cfWritten=%RU32, cfMixed=%RU32, rc=%Rrc\n",
                                 pStream->szName, cbToWrite, cfGstWritten, cfGstMixed, rc2));
            }
            else
            {
                const uint64_t tsNowNs = RTTimeNanoTS();

                Log3Func(("[%s] Writing %RU32 frames (%RU64ms)\n",
                          pStream->szName, cfGstWritten, PDMAudioPropsFramesToMilli(&pStream->Guest.Cfg.Props, cfGstWritten)));

                Log3Func(("[%s] Last written %RU64ns (%RU64ms), now filled with %RU64ms -- %RU8%%\n",
                          pStream->szName, tsNowNs - pStream->tsLastReadWrittenNs,
                          (tsNowNs - pStream->tsLastReadWrittenNs) / RT_NS_1MS,
                          PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, AudioMixBufUsed(&pStream->Host.MixBuf)),
                          AudioMixBufUsed(&pStream->Host.MixBuf) * 100 / AudioMixBufSize(&pStream->Host.MixBuf)));

                pStream->tsLastReadWrittenNs = tsNowNs;
                /* Keep going. */
            }

            if (RT_SUCCESS(rc))
                rc = rc2;

            cbWrittenTotal = AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfGstWritten);

#ifdef VBOX_WITH_STATISTICS
            STAM_COUNTER_ADD(&pThis->Stats.TotalFramesWritten,  cfGstWritten);
            STAM_COUNTER_ADD(&pThis->Stats.TotalFramesMixedOut, cfGstMixed);
            Assert(cfGstWritten >= cfGstMixed);
            STAM_COUNTER_ADD(&pThis->Stats.TotalFramesLostOut,  cfGstWritten - cfGstMixed);
            STAM_COUNTER_ADD(&pThis->Stats.TotalBytesWritten,   cbWrittenTotal);

            STAM_COUNTER_ADD(&pStream->Out.Stats.TotalFramesWritten, cfGstWritten);
            STAM_COUNTER_INC(&pStream->Out.Stats.TotalTimesWritten);
#endif
        }

        Log3Func(("[%s] Dbg: cbBuf=%RU32, cbToWrite=%RU32, cfHstUsed=%RU32, cfHstfLive=%RU32, cfGstWritten=%RU32, "
                  "cfGstMixed=%RU32, cbWrittenTotal=%RU32, rc=%Rrc\n",
                  pStream->szName, cbBuf, cbToWrite, AudioMixBufUsed(&pStream->Host.MixBuf),
                  AudioMixBufLive(&pStream->Host.MixBuf), cfGstWritten, cfGstMixed, cbWrittenTotal, rc));

    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_SUCCESS(rc))
    {
        if (fToBitBucket)
        {
            Log3Func(("[%s] Backend stream not ready (yet), discarding written data\n", pStream->szName));
            cbWrittenTotal = cbBuf; /* Report all data as being written to the caller. */
        }

        if (pcbWritten)
            *pcbWritten = cbWrittenTotal;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRetain}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRetain(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, UINT32_MAX);
   AssertPtrReturn(pStream,    UINT32_MAX);

   NOREF(pInterface);

   return ++pStream->cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRelease}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRelease(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, UINT32_MAX);
   AssertPtrReturn(pStream,    UINT32_MAX);

   NOREF(pInterface);

   if (pStream->cRefs > 1) /* 1 reference always is kept by this audio driver. */
       pStream->cRefs--;

   return pStream->cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioStreamIterate(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = drvAudioStreamIterateInternal(pThis, pStream);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Re-initializes the given stream if it is scheduled for this operation.
 *
 * @note This caller must have entered the critical section of the driver instance,
 *       needed for the host device (re-)enumeration.
 *
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to check and maybe re-initialize.
 */
static void drvAudioStreamMaybeReInit(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    if (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT)
    {
        const unsigned cMaxTries = 3; /** @todo Make this configurable? */
        const uint64_t tsNowNs   = RTTimeNanoTS();

        /* Throttle re-initializing streams on failure. */
        if (   pStream->cTriesReInit < cMaxTries
            && tsNowNs - pStream->tsLastReInitNs >= RT_NS_1SEC * pStream->cTriesReInit) /** @todo Ditto. */
        {
#ifdef VBOX_WITH_AUDIO_ENUM
            if (pThis->fEnumerateDevices)
            {
                /* Make sure to leave the driver's critical section before enumerating host stuff. */
                int rc2 = RTCritSectLeave(&pThis->CritSect);
                AssertRC(rc2);

                /* Re-enumerate all host devices. */
                drvAudioDevicesEnumerateInternal(pThis, true /* fLog */, NULL /* pDevEnum */);

                /* Re-enter the critical section again. */
                rc2 = RTCritSectEnter(&pThis->CritSect);
                AssertRC(rc2);

                pThis->fEnumerateDevices = false;
            }
#endif /* VBOX_WITH_AUDIO_ENUM */

            int rc = drvAudioStreamReInitInternal(pThis, pStream);
            if (RT_FAILURE(rc))
            {
                pStream->cTriesReInit++;
                pStream->tsLastReInitNs = tsNowNs;
            }
            else
            {
                /* Remove the pending re-init flag on success. */
                pStream->fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT;
            }
        }
        else
        {
            /* Did we exceed our tries re-initializing the stream?
             * Then this one is dead-in-the-water, so disable it for further use. */
            if (pStream->cTriesReInit == cMaxTries)
            {
                LogRel(("Audio: Re-initializing stream '%s' exceeded maximum retries (%u), leaving as disabled\n",
                        pStream->szName, cMaxTries));

                /* Don't try to re-initialize anymore and mark as disabled. */
                pStream->fStatus &= ~(PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT | PDMAUDIOSTREAMSTS_FLAGS_ENABLED);

                /* Note: Further writes to this stream go to / will be read from the bit bucket (/dev/null) from now on. */
            }
        }

#ifdef LOG_ENABLED
        char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
        Log3Func(("[%s] fStatus=%s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));
    }
}

/**
 * Does one iteration of an audio stream.
 * This function gives the backend the chance of iterating / altering data and
 * does the actual mixing between the guest <-> host mixing buffers.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to iterate.
 */
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pThis->pHostDrvAudio)
        return VINF_SUCCESS;

    if (!pStream)
        return VINF_SUCCESS;

    /* Is the stream scheduled for re-initialization? Do so now. */
    drvAudioStreamMaybeReInit(pThis, pStream);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] fStatus=%s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));

    /* Not enabled or paused? Skip iteration. */
    if (   !(pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED)
        ||  (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED))
    {
        return VINF_SUCCESS;
    }

    /* Whether to try closing a pending to close stream. */
    bool fTryClosePending = false;

    int rc;

    do
    {
        rc = pThis->pHostDrvAudio->pfnStreamIterate(pThis->pHostDrvAudio, pStream->pvBackend);
        if (RT_FAILURE(rc))
            break;

        if (pStream->enmDir == PDMAUDIODIR_OUT)
        {
            /* No audio frames to transfer from guest to host (anymore)?
             * Then try closing this stream if marked so in the next block. */
            const uint32_t cFramesLive = AudioMixBufLive(&pStream->Host.MixBuf);
            fTryClosePending = cFramesLive == 0;
            Log3Func(("[%s] fTryClosePending=%RTbool, cFramesLive=%RU32\n", pStream->szName, fTryClosePending, cFramesLive));
        }

        /* Has the host stream marked as pending to disable?
         * Try disabling the stream then. */
        if (   pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE
            && fTryClosePending)
        {
            /* Tell the backend to drain the stream, that is, play the remaining (buffered) data
             * on the backend side. */
            rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DRAIN);
            if (rc == VERR_NOT_SUPPORTED) /* Not all backends support draining. */
                rc = VINF_SUCCESS;

            if (RT_SUCCESS(rc))
            {
                if (pThis->pHostDrvAudio->pfnStreamGetPending) /* Optional to implement. */
                {
                    const uint32_t cxPending = pThis->pHostDrvAudio->pfnStreamGetPending(pThis->pHostDrvAudio, pStream->pvBackend);
                    Log3Func(("[%s] cxPending=%RU32\n", pStream->szName, cxPending));

                    /* Only try close pending if no audio data is pending on the backend-side anymore. */
                    fTryClosePending = cxPending == 0;
                }

                if (fTryClosePending)
                {
                    LogFunc(("[%s] Closing pending stream\n", pStream->szName));
                    rc = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                    {
                        pStream->fStatus &= ~(PDMAUDIOSTREAMSTS_FLAGS_ENABLED | PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE);
                        drvAudioStreamDropInternal(pThis, pStream);
                    }
                    else
                       LogFunc(("[%s] Backend vetoed against closing pending input stream, rc=%Rrc\n", pStream->szName, rc));
                }
            }
        }

    } while (0);

    /* Update timestamps. */
    pStream->tsLastIteratedNs = RTTimeNanoTS();

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n",  pStream->szName, rc));

    return rc;
}

/**
 * Worker for drvAudioStreamPlay that does the actual playing.
 *
 * @returns VBox status code.
 * @param   pThis           The audio driver instance data.
 * @param   pStream         The stream to play.
 * @param   cFramesToPlay   Number of audio frames to play.  The backend is
 *                          supposed to have buffer space for this.
 * @param   pcFramesPlayed  Where to return the number of audio frames played.
 */
static int drvAudioStreamPlayDoIt(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, uint32_t cFramesToPlay, uint32_t *pcFramesPlayed)
{
    Assert(pStream->enmDir == PDMAUDIODIR_OUT);

    /*
     * Push data to the host device.
     */
    int      rc          = VINF_SUCCESS;
    uint32_t cFramesLeft = cFramesToPlay;
    while (cFramesLeft > 0)
    {
        /*
         * Grab a chunk of audio data in the backend format.
         */
        uint8_t  abChunk[_4K];
        uint32_t cFramesRead = 0;
        rc = AudioMixBufAcquireReadBlock(&pStream->Host.MixBuf, abChunk,
                                         RT_MIN(sizeof(abChunk), AUDIOMIXBUF_F2B(&pStream->Host.MixBuf, cFramesLeft)),
                                         &cFramesRead);
        AssertRCBreak(rc);

        uint32_t cbRead = AUDIOMIXBUF_F2B(&pStream->Host.MixBuf, cFramesRead);
        Assert(cbRead <= sizeof(abChunk));

        /*
         * Feed it to the backend.
         */
        uint32_t cFramesPlayed = 0;
        uint32_t cbPlayed      = 0;
        rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pStream->pvBackend, abChunk, cbRead, &cbPlayed);
        if (RT_SUCCESS(rc))
        {
            if (pThis->Out.Cfg.Dbg.fEnabled)
                AudioHlpFileWrite(pStream->Out.Dbg.pFilePlayNonInterleaved, abChunk, cbPlayed, 0 /* fFlags */);

            if (cbRead != cbPlayed)
                LogRel2(("Audio: Host stream '%s' played wrong amount (%RU32 bytes read but played %RU32)\n",
                         pStream->szName, cbRead, cbPlayed));

            cFramesPlayed  = AUDIOMIXBUF_B2F(&pStream->Host.MixBuf, cbPlayed);
            AssertStmt(cFramesLeft >= cFramesPlayed, cFramesPlayed = cFramesLeft);
            cFramesLeft   -= cFramesPlayed;
        }

        AudioMixBufReleaseReadBlock(&pStream->Host.MixBuf, cFramesPlayed);

        AssertRCBreak(rc); /* (this is here for Acquire/Release symmetry - which isn't at all necessary) */
        AssertBreak(cbPlayed > 0); /* (ditto) */
    }

    Log3Func(("[%s] Played %RU32/%RU32 frames, rc=%Rrc\n", pStream->szName, cFramesToPlay - cFramesLeft, cFramesToPlay, rc));
    *pcFramesPlayed = cFramesToPlay - cFramesLeft;
    return rc;
}

/**
 * Worker for drvAudioStreamPlay.
 */
static int drvAudioStreamPlayLocked(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, uint32_t *pcFramesPlayed)
{
    /*
     * Zero the frame count so we can return at will.
     */
    *pcFramesPlayed = 0;

    PDMAUDIOSTREAMSTS fStrmStatus = pStream->fStatus;
#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] Start fStatus=%s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, fStrmStatus)));

    /*
     * Operational?
     */
    if (pThis->pHostDrvAudio)
    { /* likely? */ }
    else
        return VERR_PDM_NO_ATTACHED_DRIVER;

    if (   pThis->Out.fEnabled
        && PDMAudioStrmStatusIsReady(fStrmStatus))
    { /* likely? */ }
    else
        return VERR_AUDIO_STREAM_NOT_READY;

    /*
     * Get number of frames in the mix buffer and do some logging.
     */
    uint32_t const cFramesLive = AudioMixBufLive(&pStream->Host.MixBuf);
    Log3Func(("[%s] Last played %'RI64 ns ago; filled with %u frm / %RU64 ms / %RU8%% total%s\n",
              pStream->szName, pStream->fThresholdReached ? RTTimeNanoTS() - pStream->tsLastPlayedCapturedNs : -1, cFramesLive,
              PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, cFramesLive),
              (100 * cFramesLive) / AudioMixBufSize(&pStream->Host.MixBuf), pStream->fThresholdReached ? "" : ", pre-buffering"));

    /*
     * Restart pre-buffering if we're having a buffer-underrun.
     */
    if (   cFramesLive != 0                     /* no underrun */
        || !pStream->fThresholdReached          /* or still pre-buffering. */)
    { /* likely */ }
    else
    {
        /* It's not an underrun if the host audio driver still has an reasonable amount
           buffered.  We don't have a direct way of querying that, so instead we'll use
           some heuristics based on number of writable bytes now compared to when
           prebuffering ended the first time around. */
        uint32_t cbBuffered = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStream->pvBackend);
        if (cbBuffered < pStream->Out.cbBackendMaxWritable)
            cbBuffered = pStream->Out.cbBackendMaxWritable - cbBuffered;
        else
            cbBuffered = 0;
        uint32_t cbMinBuf = PDMAudioPropsMilliToBytes(&pStream->Host.Cfg.Props, pStream->Guest.Cfg.Device.cMsSchedulingHint * 2);
        Log3Func(("Potential underrun: cbBuffered=%#x vs cbMinBuf=%#x\n", cbBuffered, cbMinBuf));
        if (cbBuffered < cbMinBuf)
        {
            LogRel2(("Audio: Buffer underrun for stream '%s' (%RI64 ms since last call, %u buffered)\n",
                     pStream->szName, RTTimeNanoTS() - pStream->tsLastPlayedCapturedNs, cbBuffered));

            /* Re-enter the pre-buffering stage again if enabled. */
            if (pStream->Host.Cfg.Backend.cFramesPreBuffering > 0)
            {
                pStream->fThresholdReached = false;
                STAM_REL_COUNTER_INC(&pThis->Out.StatsReBuffering);
            }
        }
    }

    /*
     * Work the pre-buffering.
     *
     * This is straight forward, the backend
     */
    uint32_t cbWritable;
    bool fJustStarted = false;
    if (pStream->fThresholdReached)
    {
        /* not-prebuffering, likely after a while at least */
        cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStream->pvBackend);
    }
    else
    {
        /*
         * Did we reach the backend's playback (pre-buffering) threshold?
         * Can be 0 if no pre-buffering desired.
         */
        if (cFramesLive >= pStream->Host.Cfg.Backend.cFramesPreBuffering)
        {
            LogRel2(("Audio: Stream '%s' buffering complete!\n", pStream->szName));
            pStream->fThresholdReached = fJustStarted = true;
        }
        /*
         * Some audio files are shorter than the pre-buffering level (e.g. the
         * "click" Explorer sounds on some Windows guests), so make sure that we
         * also play those by checking if the stream already is pending disable
         * mode, even if we didn't hit the pre-buffering watermark yet.
         *
         * Try play "Windows Navigation Start.wav" on Windows 7 (2824 samples).
         */
        else if (   cFramesLive > 0
                 && (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE))
        {
            LogRel2(("Audio: Stream '%s' buffering complete (short sound)!\n", pStream->szName));
            pStream->fThresholdReached = fJustStarted = true;
        }
        /*
         * Not yet, so still buffering audio data.
         */
        else
        {
            LogRel2(("Audio: Stream '%s' is buffering (%RU8%% complete)...\n",
                     pStream->szName, (100 * cFramesLive) / pStream->Host.Cfg.Backend.cFramesPreBuffering));
            return VINF_SUCCESS;
        }

        /* Hack alert! This is for the underrun detection.  */
        cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStream->pvBackend);
        if (cbWritable > pStream->Out.cbBackendMaxWritable)
            pStream->Out.cbBackendMaxWritable = cbWritable;
    }

    /*
     * Figure out how much to play now.
     * Easy, as much as the host audio backend will allow us to.
     */
    uint32_t cFramesWritable = PDMAUDIOPCMPROPS_B2F(&pStream->Host.Cfg.Props, cbWritable);
    uint32_t cFramesToPlay   = cFramesWritable;
    if (cFramesToPlay > cFramesLive) /* Don't try to play more than available, we don't want to block. */
        cFramesToPlay = cFramesLive;

    Log3Func(("[%s] Playing %RU32 frames (%RU64 ms), now filled with %RU64 ms -- %RU8%%\n",
              pStream->szName, cFramesToPlay, PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, cFramesToPlay),
              PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, AudioMixBufUsed(&pStream->Host.MixBuf)),
              AudioMixBufUsed(&pStream->Host.MixBuf) * 100 / AudioMixBufSize(&pStream->Host.MixBuf)));

    /*
     * Do the playing if we decided to play something.
     */
    int rc;
    if (cFramesToPlay)
    {
        if (pThis->pHostDrvAudio->pfnStreamPlayBegin)
            pThis->pHostDrvAudio->pfnStreamPlayBegin(pThis->pHostDrvAudio, pStream->pvBackend);

        rc = drvAudioStreamPlayDoIt(pThis, pStream, cFramesToPlay, pcFramesPlayed);

        if (pThis->pHostDrvAudio->pfnStreamPlayEnd)
            pThis->pHostDrvAudio->pfnStreamPlayEnd(pThis->pHostDrvAudio, pStream->pvBackend);

        pStream->tsLastPlayedCapturedNs = RTTimeNanoTS();
    }
    else
        rc = VINF_SUCCESS;

    Log3Func(("[%s] Live=%RU32 fr (%RU64 ms) Period=%RU32 fr (%RU64 ms) Writable=%RU32 fr (%RU64 ms) -> ToPlay=%RU32 fr (%RU64 ms) Played=%RU32 fr (%RU64 ms)%s\n",
              pStream->szName,
              cFramesLive, PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, cFramesLive),
              pStream->Host.Cfg.Backend.cFramesPeriod,
              PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, pStream->Host.Cfg.Backend.cFramesPeriod),
              cFramesWritable, PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, cFramesWritable),
              cFramesToPlay, PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, cFramesToPlay),
              *pcFramesPlayed, PDMAudioPropsFramesToMilli(&pStream->Host.Cfg.Props, *pcFramesPlayed),
              fJustStarted ? "just-started" : ""));
    RT_NOREF(fJustStarted);

    if (RT_SUCCESS(rc))
    {
        AudioMixBufFinish(&pStream->Host.MixBuf, *pcFramesPlayed);

        STAM_PROFILE_ADV_STOP(&pThis->Stats.DelayOut, out);
        STAM_COUNTER_ADD(&pThis->Stats.TotalFramesOut, *pcFramesPlayed);
        STAM_COUNTER_ADD(&pStream->Out.Stats.TotalFramesPlayed, *pcFramesPlayed);
        STAM_COUNTER_INC(&pStream->Out.Stats.TotalTimesPlayed);
    }
    return rc;
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioStreamPlay(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcFramesPlayed)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pcFramesPlayed, VERR_INVALID_POINTER);
    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be played back (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    uint32_t cFramesPlayed = 0;
    rc = drvAudioStreamPlayLocked(pThis, pStream, &cFramesPlayed);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_SUCCESS(rc) && pcFramesPlayed)
        *pcFramesPlayed = cFramesPlayed;

    if (RT_FAILURE(rc))
        LogFlowFunc(("[%s] Failed with %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * Captures non-interleaved input from a host stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pStream             Stream to capture from.
 * @param   pcfCaptured         Number of (host) audio frames captured. Optional.
 */
static int drvAudioStreamCaptureNonInterleaved(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, uint32_t *pcfCaptured)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pcfCaptured, VERR_INVALID_POINTER);
    Assert(pStream->enmDir == PDMAUDIODIR_IN);
    Assert(pStream->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED);

    /*
     * ...
     */
    AssertPtr(pThis->pHostDrvAudio->pfnStreamGetReadable);
    uint32_t cbReadable = pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStream->pvBackend);
    if (!cbReadable)
        Log2Func(("[%s] No readable data available\n", pStream->szName));

    uint32_t cbFree = AudioMixBufFreeBytes(&pStream->Guest.MixBuf); /* Parent */
    if (!cbFree)
        Log2Func(("[%s] Buffer full\n", pStream->szName));

    if (cbReadable > cbFree) /* More data readable than we can store at the moment? Limit. */
        cbReadable = cbFree;

    /*
     * ...
     */
    int      rc = VINF_SUCCESS;
    uint32_t cfCapturedTotal = 0;
    while (cbReadable)
    {
        uint8_t  abChunk[_4K];
        uint32_t cbCaptured;
        rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pStream->pvBackend,
                                                    abChunk, RT_MIN(cbReadable, (uint32_t)sizeof(abChunk)), &cbCaptured);
        if (RT_FAILURE(rc))
        {
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
            AssertRC(rc2);
            break;
        }

        Assert(cbCaptured <= sizeof(abChunk));
        if (cbCaptured > sizeof(abChunk)) /* Paranoia. */
            cbCaptured = (uint32_t)sizeof(abChunk);

        if (!cbCaptured) /* Nothing captured? Take a shortcut. */
            break;

        /* We use the host side mixing buffer as an intermediate buffer to do some
         * (first) processing (if needed), so always write the incoming data at offset 0. */
        uint32_t cfHstWritten = 0;
        rc = AudioMixBufWriteAt(&pStream->Host.MixBuf, 0 /* offFrames */, abChunk, cbCaptured, &cfHstWritten);
        if (   RT_FAILURE(rc)
            || !cfHstWritten)
        {
            AssertMsgFailed(("[%s] Write failed: cbCaptured=%RU32, cfHstWritten=%RU32, rc=%Rrc\n",
                             pStream->szName, cbCaptured, cfHstWritten, rc));
            break;
        }

        if (pThis->In.Cfg.Dbg.fEnabled)
            AudioHlpFileWrite(pStream->In.Dbg.pFileCaptureNonInterleaved, abChunk, cbCaptured, 0 /* fFlags */);

        uint32_t cfHstMixed = 0;
        if (cfHstWritten)
        {
            int rc2 = AudioMixBufMixToParentEx(&pStream->Host.MixBuf, 0 /* cSrcOffset */, cfHstWritten /* cSrcFrames */,
                                               &cfHstMixed /* pcSrcMixed */);
            Log3Func(("[%s] cbCaptured=%RU32, cfWritten=%RU32, cfMixed=%RU32, rc=%Rrc\n",
                      pStream->szName, cbCaptured, cfHstWritten, cfHstMixed, rc2));
            AssertRC(rc2);
        }

        Assert(cbReadable >= cbCaptured);
        cbReadable      -= cbCaptured;
        cfCapturedTotal += cfHstMixed;
    }

    if (RT_SUCCESS(rc))
    {
        if (cfCapturedTotal)
            Log2Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStream->szName, cfCapturedTotal, rc));
    }
    else
        LogFunc(("[%s] Capturing failed with rc=%Rrc\n", pStream->szName, rc));

    if (pcfCaptured)
        *pcfCaptured = cfCapturedTotal;

    return rc;
}

/**
 * Captures raw input from a host stream.
 * Raw input means that the backend directly operates on PDMAUDIOFRAME structs without
 * no data layout processing done in between.
 *
 * Needed for e.g. the VRDP audio backend (in Main).
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pStream             Stream to capture from.
 * @param   pcfCaptured         Number of (host) audio frames captured. Optional.
 */
static int drvAudioStreamCaptureRaw(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, uint32_t *pcfCaptured)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pcfCaptured, VERR_INVALID_POINTER);
    Assert(pStream->enmDir == PDMAUDIODIR_IN);
    Assert(pStream->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_RAW);
    AssertPtr(pThis->pHostDrvAudio->pfnStreamGetReadable);

    /*
     * ...
     */
    /* Note: Raw means *audio frames*, not bytes! */
    uint32_t cfReadable = pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStream->pvBackend);
    if (!cfReadable)
        Log2Func(("[%s] No readable data available\n", pStream->szName));

    const uint32_t cfFree = AudioMixBufFree(&pStream->Guest.MixBuf); /* Parent */
    if (!cfFree)
        Log2Func(("[%s] Buffer full\n", pStream->szName));

    if (cfReadable > cfFree) /* More data readable than we can store at the moment? Limit. */
        cfReadable = cfFree;

    /*
     * ...
     */
    int      rc              = VINF_SUCCESS;
    uint32_t cfCapturedTotal = 0;
    while (cfReadable)
    {
        PPDMAUDIOFRAME paFrames;
        uint32_t cfWritable;
        rc = AudioMixBufPeekMutable(&pStream->Host.MixBuf, cfReadable, &paFrames, &cfWritable);
        if (   RT_FAILURE(rc)
            || !cfWritable)
            break;

        uint32_t cfCaptured;
        rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pStream->pvBackend,
                                                    paFrames, cfWritable, &cfCaptured);
        if (RT_FAILURE(rc))
        {
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
            AssertRC(rc2);
            break;
        }

        Assert(cfCaptured <= cfWritable);
        if (cfCaptured > cfWritable) /* Paranoia. */
            cfCaptured = cfWritable;

        Assert(cfReadable >= cfCaptured);
        cfReadable      -= cfCaptured;
        cfCapturedTotal += cfCaptured;
    }

    if (pcfCaptured)
        *pcfCaptured = cfCapturedTotal;
    Log2Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStream->szName, cfCapturedTotal, rc));
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioStreamCapture(PPDMIAUDIOCONNECTOR pInterface,
                                               PPDMAUDIOSTREAM pStream, uint32_t *pcFramesCaptured)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtr(pStream);
    AssertPtrNull(pcFramesCaptured);
    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be captured (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] fStatus=%s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));

    /*
     * ...
     */
    uint32_t cfCaptured = 0;
    do
    {
        if (!pThis->pHostDrvAudio)
        {
            rc = VERR_PDM_NO_ATTACHED_DRIVER;
            break;
        }

        if (   !pThis->In.fEnabled
            || !PDMAudioStrmStatusCanRead(pStream->fStatus))
        {
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        /*
         * Do the actual capturing.
         */
        if (pThis->pHostDrvAudio->pfnStreamCaptureBegin)
            pThis->pHostDrvAudio->pfnStreamCaptureBegin(pThis->pHostDrvAudio, pStream->pvBackend);

        if (RT_LIKELY(pStream->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED))
            rc = drvAudioStreamCaptureNonInterleaved(pThis, pStream, &cfCaptured);
        else if (pStream->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_RAW)
            rc = drvAudioStreamCaptureRaw(pThis, pStream, &cfCaptured);
        else
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

        if (pThis->pHostDrvAudio->pfnStreamCaptureEnd)
            pThis->pHostDrvAudio->pfnStreamCaptureEnd(pThis->pHostDrvAudio, pStream->pvBackend);

        if (RT_SUCCESS(rc))
        {
            Log3Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStream->szName, cfCaptured, rc));

#ifdef VBOX_WITH_STATISTICS
            STAM_COUNTER_ADD(&pThis->Stats.TotalFramesIn,            cfCaptured);

            STAM_COUNTER_ADD(&pStream->In.Stats.TotalFramesCaptured, cfCaptured);
#endif
        }
        else if (RT_UNLIKELY(RT_FAILURE(rc)))
            LogRel(("Audio: Capturing stream '%s' failed with %Rrc\n", pStream->szName, rc));
    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (pcFramesCaptured)
        *pcFramesCaptured = cfCaptured;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS

/**
 * Duplicates an audio callback.
 *
 * @returns Pointer to duplicated callback, or NULL on failure.
 * @param   pCB                 Callback to duplicate.
 */
static PPDMAUDIOCBRECORD drvAudioCallbackDuplicate(PPDMAUDIOCBRECORD pCB)
{
    AssertPtrReturn(pCB, NULL);

    PPDMAUDIOCBRECORD pCBCopy = (PPDMAUDIOCBRECORD)RTMemDup((void *)pCB, sizeof(PDMAUDIOCBRECORD));
    if (!pCBCopy)
        return NULL;

    if (pCB->pvCtx)
    {
        pCBCopy->pvCtx = RTMemDup(pCB->pvCtx, pCB->cbCtx);
        if (!pCBCopy->pvCtx)
        {
            RTMemFree(pCBCopy);
            return NULL;
        }

        pCBCopy->cbCtx = pCB->cbCtx;
    }

    return pCBCopy;
}

/**
 * Destroys a given callback.
 *
 * @param   pCB                 Callback to destroy.
 */
static void drvAudioCallbackDestroy(PPDMAUDIOCBRECORD pCB)
{
    if (!pCB)
        return;

    RTListNodeRemove(&pCB->Node);
    if (pCB->pvCtx)
    {
        Assert(pCB->cbCtx);
        RTMemFree(pCB->pvCtx);
    }
    RTMemFree(pCB);
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnRegisterCallbacks}
 */
static DECLCALLBACK(int) drvAudioRegisterCallbacks(PPDMIAUDIOCONNECTOR pInterface,
                                                   PPDMAUDIOCBRECORD paCallbacks, size_t cCallbacks)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(paCallbacks, VERR_INVALID_POINTER);
    AssertReturn(cCallbacks,     VERR_INVALID_PARAMETER);
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     *
     */
    for (size_t i = 0; i < cCallbacks; i++)
    {
        PPDMAUDIOCBRECORD pCB = drvAudioCallbackDuplicate(&paCallbacks[i]);
        if (!pCB)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        switch (pCB->enmSource)
        {
            case PDMAUDIOCBSOURCE_DEVICE:
            {
                switch (pCB->Device.enmType)
                {
                    case PDMAUDIODEVICECBTYPE_DATA_INPUT:
                        RTListAppend(&pThis->In.lstCB, &pCB->Node);
                        break;

                    case PDMAUDIODEVICECBTYPE_DATA_OUTPUT:
                        RTListAppend(&pThis->Out.lstCB, &pCB->Node);
                        break;

                    default:
                        AssertMsgFailed(("Not supported\n"));
                        break;
                }

                break;
            }

            default:
               AssertMsgFailed(("Not supported\n"));
               break;
        }
    }

    /** @todo Undo allocations on error. */

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}

/**
 * @callback_method_impl{FNPDMHOSTAUDIOCALLBACK, Backend callback implementation.}
 *
 * @par Important:
 * No calls back to the backend within this function, as the backend
 * might hold any locks / critical sections while executing this
 * callback. Will result in some ugly deadlocks (or at least locking
 * order violations) then.
 */
static DECLCALLBACK(int) drvAudioBackendCallback(PPDMDRVINS pDrvIns, PDMAUDIOBACKENDCBTYPE enmType, void *pvUser, size_t cbUser)
{
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    RT_NOREF(pvUser, cbUser);
    /* pvUser and cbUser are optional. */

/** @todo r=bird: WTF *is* this?  Seriously?!? */

    /* Get the upper driver (PDMIAUDIOCONNECTOR). */
    AssertPtr(pDrvIns->pUpBase);
    PPDMIAUDIOCONNECTOR pInterface = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    AssertPtr(pInterface);
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    LogFunc(("pThis=%p, enmType=%RU32, pvUser=%p, cbUser=%zu\n", pThis, enmType, pvUser, cbUser));

    switch (enmType)
    {
        case PDMAUDIOBACKENDCBTYPE_DEVICES_CHANGED:
            LogRel(("Audio: Device configuration of driver '%s' has changed\n", pThis->szName));
            rc = drvAudioScheduleReInitInternal(pThis);
            break;

        default:
            AssertMsgFailed(("Not supported\n"));
            break;
    }

    RTCritSectLeave(&pThis->CritSect);

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}

#endif /* VBOX_WITH_AUDIO_CALLBACKS */

#ifdef VBOX_WITH_AUDIO_ENUM
/**
 * Enumerates all host audio devices.
 *
 * This functionality might not be implemented by all backends and will return
 * VERR_NOT_SUPPORTED if not being supported.
 *
 * @note Must not hold the driver's critical section!
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance to be called.
 * @param   fLog                Whether to print the enumerated device to the release log or not.
 * @param   pDevEnum            Where to store the device enumeration.
 */
static int drvAudioDevicesEnumerateInternal(PDRVAUDIO pThis, bool fLog, PPDMAUDIOHOSTENUM pDevEnum)
{
    AssertReturn(RTCritSectIsOwned(&pThis->CritSect) == false, VERR_WRONG_ORDER);

    int rc;

    /*
     * If the backend supports it, do a device enumeration.
     */
    if (pThis->pHostDrvAudio->pfnGetDevices)
    {
        PDMAUDIOHOSTENUM DevEnum;
        rc = pThis->pHostDrvAudio->pfnGetDevices(pThis->pHostDrvAudio, &DevEnum);
        if (RT_SUCCESS(rc))
        {
            if (fLog)
                LogRel(("Audio: Found %RU16 devices for driver '%s'\n", DevEnum.cDevices, pThis->szName));

            PPDMAUDIOHOSTDEV pDev;
            RTListForEach(&DevEnum.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
            {
                if (fLog)
                {
                    char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
                    LogRel(("Audio: Device '%s':\n", pDev->szName));
                    LogRel(("Audio:   Usage           = %s\n",   PDMAudioDirGetName(pDev->enmUsage)));
                    LogRel(("Audio:   Flags           = %s\n",   PDMAudioHostDevFlagsToString(szFlags, pDev->fFlags)));
                    LogRel(("Audio:   Input channels  = %RU8\n", pDev->cMaxInputChannels));
                    LogRel(("Audio:   Output channels = %RU8\n", pDev->cMaxOutputChannels));
                }
            }

            if (pDevEnum)
                rc = PDMAudioHostEnumCopy(pDevEnum, &DevEnum, PDMAUDIODIR_INVALID /*all*/, true /*fOnlyCoreData*/);

            PDMAudioHostEnumDelete(&DevEnum);
        }
        else
        {
            if (fLog)
                LogRel(("Audio: Device enumeration for driver '%s' failed with %Rrc\n", pThis->szName, rc));
            /* Not fatal. */
        }
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;

        if (fLog)
            LogRel2(("Audio: Host driver '%s' does not support audio device enumeration, skipping\n", pThis->szName));
    }

    LogFunc(("Returning %Rrc\n", rc));
    return rc;
}
#endif /* VBOX_WITH_AUDIO_ENUM */

/**
 * Initializes the host backend and queries its initial configuration.
 * If the host backend fails, VERR_AUDIO_BACKEND_INIT_FAILED will be returned.
 *
 * Note: As this routine is called when attaching to the device LUN in the
 *       device emulation, we either check for success or VERR_AUDIO_BACKEND_INIT_FAILED.
 *       Everything else is considered as fatal and must be handled separately in
 *       the device emulation!
 *
 * @return  IPRT status code.
 * @param   pThis               Driver instance to be called.
 */
static int drvAudioHostInit(PDRVAUDIO pThis)
{
    LogFlowFuncEnter();
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertPtr(pThis->pHostDrvAudio);
    int rc = pThis->pHostDrvAudio->pfnInit(pThis->pHostDrvAudio);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Initialization of host driver '%s' failed with %Rrc\n", pThis->szName, rc));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    /*
     * Get the backend configuration.
     */
    rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Getting configuration for driver '%s' failed with %Rrc\n", pThis->szName, rc));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    pThis->In.cStreamsFree  = pThis->BackendCfg.cMaxStreamsIn;
    pThis->Out.cStreamsFree = pThis->BackendCfg.cMaxStreamsOut;

    LogFlowFunc(("cStreamsFreeIn=%RU8, cStreamsFreeOut=%RU8\n", pThis->In.cStreamsFree, pThis->Out.cStreamsFree));

    LogRel2(("Audio: Host driver '%s' supports %RU32 input streams and %RU32 output streams at once\n",
             pThis->szName,
             /* Clamp for logging. Unlimited streams are defined by UINT32_MAX. */
             RT_MIN(64, pThis->In.cStreamsFree), RT_MIN(64, pThis->Out.cStreamsFree)));

#ifdef VBOX_WITH_AUDIO_ENUM
    int rc2 = drvAudioDevicesEnumerateInternal(pThis, true /* fLog */, NULL /* pDevEnum */);
    if (rc2 != VERR_NOT_SUPPORTED) /* Some backends don't implement device enumeration. */
        AssertRC(rc2);

    RT_NOREF(rc2);
    /* Ignore rc. */
#endif

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /*
     * If the backend supports it, offer a callback to this connector.
     */
/** @todo r=bird: Total misdesign.  If the backend wants to talk to us, it
 *                should use PDMIBASE_QUERY_INTERFACE to get an interface from us. */
    if (pThis->pHostDrvAudio->pfnSetCallback)
    {
        rc2 = pThis->pHostDrvAudio->pfnSetCallback(pThis->pHostDrvAudio, drvAudioBackendCallback);
        if (RT_FAILURE(rc2))
             LogRel(("Audio: Error registering callback for host driver '%s', rc=%Rrc\n", pThis->szName, rc2));
        /* Not fatal. */
    }
#endif

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Handles state changes for all audio streams.
 *
 * @param   pDrvIns             Pointer to driver instance.
 * @param   enmCmd              Stream command to set for all streams.
 */
static void drvAudioStateHandler(PPDMDRVINS pDrvIns, PDMAUDIOSTREAMCMD enmCmd)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlowFunc(("enmCmd=%s\n", PDMAudioStrmCmdGetName(enmCmd)));

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturnVoid(rc2);

    if (pThis->pHostDrvAudio)
    {
        PPDMAUDIOSTREAM pStream;
        RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, ListEntry)
            drvAudioStreamControlInternal(pThis, pStream, enmCmd);
    }

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRead}
 */
static DECLCALLBACK(int) drvAudioStreamRead(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                            void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,   VERR_INVALID_POINTER);
    AssertReturn(cbBuf,      VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbRead, VERR_INVALID_POINTER);
    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be read from (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * ...
     */
    uint32_t cbReadTotal = 0;

    do
    {
        uint32_t cfReadTotal = 0;

        const uint32_t cfBuf = AUDIOMIXBUF_B2F(&pStream->Guest.MixBuf, cbBuf);

        if (pThis->In.fEnabled) /* Input for this audio driver enabled? See #9822. */
        {
            if (!PDMAudioStrmStatusCanRead(pStream->fStatus))
            {
                rc = VERR_AUDIO_STREAM_NOT_READY;
                break;
            }

            /*
             * Read from the parent buffer (that is, the guest buffer) which
             * should have the audio data in the format the guest needs.
             */
            uint32_t cfToRead = RT_MIN(cfBuf, AudioMixBufLive(&pStream->Guest.MixBuf));
            while (cfToRead)
            {
                uint32_t cfRead;
                rc = AudioMixBufAcquireReadBlock(&pStream->Guest.MixBuf,
                                                 (uint8_t *)pvBuf + AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfReadTotal),
                                                 AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfToRead), &cfRead);
                if (RT_FAILURE(rc))
                    break;

#ifdef VBOX_WITH_STATISTICS
                const uint32_t cbRead = AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfRead);

                STAM_COUNTER_ADD(&pThis->Stats.TotalBytesRead,       cbRead);

                STAM_COUNTER_ADD(&pStream->In.Stats.TotalFramesRead, cfRead);
                STAM_COUNTER_INC(&pStream->In.Stats.TotalTimesRead);
#endif
                Assert(cfToRead >= cfRead);
                cfToRead -= cfRead;

                cfReadTotal += cfRead;

                AudioMixBufReleaseReadBlock(&pStream->Guest.MixBuf, cfRead);
            }

            if (cfReadTotal)
            {
                if (pThis->In.Cfg.Dbg.fEnabled)
                    AudioHlpFileWrite(pStream->In.Dbg.pFileStreamRead,
                                         pvBuf, AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfReadTotal), 0 /* fFlags */);

                AudioMixBufFinish(&pStream->Guest.MixBuf, cfReadTotal);
            }
        }

        /* If we were not able to read as much data as requested, fill up the returned
         * data with silence.
         *
         * This is needed to keep the device emulation DMA transfers up and running at a constant rate. */
        if (cfReadTotal < cfBuf)
        {
            Log3Func(("[%s] Filling in silence (%RU64ms / %RU64ms)\n", pStream->szName,
                      PDMAudioPropsFramesToMilli(&pStream->Guest.Cfg.Props, cfBuf - cfReadTotal),
                      PDMAudioPropsFramesToMilli(&pStream->Guest.Cfg.Props, cfBuf)));

            PDMAudioPropsClearBuffer(&pStream->Guest.Cfg.Props,
                                     (uint8_t *)pvBuf + AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfReadTotal),
                                     AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfBuf - cfReadTotal),
                                     cfBuf - cfReadTotal);

            cfReadTotal = cfBuf;
        }

        cbReadTotal = AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfReadTotal);

        pStream->tsLastReadWrittenNs = RTTimeNanoTS();

        Log3Func(("[%s] fEnabled=%RTbool, cbReadTotal=%RU32, rc=%Rrc\n", pStream->szName, pThis->In.fEnabled, cbReadTotal, rc));

    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_SUCCESS(rc) && pcbRead)
        *pcbRead = cbReadTotal;
    return rc;
}

/**
 * Worker for drvAudioStreamInitInternal and drvAudioStreamReInitInternal that
 * creates the backend (host driver) side of an audio stream.
 *
 * @returns VBox status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Audio stream to create the backend side for.
 * @param   pCfgReq             Requested audio stream configuration to use for stream creation.
 * @param   pCfgAcq             Acquired audio stream configuration returned by the backend.
 *
 * @note    Configuration precedence for requested audio stream configuration (first has highest priority, if set):
 *          - per global extra-data
 *          - per-VM extra-data
 *          - requested configuration (by pCfgReq)
 *          - default value
 */
static int drvAudioStreamCreateInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream,
                                               PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    AssertMsg((pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED) == 0,
              ("Stream '%s' already initialized in backend\n", pStream->szName));

    /* Get the right configuration for the stream to be created. */
    PDRVAUDIOCFG pDrvCfg = pCfgReq->enmDir == PDMAUDIODIR_IN ? &pThis->In.Cfg : &pThis->Out.Cfg;

    /* Fill in the tweakable parameters into the requested host configuration.
     * All parameters in principle can be changed and returned by the backend via the acquired configuration. */

    /*
     * PCM
     */
    if (PDMAudioPropsSampleSize(&pDrvCfg->Props) != 0) /* Anything set via custom extra-data? */
    {
        PDMAudioPropsSetSampleSize(&pCfgReq->Props, PDMAudioPropsSampleSize(&pDrvCfg->Props));
        LogRel2(("Audio: Using custom sample size of %RU8 bytes for stream '%s'\n",
                 PDMAudioPropsSampleSize(&pCfgReq->Props), pStream->szName));
    }

    if (pDrvCfg->Props.uHz) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.uHz = pDrvCfg->Props.uHz;
        LogRel2(("Audio: Using custom Hz rate %RU32 for stream '%s'\n", pCfgReq->Props.uHz, pStream->szName));
    }

    if (pDrvCfg->uSigned != UINT8_MAX) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.fSigned = RT_BOOL(pDrvCfg->uSigned);
        LogRel2(("Audio: Using custom %s sample format for stream '%s'\n",
                 pCfgReq->Props.fSigned ? "signed" : "unsigned", pStream->szName));
    }

    if (pDrvCfg->uSwapEndian != UINT8_MAX) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.fSwapEndian = RT_BOOL(pDrvCfg->uSwapEndian);
        LogRel2(("Audio: Using custom %s endianess for samples of stream '%s'\n",
                 pCfgReq->Props.fSwapEndian ? "swapped" : "original", pStream->szName));
    }

    if (PDMAudioPropsChannels(&pDrvCfg->Props) != 0) /* Anything set via custom extra-data? */
    {
        PDMAudioPropsSetChannels(&pCfgReq->Props, PDMAudioPropsChannels(&pDrvCfg->Props));
        LogRel2(("Audio: Using custom %RU8 channel(s) for stream '%s'\n", PDMAudioPropsChannels(&pDrvCfg->Props), pStream->szName));
    }

    /* Validate PCM properties. */
    if (!AudioHlpPcmPropsAreValid(&pCfgReq->Props))
    {
        LogRel(("Audio: Invalid custom PCM properties set for stream '%s', cannot create stream\n", pStream->szName));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Period size
     */
    const char *pszWhat = "device-specific";
    if (pDrvCfg->uPeriodSizeMs)
    {
        pCfgReq->Backend.cFramesPeriod = PDMAudioPropsMilliToFrames(&pCfgReq->Props, pDrvCfg->uPeriodSizeMs);
        pszWhat = "custom";
    }

    if (!pCfgReq->Backend.cFramesPeriod) /* Set default period size if nothing explicitly is set. */
    {
        pCfgReq->Backend.cFramesPeriod = PDMAudioPropsMilliToFrames(&pCfgReq->Props, 150 /*ms*/);
        pszWhat = "default";
    }

    LogRel2(("Audio: Using %s period size %RU64 ms / %RU32 frames for stream '%s'\n",
             pszWhat, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPeriod),
             pCfgReq->Backend.cFramesPeriod, pStream->szName));

    /*
     * Buffer size
     */
    pszWhat = "device-specific";
    if (pDrvCfg->uBufferSizeMs)
    {
        pCfgReq->Backend.cFramesBufferSize = PDMAudioPropsMilliToFrames(&pCfgReq->Props, pDrvCfg->uBufferSizeMs);
        pszWhat = "custom";
    }

    if (!pCfgReq->Backend.cFramesBufferSize) /* Set default buffer size if nothing explicitly is set. */
    {
        pCfgReq->Backend.cFramesBufferSize = PDMAudioPropsMilliToFrames(&pCfgReq->Props, 300 /*ms*/);
        pszWhat = "default";
    }

    LogRel2(("Audio: Using %s buffer size %RU64 ms / %RU32 frames for stream '%s'\n",
             pszWhat, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize),
             pCfgReq->Backend.cFramesBufferSize, pStream->szName));

    /*
     * Pre-buffering size
     */
    pszWhat = "device-specific";
    if (pDrvCfg->uPreBufSizeMs != UINT32_MAX) /* Anything set via global / per-VM extra-data? */
    {
        pCfgReq->Backend.cFramesPreBuffering = PDMAudioPropsMilliToFrames(&pCfgReq->Props, pDrvCfg->uPreBufSizeMs);
        pszWhat = "custom";
    }
    else /* No, then either use the default or device-specific settings (if any). */
    {
        if (pCfgReq->Backend.cFramesPreBuffering == UINT32_MAX) /* Set default pre-buffering size if nothing explicitly is set. */
        {
            /* For pre-buffering to finish the buffer at least must be full one time. */
            pCfgReq->Backend.cFramesPreBuffering = pCfgReq->Backend.cFramesBufferSize;
            pszWhat = "default";
        }
    }

    LogRel2(("Audio: Using %s pre-buffering size %RU64 ms / %RU32 frames for stream '%s'\n",
             pszWhat, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPreBuffering),
             pCfgReq->Backend.cFramesPreBuffering, pStream->szName));

    /*
     * Validate input.
     */
    if (pCfgReq->Backend.cFramesBufferSize < pCfgReq->Backend.cFramesPeriod)
    {
        LogRel(("Audio: Error for stream '%s': Buffering size (%RU64ms) must not be smaller than the period size (%RU64ms)\n",
                pStream->szName, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize),
                PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPeriod)));
        return VERR_INVALID_PARAMETER;
    }

    if (   pCfgReq->Backend.cFramesPreBuffering != UINT32_MAX /* Custom pre-buffering set? */
        && pCfgReq->Backend.cFramesPreBuffering)
    {
        if (pCfgReq->Backend.cFramesBufferSize < pCfgReq->Backend.cFramesPreBuffering)
        {
            LogRel(("Audio: Error for stream '%s': Buffering size (%RU64ms) must not be smaller than the pre-buffering size (%RU64ms)\n",
                    pStream->szName, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPreBuffering),
                    PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize)));
            return VERR_INVALID_PARAMETER;
        }
    }

    /*
     * Make the acquired host configuration the requested host configuration initially,
     * in case the backend does not report back an acquired configuration.
     */
    /** @todo r=bird: This is conveniently not documented in the interface... */
    int rc = PDMAudioStrmCfgCopy(pCfgAcq, pCfgReq);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Creating stream '%s' with an invalid backend configuration not possible, skipping\n",
                pStream->szName));
        return rc;
    }

    /*
     * Call the host driver to create the stream.
     */
    AssertPtr(pThis->pHostDrvAudio);
    if (pThis->pHostDrvAudio)
        rc = pThis->pHostDrvAudio->pfnStreamCreate(pThis->pHostDrvAudio, pStream->pvBackend, pCfgReq, pCfgAcq);
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NOT_SUPPORTED)
            LogRel2(("Audio: Creating stream '%s' in backend not supported\n", pStream->szName));
        else if (rc == VERR_AUDIO_STREAM_COULD_NOT_CREATE)
            LogRel2(("Audio: Stream '%s' could not be created in backend because of missing hardware / drivers\n", pStream->szName));
        else
            LogRel(("Audio: Creating stream '%s' in backend failed with %Rrc\n", pStream->szName, rc));
        return rc;
    }

    /* Validate acquired configuration. */
    char szTmp[PDMAUDIOPROPSTOSTRING_MAX];
    AssertLogRelMsgReturn(AudioHlpStreamCfgIsValid(pCfgAcq),
                          ("Audio: Creating stream '%s' returned an invalid backend configuration (%s), skipping\n",
                           pStream->szName, PDMAudioPropsToString(&pCfgAcq->Props, szTmp, sizeof(szTmp))),
                          VERR_INVALID_PARAMETER);

    /* Let the user know that the backend changed one of the values requested above. */
    if (pCfgAcq->Backend.cFramesBufferSize != pCfgReq->Backend.cFramesBufferSize)
        LogRel2(("Audio: Buffer size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                 pStream->szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesBufferSize), pCfgAcq->Backend.cFramesBufferSize));

    if (pCfgAcq->Backend.cFramesPeriod != pCfgReq->Backend.cFramesPeriod)
        LogRel2(("Audio: Period size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                 pStream->szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPeriod), pCfgAcq->Backend.cFramesPeriod));

    /* Was pre-buffering requested, but the acquired configuration from the backend told us something else? */
    if (pCfgReq->Backend.cFramesPreBuffering)
    {
        if (pCfgAcq->Backend.cFramesPreBuffering != pCfgReq->Backend.cFramesPreBuffering)
            LogRel2(("Audio: Pre-buffering size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                     pStream->szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPreBuffering), pCfgAcq->Backend.cFramesPreBuffering));

        if (pCfgAcq->Backend.cFramesPreBuffering > pCfgAcq->Backend.cFramesBufferSize)
        {
            pCfgAcq->Backend.cFramesPreBuffering = pCfgAcq->Backend.cFramesBufferSize;
            LogRel2(("Audio: Pre-buffering size bigger than buffer size for stream '%s', adjusting to %RU64ms (%RU32 frames)\n",
                     pStream->szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPreBuffering), pCfgAcq->Backend.cFramesPreBuffering));
        }
    }
    else if (pCfgReq->Backend.cFramesPreBuffering == 0) /* Was the pre-buffering requested as being disabeld? Tell the users. */
    {
        LogRel2(("Audio: Pre-buffering is disabled for stream '%s'\n", pStream->szName));
        pCfgAcq->Backend.cFramesPreBuffering = 0;
    }

    /* Sanity for detecting buggy backends. */
    AssertMsgReturn(pCfgAcq->Backend.cFramesPeriod < pCfgAcq->Backend.cFramesBufferSize,
                    ("Acquired period size must be smaller than buffer size\n"),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(pCfgAcq->Backend.cFramesPreBuffering <= pCfgAcq->Backend.cFramesBufferSize,
                    ("Acquired pre-buffering size must be smaller or as big as the buffer size\n"),
                    VERR_INVALID_PARAMETER);

    pStream->fStatus |= PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;

    return VINF_SUCCESS;
}


/**
 * Worker for drvAudioStreamCreate that initializes the audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to initialize.
 * @param   pCfgHost            Stream configuration to use for the host side (backend).
 * @param   pCfgGuest           Stream configuration to use for the guest side.
 */
static int drvAudioStreamInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream,
                                      PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest)
{
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgHost,  VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgGuest, VERR_INVALID_POINTER);

    /*
     * Init host stream.
     */
    pStream->uMagic = PDMAUDIOSTREAM_MAGIC;

    /* Set the host's default audio data layout. */
    pCfgHost->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

#ifdef LOG_ENABLED
    LogFunc(("[%s] Requested host format:\n", pStream->szName));
    PDMAudioStrmCfgLog(pCfgHost);
#endif

    LogRel2(("Audio: Creating stream '%s'\n", pStream->szName));
    LogRel2(("Audio: Guest %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             pCfgGuest->enmDir == PDMAUDIODIR_IN ? "recording" : "playback", pStream->szName,
             pCfgGuest->Props.uHz, PDMAudioPropsSampleBits(&pCfgGuest->Props), pCfgGuest->Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&pCfgGuest->Props), PDMAudioPropsChannels(&pCfgGuest->Props) == 1 ? "" : "s"));
    LogRel2(("Audio: Requested host %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             pCfgHost->enmDir == PDMAUDIODIR_IN ? "recording" : "playback", pStream->szName,
             pCfgHost->Props.uHz, PDMAudioPropsSampleBits(&pCfgHost->Props), pCfgHost->Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&pCfgHost->Props), PDMAudioPropsChannels(&pCfgHost->Props) == 1 ? "" : "s"));

    PDMAUDIOSTREAMCFG CfgHostAcq;
    int rc = drvAudioStreamCreateInternalBackend(pThis, pStream, pCfgHost, &CfgHostAcq);
    if (RT_FAILURE(rc))
        return rc;

    LogFunc(("[%s] Acquired host format:\n",  pStream->szName));
    PDMAudioStrmCfgLog(&CfgHostAcq);
    LogRel2(("Audio: Acquired host %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             CfgHostAcq.enmDir == PDMAUDIODIR_IN ? "recording" : "playback",  pStream->szName,
             CfgHostAcq.Props.uHz, PDMAudioPropsSampleBits(&CfgHostAcq.Props), CfgHostAcq.Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&CfgHostAcq.Props), PDMAudioPropsChannels(&CfgHostAcq.Props) == 1 ? "" : "s"));
    Assert(PDMAudioPropsAreValid(&CfgHostAcq.Props));

    /* Let the user know if the backend changed some of the tweakable values. */
    if (CfgHostAcq.Backend.cFramesBufferSize != pCfgHost->Backend.cFramesBufferSize)
        LogRel2(("Audio: Backend changed buffer size from %RU64ms (%RU32 frames) to %RU64ms (%RU32 frames)\n",
                 PDMAudioPropsFramesToMilli(&pCfgHost->Props, pCfgHost->Backend.cFramesBufferSize), pCfgHost->Backend.cFramesBufferSize,
                 PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize), CfgHostAcq.Backend.cFramesBufferSize));

    if (CfgHostAcq.Backend.cFramesPeriod != pCfgHost->Backend.cFramesPeriod)
        LogRel2(("Audio: Backend changed period size from %RU64ms (%RU32 frames) to %RU64ms (%RU32 frames)\n",
                 PDMAudioPropsFramesToMilli(&pCfgHost->Props, pCfgHost->Backend.cFramesPeriod), pCfgHost->Backend.cFramesPeriod,
                 PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPeriod), CfgHostAcq.Backend.cFramesPeriod));

    if (CfgHostAcq.Backend.cFramesPreBuffering != pCfgHost->Backend.cFramesPreBuffering)
        LogRel2(("Audio: Backend changed pre-buffering size from %RU64ms (%RU32 frames) to %RU64ms (%RU32 frames)\n",
                 PDMAudioPropsFramesToMilli(&pCfgHost->Props, pCfgHost->Backend.cFramesPreBuffering), pCfgHost->Backend.cFramesPreBuffering,
                 PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPreBuffering), CfgHostAcq.Backend.cFramesPreBuffering));

    /*
     * Configure host buffers.
     */

    /* Check if the backend did return sane values and correct if necessary.
     * Should never happen with our own backends, but you never know ... */
    if (CfgHostAcq.Backend.cFramesBufferSize < CfgHostAcq.Backend.cFramesPreBuffering)
    {
        LogRel2(("Audio: Warning: Pre-buffering size (%RU32 frames) of stream '%s' does not match buffer size (%RU32 frames), "
                 "setting pre-buffering size to %RU32 frames\n",
                 CfgHostAcq.Backend.cFramesPreBuffering, pStream->szName, CfgHostAcq.Backend.cFramesBufferSize, CfgHostAcq.Backend.cFramesBufferSize));
        CfgHostAcq.Backend.cFramesPreBuffering = CfgHostAcq.Backend.cFramesBufferSize;
    }

    if (CfgHostAcq.Backend.cFramesPeriod > CfgHostAcq.Backend.cFramesBufferSize)
    {
        LogRel2(("Audio: Warning: Period size (%RU32 frames) of stream '%s' does not match buffer size (%RU32 frames), setting to %RU32 frames\n",
                 CfgHostAcq.Backend.cFramesPeriod, pStream->szName, CfgHostAcq.Backend.cFramesBufferSize, CfgHostAcq.Backend.cFramesBufferSize));
        CfgHostAcq.Backend.cFramesPeriod = CfgHostAcq.Backend.cFramesBufferSize;
    }

    uint64_t msBufferSize = PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize);
    LogRel2(("Audio: Buffer size of stream '%s' is %RU64ms (%RU32 frames)\n",
             pStream->szName, msBufferSize, CfgHostAcq.Backend.cFramesBufferSize));

    /* If no own pre-buffer is set, let the backend choose. */
    uint64_t msPreBuf = PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPreBuffering);
    LogRel2(("Audio: Pre-buffering size of stream '%s' is %RU64ms (%RU32 frames)\n",
             pStream->szName, msPreBuf, CfgHostAcq.Backend.cFramesPreBuffering));

    /* Make sure the configured buffer size by the backend at least can hold the configured latency. */
    const uint32_t msPeriod = PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPeriod);

    LogRel2(("Audio: Period size of stream '%s' is %RU64ms (%RU32 frames)\n",
             pStream->szName, msPeriod, CfgHostAcq.Backend.cFramesPeriod));

    if (   pCfgGuest->Device.cMsSchedulingHint             /* Any scheduling hint set? */
        && pCfgGuest->Device.cMsSchedulingHint > msPeriod) /* This might lead to buffer underflows. */
    {
        LogRel(("Audio: Warning: Scheduling hint of stream '%s' is bigger (%RU64ms) than used period size (%RU64ms)\n",
                pStream->szName, pCfgGuest->Device.cMsSchedulingHint, msPeriod));
    }

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pStream->Host.MixBuf);

    rc = AudioMixBufInit(&pStream->Host.MixBuf, pStream->szName, &CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize);
    AssertRC(rc);

    /* Make a copy of the acquired host stream configuration. */
    rc = PDMAudioStrmCfgCopy(&pStream->Host.Cfg, &CfgHostAcq);
    AssertRC(rc);

    /*
     * Init guest stream.
     */

    if (pCfgGuest->Device.cMsSchedulingHint)
        LogRel2(("Audio: Stream '%s' got a scheduling hint of %RU32ms (%RU32 bytes)\n",
                 pStream->szName, pCfgGuest->Device.cMsSchedulingHint,
                 PDMAudioPropsMilliToBytes(&pCfgGuest->Props, pCfgGuest->Device.cMsSchedulingHint)));

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pStream->Guest.MixBuf);

    /* Set the guests's default audio data layout. */
    pCfgGuest->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

    rc = AudioMixBufInit(&pStream->Guest.MixBuf, pStream->szName, &pCfgGuest->Props, CfgHostAcq.Backend.cFramesBufferSize);
    AssertRC(rc);

    /* Make a copy of the guest stream configuration. */
    rc = PDMAudioStrmCfgCopy(&pStream->Guest.Cfg, pCfgGuest);
    AssertRC(rc);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Creating stream '%s' failed with %Rrc\n", pStream->szName, rc));

    if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
    {
        /* Host (Parent) -> Guest (Child). */
        rc = AudioMixBufLinkTo(&pStream->Host.MixBuf, &pStream->Guest.MixBuf);
        AssertRC(rc);
    }
    else
    {
        /* Guest (Parent) -> Host (Child). */
        rc = AudioMixBufLinkTo(&pStream->Guest.MixBuf, &pStream->Host.MixBuf);
        AssertRC(rc);
    }

    /*
     * Register statistics.
     */
#ifdef VBOX_WITH_STATISTICS
    char szStatName[255];
    if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
    {
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalFramesCaptured", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->In.Stats.TotalFramesCaptured,
                                  szStatName, STAMUNIT_COUNT, "Total frames played.");
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalTimesCaptured", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->In.Stats.TotalTimesCaptured,
                                  szStatName, STAMUNIT_COUNT, "Total number of playbacks.");
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalFramesRead", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->In.Stats.TotalFramesRead,
                                  szStatName, STAMUNIT_COUNT, "Total frames read.");
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalTimesRead", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->In.Stats.TotalTimesRead,
                                  szStatName, STAMUNIT_COUNT, "Total number of reads.");
    }
    else
    {
        Assert(pCfgGuest->enmDir == PDMAUDIODIR_OUT);
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalFramesPlayed", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->Out.Stats.TotalFramesPlayed,
                                  szStatName, STAMUNIT_COUNT, "Total frames played.");

        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalTimesPlayed", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->Out.Stats.TotalTimesPlayed,
                                  szStatName, STAMUNIT_COUNT, "Total number of playbacks.");
        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalFramesWritten", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->Out.Stats.TotalFramesWritten,
                                  szStatName, STAMUNIT_COUNT, "Total frames written.");

        RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/TotalTimesWritten", pStream->szName);
        PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pStream->Out.Stats.TotalTimesWritten,
                                  szStatName, STAMUNIT_COUNT, "Total number of writes.");
    }
#endif /* VBOX_WITH_STATISTICS */

    LogFlowFunc(("[%s] Returning %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioStreamCreate(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAMCFG pCfgHost,
                                              PPDMAUDIOSTREAMCFG pCfgGuest, PPDMAUDIOSTREAM *ppStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /*
     * Assert sanity.
     */
    AssertPtrReturn(pCfgHost,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgGuest,  VERR_INVALID_POINTER);
    AssertPtrReturn(ppStream,   VERR_INVALID_POINTER);
    LogFlowFunc(("Host=%s, Guest=%s\n", pCfgHost->szName, pCfgGuest->szName));
#ifdef LOG_ENABLED
    PDMAudioStrmCfgLog(pCfgHost);
    PDMAudioStrmCfgLog(pCfgGuest);
#endif
    AssertReturn(AudioHlpStreamCfgIsValid(pCfgHost), VERR_INVALID_PARAMETER);
    AssertReturn(AudioHlpStreamCfgIsValid(pCfgGuest), VERR_INVALID_PARAMETER);
    AssertReturn(pCfgHost->enmDir == pCfgGuest->enmDir, VERR_MISMATCH);
    AssertReturn(pCfgHost->enmDir == PDMAUDIODIR_IN || pCfgHost->enmDir == PDMAUDIODIR_OUT, VERR_NOT_SUPPORTED);

    /*
     * Lock the whole driver instance.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * Check that we have free streams in the backend and get the
     * size of the backend specific stream data.
     */
    uint32_t *pcFreeStreams;
    size_t    cbHstStrm;
    if (pCfgHost->enmDir == PDMAUDIODIR_IN)
    {
        if (!pThis->In.cStreamsFree)
        {
            LogFlowFunc(("Maximum number of host input streams reached\n"));
            rc = VERR_AUDIO_NO_FREE_INPUT_STREAMS;
        }
        pcFreeStreams = &pThis->In.cStreamsFree;
        cbHstStrm     = pThis->BackendCfg.cbStreamIn;
    }
    else /* Out */
    {
        if (!pThis->Out.cStreamsFree)
        {
            LogFlowFunc(("Maximum number of host output streams reached\n"));
            rc = VERR_AUDIO_NO_FREE_OUTPUT_STREAMS;
        }
        pcFreeStreams = &pThis->Out.cStreamsFree;
        cbHstStrm     = pThis->BackendCfg.cbStreamOut;
    }
    AssertStmt(cbHstStrm < _16M, rc = VERR_OUT_OF_RANGE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize common state.
         */
        PPDMAUDIOSTREAM pStream = (PPDMAUDIOSTREAM)RTMemAllocZ(sizeof(PDMAUDIOSTREAM) + RT_ALIGN_Z(cbHstStrm, 64));
        if (pStream)
        {
            /* Retrieve host driver name for easier identification. */
            AssertPtr(pThis->pHostDrvAudio);
            PPDMDRVINS pDrvAudioInst = PDMIBASE_2_PDMDRV(pThis->pDrvIns->pDownBase);
            RTStrPrintf(pStream->szName, RT_ELEMENTS(pStream->szName), "[%s] %s",
                        pDrvAudioInst && pDrvAudioInst->pReg && pDrvAudioInst->pReg->szName[0]
                        ? pDrvAudioInst->pReg->szName : "none",
                        pCfgHost->szName[0] != '\0' ? pCfgHost->szName : "<Untitled>");

            pStream->enmDir    = pCfgHost->enmDir;
            pStream->cbBackend = (uint32_t)cbHstStrm;
            if (cbHstStrm)
                pStream->pvBackend = pStream + 1;

            /*
             * Try to init the rest.
             */
            rc = drvAudioStreamInitInternal(pThis, pStream, pCfgHost, pCfgGuest);
            if (RT_SUCCESS(rc))
            {
                /* Set initial reference counts. */
                pStream->cRefs = 1;

                /* Decrement the free stream counter. */
                Assert(*pcFreeStreams > 0);
                *pcFreeStreams -= 1;

                /*
                 * We're good.
                 */
                RTListAppend(&pThis->lstStreams, &pStream->ListEntry);
                STAM_COUNTER_INC(&pThis->Stats.TotalStreamsCreated);
                *ppStream = pStream;

                /*
                 * Init debug stuff if enabled (ignore failures).
                 */
                if (pCfgHost->enmDir == PDMAUDIODIR_IN)
                {
                    if (pThis->In.Cfg.Dbg.fEnabled)
                    {
                        AudioHlpFileCreateAndOpen(&pStream->In.Dbg.pFileCaptureNonInterleaved, pThis->In.Cfg.Dbg.szPathOut,
                                                  "DrvAudioCapNonInt", pThis->pDrvIns->iInstance, &pStream->Host.Cfg.Props);
                        AudioHlpFileCreateAndOpen(&pStream->In.Dbg.pFileStreamRead, pThis->In.Cfg.Dbg.szPathOut,
                                                  "DrvAudioRead", pThis->pDrvIns->iInstance, &pStream->Host.Cfg.Props);
                    }
                }
                else /* Out */
                {
                    if (pThis->Out.Cfg.Dbg.fEnabled)
                    {
                        AudioHlpFileCreateAndOpen(&pStream->Out.Dbg.pFilePlayNonInterleaved, pThis->Out.Cfg.Dbg.szPathOut,
                                                  "DrvAudioPlayNonInt", pThis->pDrvIns->iInstance, &pStream->Host.Cfg.Props);
                        AudioHlpFileCreateAndOpen(&pStream->Out.Dbg.pFileStreamWrite, pThis->Out.Cfg.Dbg.szPathOut,
                                                  "DrvAudioWrite", pThis->pDrvIns->iInstance, &pStream->Host.Cfg.Props);
                    }
                }
            }
            else
            {
                LogFunc(("drvAudioStreamInitInternal failed: %Rrc\n", rc));
                int rc2 = drvAudioStreamUninitInternal(pThis, pStream);
                AssertRC(rc2);
                drvAudioStreamFree(pStream);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnEnable}
 */
static DECLCALLBACK(int) drvAudioEnable(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir, bool fEnable)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    bool *pfEnabled;
    if (enmDir == PDMAUDIODIR_IN)
        pfEnabled = &pThis->In.fEnabled;
    else if (enmDir == PDMAUDIODIR_OUT)
        pfEnabled = &pThis->Out.fEnabled;
    else
        AssertFailedReturn(VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    if (fEnable != *pfEnabled)
    {
        LogRel(("Audio: %s %s for driver '%s'\n",
                fEnable ? "Enabling" : "Disabling", enmDir == PDMAUDIODIR_IN ? "input" : "output", pThis->szName));

        /* Update the status first, as this will be checked for in drvAudioStreamControlInternalBackend() below. */
        *pfEnabled = fEnable;

        PPDMAUDIOSTREAM pStream;
        RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, ListEntry)
        {
            if (pStream->enmDir != enmDir) /* Skip unwanted streams. */
                continue;

            /* Note: Only enable / disable the backend, do *not* change the stream's internal status.
             *       Callers (device emulation, mixer, ...) from outside will not see any status or behavior change,
             *       to not confuse the rest of the state machine.
             *
             *       When disabling:
             *          - playing back audo data would go to /dev/null
             *          - recording audio data would return silence instead
             *
             * See @bugref{9882}.
             */
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStream,
                                                           fEnable ? PDMAUDIOSTREAMCMD_ENABLE : PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_FAILURE(rc2))
            {
                if (rc2 == VERR_AUDIO_STREAM_NOT_READY)
                {
                    LogRel(("Audio: Stream '%s' not available\n", pStream->szName));
                }
                else
                    LogRel(("Audio: Failed to %s %s stream '%s', rc=%Rrc\n",
                            fEnable ? "enable" : "disable", enmDir == PDMAUDIODIR_IN ? "input" : "output", pStream->szName, rc2));
            }
            else
            {
                /* When (re-)enabling a stream, clear the disabled warning bit again. */
                if (fEnable)
                    pStream->fWarningsShown &= ~PDMAUDIOSTREAM_WARN_FLAGS_DISABLED;
            }

            if (RT_SUCCESS(rc))
                rc = rc2;

            /* Keep going. */
        }
    }

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnIsEnabled}
 */
static DECLCALLBACK(bool) drvAudioIsEnabled(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, false);

    bool fEnabled;
    if (enmDir == PDMAUDIODIR_IN)
        fEnabled = pThis->In.fEnabled;
    else if (enmDir == PDMAUDIODIR_OUT)
        fEnabled = pThis->Out.fEnabled;
    else
        AssertFailedStmt(fEnabled = false);

    RTCritSectLeave(&pThis->CritSect);
    return fEnabled;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioGetConfig(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    if (pThis->pHostDrvAudio)
    {
        if (pThis->pHostDrvAudio->pfnGetConfig)
            rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, pCfg);
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioGetStatus(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, PDMAUDIOBACKENDSTS_UNKNOWN);

    PDMAUDIOBACKENDSTS fBackendStatus;
    if (pThis->pHostDrvAudio)
    {
        if (pThis->pHostDrvAudio->pfnGetStatus)
            fBackendStatus = pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, enmDir);
        else
            fBackendStatus = PDMAUDIOBACKENDSTS_UNKNOWN;
    }
    else
        fBackendStatus = PDMAUDIOBACKENDSTS_NOT_ATTACHED;

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFunc(("LEAVE - %#x\n", fBackendStatus));
    return fBackendStatus;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetReadable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pStream, 0);
    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN, ("Can't read from a non-input stream\n"));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, 0);

    /*
     * ...
     */
    uint32_t cbReadable = 0;

    /* All input streams for this driver disabled? See @bugref{9882}. */
    const bool fDisabled = !pThis->In.fEnabled;

    if (   pThis->pHostDrvAudio
        && (   PDMAudioStrmStatusCanRead(pStream->fStatus)
            || fDisabled)
       )
    {
        const uint32_t cfReadable = AudioMixBufLive(&pStream->Guest.MixBuf);

        cbReadable = AUDIOMIXBUF_F2B(&pStream->Guest.MixBuf, cfReadable);

        if (!cbReadable)
        {
            /*
             * If nothing is readable, check if the stream on the backend side is ready to be read from.
             * If it isn't, return the number of bytes readable since the last read from this stream.
             *
             * This is needed for backends (e.g. VRDE) which do not provide any input data in certain
             * situations, but the device emulation needs input data to keep the DMA transfers moving.
             * Reading the actual data from a stream then will return silence then.
             */
            PDMAUDIOSTREAMSTS fStatus = PDMAUDIOSTREAMSTS_FLAGS_NONE;
            if (pThis->pHostDrvAudio->pfnStreamGetStatus)
                fStatus = pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pStream->pvBackend);
            if (   !PDMAudioStrmStatusCanRead(fStatus)
                || fDisabled)
            {
                cbReadable = PDMAudioPropsNanoToBytes(&pStream->Host.Cfg.Props, RTTimeNanoTS() - pStream->tsLastReadWrittenNs);
                if (!(pStream->fWarningsShown & PDMAUDIOSTREAM_WARN_FLAGS_DISABLED))
                {
                    if (fDisabled)
                        LogRel(("Audio: Input for driver '%s' has been disabled, returning silence\n", pThis->szName));
                    else
                        LogRel(("Audio: Warning: Input for stream '%s' of driver '%s' not ready (current input status is %#x), returning silence\n",
                                pStream->szName, pThis->szName, fStatus));

                    pStream->fWarningsShown |= PDMAUDIOSTREAM_WARN_FLAGS_DISABLED;
                }
            }
        }

        /* Make sure to align the readable size to the guest's frame size. */
        if (cbReadable)
            cbReadable = PDMAudioPropsFloorBytesToFrame(&pStream->Guest.Cfg.Props, cbReadable);
    }

    RTCritSectLeave(&pThis->CritSect);
    Log3Func(("[%s] cbReadable=%RU32 (%RU64ms)\n",
              pStream->szName, cbReadable, PDMAudioPropsBytesToMilli(&pStream->Host.Cfg.Props, cbReadable)));
    return cbReadable;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetWritable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    AssertPtrReturn(pStream,    0);
    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT, ("Can't write to a non-output stream\n"));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, 0);

    /*
     * ...
     */
    uint32_t cbWritable = 0;

    /* Note: We don't propage the backend stream's status to the outside -- it's the job of this
     *       audio connector to make sense of it. */
    if (PDMAudioStrmStatusCanWrite(pStream->fStatus))
    {
        cbWritable = AudioMixBufFreeBytes(&pStream->Host.MixBuf);

        /* Make sure to align the writable size to the host's frame size. */
        cbWritable = PDMAudioPropsFloorBytesToFrame(&pStream->Host.Cfg.Props, cbWritable);
    }

    RTCritSectLeave(&pThis->CritSect);
    Log3Func(("[%s] cbWritable=%RU32 (%RU64ms)\n",
              pStream->szName, cbWritable, PDMAudioPropsBytesToMilli(&pStream->Host.Cfg.Props, cbWritable)));
    return cbWritable;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvAudioStreamGetStatus(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    if (!pStream)
        return PDMAUDIOSTREAMSTS_FLAGS_NONE;

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, PDMAUDIOSTREAMSTS_FLAGS_NONE);

    /* Is the stream scheduled for re-initialization? Do so now. */
    drvAudioStreamMaybeReInit(pThis, pStream);

    PDMAUDIOSTREAMSTS fStrmStatus = pStream->fStatus;

    RTCritSectLeave(&pThis->CritSect);
#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] %s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, fStrmStatus)));
    return fStrmStatus;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamSetVolume}
 */
static DECLCALLBACK(int) drvAudioStreamSetVolume(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,       VERR_INVALID_POINTER);

    LogFlowFunc(("[%s] volL=%RU32, volR=%RU32, fMute=%RTbool\n", pStream->szName, pVol->uLeft, pVol->uRight, pVol->fMuted));

    AudioMixBufSetVolume(&pStream->Guest.MixBuf, pVol);
    AudioMixBufSetVolume(&pStream->Host.MixBuf, pVol);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    if (!pStream)
        return VINF_SUCCESS;
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    Assert(pStream->uMagic == PDMAUDIOSTREAM_MAGIC);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    LogRel2(("Audio: Destroying stream '%s'\n", pStream->szName));

    LogFlowFunc(("[%s] cRefs=%RU32\n", pStream->szName, pStream->cRefs));
    if (pStream->cRefs <= 1)
    {
        rc = drvAudioStreamUninitInternal(pThis, pStream);
        if (RT_SUCCESS(rc))
        {
            if (pStream->enmDir == PDMAUDIODIR_IN)
                pThis->In.cStreamsFree++;
            else /* Out */
                pThis->Out.cStreamsFree++;

            RTListNodeRemove(&pStream->ListEntry);

            drvAudioStreamFree(pStream);
            pStream = NULL;
        }
        else
            LogRel(("Audio: Uninitializing stream '%s' failed with %Rrc\n", pStream->szName, rc));
    }
    else
        rc = VERR_WRONG_ORDER;

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Calls the backend to give it the chance to destroy its part of the audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Audio stream destruct backend for.
 */
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFunc(("[%s] fStatus=%s\n", pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));

    if (pStream->fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED)
    {
        AssertPtr(pStream->pvBackend);

        /* Check if the pointer to  the host audio driver is still valid.
         * It can be NULL if we were called in drvAudioDestruct, for example. */
        if (pThis->pHostDrvAudio)
            rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pStream->pvBackend);

        pStream->fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;
    }

    LogFlowFunc(("[%s] Returning %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * Uninitializes an audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Pointer to audio stream to uninitialize.
 */
static int drvAudioStreamUninitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertMsgReturn(pStream->cRefs <= 1,
                    ("Stream '%s' still has %RU32 references held when uninitializing\n", pStream->szName, pStream->cRefs),
                    VERR_WRONG_ORDER);
    LogFlowFunc(("[%s] cRefs=%RU32\n", pStream->szName, pStream->cRefs));

    /*
     * ...
     */
    int rc = drvAudioStreamControlInternal(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
        rc = drvAudioStreamDestroyInternalBackend(pThis, pStream);

    /* Destroy mixing buffers. */
    AudioMixBufDestroy(&pStream->Guest.MixBuf);
    AudioMixBufDestroy(&pStream->Host.MixBuf);

    if (RT_SUCCESS(rc))
    {
#ifdef LOG_ENABLED
        if (pStream->fStatus != PDMAUDIOSTREAMSTS_FLAGS_NONE)
        {
            char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
            LogFunc(("[%s] Warning: Still has %s set when uninitializing\n",
                     pStream->szName, dbgAudioStreamStatusToStr(szStreamSts, pStream->fStatus)));
        }
#endif
        pStream->fStatus = PDMAUDIOSTREAMSTS_FLAGS_NONE;
    }

    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
#ifdef VBOX_WITH_STATISTICS
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.Stats.TotalFramesCaptured);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.Stats.TotalTimesCaptured);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.Stats.TotalFramesRead);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.Stats.TotalTimesRead);
#endif
        if (pThis->In.Cfg.Dbg.fEnabled)
        {
            AudioHlpFileDestroy(pStream->In.Dbg.pFileCaptureNonInterleaved);
            pStream->In.Dbg.pFileCaptureNonInterleaved = NULL;

            AudioHlpFileDestroy(pStream->In.Dbg.pFileStreamRead);
            pStream->In.Dbg.pFileStreamRead = NULL;
        }
    }
    else if (pStream->enmDir == PDMAUDIODIR_OUT)
    {
#ifdef VBOX_WITH_STATISTICS
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.Stats.TotalFramesPlayed);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.Stats.TotalTimesPlayed);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.Stats.TotalFramesWritten);
        PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.Stats.TotalTimesWritten);
#endif
        if (pThis->Out.Cfg.Dbg.fEnabled)
        {
            AudioHlpFileDestroy(pStream->Out.Dbg.pFilePlayNonInterleaved);
            pStream->Out.Dbg.pFilePlayNonInterleaved = NULL;

            AudioHlpFileDestroy(pStream->Out.Dbg.pFileStreamWrite);
            pStream->Out.Dbg.pFileStreamWrite = NULL;
        }
    }
    else
        AssertFailed();

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}


/*********************************************************************************************************************************
*   PDMIBASE interface implementation.                                                                                           *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    LogFlowFunc(("pInterface=%p, pszIID=%s\n", pInterface, pszIID));

    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOCONNECTOR, &pThis->IAudioConnector);

    return NULL;
}


/*********************************************************************************************************************************
*   PDMDRVREG interface implementation.                                                                                          *
*********************************************************************************************************************************/

/**
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFuncEnter();

    if (!pThis->pHostDrvAudio) /* If not lower driver is configured, bail out. */
        return;

    /* Just destroy the host stream on the backend side.
     * The rest will either be destructed by the device emulation or
     * in drvAudioDestruct(). */
    PPDMAUDIOSTREAM pStream;
    RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, ListEntry)
    {
        drvAudioStreamControlInternalBackend(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
        drvAudioStreamDestroyInternalBackend(pThis, pStream);
    }

    /*
     * Last call for the driver below us.
     * Let it know that we reached end of life.
     */
    if (pThis->pHostDrvAudio->pfnShutdown)
        pThis->pHostDrvAudio->pfnShutdown(pThis->pHostDrvAudio);

    pThis->pHostDrvAudio = NULL;

    LogFlowFuncLeave();
}


/**
 * Detach notification.
 *
 * @param   pDrvIns     The driver instance data.
 * @param   fFlags      Detach flags.
 */
static DECLCALLBACK(void) drvAudioDetach(PPDMDRVINS pDrvIns, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    RT_NOREF(fFlags);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc);

    LogFunc(("%s (detached %p)\n", pThis->szName, pThis->pHostDrvAudio));
    pThis->pHostDrvAudio = NULL;

    RTCritSectLeave(&pThis->CritSect);
}


/**
 * Does the actual backend driver attaching and queries the backend's interface.
 *
 * This is a worker for both drvAudioAttach and drvAudioConstruct.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   fFlags      Attach flags; see PDMDrvHlpAttach().
 */
static int drvAudioDoAttachInternal(PDRVAUDIO pThis, uint32_t fFlags)
{
    Assert(pThis->pHostDrvAudio == NULL); /* No nested attaching. */

    /*
     * Attach driver below and query its connector interface.
     */
    PPDMIBASE pDownBase;
    int rc = PDMDrvHlpAttach(pThis->pDrvIns, fFlags, &pDownBase);
    if (RT_SUCCESS(rc))
    {
        pThis->pHostDrvAudio = PDMIBASE_QUERY_INTERFACE(pDownBase, PDMIHOSTAUDIO);
        if (pThis->pHostDrvAudio)
        {
            /*
             * If everything went well, initialize the lower driver.
             */
            rc = drvAudioHostInit(pThis);
        }
        else
        {
            LogRel(("Audio: Failed to query interface for underlying host driver '%s'\n", pThis->szName));
            rc = PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                  N_("Host audio backend missing or invalid"));
        }
    }

    LogFunc(("[%s] rc=%Rrc\n", pThis->szName, rc));
    return rc;
}


/**
 * Attach notification.
 *
 * @param   pDrvIns     The driver instance data.
 * @param   fFlags      Attach flags.
 */
static DECLCALLBACK(int) drvAudioAttach(PPDMDRVINS pDrvIns, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFunc(("%s\n", pThis->szName));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = drvAudioDoAttachInternal(pThis, fFlags);

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioResume(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_RESUME);
}


/**
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioSuspend(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_PAUSE);
}


/**
 * Destructs an audio driver instance.
 *
 * @copydoc FNPDMDRVDESTRUCT
 */
static DECLCALLBACK(void) drvAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFuncEnter();

    if (RTCritSectIsInitialized(&pThis->CritSect))
    {
        int rc = RTCritSectEnter(&pThis->CritSect);
        AssertRC(rc);
    }

    /*
     * Note: No calls here to the driver below us anymore,
     *       as PDM already has destroyed it.
     *       If you need to call something from the host driver,
     *       do this in drvAudioPowerOff() instead.
     */

    /* Thus, NULL the pointer to the host audio driver first,
     * so that routines like drvAudioStreamDestroyInternal() don't call the driver(s) below us anymore. */
    pThis->pHostDrvAudio = NULL;

    PPDMAUDIOSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pThis->lstStreams, pStream, pStreamNext, PDMAUDIOSTREAM, ListEntry)
    {
        int rc = drvAudioStreamUninitInternal(pThis, pStream);
        if (RT_SUCCESS(rc))
        {
            RTListNodeRemove(&pStream->ListEntry);

            drvAudioStreamFree(pStream);
            pStream = NULL;
        }
    }

    /* Sanity. */
    Assert(RTListIsEmpty(&pThis->lstStreams));

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /*
     * Destroy callbacks, if any.
     */
    PPDMAUDIOCBRECORD pCB, pCBNext;
    RTListForEachSafe(&pThis->In.lstCB, pCB, pCBNext, PDMAUDIOCBRECORD, Node)
        drvAudioCallbackDestroy(pCB);

    RTListForEachSafe(&pThis->Out.lstCB, pCB, pCBNext, PDMAUDIOCBRECORD, Node)
        drvAudioCallbackDestroy(pCB);
#endif

    if (RTCritSectIsInitialized(&pThis->CritSect))
    {
        int rc = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc);

        rc = RTCritSectDelete(&pThis->CritSect);
        AssertRC(rc);
    }

    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Out.StatsReBuffering);
#ifdef VBOX_WITH_STATISTICS
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalStreamsActive);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalStreamsCreated);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesRead);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesWritten);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesMixedIn);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesMixedOut);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesLostIn);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesLostOut);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesOut);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesIn);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalBytesRead);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalBytesWritten);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.DelayIn);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.DelayOut);
#endif

    LogFlowFuncLeave();
}


/**
 * Constructs an audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlowFunc(("pDrvIns=%#p, pCfgHandle=%#p, fFlags=%x\n", pDrvIns, pCfg, fFlags));

    /*
     * Basic instance init.
     */
    RTListInit(&pThis->lstStreams);
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    RTListInit(&pThis->In.lstCB);
    RTListInit(&pThis->Out.lstCB);
#endif

    /*
     * Read configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns,
                                  "DriverName|"
                                  "InputEnabled|"
                                  "OutputEnabled|"
                                  "DebugEnabled|"
                                  "DebugPathOut|"
                                  /* Deprecated: */
                                  "PCMSampleBitIn|"
                                  "PCMSampleBitOut|"
                                  "PCMSampleHzIn|"
                                  "PCMSampleHzOut|"
                                  "PCMSampleSignedIn|"
                                  "PCMSampleSignedOut|"
                                  "PCMSampleSwapEndianIn|"
                                  "PCMSampleSwapEndianOut|"
                                  "PCMSampleChannelsIn|"
                                  "PCMSampleChannelsOut|"
                                  "PeriodSizeMsIn|"
                                  "PeriodSizeMsOut|"
                                  "BufferSizeMsIn|"
                                  "BufferSizeMsOut|"
                                  "PreBufferSizeMsIn|"
                                  "PreBufferSizeMsOut",
                                  "In|Out");

    int rc = CFGMR3QueryStringDef(pCfg, "DriverName", pThis->szName, sizeof(pThis->szName), "Untitled");
    AssertLogRelRCReturn(rc, rc);

    /* Neither input nor output by default for security reasons. */
    rc = CFGMR3QueryBoolDef(pCfg, "InputEnabled",  &pThis->In.fEnabled, false);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryBoolDef(pCfg, "OutputEnabled", &pThis->Out.fEnabled, false);
    AssertLogRelRCReturn(rc, rc);

    /* Debug stuff (same for both directions). */
    rc = CFGMR3QueryBoolDef(pCfg, "DebugEnabled", &pThis->In.Cfg.Dbg.fEnabled, false);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryStringDef(pCfg, "DebugPathOut", pThis->In.Cfg.Dbg.szPathOut, sizeof(pThis->In.Cfg.Dbg.szPathOut), "");
    AssertLogRelRCReturn(rc, rc);
    if (pThis->In.Cfg.Dbg.szPathOut[0] == '\0')
    {
        rc = RTPathTemp(pThis->In.Cfg.Dbg.szPathOut, sizeof(pThis->In.Cfg.Dbg.szPathOut));
        if (RT_FAILURE(rc))
        {
            LogRel(("Audio: Warning! Failed to retrieve temporary directory: %Rrc - disabling debugging.\n", rc));
            pThis->In.Cfg.Dbg.szPathOut[0] = '\0';
            pThis->In.Cfg.Dbg.fEnabled = false;
        }
    }
    if (pThis->In.Cfg.Dbg.fEnabled)
        LogRel(("Audio: Debugging for driver '%s' enabled (audio data written to '%s')\n", pThis->szName, pThis->In.Cfg.Dbg.szPathOut));

    /* Copy debug setup to the output direction. */
    pThis->Out.Cfg.Dbg = pThis->In.Cfg.Dbg;

    LogRel2(("Audio: Verbose logging for driver '%s' is probably enabled too.\n", pThis->szName));
    /* This ^^^^^^^ is the *WRONG* place for that kind of statement. Verbose logging might only be enabled for DrvAudio. */
    LogRel2(("Audio: Initial status for driver '%s' is: input is %s, output is %s\n",
             pThis->szName, pThis->In.fEnabled ? "enabled" : "disabled", pThis->Out.fEnabled ? "enabled" : "disabled"));

    /*
     * Per direction configuration.  A bit complicated as
     * these wasn't originally in sub-nodes.
     */
    for (unsigned iDir = 0; iDir < 2; iDir++)
    {
        char         szNm[48];
        PDRVAUDIOCFG pAudioCfg = iDir == 0 ? &pThis->In.Cfg : &pThis->Out.Cfg;
        const char  *pszDir    = iDir == 0 ? "In"           : "Out";

#define QUERY_VAL_RET(a_Width, a_szName, a_pValue, a_uDefault, a_ExprValid, a_szValidRange) \
            do { \
                rc = RT_CONCAT(CFGMR3QueryU,a_Width)(pDirNode, strcpy(szNm, a_szName), a_pValue); \
                if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT) \
                { \
                    rc = RT_CONCAT(CFGMR3QueryU,a_Width)(pCfg, strcat(szNm, pszDir), a_pValue); \
                    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT) \
                    { \
                        *(a_pValue) = a_uDefault; \
                        rc = VINF_SUCCESS; \
                    } \
                    else \
                        LogRel(("DrvAudio: Warning! Please use '%s/" a_szName "' instead of '%s' for your VBoxInternal hacks\n", pszDir, szNm)); \
                } \
                AssertRCReturn(rc, PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, \
                                                       N_("Configuration error: Failed to read %s config value '%s'"), pszDir, szNm)); \
                if (!(a_ExprValid)) \
                    return PDMDrvHlpVMSetError(pDrvIns, VERR_OUT_OF_RANGE, RT_SRC_POS, \
                                               N_("Configuration error: Unsupported %s value %u. " a_szValidRange), szNm, *(a_pValue)); \
            } while (0)

        PCFGMNODE const pDirNode = CFGMR3GetChild(pCfg, pszDir);
        rc = CFGMR3ValidateConfig(pDirNode, iDir == 0 ? "In/" : "Out/",
                                  "PCMSampleBit|"
                                  "PCMSampleHz|"
                                  "PCMSampleSigned|"
                                  "PCMSampleSwapEndian|"
                                  "PCMSampleChannels|"
                                  "PeriodSizeMs|"
                                  "BufferSizeMs|"
                                  "PreBufferSizeMs",
                                  "", pDrvIns->pReg->szName, pDrvIns->iInstance);
        AssertRCReturn(rc, rc);

        uint8_t cSampleBits = 0;
        QUERY_VAL_RET(8,  "PCMSampleBit",        &cSampleBits,                  0,
                         cSampleBits == 0
                      || cSampleBits == 8
                      || cSampleBits == 16
                      || cSampleBits == 32
                      || cSampleBits == 64,
                      "Must be either 0, 8, 16, 32 or 64");
        if (cSampleBits)
            PDMAudioPropsSetSampleSize(&pAudioCfg->Props, cSampleBits / 8);

        uint8_t cChannels;
        QUERY_VAL_RET(8,  "PCMSampleChannels",   &cChannels,                    0, cChannels <= 16, "Max 16");
        if (cChannels)
            PDMAudioPropsSetChannels(&pAudioCfg->Props, cChannels);

        QUERY_VAL_RET(32, "PCMSampleHz",         &pAudioCfg->Props.uHz,         0,
                      pAudioCfg->Props.uHz == 0 || (pAudioCfg->Props.uHz >= 6000 && pAudioCfg->Props.uHz <= 768000),
                      "In the range 6000 thru 768000, or 0");

        QUERY_VAL_RET(8,  "PCMSampleSigned",     &pAudioCfg->uSigned,           UINT8_MAX,
                      pAudioCfg->uSigned == 0 || pAudioCfg->uSigned == 1 || pAudioCfg->uSigned == UINT8_MAX,
                      "Must be either 0, 1, or 255");

        QUERY_VAL_RET(8,  "PCMSampleSwapEndian", &pAudioCfg->uSwapEndian,       UINT8_MAX,
                      pAudioCfg->uSwapEndian == 0 || pAudioCfg->uSwapEndian == 1 || pAudioCfg->uSwapEndian == UINT8_MAX,
                      "Must be either 0, 1, or 255");

        QUERY_VAL_RET(32, "PeriodSizeMs",        &pAudioCfg->uPeriodSizeMs,     0,
                      pAudioCfg->uPeriodSizeMs <= RT_MS_1SEC, "Max 1000");

        QUERY_VAL_RET(32, "BufferSizeMs",        &pAudioCfg->uBufferSizeMs,     0,
                      pAudioCfg->uBufferSizeMs <= RT_MS_5SEC, "Max 5000");

        QUERY_VAL_RET(32, "PreBufferSizeMs",     &pAudioCfg->uPreBufSizeMs,     UINT32_MAX,
                      pAudioCfg->uPreBufSizeMs <= RT_MS_1SEC || pAudioCfg->uPreBufSizeMs == UINT32_MAX,
                      "Max 1000, or 0xffffffff");
#undef QUERY_VAL_RET
    }

    /*
     * Init the rest of the driver instance data.
     */
    rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    pThis->fTerminate                           = false;
    pThis->pDrvIns                              = pDrvIns;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface            = drvAudioQueryInterface;
    /* IAudioConnector. */
    pThis->IAudioConnector.pfnEnable            = drvAudioEnable;
    pThis->IAudioConnector.pfnIsEnabled         = drvAudioIsEnabled;
    pThis->IAudioConnector.pfnGetConfig         = drvAudioGetConfig;
    pThis->IAudioConnector.pfnGetStatus         = drvAudioGetStatus;
    pThis->IAudioConnector.pfnStreamCreate      = drvAudioStreamCreate;
    pThis->IAudioConnector.pfnStreamDestroy     = drvAudioStreamDestroy;
    pThis->IAudioConnector.pfnStreamRetain      = drvAudioStreamRetain;
    pThis->IAudioConnector.pfnStreamRelease     = drvAudioStreamRelease;
    pThis->IAudioConnector.pfnStreamControl     = drvAudioStreamControl;
    pThis->IAudioConnector.pfnStreamRead        = drvAudioStreamRead;
    pThis->IAudioConnector.pfnStreamWrite       = drvAudioStreamWrite;
    pThis->IAudioConnector.pfnStreamIterate     = drvAudioStreamIterate;
    pThis->IAudioConnector.pfnStreamGetReadable = drvAudioStreamGetReadable;
    pThis->IAudioConnector.pfnStreamGetWritable = drvAudioStreamGetWritable;
    pThis->IAudioConnector.pfnStreamGetStatus   = drvAudioStreamGetStatus;
    pThis->IAudioConnector.pfnStreamSetVolume   = drvAudioStreamSetVolume;
    pThis->IAudioConnector.pfnStreamPlay        = drvAudioStreamPlay;
    pThis->IAudioConnector.pfnStreamCapture     = drvAudioStreamCapture;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    pThis->IAudioConnector.pfnRegisterCallbacks = drvAudioRegisterCallbacks;
#endif

    /*
     * Statistics.
     */
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Out.StatsReBuffering, "OutputReBuffering",
                              STAMUNIT_COUNT, "Number of times the output stream was re-buffered after starting.");

#ifdef VBOX_WITH_STATISTICS
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalStreamsActive,   "TotalStreamsActive",
                              STAMUNIT_COUNT, "Total active audio streams.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalStreamsCreated,  "TotalStreamsCreated",
                              STAMUNIT_COUNT, "Total created audio streams.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesRead,      "TotalFramesRead",
                              STAMUNIT_COUNT, "Total frames read by device emulation.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesWritten,   "TotalFramesWritten",
                              STAMUNIT_COUNT, "Total frames written by device emulation ");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesMixedIn,   "TotalFramesMixedIn",
                              STAMUNIT_COUNT, "Total input frames mixed.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesMixedOut,  "TotalFramesMixedOut",
                              STAMUNIT_COUNT, "Total output frames mixed.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesLostIn,    "TotalFramesLostIn",
                              STAMUNIT_COUNT, "Total input frames lost.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesLostOut,   "TotalFramesLostOut",
                              STAMUNIT_COUNT, "Total output frames lost.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesOut,       "TotalFramesOut",
                              STAMUNIT_COUNT, "Total frames played by backend.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesIn,        "TotalFramesIn",
                              STAMUNIT_COUNT, "Total frames captured by backend.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalBytesRead,       "TotalBytesRead",
                              STAMUNIT_BYTES, "Total bytes read.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalBytesWritten,    "TotalBytesWritten",
                              STAMUNIT_BYTES, "Total bytes written.");

    PDMDrvHlpSTAMRegProfileAdvEx(pDrvIns, &pThis->Stats.DelayIn,           "DelayIn",
                                 STAMUNIT_NS_PER_CALL, "Profiling of input data processing.");
    PDMDrvHlpSTAMRegProfileAdvEx(pDrvIns, &pThis->Stats.DelayOut,          "DelayOut",
                                 STAMUNIT_NS_PER_CALL, "Profiling of output data processing.");
#endif

    /*
     * Attach the host driver, if present.
     */
    rc = drvAudioDoAttachInternal(pThis, fFlags);
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Audio driver registration record.
 */
const PDMDRVREG g_DrvAUDIO =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "AUDIO",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio connector driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    UINT32_MAX,
    /* cbInstance */
    sizeof(DRVAUDIO),
    /* pfnConstruct */
    drvAudioConstruct,
    /* pfnDestruct */
    drvAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvAudioSuspend,
    /* pfnResume */
    drvAudioResume,
    /* pfnAttach */
    drvAudioAttach,
    /* pfnDetach */
    drvAudioDetach,
    /* pfnPowerOff */
    drvAudioPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

