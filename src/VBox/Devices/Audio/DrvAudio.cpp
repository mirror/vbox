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

/**
 * Audio stream context.
 *
 * Needed for separating data from the guest and host side (per stream).
 */
typedef struct DRVAUDIOSTREAMCTX
{
    /** The stream's audio configuration. */
    PDMAUDIOSTREAMCFG   Cfg;
    /** This stream's mixing buffer. */
    AUDIOMIXBUF         MixBuf;
} DRVAUDIOSTREAMCTX;

/**
 * Extended stream structure.
 */
typedef struct DRVAUDIOSTREAM
{
    /** The publicly visible bit. */
    PDMAUDIOSTREAM      Core;

    /** Just an extra magic to verify that we allocated the stream rather than some
     * faked up stuff from the device (DRVAUDIOSTREAM_MAGIC). */
    uintptr_t           uMagic;

    /** List entry in DRVAUDIO::lstStreams. */
    RTLISTNODE          ListEntry;

    /** Data to backend-specific stream data.
     *  This data block will be casted by the backend to access its backend-dependent data.
     *
     *  That way the backends do not have access to the audio connector's data. */
    PPDMAUDIOBACKENDSTREAM pBackend;

    /** For output streams this indicates whether the stream has reached
     *  its playback threshold, e.g. is playing audio.
     *  For input streams this  indicates whether the stream has enough input
     *  data to actually start reading audio. */
    bool                fThresholdReached;
    /** Do not use the mixing buffers (Guest::MixBuf, Host::MixBuf). */
    bool                fNoMixBufs;
    bool                afPadding[2];

    /** Number of (re-)tries while re-initializing the stream. */
    uint32_t            cTriesReInit;

    /** The guest side of the stream. */
    DRVAUDIOSTREAMCTX   Guest;
    /** The host side of the stream. */
    DRVAUDIOSTREAMCTX   Host;


    /** Timestamp (in ns) since last trying to re-initialize.
     *  Might be 0 if has not been tried yet. */
    uint64_t            nsLastReInit;
    /** Timestamp (in ns) since last iteration. */
    uint64_t            nsLastIterated;
    /** Timestamp (in ns) since last playback / capture. */
    uint64_t            nsLastPlayedCaptured;
    /** Timestamp (in ns) since last read (input streams) or
     *  write (output streams). */
    uint64_t            nsLastReadWritten;
    /** Internal stream position (as per pfnStreamWrite/Read). */
    uint64_t            offInternal;


    /** Union for input/output specifics depending on enmDir. */
    union
    {
        /**
         * The specifics for an audio input stream.
         */
        struct
        {
            struct
            {
                /** File for writing stream reads. */
                PAUDIOHLPFILE   pFileStreamRead;
                /** File for writing non-interleaved captures. */
                PAUDIOHLPFILE   pFileCaptureNonInterleaved;
            } Dbg;
            struct
            {
                STAMCOUNTER     TotalFramesCaptured;
                STAMCOUNTER     AvgFramesCaptured;
                STAMCOUNTER     TotalTimesCaptured;
                STAMCOUNTER     TotalFramesRead;
                STAMCOUNTER     AvgFramesRead;
                STAMCOUNTER     TotalTimesRead;
            } Stats;
        } In;

        /**
         * The specifics for an audio output stream.
         */
        struct
        {
            struct
            {
                /** File for writing stream writes. */
                PAUDIOHLPFILE   pFileStreamWrite;
                /** File for writing stream playback. */
                PAUDIOHLPFILE   pFilePlayNonInterleaved;
            } Dbg;
            struct
            {
                STAMCOUNTER     TotalFramesPlayed;
                STAMCOUNTER     AvgFramesPlayed;
                STAMCOUNTER     TotalTimesPlayed;
                STAMCOUNTER     TotalFramesWritten;
                STAMCOUNTER     AvgFramesWritten;
                STAMCOUNTER     TotalTimesWritten;
                uint32_t        cbBackendWritableBefore;
                uint32_t        cbBackendWritableAfter;
            } Stats;

            /** Space for pre-buffering. */
            uint8_t            *pbPreBuf;
            /** The size of the pre-buffer allocation (in bytes). */
            uint32_t            cbPreBufAlloc;
            /** Number of bytes we've prebuffered. */
            uint32_t            cbPreBuffered;
            /** The pre-buffering threshold expressed in bytes. */
            uint32_t            cbPreBufThreshold;

            /** Hack alert: Max writable amount reported by the backend.
             * This is used to aid buffer underrun detection in DrvAudio while playing.
             * Ideally, the backend should have a method for querying number of buffered
             * bytes instead.  However this will do for now. */
            uint32_t            cbBackendMaxWritable;
        } Out;
    } RT_UNION_NM(u);
} DRVAUDIOSTREAM;
/** Pointer to an extended stream structure. */
typedef DRVAUDIOSTREAM *PDRVAUDIOSTREAM;

/** Value for DRVAUDIOSTREAM::uMagic (Johann Sebastian Bach). */
#define DRVAUDIOSTREAM_MAGIC        UINT32_C(0x16850331)
/** Value for DRVAUDIOSTREAM::uMagic after destruction */
#define DRVAUDIOSTREAM_MAGIC_DEAD   UINT32_C(0x17500728)


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
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /** Interface used by the host backend. */
    PDMIAUDIONOTIFYFROMHOST IAudioNotifyFromHost;
#endif
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to audio driver below us. */
    PPDMIHOSTAUDIO          pHostDrvAudio;
    /** List of audio streams (DRVAUDIOSTREAM). */
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
        /** The driver's output confguration (tweakable via CFGM). */
        DRVAUDIOCFG         Cfg;
        /** Number of times underruns triggered re-(pre-)buffering. */
        STAMCOUNTER         StatsReBuffering;
    } Out;

    /** Handle to the disable-iteration timer. */
    TMTIMERHANDLE           hTimer;
    /** Set if hTimer is armed. */
    bool volatile           fTimerArmed;
    /** Unique name for the the disable-iteration timer.  */
    char                    szTimerName[23];

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

static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamCreateInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq);
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);
static void drvAudioStreamFree(PDRVAUDIOSTREAM pStream);
static int drvAudioStreamUninitInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, bool fWorkMixBuf);
static int drvAudioStreamPlayLocked(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t *pcFramesPlayed);
static void drvAudioStreamDropInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);
static void drvAudioStreamResetInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);


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
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    LogFlowFunc(("[%s] enmStreamCmd=%s\n", pStream->szName, PDMAudioStrmCmdGetName(enmStreamCmd)));

    rc = drvAudioStreamControlInternal(pThis, pStreamEx, enmStreamCmd);

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}

/**
 * Controls an audio stream.
 *
 * @returns VBox status code.
 * @param   pThis           Pointer to driver instance.
 * @param   pStreamEx       Stream to control.
 * @param   enmStreamCmd    Control command.
 */
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtr(pThis);
    AssertPtr(pStreamEx);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFunc(("[%s] enmStreamCmd=%s fStatus=%s\n", pStreamEx->Core.szName, PDMAudioStrmCmdGetName(enmStreamCmd),
             dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (!(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED))
            {
                /* Is a pending disable outstanding? Then disable first. */
                if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);

                if (RT_SUCCESS(rc))
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);

                if (RT_SUCCESS(rc))
                    pStreamEx->Core.fStatus |= PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED)
            {
                /*
                 * For playback (output) streams first mark the host stream as pending disable,
                 * so that the rest of the remaining audio data will be played first before
                 * closing the stream.
                 */
                if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
                {
                    LogFunc(("[%s] Pending disable/pause\n", pStreamEx->Core.szName));
                    pStreamEx->Core.fStatus |= PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE;

                    /* Schedule a follow up timer to the pending-disable state.  We cannot rely
                       on the device to provide further callouts to finish the state transition.
                       10ms is taking out of thin air and may be too course grained, we should
                       really consider the amount of unplayed buffer in the backend and what not... */
                    if (!pThis->fTimerArmed)
                    {
                        LogFlowFunc(("Arming emergency pending-disable hack...\n"));
                        int rc2 = PDMDrvHlpTimerSetMillies(pThis->pDrvIns, pThis->hTimer, 10 /*ms*/);
                        AssertRC(rc2);
                        pThis->fTimerArmed = true;
                    }
                }

                /* Can we close the host stream as well (not in pending disable mode)? */
                if (!(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE))
                {
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                        drvAudioStreamResetInternal(pThis, pStreamEx);
                }
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (!(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED))
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_PAUSE);
                if (RT_SUCCESS(rc))
                    pStreamEx->Core.fStatus |= PDMAUDIOSTREAMSTS_FLAGS_PAUSED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED)
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_RESUME);
                if (RT_SUCCESS(rc))
                    pStreamEx->Core.fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_PAUSED;
            }
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStreamEx->Core.szName, rc));

    return rc;
}

/**
 * Controls a stream's backend.
 *
 * If the stream has no backend available, VERR_NOT_FOUND is returned
 * (bird: actually the code returns VINF_SUCCESS).
 *
 * @returns VBox status code.
 * @param   pThis           Pointer to driver instance.
 * @param   pStreamEx       Stream to control.
 * @param   enmStreamCmd    Control command.
 */
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtr(pThis);
    AssertPtr(pStreamEx);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFlowFunc(("[%s] enmStreamCmd=%s, fStatus=%s\n", pStreamEx->Core.szName, PDMAudioStrmCmdGetName(enmStreamCmd),
                 dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));

    if (!pThis->pHostDrvAudio) /* If not lower driver is configured, bail out. */
        return VINF_SUCCESS;


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
    const bool fEnabled =    (   pStreamEx->Core.enmDir == PDMAUDIODIR_IN
                              && pThis->In.fEnabled)
                          || (   pStreamEx->Core.enmDir == PDMAUDIODIR_OUT
                              && pThis->Out.fEnabled);

    LogRel2(("Audio: %s stream '%s' in backend (%s is %s)\n", PDMAudioStrmCmdGetName(enmStreamCmd), pStreamEx->Core.szName,
                                                              PDMAudioDirGetName(pStreamEx->Core.enmDir),
                                                              fEnabled ? "enabled" : "disabled"));
    int rc = VINF_SUCCESS;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (fEnabled)
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend, PDMAUDIOSTREAMCMD_ENABLE);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend, PDMAUDIOSTREAMCMD_DISABLE);
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (fEnabled) /* Needed, as resume below also is being checked for. */
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend, PDMAUDIOSTREAMCMD_PAUSE);
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (fEnabled)
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend, PDMAUDIOSTREAMCMD_RESUME);
            break;
        }

        case PDMAUDIOSTREAMCMD_DRAIN:
        {
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend, PDMAUDIOSTREAMCMD_DRAIN);
            break;
        }

        default:
            AssertMsgFailedReturn(("Command %RU32 not implemented\n", enmStreamCmd), VERR_INTERNAL_ERROR_2);
    }

    if (RT_FAILURE(rc))
    {
        if (   rc != VERR_NOT_IMPLEMENTED
            && rc != VERR_NOT_SUPPORTED
            && rc != VERR_AUDIO_STREAM_NOT_READY)
        {
            LogRel(("Audio: %s stream '%s' failed with %Rrc\n", PDMAudioStrmCmdGetName(enmStreamCmd), pStreamEx->Core.szName, rc));
        }

        LogFunc(("[%s] %s failed with %Rrc\n", pStreamEx->Core.szName, PDMAudioStrmCmdGetName(enmStreamCmd), rc));
    }

    return rc;
}

/**
 * Frees an audio stream and its allocated resources.
 *
 * @param   pStreamEx   Audio stream to free. After this call the pointer will
 *                      not be valid anymore.
 */
static void drvAudioStreamFree(PDRVAUDIOSTREAM pStreamEx)
{
    if (pStreamEx)
    {
        LogFunc(("[%s]\n", pStreamEx->Core.szName));
        Assert(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC);
        Assert(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC);

        pStreamEx->Core.uMagic    = ~PDMAUDIOSTREAM_MAGIC;
        pStreamEx->pBackend       = NULL;
        pStreamEx->uMagic         = DRVAUDIOSTREAM_MAGIC_DEAD;

        RTMemFree(pStreamEx);
    }
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * Schedules a re-initialization of all current audio streams.
 * The actual re-initialization will happen at some later point in time.
 *
 * @param   pThis               Pointer to driver instance.
 */
static void drvAudioScheduleReInitInternal(PDRVAUDIO pThis)
{
    LogFunc(("\n"));

    /* Mark all host streams to re-initialize. */
    PDRVAUDIOSTREAM pStreamEx;
    RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
    {
        pStreamEx->Core.fStatus        |= PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT;
        pStreamEx->cTriesReInit    = 0;
        pStreamEx->nsLastReInit         = 0;
    }

# ifdef VBOX_WITH_AUDIO_ENUM
    /* Re-enumerate all host devices as soon as possible. */
    pThis->fEnumerateDevices = true;
# endif
}
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

/**
 * Re-initializes an audio stream with its existing host and guest stream
 * configuration.
 *
 * This might be the case if the backend told us we need to re-initialize
 * because something on the host side has changed.
 *
 * @note    Does not touch the stream's status flags.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to re-initialize.
 */
static int drvAudioStreamReInitInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    AssertPtr(pThis);
    AssertPtr(pStreamEx);

    LogFlowFunc(("[%s]\n", pStreamEx->Core.szName));

    /*
     * Gather current stream status.
     */
    const bool fIsEnabled = RT_BOOL(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED); /* Stream is enabled? */

    /*
     * Destroy and re-create stream on backend side.
     */
    int rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
    {
        rc = drvAudioStreamDestroyInternalBackend(pThis, pStreamEx);
        if (RT_SUCCESS(rc))
        {
            PDMAUDIOSTREAMCFG CfgHostAcq;
            rc = drvAudioStreamCreateInternalBackend(pThis, pStreamEx, &pStreamEx->Host.Cfg, &CfgHostAcq);
            /** @todo Validate (re-)acquired configuration with pStreamEx->Core.Host.Cfg? */
            if (RT_SUCCESS(rc))
            {
#ifdef LOG_ENABLED
                LogFunc(("[%s] Acquired host format:\n",  pStreamEx->Core.szName));
                PDMAudioStrmCfgLog(&CfgHostAcq);
#endif
            }
        }
    }

    /* Drop all old data. */
    drvAudioStreamDropInternal(pThis, pStreamEx);

    /*
     * Restore previous stream state.
     */
    if (fIsEnabled)
        rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Re-initializing stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));

    LogFunc(("[%s] Returning %Rrc\n", pStreamEx->Core.szName, rc));
    return rc;
}

/**
 * Drops all audio data (and associated state) of a stream.
 *
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to drop data for.
 */
static void drvAudioStreamDropInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    RT_NOREF(pThis);

    LogFunc(("[%s]\n", pStreamEx->Core.szName));

    if (pStreamEx->fNoMixBufs)
    {
        AudioMixBufReset(&pStreamEx->Guest.MixBuf);
        AudioMixBufReset(&pStreamEx->Host.MixBuf);
    }

    pStreamEx->fThresholdReached    = false;
    pStreamEx->nsLastIterated       = 0;
    pStreamEx->nsLastPlayedCaptured = 0;
    pStreamEx->nsLastReadWritten    = 0;
}

/**
 * Resets the given audio stream.
 *
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to reset.
 */
static void drvAudioStreamResetInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    drvAudioStreamDropInternal(pThis, pStreamEx);

    LogFunc(("[%s]\n", pStreamEx->Core.szName));

    pStreamEx->Core.fStatus        = PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;
    pStreamEx->Core.fWarningsShown = PDMAUDIOSTREAM_WARN_FLAGS_NONE;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Reset statistics.
     */
    if (pStreamEx->Core.enmDir == PDMAUDIODIR_IN)
    {
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalFramesCaptured);
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalFramesRead);
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalTimesCaptured);
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalTimesRead);
    }
    else if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
    {
        STAM_COUNTER_RESET(&pStreamEx->Out.Stats.TotalFramesPlayed);
        STAM_COUNTER_RESET(&pStreamEx->Out.Stats.TotalFramesWritten);
        STAM_COUNTER_RESET(&pStreamEx->Out.Stats.TotalTimesPlayed);
        STAM_COUNTER_RESET(&pStreamEx->Out.Stats.TotalTimesWritten);
    }
    else
        AssertFailed();
#endif
}


/**
 * The no-mixing-buffers worker for drvAudioStreamWrite and
 * drvAudioStreamIterateInternal.
 *
 * The buffer is NULL and has a zero length when called from the interate
 * function.  This only occures when there is pre-buffered audio data that need
 * to be pushed to the backend due to a pending disabling of the stream.
 *
 * Caller owns the lock.
 */
static int drvAudioStreamWriteNoMixBufs(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx,
                                        const uint8_t *pbBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    Log3Func(("%s: @%#RX64: cbBuf=%#x\n", pStreamEx->Core.szName, pStreamEx->offInternal, cbBuf));

    /*
     * Are we pre-buffering?
     *
     * Note! We do not restart pre-buffering in this version, as we'd
     *       need some kind of cooperation with the backend buffer
     *       managment to correctly detect an underrun.
     */
    bool     fJustStarted = false;
    uint32_t cbWritten = 0;
    int      rc;
    if (   pStreamEx->fThresholdReached
        && pStreamEx->Out.cbPreBuffered == 0)
    {
        /* not-prebuffering, likely after a while at least */
        rc = VINF_SUCCESS;
    }
    else
    {
        /*
         * Copy as much as we can to the pre-buffer.
         */
        uint32_t cbFree = pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBuffered;
        AssertReturn((int32_t)cbFree >= 0, VERR_INTERNAL_ERROR_2);
        if (cbFree > 0 && cbBuf > 0)
        {
            cbWritten = RT_MIN(cbFree, cbBuf);
            cbWritten = PDMAudioPropsFloorBytesToFrame(&pStreamEx->Core.Props, cbWritten);
            memcpy(&pStreamEx->Out.pbPreBuf[pStreamEx->Out.cbPreBuffered], pbBuf, cbWritten);
            pStreamEx->Out.cbPreBuffered += cbWritten;
            cbBuf                        -= cbWritten;
            pbBuf                        += cbWritten;
            pStreamEx->offInternal       += cbWritten;
        }

        /*
         * Get the special case of buggy backend drivers out of the way.
         * We get here if we couldn't write out all the pre-buffered data when
         * we hit the threshold.
         */
        if (pStreamEx->fThresholdReached)
            LogRel2(("Audio: @%#RX64: Stream '%s' pre-buffering commit problem: cbBuf=%#x cbPreBuffered=%#x\n",
                     pStreamEx->offInternal, pStreamEx->Core.szName, cbBuf, pStreamEx->Out.cbPreBuffered));
        /*
         * Did we reach the backend's playback (pre-buffering) threshold?
         * Can be 0 if no pre-buffering desired.
         */
        else if (pStreamEx->Out.cbPreBuffered + cbBuf >= pStreamEx->Out.cbPreBufThreshold)
        {
            LogRel2(("Audio: @%#RX64: Stream '%s' buffering complete! (%#x + %#x bytes)\n",
                     pStreamEx->offInternal, pStreamEx->Core.szName, pStreamEx->Out.cbPreBuffered, cbBuf));
            pStreamEx->fThresholdReached = fJustStarted = true;
        }
        /*
         * Some audio files are shorter than the pre-buffering level (e.g. the
         * "click" Explorer sounds on some Windows guests), so make sure that we
         * also play those by checking if the stream already is pending disable
         * mode, even if we didn't hit the pre-buffering watermark yet.
         *
         * Try play "Windows Navigation Start.wav" on Windows 7 (2824 samples).
         */
        else if (   (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
                 && pStreamEx->Out.cbPreBuffered > 0)
        {
            LogRel2(("Audio: @%#RX64: Stream '%s' buffering complete - short sound! (%#x + %#x bytes)\n",
                     pStreamEx->offInternal, pStreamEx->Core.szName, pStreamEx->Out.cbPreBuffered, cbBuf));
            pStreamEx->fThresholdReached = fJustStarted = true;
        }
        /*
         * Not yet, so still buffering audio data.
         */
        else
        {
            LogRel2(("Audio: @%#RX64: Stream '%s' is buffering (%RU8%% complete)...\n", pStreamEx->offInternal,
                     pStreamEx->Core.szName, (100 * pStreamEx->Out.cbPreBuffered) / pStreamEx->Out.cbPreBufThreshold));
            Assert(cbBuf == 0);
            *pcbWritten = cbWritten;
            return VINF_SUCCESS;
        }

        /*
         * Write the pre-buffered chunk.
         */
        uint32_t off = 0;
        uint32_t cbPreBufWritten;
        do
        {
            cbPreBufWritten = 0;
            rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pStreamEx->pBackend, &pStreamEx->Out.pbPreBuf[off],
                                                     pStreamEx->Out.cbPreBuffered - off, &cbPreBufWritten);
            AssertRCBreak(rc);
            off += cbPreBufWritten;
        } while (off < pStreamEx->Out.cbPreBuffered && cbPreBufWritten != 0);

        if (off >= pStreamEx->Out.cbPreBuffered)
        {
            Assert(off == pStreamEx->Out.cbPreBuffered);
            LogFunc(("@%#RX64: Wrote all %#x bytes of pre-buffered audio data.\n", pStreamEx->offInternal, off));
            pStreamEx->Out.cbPreBuffered = 0;
        }
        else
        {
            LogRel2(("Audio: @%#RX64: Stream '%s' pre-buffering commit problem: wrote %#x out of %#x + %#x%s - rc=%Rrc *pcbWritten=%#x\n",
                     pStreamEx->offInternal, pStreamEx->Core.szName, off, pStreamEx->Out.cbPreBuffered, cbBuf,
                     fJustStarted ? " (just started)" : "", rc, cbWritten));
            AssertMsg(!fJustStarted || RT_FAILURE(rc),
                      ("Buggy host driver buffer reporting: off=%#x cbPreBuffered=%#x\n", off, pStreamEx->Out.cbPreBuffered));
            if (off > 0)
            {
                memmove(pStreamEx->Out.pbPreBuf, &pStreamEx->Out.pbPreBuf[off], pStreamEx->Out.cbPreBuffered - off);
                pStreamEx->Out.cbPreBuffered -= off;
            }
            pStreamEx->nsLastPlayedCaptured = RTTimeNanoTS();
            *pcbWritten = cbWritten;
            return cbWritten ? VINF_SUCCESS : rc;
        }

        if (RT_FAILURE(rc))
        {
            *pcbWritten = cbWritten;
            return rc;
        }
    }

    /*
     * Do the writing.
     */
    uint32_t cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    pStreamEx->Out.Stats.cbBackendWritableBefore = cbWritable;

    uint8_t const cbFrame = PDMAudioPropsFrameSize(&pStreamEx->Core.Props);
    while (cbBuf >= cbFrame && cbWritable >= cbFrame)
    {
        uint32_t const cbToWrite    = PDMAudioPropsFloorBytesToFrame(&pStreamEx->Core.Props, RT_MIN(cbBuf, cbWritable));
        uint32_t       cbWrittenNow = 0;
        rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pStreamEx->pBackend, pbBuf, cbToWrite, &cbWrittenNow);
        if (RT_SUCCESS(rc))
        {
            if (cbWrittenNow != cbToWrite)
                Log3Func(("%s: @%#RX64: Wrote less bytes than requested: %#x, requested %#x\n",
                          pStreamEx->Core.szName, pStreamEx->offInternal, cbWrittenNow, cbToWrite));
#ifdef DEBUG_bird
            Assert(cbWrittenNow == cbToWrite);
#endif
            AssertStmt(cbWrittenNow <= cbToWrite, cbWrittenNow = cbToWrite);
            cbWritten += cbWrittenNow;
            cbBuf     -= cbWrittenNow;
            pbBuf     += cbWrittenNow;
            pStreamEx->offInternal += cbWrittenNow;
        }
        else
        {
            *pcbWritten = cbWritten;
            LogFunc(("%s: @%#RX64: pfnStreamPlay failed writing %#x bytes (%#x previous written, %#x writable): %Rrc\n",
                     pStreamEx->Core.szName, pStreamEx->offInternal, cbToWrite, cbWritten, cbWritable, rc));
            return cbWritten ? VINF_SUCCESS : rc;
        }
        cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    }

    *pcbWritten = cbWritten;
    pStreamEx->Out.Stats.cbBackendWritableAfter = cbWritable;
    if (cbWritten)
        pStreamEx->nsLastPlayedCaptured = RTTimeNanoTS();

    Log3Func(("%s: @%#RX64: Wrote %#x bytes (%#x bytes left)\n", pStreamEx->Core.szName, pStreamEx->offInternal, cbWritten, cbBuf));
    return rc;
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamWrite}
 */
static DECLCALLBACK(int) drvAudioStreamWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                             const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /*
     * Check input and sanity.
     */
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    uint32_t uTmp;
    if (!pcbWritten)
        pcbWritten = &uTmp;
    AssertPtrReturn(pcbWritten, VERR_INVALID_PARAMETER);

    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertMsg(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be written to (direction is '%s')\n",
               pStreamEx->Core.szName, PDMAudioDirGetName(pStreamEx->Core.enmDir)));

    AssertMsg(PDMAudioPropsIsSizeAligned(&pStreamEx->Guest.Cfg.Props, cbBuf),
              ("Stream '%s' got a non-frame-aligned write (%RU32 bytes)\n", pStreamEx->Core.szName, cbBuf));

    STAM_PROFILE_ADV_START(&pThis->Stats.DelayOut, out); /* (stopped in drvAudioStreamPlayLocked) */

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * First check that we can write to the stream, and if not,
     * whether to just drop the input into the bit bucket.
     */
    if (PDMAudioStrmStatusIsReady(pStreamEx->Core.fStatus))
    {
        if (   !pThis->Out.fEnabled         /* (see @bugref{9882}) */
            || pThis->pHostDrvAudio == NULL /* (we used to work this condition differently) */
            || !PDMAudioStrmStatusCanWrite(pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pStreamEx->pBackend)))
        {
            Log3Func(("[%s] Backend stream %s, discarding the data\n", pStreamEx->Core.szName,
                      !pThis->Out.fEnabled ? "disabled" : !pThis->pHostDrvAudio ? "not attached" : "not ready yet"));
            *pcbWritten = cbBuf;
            pStreamEx->offInternal += cbBuf;
        }
        /*
         * No-mixing buffer mode:  Write the data directly to the backend, unless
         * we're prebuffering.  There will be no pfnStreamPlay call in this mode.
         */
        else if (pStreamEx->fNoMixBufs)
        {
            uint64_t offInternalBefore = pStreamEx->offInternal; RT_NOREF(offInternalBefore);
            rc = drvAudioStreamWriteNoMixBufs(pThis, pStreamEx, (uint8_t const *)pvBuf, cbBuf, pcbWritten);
            Assert(offInternalBefore + *pcbWritten == pStreamEx->offInternal);
            if (!pThis->Out.Cfg.Dbg.fEnabled || RT_FAILURE(rc))
            { /* likely */ }
            else
                AudioHlpFileWrite(pStreamEx->Out.Dbg.pFilePlayNonInterleaved, pvBuf, *pcbWritten, 0 /* fFlags */);
        }
        /*
         * Legacy mode:  Here we just dump the data in the guest side mixing buffer
         * and then mixes it into the host side buffer.  Later the device code will
         * make a pfnStreamPlay call which  recodes the data from the host side
         * buffer and writes it to the host backend.
         */
        else
        {
            uint32_t cbWrittenTotal = 0;

            const uint32_t cbFree = AudioMixBufFreeBytes(&pStreamEx->Host.MixBuf);
            if (cbFree < cbBuf)
                LogRel2(("Audio: Lost audio output (%RU64ms, %RU32 free but needs %RU32) due to full host stream buffer '%s'\n",
                         PDMAudioPropsBytesToMilli(&pStreamEx->Host.Cfg.Props, cbBuf - cbFree), cbFree, cbBuf, pStreamEx->Core.szName));

            uint32_t cbToWrite = RT_MIN(cbBuf, cbFree);
            if (cbToWrite)
            {
                /* We use the guest side mixing buffer as an intermediate buffer to do some
                 * (first) processing (if needed), so always write the incoming data at offset 0. */
                uint32_t cFramesGstWritten = 0;
                rc = AudioMixBufWriteAt(&pStreamEx->Guest.MixBuf, 0 /* offFrames */, pvBuf, cbToWrite, &cFramesGstWritten);
                if (RT_SUCCESS(rc) && cFramesGstWritten > 0)
                {
                    if (pThis->Out.Cfg.Dbg.fEnabled)
                        AudioHlpFileWrite(pStreamEx->Out.Dbg.pFileStreamWrite, pvBuf, cbToWrite, 0 /* fFlags */);

                    uint32_t cFramesGstMixed = 0;
                    if (cFramesGstWritten)
                    {
                        int rc2 = AudioMixBufMixToParentEx(&pStreamEx->Guest.MixBuf, 0 /* cSrcOffset */,
                                                           cFramesGstWritten /* cSrcFrames */, &cFramesGstMixed /* pcSrcMixed */);
                        if (RT_SUCCESS(rc2))
                        {
                            const uint64_t tsNowNs = RTTimeNanoTS();

                            Log3Func(("[%s] Writing %RU32 frames (%RU64ms)\n",
                                      pStreamEx->Core.szName, cFramesGstWritten, PDMAudioPropsFramesToMilli(&pStreamEx->Guest.Cfg.Props, cFramesGstWritten)));

                            Log3Func(("[%s] Last written %RU64ns (%RU64ms), now filled with %RU64ms -- %RU8%%\n",
                                      pStreamEx->Core.szName, tsNowNs - pStreamEx->nsLastReadWritten,
                                      (tsNowNs - pStreamEx->nsLastReadWritten) / RT_NS_1MS,
                                      PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, AudioMixBufUsed(&pStreamEx->Host.MixBuf)),
                                      AudioMixBufUsed(&pStreamEx->Host.MixBuf) * 100 / AudioMixBufSize(&pStreamEx->Host.MixBuf)));

                            pStreamEx->nsLastReadWritten = tsNowNs;
                            /* Keep going. */
                        }
                        else
                        {
                            AssertMsgFailed(("[%s] Mixing failed: cbToWrite=%RU32, cfWritten=%RU32, cfMixed=%RU32, rc=%Rrc\n",
                                             pStreamEx->Core.szName, cbToWrite, cFramesGstWritten, cFramesGstMixed, rc2));
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                        }

                        cbWrittenTotal = AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cFramesGstWritten);

                        STAM_COUNTER_ADD(&pThis->Stats.TotalFramesWritten,  cFramesGstWritten);
                        STAM_COUNTER_ADD(&pThis->Stats.TotalFramesMixedOut, cFramesGstMixed);
                        Assert(cFramesGstWritten >= cFramesGstMixed);
                        STAM_COUNTER_ADD(&pThis->Stats.TotalFramesLostOut,  cFramesGstWritten - cFramesGstMixed);
                        STAM_COUNTER_ADD(&pThis->Stats.TotalBytesWritten,   cbWrittenTotal);

                        STAM_COUNTER_ADD(&pStreamEx->Out.Stats.TotalFramesWritten, cFramesGstWritten);
                        STAM_COUNTER_INC(&pStreamEx->Out.Stats.TotalTimesWritten);
                    }

                    Log3Func(("[%s] Dbg: cbBuf=%RU32, cbToWrite=%RU32, cfHstUsed=%RU32, cfHstfLive=%RU32, cFramesGstWritten=%RU32, "
                              "cFramesGstMixed=%RU32, cbWrittenTotal=%RU32, rc=%Rrc\n",
                              pStreamEx->Core.szName, cbBuf, cbToWrite, AudioMixBufUsed(&pStreamEx->Host.MixBuf),
                              AudioMixBufLive(&pStreamEx->Host.MixBuf), cFramesGstWritten, cFramesGstMixed, cbWrittenTotal, rc));
                }
                else
                    AssertMsgFailed(("[%s] Write failed: cbToWrite=%RU32, cFramesGstWritten=%RU32, rc=%Rrc\n",
                                     pStreamEx->Core.szName, cbToWrite, cFramesGstWritten, rc));
            }
            else
                rc = VERR_BUFFER_OVERFLOW;
            *pcbWritten = cbWrittenTotal;
            pStreamEx->offInternal += cbWrittenTotal;
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRetain}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRetain(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, UINT32_MAX);
   AssertPtrReturn(pStream,    UINT32_MAX);
   AssertReturn(pStream->uMagic == PDMAUDIOSTREAM_MAGIC, UINT32_MAX);
   AssertReturn(((PDRVAUDIOSTREAM)pStream)->uMagic == DRVAUDIOSTREAM_MAGIC, UINT32_MAX);
   RT_NOREF(pInterface);

   uint32_t const cRefs = ASMAtomicIncU32(&pStream->cRefs);
   Assert(cRefs > 1);
   Assert(cRefs < _1K);

   return cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRelease}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRelease(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, UINT32_MAX);
   AssertPtrReturn(pStream,    UINT32_MAX);
   AssertReturn(pStream->uMagic == PDMAUDIOSTREAM_MAGIC, UINT32_MAX);
   AssertReturn(((PDRVAUDIOSTREAM)pStream)->uMagic == DRVAUDIOSTREAM_MAGIC, UINT32_MAX);
   RT_NOREF(pInterface);

   uint32_t cRefs = ASMAtomicDecU32(&pStream->cRefs);
   AssertStmt(cRefs >= 1, cRefs = ASMAtomicIncU32(&pStream->cRefs));
   Assert(cRefs < _1K);

   return cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioStreamIterate(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = drvAudioStreamIterateInternal(pThis, pStreamEx, false /*fWorkMixBuf*/); /** @todo r=bird: why didn't it work the mixing buffer initially.  We can probably set this to true...  It may cause repeat work though. */

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
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to check and maybe re-initialize.
 */
static void drvAudioStreamMaybeReInit(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT)
    {
        const unsigned cMaxTries = 3; /** @todo Make this configurable? */
        const uint64_t tsNowNs   = RTTimeNanoTS();

        /* Throttle re-initializing streams on failure. */
        if (   pStreamEx->cTriesReInit < cMaxTries
            && tsNowNs - pStreamEx->nsLastReInit >= RT_NS_1SEC * pStreamEx->cTriesReInit) /** @todo Ditto. */
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

            int rc = drvAudioStreamReInitInternal(pThis, pStreamEx);
            if (RT_FAILURE(rc))
            {
                pStreamEx->cTriesReInit++;
                pStreamEx->nsLastReInit = tsNowNs;
            }
            else
            {
                /* Remove the pending re-init flag on success. */
                pStreamEx->Core.fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT;
            }
        }
        else
        {
            /* Did we exceed our tries re-initializing the stream?
             * Then this one is dead-in-the-water, so disable it for further use. */
            if (pStreamEx->cTriesReInit == cMaxTries)
            {
                LogRel(("Audio: Re-initializing stream '%s' exceeded maximum retries (%u), leaving as disabled\n",
                        pStreamEx->Core.szName, cMaxTries));

                /* Don't try to re-initialize anymore and mark as disabled. */
                pStreamEx->Core.fStatus &= ~(PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT | PDMAUDIOSTREAMSTS_FLAGS_ENABLED);

                /* Note: Further writes to this stream go to / will be read from the bit bucket (/dev/null) from now on. */
            }
        }

#ifdef LOG_ENABLED
        char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
        Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));
    }
}

/**
 * Does one iteration of an audio stream.
 *
 * This function gives the backend the chance of iterating / altering data and
 * does the actual mixing between the guest <-> host mixing buffers.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to iterate.
 * @param   fWorkMixBuf Push data from the mixing buffer to the backend.
 * @todo    r=bird: Don't know why the default behavior isn't to push data into
 *          the backend...  We'll never get out of the pending-disable state if
 *          the mixing buffer doesn't empty out.
 */
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, bool fWorkMixBuf)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pThis->pHostDrvAudio)
        return VINF_SUCCESS;

    /* Is the stream scheduled for re-initialization? Do so now. */
    drvAudioStreamMaybeReInit(pThis, pStreamEx);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));

    /* Not enabled or paused? Skip iteration. */
    if (   !(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED)
        ||  (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED))
    {
        return VINF_SUCCESS;
    }

    /*
     * Pending disable is really what we're here for.  This only happens to output streams.
     */
    int rc = VINF_SUCCESS;
    if (!(pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE))
    { /* likely until we get to the end of the stream at least. */ }
    else
    {
        AssertReturn(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT, VINF_SUCCESS);
        /** @todo Add a timeout to these proceedings.  A few that of the reported buffer
         *        size. */

        /*
         * Check if we have any data we need to write to the backend, try
         * move it now.
         */
        /** @todo r=bird: It is possible the device has data buffered (e.g.
         *        internal DMA buffer (HDA) or mixing buffer (HDA + AC'97).  We're
         *        not taking that into account here.  I also suspect that neither is
         *        the device/mixer code, and that if the guest is too quick disabling
         *        the stream, it will just remain there till the next time something
         *        is played.  That means that this code and associated timer hack
         *        should probably not be here at all. */
        uint32_t cFramesLive;
        if (pStreamEx->fNoMixBufs)
        {
            cFramesLive = pStreamEx->Out.cbPreBuffered;
            if (cFramesLive > 0)
            {
                uint32_t cbIgnored = 0;
                drvAudioStreamWriteNoMixBufs(pThis, pStreamEx, NULL, 0, &cbIgnored);
                cFramesLive = PDMAudioPropsBytesToFrames(&pStreamEx->Core.Props, pStreamEx->Out.cbPreBuffered);
            }
        }
        else
        {
            cFramesLive = AudioMixBufLive(&pStreamEx->Host.MixBuf);
            if (cFramesLive > 0 && fWorkMixBuf)
            {
                uint32_t cIgnored = 0;
                drvAudioStreamPlayLocked(pThis, pStreamEx, &cIgnored);

                cFramesLive = AudioMixBufLive(&pStreamEx->Host.MixBuf);
            }
        }
        Log3Func(("[%s] cFramesLive=%RU32\n", pStreamEx->Core.szName, cFramesLive));
        if (cFramesLive == 0)
        {
            /*
             * Tell the backend to start draining the stream, that is,
             * play the remaining buffered data and stop.
             */
            rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DRAIN);
            if (rc == VERR_NOT_SUPPORTED) /* Not all backends support draining yet. */
                rc = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
            {
                /*
                 * Before we disable the stream, check if the backend has
                 * finished playing the buffered data.
                 */
                uint32_t cbPending;
                if (!pThis->pHostDrvAudio->pfnStreamGetPending) /* Optional. */
                    cbPending = 0;
                else
                {
                    cbPending = pThis->pHostDrvAudio->pfnStreamGetPending(pThis->pHostDrvAudio, pStreamEx->pBackend);
                    Log3Func(("[%s] cbPending=%RU32 (%#RX32)\n", pStreamEx->Core.szName, cbPending, cbPending));
                }
                if (cbPending == 0)
                {
                    /*
                     * Okay, disable it.
                     */
                    LogFunc(("[%s] Closing pending stream\n", pStreamEx->Core.szName));
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                    {
                        pStreamEx->Core.fStatus &= ~(PDMAUDIOSTREAMSTS_FLAGS_ENABLED | PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE);
                        drvAudioStreamDropInternal(pThis, pStreamEx); /* Not a DROP command, just a stream reset. */
                    }
                    else
                        LogFunc(("[%s] Backend vetoed against closing pending input stream, rc=%Rrc\n", pStreamEx->Core.szName, rc));
                }
            }
        }

    }

    /* Update timestamps. */
    pStreamEx->nsLastIterated = RTTimeNanoTS();

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStreamEx->Core.szName, rc));

    return rc;
}

/**
 * @callback_method_impl{FNTMTIMERDRV}
 */
static DECLCALLBACK(void) drvAudioEmergencyIterateTimer(PPDMDRVINS pDrvIns, TMTIMERHANDLE hTimer, void *pvUser)
{
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    RT_NOREF(hTimer, pvUser);
    RTCritSectEnter(&pThis->CritSect);

    /*
     * Iterate any stream with the pending-disable flag set.
     */
    uint32_t        cMilliesToNext = 0;
    PDRVAUDIOSTREAM pStreamEx, pStreamExNext;
    RTListForEachSafe(&pThis->lstStreams, pStreamEx, pStreamExNext, DRVAUDIOSTREAM, ListEntry)
    {
        if (   pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC
            && pStreamEx->Core.cRefs >= 1)
        {
            if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
            {
                drvAudioStreamIterateInternal(pThis, pStreamEx, true /*fWorkMixBuf*/);

                if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
                    cMilliesToNext = 10;
            }
        }
    }

    /*
     * Re-arm the timer if we still got streams in the pending state.
     */
    if (cMilliesToNext)
    {
        pThis->fTimerArmed = true;
        PDMDrvHlpTimerSetMillies(pDrvIns, pThis->hTimer, cMilliesToNext);
    }
    else
        pThis->fTimerArmed = false;

    RTCritSectLeave(&pThis->CritSect);
}

/**
 * Worker for drvAudioStreamPlay that does the actual playing.
 *
 * @returns VBox status code.
 * @param   pThis           The audio driver instance data.
 * @param   pStreamEx       The stream to play.
 * @param   cFramesToPlay   Number of audio frames to play.  The backend is
 *                          supposed to have buffer space for this.
 * @param   pcFramesPlayed  Where to return the number of audio frames played.
 */
static int drvAudioStreamPlayDoIt(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t cFramesToPlay, uint32_t *pcFramesPlayed)
{
    Assert(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT);

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
        rc = AudioMixBufAcquireReadBlock(&pStreamEx->Host.MixBuf, abChunk,
                                         RT_MIN(sizeof(abChunk), AUDIOMIXBUF_F2B(&pStreamEx->Host.MixBuf, cFramesLeft)),
                                         &cFramesRead);
        AssertRCBreak(rc);

        uint32_t cbRead = AUDIOMIXBUF_F2B(&pStreamEx->Host.MixBuf, cFramesRead);
        Assert(cbRead <= sizeof(abChunk));

        /*
         * Feed it to the backend.
         */
        uint32_t cFramesPlayed = 0;
        uint32_t cbPlayed      = 0;
        rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pStreamEx->pBackend, abChunk, cbRead, &cbPlayed);
        if (RT_SUCCESS(rc))
        {
            if (pThis->Out.Cfg.Dbg.fEnabled)
                AudioHlpFileWrite(pStreamEx->Out.Dbg.pFilePlayNonInterleaved, abChunk, cbPlayed, 0 /* fFlags */);

            if (cbRead != cbPlayed)
                LogRel2(("Audio: Host stream '%s' played wrong amount (%RU32 bytes read but played %RU32)\n",
                         pStreamEx->Core.szName, cbRead, cbPlayed));

            cFramesPlayed  = AUDIOMIXBUF_B2F(&pStreamEx->Host.MixBuf, cbPlayed);
            AssertStmt(cFramesLeft >= cFramesPlayed, cFramesPlayed = cFramesLeft);
            cFramesLeft   -= cFramesPlayed;
        }

        AudioMixBufReleaseReadBlock(&pStreamEx->Host.MixBuf, cFramesPlayed);

        AssertRCBreak(rc); /* (this is here for Acquire/Release symmetry - which isn't at all necessary) */
        AssertBreak(cbPlayed > 0); /* (ditto) */
    }

    Log3Func(("[%s] Played %RU32/%RU32 frames, rc=%Rrc\n", pStreamEx->Core.szName, cFramesToPlay - cFramesLeft, cFramesToPlay, rc));
    *pcFramesPlayed = cFramesToPlay - cFramesLeft;
    return rc;
}

/**
 * Worker for drvAudioStreamPlay.
 */
static int drvAudioStreamPlayLocked(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t *pcFramesPlayed)
{
    /*
     * Zero the frame count so we can return at will.
     */
    *pcFramesPlayed = 0;

    PDMAUDIOSTREAMSTS fStrmStatus = pStreamEx->Core.fStatus;
#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] Start fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, fStrmStatus)));

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
    uint32_t const cFramesLive = AudioMixBufLive(&pStreamEx->Host.MixBuf);
    Log3Func(("[%s] Last played %'RI64 ns ago; filled with %u frm / %RU64 ms / %RU8%% total%s\n",
              pStreamEx->Core.szName, pStreamEx->fThresholdReached ? RTTimeNanoTS() - pStreamEx->nsLastPlayedCaptured : -1, cFramesLive,
              PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, cFramesLive),
              (100 * cFramesLive) / AudioMixBufSize(&pStreamEx->Host.MixBuf), pStreamEx->fThresholdReached ? "" : ", pre-buffering"));

    /*
     * Restart pre-buffering if we're having a buffer-underrun.
     */
    if (   cFramesLive != 0              /* no underrun */
        || !pStreamEx->fThresholdReached /* or still pre-buffering. */)
    { /* likely */ }
    else
    {
        /* It's not an underrun if the host audio driver still has an reasonable amount
           buffered.  We don't have a direct way of querying that, so instead we'll use
           some heuristics based on number of writable bytes now compared to when
           prebuffering ended the first time around. */
        uint32_t cbBuffered = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
        if (cbBuffered < pStreamEx->Out.cbBackendMaxWritable)
            cbBuffered = pStreamEx->Out.cbBackendMaxWritable - cbBuffered;
        else
            cbBuffered = 0;
        uint32_t cbMinBuf = PDMAudioPropsMilliToBytes(&pStreamEx->Host.Cfg.Props, pStreamEx->Guest.Cfg.Device.cMsSchedulingHint * 2);
        Log3Func(("Potential underrun: cbBuffered=%#x vs cbMinBuf=%#x\n", cbBuffered, cbMinBuf));
        if (cbBuffered < cbMinBuf)
        {
            LogRel2(("Audio: Buffer underrun for stream '%s' (%RI64 ms since last call, %u buffered)\n",
                     pStreamEx->Core.szName, RTTimeNanoTS() - pStreamEx->nsLastPlayedCaptured, cbBuffered));

            /* Re-enter the pre-buffering stage again if enabled. */
            if (pStreamEx->Host.Cfg.Backend.cFramesPreBuffering > 0)
            {
                pStreamEx->fThresholdReached = false;
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
    if (pStreamEx->fThresholdReached)
    {
        /* not-prebuffering, likely after a while at least */
        cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    }
    else
    {
        /*
         * Did we reach the backend's playback (pre-buffering) threshold?
         * Can be 0 if no pre-buffering desired.
         */
        if (cFramesLive >= pStreamEx->Host.Cfg.Backend.cFramesPreBuffering)
        {
            LogRel2(("Audio: Stream '%s' buffering complete!\n", pStreamEx->Core.szName));
            pStreamEx->fThresholdReached = fJustStarted = true;
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
                 && (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE))
        {
            LogRel2(("Audio: Stream '%s' buffering complete (short sound)!\n", pStreamEx->Core.szName));
            pStreamEx->fThresholdReached = fJustStarted = true;
        }
        /*
         * Not yet, so still buffering audio data.
         */
        else
        {
            LogRel2(("Audio: Stream '%s' is buffering (%RU8%% complete)...\n",
                     pStreamEx->Core.szName, (100 * cFramesLive) / pStreamEx->Host.Cfg.Backend.cFramesPreBuffering));
            return VINF_SUCCESS;
        }

        /* Hack alert! This is for the underrun detection.  */
        cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
        if (cbWritable > pStreamEx->Out.cbBackendMaxWritable)
            pStreamEx->Out.cbBackendMaxWritable = cbWritable;
    }
    pStreamEx->Out.Stats.cbBackendWritableBefore = cbWritable;

    /*
     * Figure out how much to play now.
     * Easy, as much as the host audio backend will allow us to.
     */
    uint32_t cFramesWritable = PDMAUDIOPCMPROPS_B2F(&pStreamEx->Host.Cfg.Props, cbWritable);
    uint32_t cFramesToPlay   = cFramesWritable;
    if (cFramesToPlay > cFramesLive) /* Don't try to play more than available, we don't want to block. */
        cFramesToPlay = cFramesLive;

    Log3Func(("[%s] Playing %RU32 frames (%RU64 ms), now filled with %RU64 ms -- %RU8%%\n",
              pStreamEx->Core.szName, cFramesToPlay, PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, cFramesToPlay),
              PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, AudioMixBufUsed(&pStreamEx->Host.MixBuf)),
              AudioMixBufUsed(&pStreamEx->Host.MixBuf) * 100 / AudioMixBufSize(&pStreamEx->Host.MixBuf)));

    /*
     * Do the playing if we decided to play something.
     */
    int rc;
    if (cFramesToPlay)
    {
        rc = drvAudioStreamPlayDoIt(pThis, pStreamEx, cFramesToPlay, pcFramesPlayed);

        pStreamEx->nsLastPlayedCaptured = RTTimeNanoTS();
        pStreamEx->Out.Stats.cbBackendWritableAfter = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio,
                                                                                                 pStreamEx->pBackend);
    }
    else
        rc = VINF_SUCCESS;

    Log3Func(("[%s] Live=%RU32 fr (%RU64 ms) Period=%RU32 fr (%RU64 ms) Writable=%RU32 fr (%RU64 ms) -> ToPlay=%RU32 fr (%RU64 ms) Played=%RU32 fr (%RU64 ms)%s\n",
              pStreamEx->Core.szName,
              cFramesLive, PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, cFramesLive),
              pStreamEx->Host.Cfg.Backend.cFramesPeriod,
              PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, pStreamEx->Host.Cfg.Backend.cFramesPeriod),
              cFramesWritable, PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, cFramesWritable),
              cFramesToPlay, PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, cFramesToPlay),
              *pcFramesPlayed, PDMAudioPropsFramesToMilli(&pStreamEx->Host.Cfg.Props, *pcFramesPlayed),
              fJustStarted ? "just-started" : ""));
    RT_NOREF(fJustStarted);

    if (RT_SUCCESS(rc))
    {
        AudioMixBufFinish(&pStreamEx->Host.MixBuf, *pcFramesPlayed);

        STAM_PROFILE_ADV_STOP(&pThis->Stats.DelayOut, out);
        STAM_COUNTER_ADD(&pThis->Stats.TotalFramesOut, *pcFramesPlayed);
        STAM_COUNTER_ADD(&pStreamEx->Out.Stats.TotalFramesPlayed, *pcFramesPlayed);
        STAM_COUNTER_INC(&pStreamEx->Out.Stats.TotalTimesPlayed);
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
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pcFramesPlayed, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertMsg(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be played back (direction is 0x%x)\n",
               pStreamEx->Core.szName, pStreamEx->Core.enmDir));
    AssertReturn(!pStreamEx->fNoMixBufs, VERR_INVALID_FUNCTION);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    uint32_t cFramesPlayed = 0;
    rc = drvAudioStreamPlayLocked(pThis, pStreamEx, &cFramesPlayed);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_SUCCESS(rc) && pcFramesPlayed)
        *pcFramesPlayed = cFramesPlayed;

    if (RT_FAILURE(rc))
        LogFlowFunc(("[%s] Failed with %Rrc\n", pStreamEx->Core.szName, rc));
    return rc;
}

/**
 * Captures non-interleaved input from a host stream.
 *
 * @returns VBox status code.
 * @param   pThis       Driver instance.
 * @param   pStreamEx   Stream to capture from.
 * @param   pcfCaptured Number of (host) audio frames captured.
 */
static int drvAudioStreamCaptureNonInterleaved(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t *pcfCaptured)
{
    Assert(pStreamEx->Core.enmDir == PDMAUDIODIR_IN);
    Assert(pStreamEx->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED);

    /*
     * ...
     */
    AssertPtr(pThis->pHostDrvAudio->pfnStreamGetReadable);
    uint32_t cbReadable = pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    if (!cbReadable)
        Log2Func(("[%s] No readable data available\n", pStreamEx->Core.szName));

    uint32_t cbFree = AudioMixBufFreeBytes(&pStreamEx->Guest.MixBuf); /* Parent */
    if (!cbFree)
        Log2Func(("[%s] Buffer full\n", pStreamEx->Core.szName));

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
        rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                    abChunk, RT_MIN(cbReadable, (uint32_t)sizeof(abChunk)), &cbCaptured);
        if (RT_FAILURE(rc))
        {
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
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
        rc = AudioMixBufWriteAt(&pStreamEx->Host.MixBuf, 0 /* offFrames */, abChunk, cbCaptured, &cfHstWritten);
        if (   RT_FAILURE(rc)
            || !cfHstWritten)
        {
            AssertMsgFailed(("[%s] Write failed: cbCaptured=%RU32, cfHstWritten=%RU32, rc=%Rrc\n",
                             pStreamEx->Core.szName, cbCaptured, cfHstWritten, rc));
            break;
        }

        if (pThis->In.Cfg.Dbg.fEnabled)
            AudioHlpFileWrite(pStreamEx->In.Dbg.pFileCaptureNonInterleaved, abChunk, cbCaptured, 0 /* fFlags */);

        uint32_t cfHstMixed = 0;
        if (cfHstWritten)
        {
            int rc2 = AudioMixBufMixToParentEx(&pStreamEx->Host.MixBuf, 0 /* cSrcOffset */, cfHstWritten /* cSrcFrames */,
                                               &cfHstMixed /* pcSrcMixed */);
            Log3Func(("[%s] cbCaptured=%RU32, cfWritten=%RU32, cfMixed=%RU32, rc=%Rrc\n",
                      pStreamEx->Core.szName, cbCaptured, cfHstWritten, cfHstMixed, rc2));
            AssertRC(rc2);
        }

        Assert(cbReadable >= cbCaptured);
        cbReadable      -= cbCaptured;
        cfCapturedTotal += cfHstMixed;
    }

    if (RT_SUCCESS(rc))
    {
        if (cfCapturedTotal)
            Log2Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStreamEx->Core.szName, cfCapturedTotal, rc));
    }
    else
        LogFunc(("[%s] Capturing failed with rc=%Rrc\n", pStreamEx->Core.szName, rc));

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
 * @returns VBox status code.
 * @param   pThis       Driver instance.
 * @param   pStreamEx   Stream to capture from.
 * @param   pcfCaptured Number of (host) audio frames captured.
 */
static int drvAudioStreamCaptureRaw(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t *pcfCaptured)
{
    Assert(pStreamEx->Core.enmDir == PDMAUDIODIR_IN);
    Assert(pStreamEx->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_RAW);
    AssertPtr(pThis->pHostDrvAudio->pfnStreamGetReadable);

    /*
     * ...
     */
    /* Note: Raw means *audio frames*, not bytes! */
    uint32_t cfReadable = pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    if (!cfReadable)
        Log2Func(("[%s] No readable data available\n", pStreamEx->Core.szName));

    const uint32_t cfFree = AudioMixBufFree(&pStreamEx->Guest.MixBuf); /* Parent */
    if (!cfFree)
        Log2Func(("[%s] Buffer full\n", pStreamEx->Core.szName));

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
        rc = AudioMixBufPeekMutable(&pStreamEx->Host.MixBuf, cfReadable, &paFrames, &cfWritable);
        if (   RT_FAILURE(rc)
            || !cfWritable)
            break;

        uint32_t cfCaptured;
        rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                    paFrames, cfWritable, &cfCaptured);
        if (RT_FAILURE(rc))
        {
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
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
    Log2Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStreamEx->Core.szName, cfCapturedTotal, rc));
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
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertPtrNull(pcFramesCaptured);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertMsg(pStreamEx->Core.enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be captured (direction is 0x%x)\n",
               pStreamEx->Core.szName, pStreamEx->Core.enmDir));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));

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
            || !PDMAudioStrmStatusCanRead(pStreamEx->Core.fStatus))
        {
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        /*
         * Do the actual capturing.
         */
        if (RT_LIKELY(pStreamEx->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED))
            rc = drvAudioStreamCaptureNonInterleaved(pThis, pStreamEx, &cfCaptured);
        else if (pStreamEx->Host.Cfg.enmLayout == PDMAUDIOSTREAMLAYOUT_RAW)
            rc = drvAudioStreamCaptureRaw(pThis, pStreamEx, &cfCaptured);
        else
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

        if (RT_SUCCESS(rc))
        {
            Log3Func(("[%s] %RU32 frames captured, rc=%Rrc\n", pStreamEx->Core.szName, cfCaptured, rc));

            STAM_COUNTER_ADD(&pThis->Stats.TotalFramesIn,              cfCaptured);
            STAM_COUNTER_ADD(&pStreamEx->In.Stats.TotalFramesCaptured, cfCaptured);
        }
        else if (RT_UNLIKELY(RT_FAILURE(rc)))
            LogRel(("Audio: Capturing stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));
    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (pcFramesCaptured)
        *pcFramesCaptured = cfCaptured;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_AUDIO_ENUM
/**
 * Enumerates all host audio devices.
 *
 * This functionality might not be implemented by all backends and will return
 * VERR_NOT_SUPPORTED if not being supported.
 *
 * @note Must not hold the driver's critical section!
 *
 * @returns VBox status code.
 * @param   pThis               Driver instance to be called.
 * @param   fLog                Whether to print the enumerated device to the release log or not.
 * @param   pDevEnum            Where to store the device enumeration.
 *
 * @remarks This is currently ONLY used for release logging.
 */
static int drvAudioDevicesEnumerateInternal(PDRVAUDIO pThis, bool fLog, PPDMAUDIOHOSTENUM pDevEnum)
{
    AssertReturn(!RTCritSectIsOwner(&pThis->CritSect), VERR_WRONG_ORDER);

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
 *
 * @returns VBox status code.
 * @param   pThis               Driver instance to be called.
 */
static int drvAudioHostInit(PDRVAUDIO pThis)
{
    LogFlowFuncEnter();

    /*
     * Check the function pointers, make sure the ones we define as
     * mandatory are present.
     */
    PPDMIHOSTAUDIO pHostDrvAudio = pThis->pHostDrvAudio;
    AssertPtrReturn(pHostDrvAudio, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnGetConfig, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHostDrvAudio->pfnGetDevices, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHostDrvAudio->pfnGetStatus, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHostDrvAudio->pfnStreamConfigHint, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamCreate, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamDestroy, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamControl, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamGetReadable, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamGetWritable, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHostDrvAudio->pfnStreamGetPending, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamGetStatus, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamPlay, VERR_INVALID_POINTER);
    AssertPtrReturn(pHostDrvAudio->pfnStreamCapture, VERR_INVALID_POINTER);

    /*
     * Get the backend configuration.
     */
    int rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Getting configuration for driver '%s' failed with %Rrc\n", pThis->szName, rc));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    pThis->In.cStreamsFree  = pThis->BackendCfg.cMaxStreamsIn;
    pThis->Out.cStreamsFree = pThis->BackendCfg.cMaxStreamsOut;

    LogFlowFunc(("cStreamsFreeIn=%RU8, cStreamsFreeOut=%RU8\n", pThis->In.cStreamsFree, pThis->Out.cStreamsFree));

    LogRel2(("Audio: Host driver '%s' supports %RU32 input streams and %RU32 output streams at once.\n",
             pThis->szName, pThis->In.cStreamsFree, pThis->Out.cStreamsFree));

#ifdef VBOX_WITH_AUDIO_ENUM
    int rc2 = drvAudioDevicesEnumerateInternal(pThis, true /* fLog */, NULL /* pDevEnum */);
    if (rc2 != VERR_NOT_SUPPORTED) /* Some backends don't implement device enumeration. */
        AssertRC(rc2);

    RT_NOREF(rc2);
    /* Ignore rc. */
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
        PDRVAUDIOSTREAM pStreamEx;
        RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
        {
            drvAudioStreamControlInternal(pThis, pStreamEx, enmCmd);
        }
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
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbRead, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertMsg(pStreamEx->Core.enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be read from (direction is 0x%x)\n",
               pStreamEx->Core.szName, pStreamEx->Core.enmDir));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * ...
     */
    uint32_t cbReadTotal = 0;

    do
    {
        uint32_t cfReadTotal = 0;

        const uint32_t cfBuf = AUDIOMIXBUF_B2F(&pStreamEx->Guest.MixBuf, cbBuf);

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
            uint32_t cfToRead = RT_MIN(cfBuf, AudioMixBufLive(&pStreamEx->Guest.MixBuf));
            while (cfToRead)
            {
                uint32_t cfRead;
                rc = AudioMixBufAcquireReadBlock(&pStreamEx->Guest.MixBuf,
                                                 (uint8_t *)pvBuf + AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfReadTotal),
                                                 AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfToRead), &cfRead);
                if (RT_FAILURE(rc))
                    break;

#ifdef VBOX_WITH_STATISTICS
                const uint32_t cbRead = AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfRead);
                STAM_COUNTER_ADD(&pThis->Stats.TotalBytesRead,    cbRead);
                STAM_COUNTER_ADD(&pStreamEx->In.Stats.TotalFramesRead, cfRead);
                STAM_COUNTER_INC(&pStreamEx->In.Stats.TotalTimesRead);
#endif
                Assert(cfToRead >= cfRead);
                cfToRead -= cfRead;

                cfReadTotal += cfRead;

                AudioMixBufReleaseReadBlock(&pStreamEx->Guest.MixBuf, cfRead);
            }

            if (cfReadTotal)
            {
                if (pThis->In.Cfg.Dbg.fEnabled)
                    AudioHlpFileWrite(pStreamEx->In.Dbg.pFileStreamRead,
                                      pvBuf, AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfReadTotal), 0 /* fFlags */);

                AudioMixBufFinish(&pStreamEx->Guest.MixBuf, cfReadTotal);
            }
        }

        /* If we were not able to read as much data as requested, fill up the returned
         * data with silence.
         *
         * This is needed to keep the device emulation DMA transfers up and running at a constant rate. */
        if (cfReadTotal < cfBuf)
        {
            Log3Func(("[%s] Filling in silence (%RU64ms / %RU64ms)\n", pStream->szName,
                      PDMAudioPropsFramesToMilli(&pStreamEx->Guest.Cfg.Props, cfBuf - cfReadTotal),
                      PDMAudioPropsFramesToMilli(&pStreamEx->Guest.Cfg.Props, cfBuf)));

            PDMAudioPropsClearBuffer(&pStreamEx->Guest.Cfg.Props,
                                     (uint8_t *)pvBuf + AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfReadTotal),
                                     AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfBuf - cfReadTotal),
                                     cfBuf - cfReadTotal);

            cfReadTotal = cfBuf;
        }

        cbReadTotal = AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfReadTotal);

        pStreamEx->nsLastReadWritten = RTTimeNanoTS();

        Log3Func(("[%s] fEnabled=%RTbool, cbReadTotal=%RU32, rc=%Rrc\n", pStream->szName, pThis->In.fEnabled, cbReadTotal, rc));

    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_SUCCESS(rc) && pcbRead)
        *pcbRead = cbReadTotal;
    return rc;
}

/**
 * Adjusts the request stream configuration, applying our settings.
 *
 * This also does some basic validations.
 *
 * Used by both the stream creation and stream configuration hinting code.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the DrvAudio instance data.
 * @param   pCfgReq     The request configuration that should be adjusted.
 * @param   pszName     Stream name to use when logging warnings and errors.
 */
static int drvAudioStreamAdjustConfig(PDRVAUDIO pThis, PPDMAUDIOSTREAMCFG pCfgReq, const char *pszName)
{
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
                 PDMAudioPropsSampleSize(&pCfgReq->Props), pszName));
    }

    if (pDrvCfg->Props.uHz) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.uHz = pDrvCfg->Props.uHz;
        LogRel2(("Audio: Using custom Hz rate %RU32 for stream '%s'\n", pCfgReq->Props.uHz, pszName));
    }

    if (pDrvCfg->uSigned != UINT8_MAX) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.fSigned = RT_BOOL(pDrvCfg->uSigned);
        LogRel2(("Audio: Using custom %s sample format for stream '%s'\n",
                 pCfgReq->Props.fSigned ? "signed" : "unsigned", pszName));
    }

    if (pDrvCfg->uSwapEndian != UINT8_MAX) /* Anything set via custom extra-data? */
    {
        pCfgReq->Props.fSwapEndian = RT_BOOL(pDrvCfg->uSwapEndian);
        LogRel2(("Audio: Using custom %s endianess for samples of stream '%s'\n",
                 pCfgReq->Props.fSwapEndian ? "swapped" : "original", pszName));
    }

    if (PDMAudioPropsChannels(&pDrvCfg->Props) != 0) /* Anything set via custom extra-data? */
    {
        PDMAudioPropsSetChannels(&pCfgReq->Props, PDMAudioPropsChannels(&pDrvCfg->Props));
        LogRel2(("Audio: Using custom %RU8 channel(s) for stream '%s'\n", PDMAudioPropsChannels(&pDrvCfg->Props), pszName));
    }

    /* Validate PCM properties. */
    if (!AudioHlpPcmPropsAreValid(&pCfgReq->Props))
    {
        LogRel(("Audio: Invalid custom PCM properties set for stream '%s', cannot create stream\n", pszName));
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
             pCfgReq->Backend.cFramesPeriod, pszName));

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
             pCfgReq->Backend.cFramesBufferSize, pszName));

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
            /* Pre-buffer 66% of the buffer. */
            pCfgReq->Backend.cFramesPreBuffering = pCfgReq->Backend.cFramesBufferSize * 2 / 3;
            pszWhat = "default";
        }
    }

    LogRel2(("Audio: Using %s pre-buffering size %RU64 ms / %RU32 frames for stream '%s'\n",
             pszWhat, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPreBuffering),
             pCfgReq->Backend.cFramesPreBuffering, pszName));

    /*
     * Validate input.
     */
    if (pCfgReq->Backend.cFramesBufferSize < pCfgReq->Backend.cFramesPeriod)
    {
        LogRel(("Audio: Error for stream '%s': Buffering size (%RU64ms) must not be smaller than the period size (%RU64ms)\n",
                pszName, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize),
                PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPeriod)));
        return VERR_INVALID_PARAMETER;
    }

    if (   pCfgReq->Backend.cFramesPreBuffering != UINT32_MAX /* Custom pre-buffering set? */
        && pCfgReq->Backend.cFramesPreBuffering)
    {
        if (pCfgReq->Backend.cFramesBufferSize < pCfgReq->Backend.cFramesPreBuffering)
        {
            LogRel(("Audio: Error for stream '%s': Buffering size (%RU64ms) must not be smaller than the pre-buffering size (%RU64ms)\n",
                    pszName, PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesPreBuffering),
                    PDMAudioPropsFramesToMilli(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize)));
            return VERR_INVALID_PARAMETER;
        }
    }

    return VINF_SUCCESS;
}

/**
 * Worker for drvAudioStreamInitInternal and drvAudioStreamReInitInternal that
 * creates the backend (host driver) side of an audio stream.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Audio stream to create the backend side for.
 * @param   pCfgReq     Requested audio stream configuration to use for
 *                      stream creation.
 * @param   pCfgAcq     Acquired audio stream configuration returned by
 *                      the backend.
 *
 * @note    Configuration precedence for requested audio stream configuration (first has highest priority, if set):
 *          - per global extra-data
 *          - per-VM extra-data
 *          - requested configuration (by pCfgReq)
 *          - default value
 */
static int drvAudioStreamCreateInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx,
                                               PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertMsg((pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED) == 0,
              ("Stream '%s' already initialized in backend\n", pStreamEx->Core.szName));

    /*
     * Adjust the requested stream config, applying our settings.
     */
    int rc = drvAudioStreamAdjustConfig(pThis, pCfgReq, pStreamEx->Core.szName);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Make the acquired host configuration the requested host configuration initially,
     * in case the backend does not report back an acquired configuration.
     */
    /** @todo r=bird: This is conveniently not documented in the interface... */
    rc = PDMAudioStrmCfgCopy(pCfgAcq, pCfgReq);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Creating stream '%s' with an invalid backend configuration not possible, skipping\n",
                pStreamEx->Core.szName));
        return rc;
    }

    /*
     * Call the host driver to create the stream.
     */
    AssertPtr(pThis->pHostDrvAudio);
    if (pThis->pHostDrvAudio)
        rc = pThis->pHostDrvAudio->pfnStreamCreate(pThis->pHostDrvAudio, pStreamEx->pBackend, pCfgReq, pCfgAcq);
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NOT_SUPPORTED)
            LogRel2(("Audio: Creating stream '%s' in backend not supported\n", pStreamEx->Core.szName));
        else if (rc == VERR_AUDIO_STREAM_COULD_NOT_CREATE)
            LogRel2(("Audio: Stream '%s' could not be created in backend because of missing hardware / drivers\n", pStreamEx->Core.szName));
        else
            LogRel(("Audio: Creating stream '%s' in backend failed with %Rrc\n", pStreamEx->Core.szName, rc));
        return rc;
    }

    /* Validate acquired configuration. */
    char szTmp[PDMAUDIOPROPSTOSTRING_MAX];
    AssertLogRelMsgReturn(AudioHlpStreamCfgIsValid(pCfgAcq),
                          ("Audio: Creating stream '%s' returned an invalid backend configuration (%s), skipping\n",
                           pStreamEx->Core.szName, PDMAudioPropsToString(&pCfgAcq->Props, szTmp, sizeof(szTmp))),
                          VERR_INVALID_PARAMETER);

    /* Let the user know that the backend changed one of the values requested above. */
    if (pCfgAcq->Backend.cFramesBufferSize != pCfgReq->Backend.cFramesBufferSize)
        LogRel2(("Audio: Buffer size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                 pStreamEx->Core.szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesBufferSize), pCfgAcq->Backend.cFramesBufferSize));

    if (pCfgAcq->Backend.cFramesPeriod != pCfgReq->Backend.cFramesPeriod)
        LogRel2(("Audio: Period size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                 pStreamEx->Core.szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPeriod), pCfgAcq->Backend.cFramesPeriod));

    /* Was pre-buffering requested, but the acquired configuration from the backend told us something else? */
    if (pCfgReq->Backend.cFramesPreBuffering)
    {
        if (pCfgAcq->Backend.cFramesPreBuffering != pCfgReq->Backend.cFramesPreBuffering)
            LogRel2(("Audio: Pre-buffering size overwritten by backend for stream '%s' (now %RU64ms, %RU32 frames)\n",
                     pStreamEx->Core.szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPreBuffering), pCfgAcq->Backend.cFramesPreBuffering));

        if (pCfgAcq->Backend.cFramesPreBuffering > pCfgAcq->Backend.cFramesBufferSize)
        {
            pCfgAcq->Backend.cFramesPreBuffering = pCfgAcq->Backend.cFramesBufferSize;
            LogRel2(("Audio: Pre-buffering size bigger than buffer size for stream '%s', adjusting to %RU64ms (%RU32 frames)\n",
                     pStreamEx->Core.szName, PDMAudioPropsFramesToMilli(&pCfgAcq->Props, pCfgAcq->Backend.cFramesPreBuffering), pCfgAcq->Backend.cFramesPreBuffering));
        }
    }
    else if (pCfgReq->Backend.cFramesPreBuffering == 0) /* Was the pre-buffering requested as being disabeld? Tell the users. */
    {
        LogRel2(("Audio: Pre-buffering is disabled for stream '%s'\n", pStreamEx->Core.szName));
        pCfgAcq->Backend.cFramesPreBuffering = 0;
    }

    /* Sanity for detecting buggy backends. */
    AssertMsgReturn(pCfgAcq->Backend.cFramesPeriod < pCfgAcq->Backend.cFramesBufferSize,
                    ("Acquired period size must be smaller than buffer size\n"),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(pCfgAcq->Backend.cFramesPreBuffering <= pCfgAcq->Backend.cFramesBufferSize,
                    ("Acquired pre-buffering size must be smaller or as big as the buffer size\n"),
                    VERR_INVALID_PARAMETER);

    pStreamEx->Core.fStatus |= PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;

    return VINF_SUCCESS;
}


/**
 * Worker for drvAudioStreamCreate that initializes the audio stream.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Stream to initialize.
 * @param   fFlags      PDMAUDIOSTREAM_CREATE_F_XXX.
 * @param   pCfgHost    Stream configuration to use for the host side (backend).
 * @param   pCfgGuest   Stream configuration to use for the guest side.
 */
static int drvAudioStreamInitInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, uint32_t fFlags,
                                      PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest)
{
    /*
     * Init host stream.
     */
    pStreamEx->Core.uMagic = PDMAUDIOSTREAM_MAGIC;

    /* Set the host's default audio data layout. */
/** @todo r=bird: Why, oh why? OTOH, the layout stuff is non-sense anyway. */
    pCfgHost->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

#ifdef LOG_ENABLED
    LogFunc(("[%s] Requested host format:\n", pStreamEx->Core.szName));
    PDMAudioStrmCfgLog(pCfgHost);
#endif

    LogRel2(("Audio: Creating stream '%s'\n", pStreamEx->Core.szName));
    LogRel2(("Audio: Guest %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             pCfgGuest->enmDir == PDMAUDIODIR_IN ? "recording" : "playback", pStreamEx->Core.szName,
             pCfgGuest->Props.uHz, PDMAudioPropsSampleBits(&pCfgGuest->Props), pCfgGuest->Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&pCfgGuest->Props), PDMAudioPropsChannels(&pCfgGuest->Props) == 1 ? "" : "s"));
    LogRel2(("Audio: Requested host %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             pCfgHost->enmDir == PDMAUDIODIR_IN ? "recording" : "playback", pStreamEx->Core.szName,
             pCfgHost->Props.uHz, PDMAudioPropsSampleBits(&pCfgHost->Props), pCfgHost->Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&pCfgHost->Props), PDMAudioPropsChannels(&pCfgHost->Props) == 1 ? "" : "s"));

    PDMAUDIOSTREAMCFG CfgHostAcq;
    int rc = drvAudioStreamCreateInternalBackend(pThis, pStreamEx, pCfgHost, &CfgHostAcq);
    if (RT_FAILURE(rc))
        return rc;

    LogFunc(("[%s] Acquired host format:\n",  pStreamEx->Core.szName));
    PDMAudioStrmCfgLog(&CfgHostAcq);
    LogRel2(("Audio: Acquired host %s format for '%s': %RU32Hz, %u%s, %RU8 channel%s\n",
             CfgHostAcq.enmDir == PDMAUDIODIR_IN ? "recording" : "playback",  pStreamEx->Core.szName,
             CfgHostAcq.Props.uHz, PDMAudioPropsSampleBits(&CfgHostAcq.Props), CfgHostAcq.Props.fSigned ? "S" : "U",
             PDMAudioPropsChannels(&CfgHostAcq.Props), PDMAudioPropsChannels(&CfgHostAcq.Props) == 1 ? "" : "s"));
    Assert(PDMAudioPropsAreValid(&CfgHostAcq.Props));

    /* Set the stream properties (currently guest side, when DevSB16 is
       converted to mixer and PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF becomes
       default, this will just be the stream properties). */
    if (fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF)
        pStreamEx->Core.Props = CfgHostAcq.Props;
    else
        pStreamEx->Core.Props = pCfgGuest->Props;

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
     * Check if the backend did return sane values and correct if necessary.
     * Should never happen with our own backends, but you never know ...
     */
    uint32_t const cFramesPreBufferingMax = CfgHostAcq.Backend.cFramesBufferSize - RT_MIN(16, CfgHostAcq.Backend.cFramesBufferSize);
    if (CfgHostAcq.Backend.cFramesPreBuffering > cFramesPreBufferingMax)
    {
        LogRel2(("Audio: Warning: Pre-buffering size of %RU32 frames for stream '%s' is too close to or larger than the %RU32 frames buffer size, reducing it to %RU32 frames!\n",
                 CfgHostAcq.Backend.cFramesPreBuffering, pStreamEx->Core.szName, CfgHostAcq.Backend.cFramesBufferSize, cFramesPreBufferingMax));
        AssertFailed();
        CfgHostAcq.Backend.cFramesPreBuffering = cFramesPreBufferingMax;
    }

    if (CfgHostAcq.Backend.cFramesPeriod > CfgHostAcq.Backend.cFramesBufferSize)
    {
        LogRel2(("Audio: Warning: Period size of %RU32 frames for stream '%s' is larger than the %RU32 frames buffer size, reducing it to %RU32 frames!\n",
                 CfgHostAcq.Backend.cFramesPeriod, pStreamEx->Core.szName, CfgHostAcq.Backend.cFramesBufferSize, CfgHostAcq.Backend.cFramesBufferSize / 2));
        AssertFailed();
        CfgHostAcq.Backend.cFramesPeriod = CfgHostAcq.Backend.cFramesBufferSize / 2;
    }

    LogRel2(("Audio: Buffer size of stream '%s' is %RU64 ms / %RU32 frames\n", pStreamEx->Core.szName,
             PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize), CfgHostAcq.Backend.cFramesBufferSize));
    LogRel2(("Audio: Pre-buffering size of stream '%s' is %RU64 ms / %RU32 frames\n", pStreamEx->Core.szName,
             PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPreBuffering), CfgHostAcq.Backend.cFramesPreBuffering));

    /* Make sure the configured buffer size by the backend at least can hold the configured latency. */
    const uint32_t msPeriod = PDMAudioPropsFramesToMilli(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPeriod);
    LogRel2(("Audio: Period size of stream '%s' is %RU64 ms / %RU32 frames\n",
             pStreamEx->Core.szName, msPeriod, CfgHostAcq.Backend.cFramesPeriod));

    if (   pCfgGuest->Device.cMsSchedulingHint             /* Any scheduling hint set? */
        && pCfgGuest->Device.cMsSchedulingHint > msPeriod) /* This might lead to buffer underflows. */
        LogRel(("Audio: Warning: Scheduling hint of stream '%s' is bigger (%RU64ms) than used period size (%RU64ms)\n",
                pStreamEx->Core.szName, pCfgGuest->Device.cMsSchedulingHint, msPeriod));

    /*
     * Make a copy of the acquired host stream configuration and the guest side one.
     */
    rc = PDMAudioStrmCfgCopy(&pStreamEx->Host.Cfg, &CfgHostAcq);
    AssertRC(rc);

    /* Set the guests's default audio data layout. */
    pCfgGuest->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED; /** @todo r=bird: WTF DO WE DO THIS?  It's input and probably should've been const... */
    rc = PDMAudioStrmCfgCopy(&pStreamEx->Guest.Cfg, pCfgGuest);
    AssertRC(rc);

    /*
     * Configure host buffers.
     */

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pStreamEx->Host.MixBuf);

    if (!(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF))
    {
        rc = AudioMixBufInit(&pStreamEx->Host.MixBuf, pStreamEx->Core.szName, &CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize);
        AssertRCReturn(rc, rc);
    }
    /* Allocate space for pre-buffering of output stream w/o mixing buffers. */
    else if (pCfgHost->enmDir == PDMAUDIODIR_OUT)
    {
        Assert(pStreamEx->Out.cbPreBufAlloc == 0);
        Assert(pStreamEx->Out.cbPreBufThreshold == 0);
        Assert(pStreamEx->Out.cbPreBuffered == 0);
        if (CfgHostAcq.Backend.cFramesPreBuffering != 0)
        {
            pStreamEx->Out.cbPreBufThreshold = PDMAudioPropsFramesToBytes(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPreBuffering);
            pStreamEx->Out.cbPreBufAlloc = PDMAudioPropsFramesToBytes(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize - 2);
            pStreamEx->Out.cbPreBufAlloc = RT_MIN(RT_ALIGN_32(pStreamEx->Out.cbPreBufThreshold + _8K, _4K),
                                                  pStreamEx->Out.cbPreBufAlloc);
            pStreamEx->Out.pbPreBuf = (uint8_t *)RTMemAllocZ(pStreamEx->Out.cbPreBufAlloc);
            AssertReturn(pStreamEx->Out.pbPreBuf, VERR_NO_MEMORY);
        }
    }

    /*
     * Init guest stream.
     */
    if (pCfgGuest->Device.cMsSchedulingHint)
        LogRel2(("Audio: Stream '%s' got a scheduling hint of %RU32ms (%RU32 bytes)\n",
                 pStreamEx->Core.szName, pCfgGuest->Device.cMsSchedulingHint,
                 PDMAudioPropsMilliToBytes(&pCfgGuest->Props, pCfgGuest->Device.cMsSchedulingHint)));

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pStreamEx->Guest.MixBuf);

    if (!(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF))
    {
        rc = AudioMixBufInit(&pStreamEx->Guest.MixBuf, pStreamEx->Core.szName, &pCfgGuest->Props, CfgHostAcq.Backend.cFramesBufferSize);
        AssertRCReturn(rc, rc);
    }

    if (RT_FAILURE(rc))
        LogRel(("Audio: Creating stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));

    if (!(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF))
    {
        if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
        {
            /* Host (Parent) -> Guest (Child). */
            rc = AudioMixBufLinkTo(&pStreamEx->Host.MixBuf, &pStreamEx->Guest.MixBuf);
            AssertRC(rc);
        }
        else
        {
            /* Guest (Parent) -> Host (Child). */
            rc = AudioMixBufLinkTo(&pStreamEx->Guest.MixBuf, &pStreamEx->Host.MixBuf);
            AssertRC(rc);
        }
    }

    /*
     * Register statistics.
     */
    PPDMDRVINS const pDrvIns = pThis->pDrvIns;
    /** @todo expose config and more. */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.Cfg.Backend.cFramesBufferSize, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                           "Host side: The size of the backend buffer (in frames)", "%s/0-HostBackendBufSize", pStreamEx->Core.szName);
    if (!(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF))
    {
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.MixBuf.cFrames, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Host side: The size of the mixer buffer (in frames)",   "%s/1-HostMixBufSize", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Guest.MixBuf.cFrames, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Guest side: The size of the mixer buffer (in frames)",  "%s/2-GuestMixBufSize", pStreamEx->Core.szName);
        if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
        {
            PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.MixBuf.cMixed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                                   "Host side: Number of frames in the mixer buffer",   "%s/1-HostMixBufUsed", pStreamEx->Core.szName);
            PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Guest.MixBuf.cUsed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                                   "Guest side: Number of frames in the mixer buffer",  "%s/2-GuestMixBufUsed", pStreamEx->Core.szName);
        }
        else
        {
            PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.MixBuf.cUsed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                                   "Host side: Number of frames in the mixer buffer",     "%s/1-HostMixBufUsed", pStreamEx->Core.szName);
            PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Guest.MixBuf.cMixed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                                   "Guest side: Number of frames in the mixer buffer",    "%s/2-GuestMixBufUsed", pStreamEx->Core.szName);
        }
    }
    if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
    {
        /** @todo later? */
    }
    else
    {
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.cbBackendWritableBefore, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Host side: Free space in backend buffer before play", "%s/0-HostBackendBufFreeBefore", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.cbBackendWritableAfter, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Host side: Free space in backend buffer after play",  "%s/0-HostBackendBufFreeAfter", pStreamEx->Core.szName);
    }

#ifdef VBOX_WITH_STATISTICS
    if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
    {
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->In.Stats.TotalFramesCaptured, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total frames played.", "%s/TotalFramesCaptured", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->In.Stats.TotalTimesCaptured, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total number of playbacks.", "%s/TotalTimesCaptured", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->In.Stats.TotalFramesRead, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total frames read.", "%s/TotalFramesRead", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->In.Stats.TotalTimesRead, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total number of reads.", "%s/TotalTimesRead", pStreamEx->Core.szName);
    }
    else
    {
        Assert(pCfgGuest->enmDir == PDMAUDIODIR_OUT);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.TotalFramesPlayed, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total frames played.", "%s/TotalFramesPlayed", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.TotalTimesPlayed, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total number of playbacks.", "%s/TotalTimesPlayed", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.TotalFramesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total frames written.", "%s/TotalFramesWritten", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Out.Stats.TotalTimesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total number of writes.", "%s/TotalTimesWritten", pStreamEx->Core.szName);
    }
#endif /* VBOX_WITH_STATISTICS */

    LogFlowFunc(("[%s] Returning %Rrc\n", pStreamEx->Core.szName, rc));
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioStreamCreate(PPDMIAUDIOCONNECTOR pInterface, uint32_t fFlags, PPDMAUDIOSTREAMCFG pCfgHost,
                                              PPDMAUDIOSTREAMCFG pCfgGuest, PPDMAUDIOSTREAM *ppStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /*
     * Assert sanity.
     */
    AssertReturn(!(fFlags & ~PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF), VERR_INVALID_FLAGS);
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
    if (pCfgHost->enmDir == PDMAUDIODIR_IN)
    {
        if (!pThis->In.cStreamsFree)
        {
            LogFlowFunc(("Maximum number of host input streams reached\n"));
            rc = VERR_AUDIO_NO_FREE_INPUT_STREAMS;
        }
        pcFreeStreams = &pThis->In.cStreamsFree;
    }
    else /* Out */
    {
        if (!pThis->Out.cStreamsFree)
        {
            LogFlowFunc(("Maximum number of host output streams reached\n"));
            rc = VERR_AUDIO_NO_FREE_OUTPUT_STREAMS;
        }
        pcFreeStreams = &pThis->Out.cStreamsFree;
    }
    size_t const cbHstStrm = pThis->BackendCfg.cbStream;
    AssertStmt(cbHstStrm < _16M, rc = VERR_OUT_OF_RANGE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize common state.
         */
        PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)RTMemAllocZ(sizeof(DRVAUDIOSTREAM) + RT_ALIGN_Z(cbHstStrm, 64));
        if (pStreamEx)
        {
            /* Retrieve host driver name for easier identification. */
            AssertPtr(pThis->pHostDrvAudio);
            RTStrPrintf(pStreamEx->Core.szName, RT_ELEMENTS(pStreamEx->Core.szName), "[%s] %s",
                        pThis->BackendCfg.szName, pCfgHost->szName[0] != '\0' ? pCfgHost->szName : "<Untitled>");

            pStreamEx->Core.enmDir    = pCfgHost->enmDir;
            pStreamEx->Core.cbBackend = (uint32_t)cbHstStrm;
            if (cbHstStrm)
                pStreamEx->pBackend   = (PPDMAUDIOBACKENDSTREAM)(pStreamEx + 1);
            pStreamEx->fNoMixBufs     = RT_BOOL(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF);
            pStreamEx->uMagic         = DRVAUDIOSTREAM_MAGIC;

            /*
             * Try to init the rest.
             */
            rc = drvAudioStreamInitInternal(pThis, pStreamEx, fFlags, pCfgHost, pCfgGuest);
            if (RT_SUCCESS(rc))
            {
                /* Set initial reference counts. */
                pStreamEx->Core.cRefs = 1;

                /* Decrement the free stream counter. */
                Assert(*pcFreeStreams > 0);
                *pcFreeStreams -= 1;

                /*
                 * We're good.
                 */
                RTListAppend(&pThis->lstStreams, &pStreamEx->ListEntry);
                STAM_COUNTER_INC(&pThis->Stats.TotalStreamsCreated);
                *ppStream = &pStreamEx->Core;

                /*
                 * Init debug stuff if enabled (ignore failures).
                 */
                if (pCfgHost->enmDir == PDMAUDIODIR_IN)
                {
                    if (pThis->In.Cfg.Dbg.fEnabled)
                    {
                        AudioHlpFileCreateAndOpen(&pStreamEx->In.Dbg.pFileCaptureNonInterleaved, pThis->In.Cfg.Dbg.szPathOut,
                                                  "DrvAudioCapNonInt", pThis->pDrvIns->iInstance, &pStreamEx->Host.Cfg.Props);
                        AudioHlpFileCreateAndOpen(&pStreamEx->In.Dbg.pFileStreamRead, pThis->In.Cfg.Dbg.szPathOut,
                                                  "DrvAudioRead", pThis->pDrvIns->iInstance, &pStreamEx->Host.Cfg.Props);
                    }
                }
                else /* Out */
                {
                    if (pThis->Out.Cfg.Dbg.fEnabled)
                    {
                        AudioHlpFileCreateAndOpen(&pStreamEx->Out.Dbg.pFilePlayNonInterleaved, pThis->Out.Cfg.Dbg.szPathOut,
                                                  "DrvAudioPlayNonInt", pThis->pDrvIns->iInstance, &pStreamEx->Host.Cfg.Props);
                        AudioHlpFileCreateAndOpen(&pStreamEx->Out.Dbg.pFileStreamWrite, pThis->Out.Cfg.Dbg.szPathOut,
                                                  "DrvAudioWrite", pThis->pDrvIns->iInstance, &pStreamEx->Host.Cfg.Props);
                    }
                }
            }
            else
            {
                LogFunc(("drvAudioStreamInitInternal failed: %Rrc\n", rc));
                int rc2 = drvAudioStreamUninitInternal(pThis, pStreamEx);
                AssertRC(rc2);
                drvAudioStreamFree(pStreamEx);
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

        PDRVAUDIOSTREAM pStreamEx;
        RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
        {
            if (pStreamEx->Core.enmDir != enmDir) /* Skip unwanted streams. */
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
            int rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx,
                                                           fEnable ? PDMAUDIOSTREAMCMD_ENABLE : PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_FAILURE(rc2))
            {
                if (rc2 == VERR_AUDIO_STREAM_NOT_READY)
                    LogRel(("Audio: Stream '%s' not available\n", pStreamEx->Core.szName));
                else
                    LogRel(("Audio: Failed to %s %s stream '%s', rc=%Rrc\n", fEnable ? "enable" : "disable",
                            enmDir == PDMAUDIODIR_IN ? "input" : "output", pStreamEx->Core.szName, rc2));
            }
            else
            {
                /* When (re-)enabling a stream, clear the disabled warning bit again. */
                if (fEnable)
                    pStreamEx->Core.fWarningsShown &= ~PDMAUDIOSTREAM_WARN_FLAGS_DISABLED;
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
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamConfigHint}
 */
static DECLCALLBACK(void) drvAudioStreamConfigHint(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAMCFG pCfg)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertReturnVoid(pCfg->enmDir == PDMAUDIODIR_IN || pCfg->enmDir == PDMAUDIODIR_OUT);

    int rc = RTCritSectEnter(&pThis->CritSect); /** @todo Reconsider the locking for DrvAudio */
    AssertRCReturnVoid(rc);

    /*
     * Don't do anything unless the backend has a pfnStreamConfigHint method
     * and the direction is currently enabled.
     */
    if (   pThis->pHostDrvAudio
        && pThis->pHostDrvAudio->pfnStreamConfigHint)
    {
        if (pCfg->enmDir == PDMAUDIODIR_OUT ? pThis->Out.fEnabled : pThis->In.fEnabled)
        {
            /*
             * Adjust the configuration (applying out settings) then call the backend driver.
             */
            rc = drvAudioStreamAdjustConfig(pThis, pCfg, pCfg->szName);
            AssertLogRelRC(rc);
            if (RT_SUCCESS(rc))
                pThis->pHostDrvAudio->pfnStreamConfigHint(pThis->pHostDrvAudio, pCfg);
        }
        else
            LogFunc(("Ignoring hint because direction is not currently enabled\n"));
    }
    else
        LogFlowFunc(("Ignoring hint because backend has no pfnStreamConfigHint method.\n"));

    RTCritSectLeave(&pThis->CritSect);
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetReadable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, 0);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, 0);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, 0);
    AssertMsg(pStreamEx->Core.enmDir == PDMAUDIODIR_IN, ("Can't read from a non-input stream\n"));
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, 0);

    /*
     * ...
     */
    uint32_t cbReadable = 0;

    /* All input streams for this driver disabled? See @bugref{9882}. */
    const bool fDisabled = !pThis->In.fEnabled;

    if (   pThis->pHostDrvAudio
        && (   PDMAudioStrmStatusCanRead(pStreamEx->Core.fStatus)
            || fDisabled)
       )
    {
        if (pStreamEx->fNoMixBufs)
            cbReadable = pThis->pHostDrvAudio
                       ? pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStreamEx->pBackend) : 0;
        else
        {
            const uint32_t cfReadable = AudioMixBufLive(&pStreamEx->Guest.MixBuf);
            cbReadable = AUDIOMIXBUF_F2B(&pStreamEx->Guest.MixBuf, cfReadable);
        }

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
                fStatus = pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pStreamEx->pBackend);
            if (   !PDMAudioStrmStatusCanRead(fStatus)
                || fDisabled)
            {
                cbReadable = PDMAudioPropsNanoToBytes(&pStreamEx->Host.Cfg.Props,
                                                      RTTimeNanoTS() - pStreamEx->nsLastReadWritten);
                if (!(pStreamEx->Core.fWarningsShown & PDMAUDIOSTREAM_WARN_FLAGS_DISABLED))
                {
                    if (fDisabled)
                        LogRel(("Audio: Input for driver '%s' has been disabled, returning silence\n", pThis->szName));
                    else
                        LogRel(("Audio: Warning: Input for stream '%s' of driver '%s' not ready (current input status is %#x), returning silence\n",
                                pStreamEx->Core.szName, pThis->szName, fStatus));

                    pStreamEx->Core.fWarningsShown |= PDMAUDIOSTREAM_WARN_FLAGS_DISABLED;
                }
            }
        }

        /* Make sure to align the readable size to the guest's frame size. */
        if (cbReadable)
            cbReadable = PDMAudioPropsFloorBytesToFrame(&pStreamEx->Guest.Cfg.Props, cbReadable);
    }

    RTCritSectLeave(&pThis->CritSect);
    Log3Func(("[%s] cbReadable=%RU32 (%RU64ms)\n",
              pStreamEx->Core.szName, cbReadable, PDMAudioPropsBytesToMilli(&pStreamEx->Host.Cfg.Props, cbReadable)));
    return cbReadable;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetWritable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, 0);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, 0);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, 0);
    AssertMsgReturn(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT, ("Can't write to a non-output stream\n"), 0);
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, 0);

    /*
     * ...
     */
    uint32_t cbWritable = 0;

    /* Note: We don't propagate the backend stream's status to the outside -- it's the job of this
     *       audio connector to make sense of it. */
    if (PDMAudioStrmStatusCanWrite(pStreamEx->Core.fStatus))
    {
        if (pStreamEx->fNoMixBufs)
        {
            Assert(pThis->pHostDrvAudio);
            cbWritable = pThis->pHostDrvAudio
                       ? pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend) : 0;
            if (pStreamEx->fThresholdReached)
            {
                if (pStreamEx->Out.cbPreBuffered == 0)
                { /* likely */ }
                else
                {
                    /* Buggy backend: We weren't able to copy all the pre-buffered data to it
                       when reaching the threshold.  Try escape this situation, or at least
                       keep the extra buffering to a minimum.  We must try write something
                       as long as there is space for it, as we need the pfnStreamWrite call
                       to move the data. */
                    uint32_t const cbMin = PDMAudioPropsFramesToBytes(&pStreamEx->Core.Props, 8);
                    if (cbWritable >= pStreamEx->Out.cbPreBuffered + cbMin)
                        cbWritable -= pStreamEx->Out.cbPreBuffered + cbMin / 2;
                    else
                        cbWritable = RT_MIN(cbMin, pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBuffered);
                    AssertLogRel(cbWritable);
                }
            }
            else
            {
                Assert(cbWritable >= pStreamEx->Out.cbPreBufThreshold);
                cbWritable = pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBufThreshold;
            }
        }
        else
            cbWritable = AudioMixBufFreeBytes(&pStreamEx->Host.MixBuf);

        /* Make sure to align the writable size to the host's frame size. */
        cbWritable = PDMAudioPropsFloorBytesToFrame(&pStreamEx->Host.Cfg.Props, cbWritable);
    }

    RTCritSectLeave(&pThis->CritSect);
    Log3Func(("[%s] cbWritable=%RU32 (%RU64ms)\n",
              pStreamEx->Core.szName, cbWritable, PDMAudioPropsBytesToMilli(&pStreamEx->Host.Cfg.Props, cbWritable)));
    return cbWritable;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvAudioStreamGetStatus(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /** @todo r=bird: It is not documented that we ignore NULL streams...  Why is
     *        this necessary? */
    if (!pStream)
        return PDMAUDIOSTREAMSTS_FLAGS_NONE;
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, PDMAUDIOSTREAMSTS_FLAGS_NONE);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, PDMAUDIOSTREAMSTS_FLAGS_NONE);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, PDMAUDIOSTREAMSTS_FLAGS_NONE);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, PDMAUDIOSTREAMSTS_FLAGS_NONE);

    /* Is the stream scheduled for re-initialization? Do so now. */
    drvAudioStreamMaybeReInit(pThis, pStreamEx);

    PDMAUDIOSTREAMSTS fStrmStatus = pStreamEx->Core.fStatus;

    RTCritSectLeave(&pThis->CritSect);
#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] %s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, fStrmStatus)));
    return fStrmStatus;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamSetVolume}
 */
static DECLCALLBACK(int) drvAudioStreamSetVolume(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(!pStreamEx->fNoMixBufs, VWRN_INVALID_STATE);

    LogFlowFunc(("[%s] volL=%RU32, volR=%RU32, fMute=%RTbool\n", pStreamEx->Core.szName, pVol->uLeft, pVol->uRight, pVol->fMuted));

    AudioMixBufSetVolume(&pStreamEx->Guest.MixBuf, pVol);
    AudioMixBufSetVolume(&pStreamEx->Host.MixBuf,  pVol);

    return VINF_SUCCESS;
}

/**
 * Calls the backend to give it the chance to destroy its part of the audio stream.
 *
 * Called from drvAudioPowerOff, drvAudioStreamUninitInternal and
 * drvAudioStreamReInitInternal.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Audio stream destruct backend for.
 */
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    AssertPtr(pThis);
    AssertPtr(pStreamEx);

    int rc = VINF_SUCCESS;

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    LogFunc(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));

    if (pStreamEx->Core.fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED)
    {
        AssertPtr(pStreamEx->pBackend);

        /* Check if the pointer to  the host audio driver is still valid.
         * It can be NULL if we were called in drvAudioDestruct, for example. */
        if (pThis->pHostDrvAudio)
            rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pStreamEx->pBackend);

        pStreamEx->Core.fStatus &= ~PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;
    }

    LogFlowFunc(("[%s] Returning %Rrc\n", pStreamEx->Core.szName, rc));
    return rc;
}

/**
 * Uninitializes an audio stream - worker for drvAudioStreamDestroy,
 * drvAudioDestruct and drvAudioStreamCreate.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to driver instance.
 * @param   pStreamEx   Pointer to audio stream to uninitialize.
 *
 * @note    Caller owns the critical section.
 */
static int drvAudioStreamUninitInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertMsgReturn(pStreamEx->Core.cRefs <= 1,
                    ("Stream '%s' still has %RU32 references held when uninitializing\n", pStreamEx->Core.szName, pStreamEx->Core.cRefs),
                    VERR_WRONG_ORDER);
    LogFlowFunc(("[%s] cRefs=%RU32\n", pStreamEx->Core.szName, pStreamEx->Core.cRefs));

    /*
     * ...
     */
    int rc = drvAudioStreamControlInternal(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
        rc = drvAudioStreamDestroyInternalBackend(pThis, pStreamEx);

    /* Destroy mixing buffers. */
    AudioMixBufDestroy(&pStreamEx->Guest.MixBuf);
    AudioMixBufDestroy(&pStreamEx->Host.MixBuf);

    /* Free pre-buffer space. */
    if (   pStreamEx->Core.enmDir == PDMAUDIODIR_OUT
        && pStreamEx->Out.pbPreBuf)
    {
        RTMemFree(pStreamEx->Out.pbPreBuf);
        pStreamEx->Out.pbPreBuf      = NULL;
        pStreamEx->Out.cbPreBufAlloc = 0;
        pStreamEx->Out.cbPreBuffered = 0;
    }

    if (RT_SUCCESS(rc))
    {
#ifdef LOG_ENABLED
        if (pStreamEx->Core.fStatus != PDMAUDIOSTREAMSTS_FLAGS_NONE)
        {
            char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
            LogFunc(("[%s] Warning: Still has %s set when uninitializing\n",
                     pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->Core.fStatus)));
        }
#endif
        pStreamEx->Core.fStatus = PDMAUDIOSTREAMSTS_FLAGS_NONE;
    }

    PPDMDRVINS const pDrvIns = pThis->pDrvIns;
    PDMDrvHlpSTAMDeregisterByPrefix(pDrvIns, pStreamEx->Core.szName);

    if (pStreamEx->Core.enmDir == PDMAUDIODIR_IN)
    {
        if (pThis->In.Cfg.Dbg.fEnabled)
        {
            AudioHlpFileDestroy(pStreamEx->In.Dbg.pFileCaptureNonInterleaved);
            pStreamEx->In.Dbg.pFileCaptureNonInterleaved = NULL;

            AudioHlpFileDestroy(pStreamEx->In.Dbg.pFileStreamRead);
            pStreamEx->In.Dbg.pFileStreamRead = NULL;
        }
    }
    else
    {
        Assert(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT);
        if (pThis->Out.Cfg.Dbg.fEnabled)
        {
            AudioHlpFileDestroy(pStreamEx->Out.Dbg.pFilePlayNonInterleaved);
            pStreamEx->Out.Dbg.pFilePlayNonInterleaved = NULL;

            AudioHlpFileDestroy(pStreamEx->Out.Dbg.pFileStreamWrite);
            pStreamEx->Out.Dbg.pFileStreamWrite = NULL;
        }
    }
    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
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
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;   /* Note! Do not touch pStream after this! */
    Assert(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC);
    Assert(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    LogRel2(("Audio: Destroying stream '%s'\n", pStreamEx->Core.szName));

    LogFlowFunc(("[%s] cRefs=%RU32\n", pStreamEx->Core.szName, pStreamEx->Core.cRefs));
    AssertMsg(pStreamEx->Core.cRefs <= 1, ("%u %s\n", pStreamEx->Core.cRefs, pStreamEx->Core.szName));
    if (pStreamEx->Core.cRefs <= 1)
    {
        rc = drvAudioStreamUninitInternal(pThis, pStreamEx);
        if (RT_SUCCESS(rc))
        {
            if (pStreamEx->Core.enmDir == PDMAUDIODIR_IN)
                pThis->In.cStreamsFree++;
            else /* Out */
                pThis->Out.cStreamsFree++;

            RTListNodeRemove(&pStreamEx->ListEntry);

            drvAudioStreamFree(pStreamEx);
            pStreamEx = NULL;
            pStream = NULL;
        }
        else
            LogRel(("Audio: Uninitializing stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));
    }
    else
        rc = VERR_WRONG_ORDER;

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   PDMIAUDIONOTIFYFROMHOST interface implementation.                                                                            *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * @interface_method_impl{PDMIAUDIONOTIFYFROMHOST,pfnNotifyDevicesChanged}
 */
static DECLCALLBACK(void) drvAudioNotifyFromHost_NotifyDevicesChanged(PPDMIAUDIONOTIFYFROMHOST pInterface)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioNotifyFromHost);

    LogRel(("Audio: Device configuration of driver '%s' has changed\n", pThis->szName));
    drvAudioScheduleReInitInternal(pThis);
}

#endif /* VBOX_WITH_AUDIO_CALLBACKS */


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
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIONOTIFYFROMHOST, &pThis->IAudioNotifyFromHost);
#endif

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

    /** @todo locking?   */
    if (pThis->pHostDrvAudio) /* If not lower driver is configured, bail out. */
    {
        /*
         * Just destroy the host stream on the backend side.
         * The rest will either be destructed by the device emulation or
         * in drvAudioDestruct().
         */
        PDRVAUDIOSTREAM pStreamEx;
        RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
        {
            drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
            drvAudioStreamDestroyInternalBackend(pThis, pStreamEx);
        }

        pThis->pHostDrvAudio = NULL;
    }

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
 * @param   pDrvIns     The driver instance.
 * @param   pThis       Pointer to driver instance.
 * @param   fFlags      Attach flags; see PDMDrvHlpAttach().
 */
static int drvAudioDoAttachInternal(PPDMDRVINS pDrvIns, PDRVAUDIO pThis, uint32_t fFlags)
{
    Assert(pThis->pHostDrvAudio == NULL); /* No nested attaching. */

    /*
     * Attach driver below and query its connector interface.
     */
    PPDMIBASE pDownBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pDownBase);
    if (RT_SUCCESS(rc))
    {
        pThis->pHostDrvAudio = PDMIBASE_QUERY_INTERFACE(pDownBase, PDMIHOSTAUDIO);
        if (pThis->pHostDrvAudio)
        {
            /*
             * If everything went well, initialize the lower driver.
             */
            rc = drvAudioHostInit(pThis);
            if (RT_FAILURE(rc))
                pThis->pHostDrvAudio = NULL;
        }
        else
        {
            LogRel(("Audio: Failed to query interface for underlying host driver '%s'\n", pThis->szName));
            rc = PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                  N_("The host audio driver does not implement PDMIHOSTAUDIO!"));
        }
    }
    /*
     * If the host driver below us failed to construct for some beningn reason,
     * we'll report it as a runtime error and replace it with the Null driver.
     *
     * Note! We do NOT change anything in PDM (or CFGM), so pDrvIns->pDownBase
     *       will remain NULL in this case.
     */
    else if (   rc == VERR_AUDIO_BACKEND_INIT_FAILED
             || rc == VERR_MODULE_NOT_FOUND
             || rc == VERR_SYMBOL_NOT_FOUND
             || rc == VERR_FILE_NOT_FOUND
             || rc == VERR_PATH_NOT_FOUND)
    {
        /* Complain: */
        LogRel(("DrvAudio: Host audio driver '%s' init failed with %Rrc. Switching to the NULL driver for now.\n",
                pThis->szName, rc));
        PDMDrvHlpVMSetRuntimeError(pDrvIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                   N_("Host audio backend (%s) initialization has failed. Selecting the NULL audio backend with the consequence that no sound is audible"),
                                   pThis->szName);

        /* Replace with null audio: */
        pThis->pHostDrvAudio = (PPDMIHOSTAUDIO)&g_DrvHostAudioNull;
        RTStrCopy(pThis->szName, sizeof(pThis->szName), "NULL");
        rc = drvAudioHostInit(pThis);
        AssertRC(rc);
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

    rc = drvAudioDoAttachInternal(pDrvIns, pThis, fFlags);

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

    PDRVAUDIOSTREAM pStreamEx, pStreamExNext;
    RTListForEachSafe(&pThis->lstStreams, pStreamEx, pStreamExNext, DRVAUDIOSTREAM, ListEntry)
    {
        int rc = drvAudioStreamUninitInternal(pThis, pStreamEx);
        if (RT_SUCCESS(rc))
        {
            RTListNodeRemove(&pStreamEx->ListEntry);
            drvAudioStreamFree(pStreamEx);
        }
    }

    /* Sanity. */
    Assert(RTListIsEmpty(&pThis->lstStreams));

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
    pThis->IAudioConnector.pfnStreamConfigHint  = drvAudioStreamConfigHint;
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
    /* IAudioNotifyFromHost */
    pThis->IAudioNotifyFromHost.pfnNotifyDevicesChanged = drvAudioNotifyFromHost_NotifyDevicesChanged;
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
     * Create a timer to do finish closing output streams in PENDING_DISABLE state.
     *
     * The device won't call us again after it has disabled a the stream and this is
     * a real problem for truely cyclic buffer backends like DSound which will just
     * continue to loop and loop if not stopped.
     */
    RTStrPrintf(pThis->szTimerName, sizeof(pThis->szTimerName), "AudioIterate-%u", pDrvIns->iInstance);
    rc = PDMDrvHlpTMTimerCreate(pDrvIns, TMCLOCK_VIRTUAL, drvAudioEmergencyIterateTimer, NULL /*pvUser*/,
                                0 /*fFlags*/, pThis->szTimerName, &pThis->hTimer);
    AssertRCReturn(rc, rc);

    /*
     * Attach the host driver, if present.
     */
    rc = drvAudioDoAttachInternal(pDrvIns, pThis, fFlags);
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

