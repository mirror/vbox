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
#include <iprt/req.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#include <ctype.h>
#include <stdlib.h>

#include "AudioHlp.h"
#include "AudioMixBuffer.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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
 * Play state of a stream wrt backend.
 */
typedef enum DRVAUDIOPLAYSTATE
{
    /** Invalid zero value.   */
    DRVAUDIOPLAYSTATE_INVALID = 0,
    /** No playback or pre-buffering.   */
    DRVAUDIOPLAYSTATE_NOPLAY,
    /** Playing w/o any prebuffering. */
    DRVAUDIOPLAYSTATE_PLAY,
    /** Parallel pre-buffering prior to a device switch (i.e. we're outputting to
     * the old device and pre-buffering the same data in parallel). */
    DRVAUDIOPLAYSTATE_PLAY_PREBUF,
    /** Initial pre-buffering or the pre-buffering for a device switch (if it
     * the device setup took less time than filling up the pre-buffer). */
    DRVAUDIOPLAYSTATE_PREBUF,
    /** The device initialization is taking too long, pre-buffering wraps around
     * and drops samples. */
    DRVAUDIOPLAYSTATE_PREBUF_OVERDUE,
    /** Same as play-prebuf, but we don't have a working output device any more. */
    DRVAUDIOPLAYSTATE_PREBUF_SWITCHING,
    /** Working on committing the pre-buffered data.
     * We'll typically leave this state immediately and go to PLAY, however if
     * the backend cannot handle all the pre-buffered data at once, we'll stay
     * here till it does. */
    DRVAUDIOPLAYSTATE_PREBUF_COMMITTING,
    /** End of valid values. */
    DRVAUDIOPLAYSTATE_END
} DRVAUDIOPLAYSTATE;


/**
 * Extended stream structure.
 */
typedef struct DRVAUDIOSTREAM
{
    /** The publicly visible bit. */
    PDMAUDIOSTREAM          Core;

    /** Just an extra magic to verify that we allocated the stream rather than some
     * faked up stuff from the device (DRVAUDIOSTREAM_MAGIC). */
    uintptr_t               uMagic;

    /** List entry in DRVAUDIO::lstStreams. */
    RTLISTNODE              ListEntry;

    /** Number of references to this stream.
     *  Only can be destroyed when the reference count reaches 0. */
    uint32_t volatile       cRefs;
    /** Stream status - PDMAUDIOSTREAM_STS_XXX. */
    uint32_t                fStatus;

    /** Data to backend-specific stream data.
     *  This data block will be casted by the backend to access its backend-dependent data.
     *
     *  That way the backends do not have access to the audio connector's data. */
    PPDMAUDIOBACKENDSTREAM  pBackend;

    /** Do not use the mixing buffers (Guest::MixBuf, Host::MixBuf). */
    bool                    fNoMixBufs;
    /** Set if pfnStreamCreate returned VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED. */
    bool                    fNeedAsyncInit;
    bool                    afPadding[2];

    /** Number of (re-)tries while re-initializing the stream. */
    uint32_t                cTriesReInit;

    /** The backend status at the last play or capture call.
     * This is used to detect state changes.  */
    uint32_t                fLastBackendStatus;

    /** The pfnStreamInitAsync request handle. */
    PRTREQ                  hReqInitAsync;

    /** The guest side of the stream. */
    DRVAUDIOSTREAMCTX       Guest;
    /** The host side of the stream. */
    DRVAUDIOSTREAMCTX       Host;


    /** Timestamp (in ns) since last trying to re-initialize.
     *  Might be 0 if has not been tried yet. */
    uint64_t                nsLastReInit;
    /** Timestamp (in ns) since last iteration. */
    uint64_t                nsLastIterated;
    /** Timestamp (in ns) since last playback / capture. */
    uint64_t                nsLastPlayedCaptured;
    /** Timestamp (in ns) since last read (input streams) or
     *  write (output streams). */
    uint64_t                nsLastReadWritten;
    /** Internal stream position (as per pfnStreamWrite/Read). */
    uint64_t                offInternal;

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
                uint32_t        cbBackendWritableBefore;
                uint32_t        cbBackendWritableAfter;
            } Stats;

            /** Space for pre-buffering. */
            uint8_t            *pbPreBuf;
            /** The size of the pre-buffer allocation (in bytes). */
            uint32_t            cbPreBufAlloc;
            /** The current pre-buffering read offset. */
            uint32_t            offPreBuf;
            /** Number of bytes we've prebuffered. */
            uint32_t            cbPreBuffered;
            /** The pre-buffering threshold expressed in bytes. */
            uint32_t            cbPreBufThreshold;
            /** The play state. */
            DRVAUDIOPLAYSTATE   enmPlayState;
        } Out;
    } RT_UNION_NM(u);
} DRVAUDIOSTREAM;
/** Pointer to an extended stream structure. */
typedef DRVAUDIOSTREAM *PDRVAUDIOSTREAM;

/** Value for DRVAUDIOSTREAM::uMagic (Johann Sebastian Bach). */
#define DRVAUDIOSTREAM_MAGIC        UINT32_C(0x16850331)
/** Value for DRVAUDIOSTREAM::uMagic after destruction */
#define DRVAUDIOSTREAM_MAGIC_DEAD   UINT32_C(0x17500728)


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
    /** Critical section for serializing access.
     * @todo r=bird: This needs to be split up and introduce stream-level locking so
     *       that different AIO threads can work in parallel (e.g. input &
     *       output, or two output streams).  Maybe put a critect in
     *       PDMAUDIOSTREAM? */
    RTCRITSECT              CritSect;
    /** Shutdown indicator. */
    bool                    fTerminate;
#ifdef VBOX_WITH_AUDIO_ENUM
    /** Flag indicating that we need to enumerate and log of the host
     * audio devices again. */
    bool                    fEnumerateDevices;
#endif
    /** Our audio connector interface. */
    PDMIAUDIOCONNECTOR      IAudioConnector;
    /** Interface used by the host backend. */
    PDMIHOSTAUDIOPORT       IHostAudioPort;
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

    /** Request pool if the backend needs it for async stream creation. */
    RTREQPOOL               hReqPool;

    /** Handle to the disable-iteration timer. */
    TMTIMERHANDLE           hTimer;
    /** Set if hTimer is armed. */
    bool volatile           fTimerArmed;
    /** Unique name for the the disable-iteration timer.  */
    char                    szTimerName[23];

#ifdef VBOX_WITH_STATISTICS
    /** Statistics. */
    struct
    {
        STAMCOUNTER TotalStreamsActive;
        STAMCOUNTER TotalStreamsCreated;
        STAMCOUNTER TotalFramesRead;
        STAMCOUNTER TotalFramesIn;
        STAMCOUNTER TotalBytesRead;
    } Stats;
#endif
} DRVAUDIO;
/** Pointer to the instance data of an audio driver. */
typedef DRVAUDIO *PDRVAUDIO;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamUninitInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);
static uint32_t drvAudioStreamReleaseInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, bool fMayDestroy);
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx);

/** Buffer size for dbgAudioStreamStatusToStr.  */
# define DRVAUDIO_STATUS_STR_MAX sizeof("INITIALIZED ENABLED PAUSED PENDING_DISABLED NEED_REINIT BACKEND_READY PREPARING_SWITCH 0x12345678")

/**
 * Converts an audio stream status to a string.
 *
 * @returns pszDst
 * @param   pszDst      Buffer to convert into, at least minimum size is
 *                      DRVAUDIO_STATUS_STR_MAX.
 * @param   fStatus     Stream status flags to convert.
 */
static const char *dbgAudioStreamStatusToStr(char pszDst[DRVAUDIO_STATUS_STR_MAX], uint32_t fStatus)
{
    static const struct
    {
        const char *pszMnemonic;
        uint32_t    cchMnemnonic;
        uint32_t    fFlag;
    } s_aFlags[] =
    {
        { RT_STR_TUPLE("INITIALIZED "),      PDMAUDIOSTREAM_STS_INITIALIZED      },
        { RT_STR_TUPLE("ENABLED "),          PDMAUDIOSTREAM_STS_ENABLED          },
        { RT_STR_TUPLE("PAUSED "),           PDMAUDIOSTREAM_STS_PAUSED           },
        { RT_STR_TUPLE("PENDING_DISABLE "),  PDMAUDIOSTREAM_STS_PENDING_DISABLE  },
        { RT_STR_TUPLE("NEED_REINIT "),      PDMAUDIOSTREAM_STS_NEED_REINIT      },
        { RT_STR_TUPLE("BACKEND_READY "),    PDMAUDIOSTREAM_STS_BACKEND_READY    },
        { RT_STR_TUPLE("PREPARING_SWITCH "), PDMAUDIOSTREAM_STS_PREPARING_SWITCH },
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


/**
 * Get pre-buffer state name string.
 */
static const char *drvAudioPlayStateName(DRVAUDIOPLAYSTATE enmState)
{
    switch (enmState)
    {
        case DRVAUDIOPLAYSTATE_INVALID:             return "INVALID";
        case DRVAUDIOPLAYSTATE_NOPLAY:              return "NOPLAY";
        case DRVAUDIOPLAYSTATE_PLAY:                return "PLAY";
        case DRVAUDIOPLAYSTATE_PLAY_PREBUF:         return "PLAY_PREBUF";
        case DRVAUDIOPLAYSTATE_PREBUF:              return "PREBUF";
        case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:      return "PREBUF_OVERDUE";
        case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:    return "PREBUF_SWITCHING";
        case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:   return "PREBUF_COMMITTING";
        case DRVAUDIOPLAYSTATE_END:
            break;
    }
    return "BAD";
}


/**
 * Wrapper around PDMIHOSTAUDIO::pfnStreamGetStatus and checks the result.
 *
 * @returns PDMAUDIOSTREAM_STS_XXX
 * @param   pThis       Pointer to the DrvAudio instance data.
 * @param   pStreamEx   The stream to get the backend status for.
 */
DECLINLINE(uint32_t) drvAudioStreamGetBackendStatus(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    uint32_t fBackendStatus = pThis->pHostDrvAudio
                            ? pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pStreamEx->pBackend)
                            : 0;
    PDMAUDIOSTREAM_STS_ASSERT_VALID_BACKEND(fBackendStatus);
    return fBackendStatus;
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


/*********************************************************************************************************************************
*   PDMIAUDIOCONNECTOR                                                                                                           *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnEnable}
 */
static DECLCALLBACK(int) drvAudioEnable(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir, bool fEnable)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);
    LogFlowFunc(("enmDir=%s fEnable=%d\n", PDMAudioDirGetName(enmDir), fEnable));

    /*
     * Figure which status flag variable is being updated.
     */
    bool *pfEnabled;
    if (enmDir == PDMAUDIODIR_IN)
        pfEnabled = &pThis->In.fEnabled;
    else if (enmDir == PDMAUDIODIR_OUT)
        pfEnabled = &pThis->Out.fEnabled;
    else
        AssertFailedReturn(VERR_INVALID_PARAMETER);

    /*
     * Grab the driver wide lock and check it.  Ignore call if no change.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    if (fEnable != *pfEnabled)
    {
        LogRel(("Audio: %s %s for driver '%s'\n",
                fEnable ? "Enabling" : "Disabling", enmDir == PDMAUDIODIR_IN ? "input" : "output", pThis->szName));

        /*
         * When enabling, we must update flag before calling drvAudioStreamControlInternalBackend.
         */
        if (fEnable)
            *pfEnabled = true;

        /*
         * Update the backend status for the streams in the given direction.
         *
         * The pThis->Out.fEnable / pThis->In.fEnable status flags only reflect in the
         * direction of the backend, drivers and devices above us in the chain does not
         * know about this.  When disabled playback goes to /dev/null and we capture
         * only silence.  This means pStreamEx->fStatus holds the nominal status
         * and we'll use it to restore the operation.  (See also @bugref{9882}.)
         */
        PDRVAUDIOSTREAM pStreamEx;
        RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
        {
            if (pStreamEx->Core.enmDir == enmDir)
            {
                /*
                 * When (re-)enabling a stream, clear the disabled warning bit again.
                 */
                if (fEnable)
                    pStreamEx->Core.fWarningsShown &= ~PDMAUDIOSTREAM_WARN_FLAGS_DISABLED;

                /*
                 * We don't need to do anything unless the stream is enabled.
                 * Paused includes enabled, as does draining, but we only want the former.
                 */
                uint32_t const fStatus = pStreamEx->fStatus;
                if (fStatus & PDMAUDIOSTREAM_STS_ENABLED)
                {
                    const char *pszOperation;
                    int         rc2;
                    if (fEnable)
                    {
                        if (!(fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE))
                        {
                            rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);
                            pszOperation = "enable";
                            if (RT_SUCCESS(rc2) && (fStatus & PDMAUDIOSTREAM_STS_PAUSED))
                            {
                                rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_PAUSE);
                                pszOperation = "pause";
                            }
                        }
                        else
                        {
                            rc2 = VINF_SUCCESS;
                            pszOperation = NULL;
                        }
                    }
                    else
                    {
                        rc2 = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                        pszOperation = "disable";
                    }
                    if (RT_FAILURE(rc2))
                    {
                        LogRel(("Audio: Failed to %s %s stream '%s': %Rrc\n",
                                pszOperation, enmDir == PDMAUDIODIR_IN ? "input" : "output", pStreamEx->Core.szName, rc2));
                        if (RT_SUCCESS(rc))
                            rc = rc2;  /** @todo r=bird: This isn't entirely helpful to the caller since we'll update the status
                                        * regardless of the status code we return.  And anyway, there is nothing that can be done
                                        * about individual stream by the caller... */
                    }
                }
            }
        }

        /*
         * When disabling, we must update the status flag after the
         * drvAudioStreamControlInternalBackend(DISABLE) calls.
         */
        *pfEnabled = fEnable;
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
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamConfigHint}
 */
static DECLCALLBACK(void) drvAudioStreamConfigHintWorker(PPDMIHOSTAUDIO pHostDrvAudio, PPDMAUDIOSTREAMCFG pCfg)
{
    LogFlowFunc(("pHostDrvAudio=%p pCfg=%p\n", pHostDrvAudio, pCfg));
    AssertPtrReturnVoid(pCfg);
    AssertPtrReturnVoid(pHostDrvAudio);
    AssertPtrReturnVoid(pHostDrvAudio->pfnStreamConfigHint);

    pHostDrvAudio->pfnStreamConfigHint(pHostDrvAudio, pCfg);
    PDMAudioStrmCfgFree(pCfg);
    LogFlowFunc(("returns\n"));
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
            {
                rc = VERR_CALLBACK_RETURN;
                if (pThis->BackendCfg.fFlags & PDMAUDIOBACKEND_F_ASYNC_HINT)
                {
                    PPDMAUDIOSTREAMCFG pDupCfg = PDMAudioStrmCfgDup(pCfg);
                    if (pDupCfg)
                    {
                        rc = RTReqPoolCallVoidNoWait(pThis->hReqPool, (PFNRT)drvAudioStreamConfigHintWorker,
                                                     2, pThis->pHostDrvAudio, pDupCfg);
                        if (RT_SUCCESS(rc))
                            LogFlowFunc(("Asynchronous call running on worker thread.\n"));
                        else
                            PDMAudioStrmCfgFree(pDupCfg);
                    }
                }
                if (RT_FAILURE_NP(rc))
                {
                    LogFlowFunc(("Doing synchronous call...\n"));
                    pThis->pHostDrvAudio->pfnStreamConfigHint(pThis->pHostDrvAudio, pCfg);
                }
            }
        }
        else
            LogFunc(("Ignoring hint because direction is not currently enabled\n"));
    }
    else
        LogFlowFunc(("Ignoring hint because backend has no pfnStreamConfigHint method.\n"));

    RTCritSectLeave(&pThis->CritSect);
}


/**
 * For performing PDMIHOSTAUDIO::pfnStreamInitAsync on a worker thread.
 *
 * @param   pThis       Pointer to the DrvAudio instance data.
 * @param   pStreamEx   The stream.  One reference for us to release.
 */
static DECLCALLBACK(void) drvAudioStreamInitAsync(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    LogFlow(("pThis=%p pStreamEx=%p (%s)\n", pThis, pStreamEx->Core.szName));

    /*
     * Do the init job.
     *
     * This critsect entering and leaving here isn't really necessary,
     * but well, I'm a bit paranoid, so sue me.
     */
    RTCritSectEnter(&pThis->CritSect);
    PPDMIHOSTAUDIO pIHostDrvAudio = pThis->pHostDrvAudio;
    RTCritSectLeave(&pThis->CritSect);
    AssertPtr(pIHostDrvAudio);
    int rc;
    bool fDestroyed;
    if (pIHostDrvAudio && pIHostDrvAudio->pfnStreamInitAsync)
    {
        fDestroyed = pStreamEx->cRefs <= 1;
        rc = pIHostDrvAudio->pfnStreamInitAsync(pIHostDrvAudio, pStreamEx->pBackend, fDestroyed);
        LogFlow(("pfnStreamInitAsync returns %Rrc (on %p, fDestroyed=%d)\n", rc, pStreamEx, fDestroyed));
    }
    else
    {
        fDestroyed = true;
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    }

    /*
     * On success, update the backend on the stream status and mark it ready for business.
     */
    RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc) && !fDestroyed)
    {

        /*
         * Update the backend state.
         */
        pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_BACKEND_READY; /* before the backend control call! */

        if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED)
        {
            rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);
            if (RT_SUCCESS(rc))
            {
                if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PAUSED)
                {
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_PAUSE);
                    if (RT_FAILURE(rc))
                        LogRelMax(64, ("Audio: Failed to pause stream '%s' after initialization completed: %Rrc\n",
                                       pStreamEx->Core.szName, rc));
                }
            }
            else
                LogRelMax(64, ("Audio: Failed to enable stream '%s' after initialization completed: %Rrc\n",
                               pStreamEx->Core.szName, rc));
        }

        /*
         * Modify the play state if output stream.
         */
        if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
        {
            DRVAUDIOPLAYSTATE const enmPlayState = pStreamEx->Out.enmPlayState;
            switch (enmPlayState)
            {
                case DRVAUDIOPLAYSTATE_PREBUF:
                case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                    break;
                case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF_COMMITTING;
                    break;
                case DRVAUDIOPLAYSTATE_NOPLAY:
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF;
                    break;
                case DRVAUDIOPLAYSTATE_PLAY:
                case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
                    AssertFailedBreak();
                /* no default */
                case DRVAUDIOPLAYSTATE_END:
                case DRVAUDIOPLAYSTATE_INVALID:
                    break;
            }
            LogFunc(("enmPlayState: %s -> %s\n", drvAudioPlayStateName(enmPlayState),
                     drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
        }

        /*
         * Tweak the last backend status to asserting in
         * drvAudioStreamPlayProcessBackendStateChange().
         */
        pStreamEx->fLastBackendStatus |=   drvAudioStreamGetBackendStatus(pThis, pStreamEx)
                                         & PDMAUDIOSTREAM_STS_INITIALIZED;
    }
    /*
     * Don't quite know what to do on failure...
     */
    else if (!fDestroyed)
    {
        LogRelMax(64, ("Audio: Failed to initialize stream '%s': %Rrc\n", pStreamEx->Core.szName, rc));
    }

    /*
     * Release the request handle, must be done while inside the critical section.
     */
    if (pStreamEx->hReqInitAsync != NIL_RTREQ)
    {
        LogFlowFunc(("Releasing hReqInitAsync=%p\n", pStreamEx->hReqInitAsync));
        RTReqRelease(pStreamEx->hReqInitAsync);
        pStreamEx->hReqInitAsync = NIL_RTREQ;
    }

    RTCritSectLeave(&pThis->CritSect);

    /*
     * Release our stream reference.
     */
    uint32_t cRefs = drvAudioStreamReleaseInternal(pThis, pStreamEx, true /*fMayDestroy*/);
    LogFlowFunc(("returns (fDestroyed=%d, cRefs=%u)\n", fDestroyed, cRefs)); RT_NOREF(cRefs);
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
    AssertMsg((pStreamEx->fStatus & PDMAUDIOSTREAM_STS_INITIALIZED) == 0,
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
    AssertLogRelMsgReturn(RT_VALID_PTR(pThis->pHostDrvAudio), ("Audio: %p\n", pThis->pHostDrvAudio), VERR_PDM_NO_ATTACHED_DRIVER);
    rc = pThis->pHostDrvAudio->pfnStreamCreate(pThis->pHostDrvAudio, pStreamEx->pBackend, pCfgReq, pCfgAcq);
    if (RT_SUCCESS(rc))
    {
        AssertLogRelReturn(pStreamEx->pBackend->uMagic  == PDMAUDIOBACKENDSTREAM_MAGIC, VERR_INTERNAL_ERROR_3);
        AssertLogRelReturn(pStreamEx->pBackend->pStream == &pStreamEx->Core, VERR_INTERNAL_ERROR_3);

        /* Must set the backend-initialized flag now or the backend won't be
           destroyed (this used to be done at the end of this function, with
           several possible early return paths before it). */
        pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_INITIALIZED;

        pStreamEx->fLastBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx);
        PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
    }
    else
    {
        if (rc == VERR_NOT_SUPPORTED)
            LogRel2(("Audio: Creating stream '%s' in backend not supported\n", pStreamEx->Core.szName));
        else if (rc == VERR_AUDIO_STREAM_COULD_NOT_CREATE)
            LogRel2(("Audio: Stream '%s' could not be created in backend because of missing hardware / drivers\n", pStreamEx->Core.szName));
        else
            LogRel(("Audio: Creating stream '%s' in backend failed with %Rrc\n", pStreamEx->Core.szName, rc));
        return rc;
    }

    /* Remember if we need to call pfnStreamInitAsync. */
    pStreamEx->fNeedAsyncInit = rc == VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED;
    AssertStmt(rc != VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED || pThis->pHostDrvAudio->pfnStreamInitAsync != NULL,
               pStreamEx->fNeedAsyncInit = false);
    Assert(rc != VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED || !(pStreamEx->fLastBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED));

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
        Assert(pCfgHost->enmDir == PDMAUDIODIR_IN);
        rc = AudioMixBufInit(&pStreamEx->Host.MixBuf, pStreamEx->Core.szName, &CfgHostAcq.Props, CfgHostAcq.Backend.cFramesBufferSize);
        AssertRCReturn(rc, rc);
    }
    /* Allocate space for pre-buffering of output stream w/o mixing buffers. */
    else if (pCfgHost->enmDir == PDMAUDIODIR_OUT)
    {
        Assert(pStreamEx->Out.cbPreBufAlloc == 0);
        Assert(pStreamEx->Out.cbPreBufThreshold == 0);
        Assert(pStreamEx->Out.cbPreBuffered == 0);
        Assert(pStreamEx->Out.offPreBuf == 0);
        if (CfgHostAcq.Backend.cFramesPreBuffering != 0)
        {
            pStreamEx->Out.cbPreBufThreshold = PDMAudioPropsFramesToBytes(&CfgHostAcq.Props, CfgHostAcq.Backend.cFramesPreBuffering);
            pStreamEx->Out.cbPreBufAlloc     = PDMAudioPropsFramesToBytes(&CfgHostAcq.Props,
                                                                          CfgHostAcq.Backend.cFramesBufferSize - 2);
            pStreamEx->Out.cbPreBufAlloc     = RT_MIN(RT_ALIGN_32(pStreamEx->Out.cbPreBufThreshold + _8K, _4K),
                                                     pStreamEx->Out.cbPreBufAlloc);
            pStreamEx->Out.pbPreBuf          = (uint8_t *)RTMemAllocZ(pStreamEx->Out.cbPreBufAlloc);
            AssertReturn(pStreamEx->Out.pbPreBuf, VERR_NO_MEMORY);
        }
        pStreamEx->Out.enmPlayState          = DRVAUDIOPLAYSTATE_NOPLAY; /* Changed upon enable. */
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
        Assert(pCfgHost->enmDir == PDMAUDIODIR_IN);
        rc = AudioMixBufInit(&pStreamEx->Guest.MixBuf, pStreamEx->Core.szName, &pCfgGuest->Props, CfgHostAcq.Backend.cFramesBufferSize);
        AssertRCReturn(rc, rc);
    }

    if (RT_FAILURE(rc))
        LogRel(("Audio: Creating stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));

    if (!(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF))
    {
        Assert(pCfgHost->enmDir == PDMAUDIODIR_IN);
        /* Host (Parent) -> Guest (Child). */
        rc = AudioMixBufLinkTo(&pStreamEx->Host.MixBuf, &pStreamEx->Guest.MixBuf);
        AssertRC(rc);
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
        Assert(pCfgHost->enmDir == PDMAUDIODIR_IN);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.MixBuf.cFrames, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Host side: The size of the mixer buffer (in frames)",   "%s/1-HostMixBufSize", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Guest.MixBuf.cFrames, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Guest side: The size of the mixer buffer (in frames)",  "%s/2-GuestMixBufSize", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Host.MixBuf.cMixed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Host side: Number of frames in the mixer buffer",   "%s/1-HostMixBufUsed", pStreamEx->Core.szName);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->Guest.MixBuf.cUsed, STAMTYPE_U32, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Guest side: Number of frames in the mixer buffer",  "%s/2-GuestMixBufUsed", pStreamEx->Core.szName);
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
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pStreamEx->In.Stats.TotalTimesRead, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_NONE,
                               "Total number of reads.", "%s/TotalTimesRead", pStreamEx->Core.szName);
    }
    else
    {
        Assert(pCfgGuest->enmDir == PDMAUDIODIR_OUT);
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
    /* Require PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF for output streams: */
    AssertReturn((fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF) || pCfgHost->enmDir == PDMAUDIODIR_IN, VERR_INVALID_FLAGS);

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
    AssertStmt(cbHstStrm >= sizeof(PDMAUDIOBACKENDSTREAM), rc = VERR_OUT_OF_RANGE);
    AssertStmt(cbHstStrm < _16M, rc = VERR_OUT_OF_RANGE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize common state.
         */
        PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)RTMemAllocZ(sizeof(DRVAUDIOSTREAM) + RT_ALIGN_Z(cbHstStrm, 64));
        if (pStreamEx)
        {
            /* Make a unqiue stream name including the host (backend) driver name. */
            AssertPtr(pThis->pHostDrvAudio);
            size_t cchName = RTStrPrintf(pStreamEx->Core.szName, RT_ELEMENTS(pStreamEx->Core.szName), "[%s] %s:0",
                                         pThis->BackendCfg.szName, pCfgHost->szName[0] != '\0' ? pCfgHost->szName : "<Untitled>");
            if (cchName < sizeof(pStreamEx->Core.szName))
            {
                for (uint32_t i = 0; i < 256; i++)
                {
                    bool fDone = true;
                    PDRVAUDIOSTREAM pIt;
                    RTListForEach(&pThis->lstStreams, pIt, DRVAUDIOSTREAM, ListEntry)
                    {
                        if (strcmp(pIt->Core.szName, pStreamEx->Core.szName) == 0)
                        {
                            RTStrPrintf(pStreamEx->Core.szName, RT_ELEMENTS(pStreamEx->Core.szName), "[%s] %s:%u",
                                        pThis->BackendCfg.szName, pCfgHost->szName[0] != '\0' ? pCfgHost->szName : "<Untitled>",
                                        i);
                            fDone = false;
                            break;
                        }
                    }
                    if (fDone)
                        break;
                }
            }

            PPDMAUDIOBACKENDSTREAM pBackend = (PPDMAUDIOBACKENDSTREAM)(pStreamEx + 1);
            pBackend->uMagic            = PDMAUDIOBACKENDSTREAM_MAGIC;
            pBackend->pStream           = &pStreamEx->Core;
            pStreamEx->pBackend         = pBackend;
            pStreamEx->Core.enmDir      = pCfgHost->enmDir;
            pStreamEx->Core.cbBackend   = (uint32_t)cbHstStrm;
            pStreamEx->fNoMixBufs       = RT_BOOL(fFlags & PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF);
            pStreamEx->hReqInitAsync    = NIL_RTREQ;
            pStreamEx->uMagic           = DRVAUDIOSTREAM_MAGIC;

            /*
             * Try to init the rest.
             */
            rc = drvAudioStreamInitInternal(pThis, pStreamEx, fFlags, pCfgHost, pCfgGuest);
            if (RT_SUCCESS(rc))
            {
                /* Set initial reference counts. */
                pStreamEx->cRefs = pStreamEx->fNeedAsyncInit ? 2 : 1;

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

                /*
                 * Kick off the asynchronous init.
                 */
                if (!pStreamEx->fNeedAsyncInit)
                {
                    pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_BACKEND_READY;
                    PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
                }
                else
                {
                    int rc2 = RTReqPoolCallEx(pThis->hReqPool, 0 /*cMillies*/, &pStreamEx->hReqInitAsync,
                                              RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                                              (PFNRT)drvAudioStreamInitAsync, 2, pThis, pStreamEx);
                    LogFlowFunc(("hReqInitAsync=%p rc2=%Rrc\n", pStreamEx->hReqInitAsync, rc2));
                    AssertRCStmt(rc2, drvAudioStreamInitAsync(pThis, pStreamEx));
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
    LogFunc(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));

    if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_INITIALIZED)
    {
        AssertPtr(pStreamEx->pBackend);

        /* Check if the pointer to  the host audio driver is still valid.
         * It can be NULL if we were called in drvAudioDestruct, for example. */
        if (pThis->pHostDrvAudio)
            rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pStreamEx->pBackend);

        pStreamEx->fStatus &= ~(PDMAUDIOSTREAM_STS_INITIALIZED | PDMAUDIOSTREAM_STS_BACKEND_READY);
        PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
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
    AssertMsgReturn(pStreamEx->cRefs <= 1,
                    ("Stream '%s' still has %RU32 references held when uninitializing\n", pStreamEx->Core.szName, pStreamEx->cRefs),
                    VERR_WRONG_ORDER);
    LogFlowFunc(("[%s] cRefs=%RU32\n", pStreamEx->Core.szName, pStreamEx->cRefs));

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
        pStreamEx->Out.offPreBuf     = 0;
    }

    if (RT_SUCCESS(rc))
    {
#ifdef LOG_ENABLED
        if (pStreamEx->fStatus != PDMAUDIOSTREAM_STS_NONE)
        {
            char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
            LogFunc(("[%s] Warning: Still has %s set when uninitializing\n",
                     pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));
        }
#endif
        pStreamEx->fStatus = PDMAUDIOSTREAM_STS_NONE;
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
 * Internal release function.
 *
 * @returns New reference count, UINT32_MAX if bad stream.
 * @param   pThis           Pointer to the DrvAudio instance data.
 * @param   pStreamEx       The stream to reference.
 * @param   fMayDestroy     Whether the caller is allowed to implicitly destroy
 *                          the stream or not.
 */
static uint32_t drvAudioStreamReleaseInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, bool fMayDestroy)
{
    AssertPtrReturn(pStreamEx, UINT32_MAX);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, UINT32_MAX);
    AssertReturn(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pStreamEx->cRefs);
    if (cRefs != 0)
        Assert(cRefs < _1K);
    else if (fMayDestroy)
    {
/** @todo r=bird: Caching one stream in each direction for some time,
 * depending on the time it took to create it.  drvAudioStreamCreate can use it
 * if the configuration matches, otherwise it'll throw it away.  This will
 * provide a general speedup independ of device (HDA used to do this, but
 * doesn't) and backend implementation.  Ofc, the backend probably needs an
 * opt-out here. */
        int rc = RTCritSectEnter(&pThis->CritSect);
        AssertRC(rc);

        rc = drvAudioStreamUninitInternal(pThis, pStreamEx);
        if (RT_SUCCESS(rc))
        {
            if (pStreamEx->Core.enmDir == PDMAUDIODIR_IN)
                pThis->In.cStreamsFree++;
            else /* Out */
                pThis->Out.cStreamsFree++;

            RTListNodeRemove(&pStreamEx->ListEntry);

            drvAudioStreamFree(pStreamEx);
        }
        else
        {
            LogRel(("Audio: Uninitializing stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));
            /** @todo r=bird: What's the plan now? */
        }

        RTCritSectLeave(&pThis->CritSect);
    }
    else
    {
        cRefs = ASMAtomicIncU32(&pStreamEx->cRefs);
        AssertFailed();
    }

    Log12Func(("returns %u (%s)\n", cRefs, cRefs > 0 ? pStreamEx->Core.szName : "destroyed"));
    return cRefs;
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /* Ignore NULL streams. */
    if (!pStream)
        return VINF_SUCCESS;

    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;   /* Note! Do not touch pStream after this! */
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    LogFlowFunc(("ENTER - %p %s\n", pStreamEx, pStreamEx->Core.szName));
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->pBackend && pStreamEx->pBackend->uMagic == PDMAUDIOBACKENDSTREAM_MAGIC, VERR_INVALID_MAGIC);

    /*
     * The main difference from a regular release is that this will disable
     * (or drain if we could) the stream and we can cancel any pending
     * pfnStreamInitAsync call.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    if (pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC)
    {
        if (pStreamEx->cRefs > 0 && pStreamEx->cRefs < UINT32_MAX / 4)
        {
            char szStatus[DRVAUDIO_STATUS_STR_MAX], szBackendStatus[DRVAUDIO_STATUS_STR_MAX];
            LogRel2(("Audio: Destroying stream '%s': cRefs=%u; status: %s; backend: %s; hReqInitAsync=%p\n",
                     pStreamEx->Core.szName, pStreamEx->cRefs, dbgAudioStreamStatusToStr(szStatus, pStreamEx->fStatus),
                     dbgAudioStreamStatusToStr(szBackendStatus, drvAudioStreamGetBackendStatus(pThis, pStreamEx)),
                     pStreamEx->hReqInitAsync));

            /* Try cancel pending async init request and release the it. */
            if (pStreamEx->hReqInitAsync != NIL_RTREQ)
            {
                Assert(pStreamEx->cRefs >= 2);
                int rc2 = RTReqCancel(pStreamEx->hReqInitAsync);
                if (RT_SUCCESS(rc2))
                {
                    LogFlowFunc(("Successfully cancelled pending pfnStreamInitAsync call (hReqInitAsync=%p).\n",
                                 pStreamEx->hReqInitAsync));
                    drvAudioStreamReleaseInternal(pThis, pStreamEx, true /*fMayDestroy*/);
                }
                else
                {
                    LogFlowFunc(("Failed to cancel pending pfnStreamInitAsync call (hReqInitAsync=%p): %Rrc\n",
                                 pStreamEx->hReqInitAsync, rc2));
                    Assert(rc2 == VERR_RT_REQUEST_STATE);
                }

                RTReqRelease(pStreamEx->hReqInitAsync);
                pStreamEx->hReqInitAsync = NIL_RTREQ;
            }

            /* We don't really care about the status here as we'll release a reference regardless of the state. */
            /** @todo can we somehow drain it instead? */
            int rc2 = drvAudioStreamControlInternal(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
            AssertRC(rc2);

            drvAudioStreamReleaseInternal(pThis, pStreamEx, true /*fMayDestroy*/);
        }
        else
            AssertLogRelMsgFailedStmt(("%p cRefs=%#x\n", pStreamEx, pStreamEx->cRefs), rc = VERR_CALLER_NO_REFERENCE);
    }
    else
        AssertLogRelMsgFailedStmt(("%p uMagic=%#x\n", pStreamEx, pStreamEx->uMagic), rc = VERR_INVALID_MAGIC);

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Drops all audio data (and associated state) of a stream.
 *
 * Used by drvAudioStreamIterateInternal(), drvAudioStreamResetInternal(), and
 * drvAudioStreamReInitInternal().
 *
 * @param   pStreamEx   Stream to drop data for.
 */
static void drvAudioStreamDropInternal(PDRVAUDIOSTREAM pStreamEx)
{
    LogFunc(("[%s]\n", pStreamEx->Core.szName));

    if (pStreamEx->fNoMixBufs)
    {
        AudioMixBufReset(&pStreamEx->Guest.MixBuf);
        AudioMixBufReset(&pStreamEx->Host.MixBuf);
    }

    pStreamEx->nsLastIterated       = 0;
    pStreamEx->nsLastPlayedCaptured = 0;
    pStreamEx->nsLastReadWritten    = 0;
    if (pStreamEx->Host.Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        pStreamEx->Out.cbPreBuffered = 0;
        pStreamEx->Out.offPreBuf     = 0;
        pStreamEx->Out.enmPlayState  = pStreamEx->Out.cbPreBufThreshold > 0
                                      ? DRVAUDIOPLAYSTATE_PREBUF : DRVAUDIOPLAYSTATE_PLAY;
    }
}


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
    const bool fIsEnabled = RT_BOOL(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED); /* Stream is enabled? */

/** @todo r=bird: this is retried a bit too indiscriminately for my taste ... */
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

/** @todo r=bird: Why do we do the dropping and re-enabling of the stream
 *        regardless of how the above went?  It'll overwrite any above
 *        failures for starters... */
    /* Drop all old data. */
    drvAudioStreamDropInternal(pStreamEx);

    /*
     * Restore previous stream state.
     */
    /** @todo this isn't taking PAUSED or PENDING_DISABLE into consideration.   */
    if (fIsEnabled)
        rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Re-initializing stream '%s' failed with %Rrc\n", pStreamEx->Core.szName, rc));

    LogFunc(("[%s] Returning %Rrc\n", pStreamEx->Core.szName, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamReInit}
 */
static DECLCALLBACK(int) drvAudioStreamReInit(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_NEED_REINIT, VERR_INVALID_STATE);
    LogFlowFunc(("\n"));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_NEED_REINIT)
    {
        const unsigned cMaxTries = 3; /** @todo Make this configurable? */
        const uint64_t tsNowNs   = RTTimeNanoTS();

/** @todo r=bird: Must postpone if hReqInitAsync isn't NIL or cancellable. */

        /* Throttle re-initializing streams on failure. */
        if (   pStreamEx->cTriesReInit < cMaxTries
            && (   pStreamEx->nsLastReInit == 0
                || tsNowNs - pStreamEx->nsLastReInit >= RT_NS_1SEC * pStreamEx->cTriesReInit)) /** @todo Ditto. */
        {
#ifdef VBOX_WITH_AUDIO_ENUM
/** @todo do this elsewhere.  */
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

            rc = drvAudioStreamReInitInternal(pThis, pStreamEx);
            if (RT_SUCCESS(rc))
            {
                /* Remove the pending re-init flag on success. */
                pStreamEx->fStatus &= ~PDMAUDIOSTREAM_STS_NEED_REINIT;
                PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
            }
            else
            {
                pStreamEx->cTriesReInit++;
                pStreamEx->nsLastReInit = tsNowNs;
            }
        }
        else
        {
            /* Did we exceed our tries re-initializing the stream?
             * Then this one is dead-in-the-water, so disable it for further use. */
/** @todo r=bird: This should be done above when drvAudioStreamReInitInternal fails! Duh^2! */
            if (pStreamEx->cTriesReInit == cMaxTries)
            {
                LogRel(("Audio: Re-initializing stream '%s' exceeded maximum retries (%u), leaving as disabled\n",
                        pStreamEx->Core.szName, cMaxTries));

                /* Don't try to re-initialize anymore and mark as disabled. */
                /** @todo should mark it as not-initialized too, shouldn't we?   */
                pStreamEx->fStatus &= ~(PDMAUDIOSTREAM_STS_NEED_REINIT | PDMAUDIOSTREAM_STS_ENABLED);
                PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);

                /* Note: Further writes to this stream go to / will be read from the bit bucket (/dev/null) from now on. */
            }
        }

#ifdef LOG_ENABLED
        char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
        Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));
    }
    else
    {
        AssertFailed();
        rc = VERR_INVALID_STATE;
    }

    RTCritSectLeave(&pThis->CritSect);
    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Internal retain function.
 *
 * @returns New reference count, UINT32_MAX if bad stream.
 * @param   pStreamEx           The stream to reference.
 */
static uint32_t drvAudioStreamRetainInternal(PDRVAUDIOSTREAM pStreamEx)
{
    AssertPtrReturn(pStreamEx, UINT32_MAX);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, UINT32_MAX);
    AssertReturn(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, UINT32_MAX);

    uint32_t const cRefs = ASMAtomicIncU32(&pStreamEx->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < _1K);

    Log12Func(("returns %u (%s)\n", cRefs, pStreamEx->Core.szName));
    return cRefs;
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRetain}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRetain(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pInterface);
    return drvAudioStreamRetainInternal((PDRVAUDIOSTREAM)pStream);
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRelease}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRelease(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    return drvAudioStreamReleaseInternal(RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector),
                                         (PDRVAUDIOSTREAM)pStream,
                                         false /*fMayDestroy*/);
}


/**
 * Controls a stream's backend.
 *
 * @returns VBox status code.
 * @param   pThis           Pointer to driver instance.
 * @param   pStreamEx       Stream to control.
 * @param   enmStreamCmd    Control command.
 *
 * @note    Caller has entered the critical section.
 */
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtr(pThis);
    AssertPtr(pStreamEx);

    /*
     * Whether to propagate commands down to the backend.
     *
     *      1. If the stream direction is disabled on the driver level, we should
     *         obviously not call the backend.  Our stream status will reflect the
     *         actual state so drvAudioEnable() can tell the backend if the user
     *         re-enables the stream direction.
     *
     *      2. If the backend hasn't finished initializing yet, don't try call
     *         it to start/stop/pause/whatever the stream.  (Better to do it here
     *         than to replicate this in the relevant backends.)  When the backend
     *         finish initializing the stream, we'll update it about the stream state.
     */
    int             rc             = VINF_SUCCESS;
    uint32_t const  fBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx); /* (checks pThis->pHostDrvAudio too) */
    bool const      fDirEnabled    = pStreamEx->Core.enmDir == PDMAUDIODIR_IN ? pThis->In.fEnabled : pThis->Out.fEnabled;

    char szStreamSts[DRVAUDIO_STATUS_STR_MAX], szBackendStreamSts[DRVAUDIO_STATUS_STR_MAX];
    LogRel2(("Audio: %s stream '%s' backend (%s is %s; status: %s; backend-status: %s)\n",
             PDMAudioStrmCmdGetName(enmStreamCmd), pStreamEx->Core.szName, PDMAudioDirGetName(pStreamEx->Core.enmDir),
             fDirEnabled ? "enabled" : "disabled",  dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus),
             dbgAudioStreamStatusToStr(szBackendStreamSts, fBackendStatus) ));

    if (fDirEnabled)
    {
        if (   (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
            && (fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED))
        {
            /** @todo Backend will change to explicit methods here, so please don't simplify
             *        the switch. */
            switch (enmStreamCmd)
            {
                case PDMAUDIOSTREAMCMD_ENABLE:
                    rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                                PDMAUDIOSTREAMCMD_ENABLE);
                    break;

                case PDMAUDIOSTREAMCMD_DISABLE:
                    rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                                PDMAUDIOSTREAMCMD_DISABLE);
                    break;

                case PDMAUDIOSTREAMCMD_PAUSE:
                    rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                                PDMAUDIOSTREAMCMD_PAUSE);
                    break;

                case PDMAUDIOSTREAMCMD_RESUME:
                    rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                                PDMAUDIOSTREAMCMD_RESUME);
                    break;

                case PDMAUDIOSTREAMCMD_DRAIN:
                    rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pStreamEx->pBackend,
                                                                PDMAUDIOSTREAMCMD_DRAIN);
                    break;

                default:
                    AssertMsgFailedReturn(("Command %RU32 not implemented\n", enmStreamCmd), VERR_INTERNAL_ERROR_2);
            }
            if (RT_SUCCESS(rc))
                Log2Func(("[%s] %s succeeded (%Rrc)\n", pStreamEx->Core.szName, PDMAudioStrmCmdGetName(enmStreamCmd), rc));
            else
            {
                LogFunc(("[%s] %s failed with %Rrc\n", pStreamEx->Core.szName, PDMAudioStrmCmdGetName(enmStreamCmd), rc));
                if (   rc != VERR_NOT_IMPLEMENTED
                    && rc != VERR_NOT_SUPPORTED
                    && rc != VERR_AUDIO_STREAM_NOT_READY)
                    LogRel(("Audio: %s stream '%s' failed with %Rrc\n", PDMAudioStrmCmdGetName(enmStreamCmd), pStreamEx->Core.szName, rc));
            }
        }
    }
    return rc;
}


/**
 * Resets the given audio stream.
 *
 * @param   pStreamEx   Stream to reset.
 */
static void drvAudioStreamResetInternal(PDRVAUDIOSTREAM pStreamEx)
{
    drvAudioStreamDropInternal(pStreamEx);

    LogFunc(("[%s]\n", pStreamEx->Core.szName));

    pStreamEx->fStatus            &= PDMAUDIOSTREAM_STS_INITIALIZED | PDMAUDIOSTREAM_STS_BACKEND_READY;
    pStreamEx->Core.fWarningsShown = PDMAUDIOSTREAM_WARN_FLAGS_NONE;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Reset statistics.
     */
    if (pStreamEx->Core.enmDir == PDMAUDIODIR_IN)
    {
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalFramesCaptured);
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalTimesCaptured);
        STAM_COUNTER_RESET(&pStreamEx->In.Stats.TotalTimesRead);
    }
    else if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
    {
    }
    else
        AssertFailed();
#endif
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
            && pStreamEx->cRefs >= 1)
        {
            if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE)
            {
                drvAudioStreamIterateInternal(pThis, pStreamEx);

                if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE)
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
             dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            if (!(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED))
            {
                /* Is a pending disable outstanding? Then disable first. */
                if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE)
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    /* Reset the play state before we try to start. */
                    pStreamEx->fLastBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx);
                    if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
                    {
                        pStreamEx->Out.cbPreBuffered = 0;
                        pStreamEx->Out.offPreBuf     = 0;
                        pStreamEx->Out.enmPlayState  = pStreamEx->Out.cbPreBufThreshold > 0
                                                     ? DRVAUDIOPLAYSTATE_PREBUF
                                                     :    (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
                                                       && (pStreamEx->fLastBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED)
                                                     ? DRVAUDIOPLAYSTATE_PLAY
                                                     : DRVAUDIOPLAYSTATE_NOPLAY;
                        LogFunc(("ENABLE: fLastBackendStatus=%#x enmPlayState=%s\n",
                                 pStreamEx->fLastBackendStatus, drvAudioPlayStateName(pStreamEx->Out.enmPlayState)));
                    }
                    else
                        LogFunc(("ENABLE: fLastBackendStatus=%#x\n", pStreamEx->fLastBackendStatus));

                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_ENABLE);
                    if (RT_SUCCESS(rc))
                    {
                        pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_ENABLED;
                        PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
                    }
                }
            }
            break;

        case PDMAUDIOSTREAMCMD_DISABLE:
            if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED)
            {
                /*
                 * For playback (output) streams first mark the host stream as pending disable,
                 * so that the rest of the remaining audio data will be played first before
                 * closing the stream.
                 */
                if (pStreamEx->Core.enmDir == PDMAUDIODIR_OUT)
                {
                    LogFunc(("[%s] Pending disable/pause\n", pStreamEx->Core.szName));
                    pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_PENDING_DISABLE;
                    PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);

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
                if (!(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE))
                {
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                        drvAudioStreamResetInternal(pStreamEx);
                }
            }
            break;

        case PDMAUDIOSTREAMCMD_PAUSE:
            if ((pStreamEx->fStatus & (PDMAUDIOSTREAM_STS_ENABLED | PDMAUDIOSTREAM_STS_PAUSED)) == PDMAUDIOSTREAM_STS_ENABLED)
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_PAUSE);
                if (RT_SUCCESS(rc))
                {
                    pStreamEx->fStatus |= PDMAUDIOSTREAM_STS_PAUSED;
                    PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
                }
            }
            break;

        case PDMAUDIOSTREAMCMD_RESUME:
            if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PAUSED)
            {
                Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED);
                rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_RESUME);
                if (RT_SUCCESS(rc))
                {
                    pStreamEx->fStatus &= ~PDMAUDIOSTREAM_STS_PAUSED;
                    PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
                }
            }
            break;

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStreamEx->Core.szName, rc));

    return rc;
}


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
 * Copy data to the pre-buffer, ring-buffer style.
 *
 * This is used in two slightly different situations:
 *
 *      -# When the stream is started (enabled) and we only want to prebuffer up
 *         to the threshold before pushing the data to the backend.  We
 *         typically use the max buffer size for this situation.
 *
 *      -# When the backend sets the PDMAUDIOSTREAM_STS_PREPARING_SWITCH
 *         status bit and we're preparing for a smooth switch over to a
 *         different audio device.  Most of the pre-buffered data should not be
 *         played on the old device prior to the switch, due to the prebuffering
 *         at the start of the stream.  We only use the threshold size for this
 *         case.
 */
static int drvAudioStreamPreBuffer(PDRVAUDIOSTREAM pStreamEx, const uint8_t *pbBuf, uint32_t cbBuf, uint32_t cbMax)
{
    uint32_t const cbAlloc = pStreamEx->Out.cbPreBufAlloc;
    AssertReturn(cbAlloc >= cbMax, VERR_INTERNAL_ERROR_3);
    AssertReturn(cbAlloc >= 8, VERR_INTERNAL_ERROR_4);
    AssertReturn(cbMax >= 8, VERR_INTERNAL_ERROR_5);

    uint32_t offRead = pStreamEx->Out.offPreBuf;
    uint32_t cbCur   = pStreamEx->Out.cbPreBuffered;
    AssertStmt(offRead < cbAlloc, offRead %= cbAlloc);
    AssertStmt(cbCur <= cbMax, offRead = (offRead + cbCur - cbMax) % cbAlloc; cbCur = cbMax);

    /*
     * First chunk.
     */
    uint32_t offWrite = (offRead + cbCur) % cbAlloc;
    uint32_t cbToCopy = RT_MIN(cbAlloc - offWrite, cbBuf);
    memcpy(&pStreamEx->Out.pbPreBuf[offWrite], pbBuf, cbToCopy);

    /* Advance. */
    offWrite = (offWrite + cbToCopy) % cbAlloc;
    for (;;)
    {
        pbBuf    += cbToCopy;
        cbCur    += cbToCopy;
        if (cbCur > cbMax)
            offRead = (offRead + cbCur - cbMax) % cbAlloc;
        cbBuf    -= cbToCopy;
        if (!cbBuf)
            break;

        /*
         * Second+ chunk, from the start of the buffer.
         *
         * Note! It is assumed very unlikely that we will ever see a cbBuf larger than
         *       cbMax, so we don't waste space on clipping cbBuf here (can happen with
         *       custom pre-buffer sizes).
         */
        Assert(offWrite == 0);
        cbToCopy = RT_MIN(cbAlloc, cbBuf);
        memcpy(pStreamEx->Out.pbPreBuf, pbBuf, cbToCopy);
    }

    /*
     * Update the pre-buffering size and position.
     */
    pStreamEx->Out.cbPreBuffered = RT_MIN(cbCur, cbMax);
    pStreamEx->Out.offPreBuf     = offRead;
    return VINF_SUCCESS;
}


/**
 * Worker for drvAudioStreamPlay() and drvAudioStreamPreBufComitting().
 *
 * Caller owns the lock.
 */
static int drvAudioStreamPlayLocked(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx,
                                    const uint8_t *pbBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    Log3Func(("%s: @%#RX64: cbBuf=%#x\n", pStreamEx->Core.szName, pStreamEx->offInternal, cbBuf));

    uint32_t      cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
    pStreamEx->Out.Stats.cbBackendWritableBefore = cbWritable;

    uint32_t      cbWritten  = 0;
    int           rc         = VINF_SUCCESS;
    uint8_t const cbFrame    = PDMAudioPropsFrameSize(&pStreamEx->Core.Props);
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
            cbWritten              += cbWrittenNow;
            cbBuf                  -= cbWrittenNow;
            pbBuf                  += cbWrittenNow;
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
 * Worker for drvAudioStreamPlay() and drvAudioStreamPreBufComitting().
 */
static int drvAudioStreamPlayToPreBuffer(PDRVAUDIOSTREAM pStreamEx, const void *pvBuf, uint32_t cbBuf, uint32_t cbMax,
                                         uint32_t *pcbWritten)
{
    int rc = drvAudioStreamPreBuffer(pStreamEx, (uint8_t const *)pvBuf, cbBuf, cbMax);
    if (RT_SUCCESS(rc))
    {
        *pcbWritten = cbBuf;
        pStreamEx->offInternal += cbBuf;
        Log3Func(("[%s] Pre-buffering (%s): wrote %#x bytes => %#x bytes / %u%%\n",
                  pStreamEx->Core.szName, drvAudioPlayStateName(pStreamEx->Out.enmPlayState), cbBuf, pStreamEx->Out.cbPreBuffered,
                  pStreamEx->Out.cbPreBuffered * 100 / RT_MAX(pStreamEx->Out.cbPreBufThreshold, 1)));

    }
    else
        *pcbWritten = 0;
    return rc;
}


/**
 * Used when we're committing (transfering) the pre-buffered bytes to the
 * device.
 *
 * This is called both from drvAudioStreamPlay() and
 * drvAudioStreamIterateInternal().
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the DrvAudio instance data.
 * @param   pStreamEx   The stream to commit the pre-buffering for.
 * @param   pbBuf       Buffer with new bytes to write.  Can be NULL when called
 *                      in the PENDING_DISABLE state from
 *                      drvAudioStreamIterateInternal().
 * @param   cbBuf       Number of new bytes.  Can be zero.
 * @param   pcbWritten  Where to return the number of bytes written.
 */
static int drvAudioStreamPreBufComitting(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx,
                                         const uint8_t *pbBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    /*
     * First, top up the buffer with new data from pbBuf.
     */
    *pcbWritten = 0;
    if (cbBuf > 0)
    {
        uint32_t const cbToCopy = RT_MIN(pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBuffered, cbBuf);
        if (cbToCopy > 0)
        {
            int rc = drvAudioStreamPlayToPreBuffer(pStreamEx, pbBuf, cbBuf, pStreamEx->Out.cbPreBufAlloc, pcbWritten);
            AssertRCReturn(rc, rc);
            pbBuf += cbToCopy;
            cbBuf -= cbToCopy;
        }
    }

    /*
     * Write the pre-buffered chunk.
     */
    int             rc      = VINF_SUCCESS;
    uint32_t const  cbAlloc = pStreamEx->Out.cbPreBufAlloc;
    AssertReturn(cbAlloc > 0, VERR_INTERNAL_ERROR_2);
    uint32_t        off     = pStreamEx->Out.offPreBuf;
    AssertStmt(off < pStreamEx->Out.cbPreBufAlloc, off %= cbAlloc);
    uint32_t        cbLeft  = pStreamEx->Out.cbPreBuffered;
    while (cbLeft > 0)
    {
        uint32_t const cbToWrite = RT_MIN(cbAlloc - off, cbLeft);
        Assert(cbToWrite > 0);

        uint32_t cbPreBufWritten = 0;
        rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pStreamEx->pBackend, &pStreamEx->Out.pbPreBuf[off],
                                                 cbToWrite, &cbPreBufWritten);
        AssertRCBreak(rc);
        if (!cbPreBufWritten)
            break;
        AssertStmt(cbPreBufWritten <= cbToWrite, cbPreBufWritten = cbToWrite);
        off     = (off + cbPreBufWritten) % cbAlloc;
        cbLeft -= cbPreBufWritten;
    }

    if (cbLeft == 0)
    {
        LogFunc(("@%#RX64: Wrote all %#x bytes of pre-buffered audio data. %s -> PLAY\n", pStreamEx->offInternal,
                 pStreamEx->Out.cbPreBuffered, drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
        pStreamEx->Out.cbPreBuffered = 0;
        pStreamEx->Out.offPreBuf     = 0;
        pStreamEx->Out.enmPlayState  = DRVAUDIOPLAYSTATE_PLAY;

        if (cbBuf > 0)
        {
            uint32_t cbWritten2 = 0;
            rc = drvAudioStreamPlayLocked(pThis, pStreamEx, pbBuf, cbBuf, &cbWritten2);
            if (RT_SUCCESS(rc))
                *pcbWritten += cbWritten2;
        }
        else
            pStreamEx->nsLastPlayedCaptured = RTTimeNanoTS();
    }
    else
    {
        if (cbLeft != pStreamEx->Out.cbPreBuffered)
            pStreamEx->nsLastPlayedCaptured = RTTimeNanoTS();

        LogRel2(("Audio: @%#RX64: Stream '%s' pre-buffering commit problem: wrote %#x out of %#x + %#x - rc=%Rrc *pcbWritten=%#x %s -> PREBUF_COMMITTING\n",
                 pStreamEx->offInternal, pStreamEx->Core.szName, pStreamEx->Out.cbPreBuffered - cbLeft,
                 pStreamEx->Out.cbPreBuffered, cbBuf, rc, *pcbWritten, drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
        AssertMsg(pStreamEx->Out.enmPlayState == DRVAUDIOPLAYSTATE_PREBUF_COMMITTING || RT_FAILURE(rc),
                  ("Buggy host driver buffer reporting? cbLeft=%#x cbPreBuffered=%#x\n", cbLeft, pStreamEx->Out.cbPreBuffered));

        pStreamEx->Out.cbPreBuffered = cbLeft;
        pStreamEx->Out.offPreBuf     = off;
        pStreamEx->Out.enmPlayState  = DRVAUDIOPLAYSTATE_PREBUF_COMMITTING;
    }

    return *pcbWritten ? VINF_SUCCESS : rc;
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
 */
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

#ifdef LOG_ENABLED
    char szStreamSts[DRVAUDIO_STATUS_STR_MAX];
#endif
    Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));

    /* Not enabled or paused? Skip iteration. */
    if ((pStreamEx->fStatus & (PDMAUDIOSTREAM_STS_ENABLED | PDMAUDIOSTREAM_STS_PAUSED)) != PDMAUDIOSTREAM_STS_ENABLED)
        return VINF_SUCCESS;

    /*
     * Pending disable is really what we're here for.  This only happens to output streams.
     */
    int rc = VINF_SUCCESS;
    if (!(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_PENDING_DISABLE))
    { /* likely until we get to the end of the stream at least. */ }
    else
    {
        AssertReturn(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT, VINF_SUCCESS);
        /** @todo Add a timeout to these proceedings.  A few times that of the reported
         *        buffer size or something like that. */

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
        uint32_t cFramesLive = 0;
        switch (pStreamEx->Out.enmPlayState)
        {
            case DRVAUDIOPLAYSTATE_PLAY:                /* Nothing prebuffered. */
            case DRVAUDIOPLAYSTATE_PLAY_PREBUF:         /* Not pre-buffering for current device. */
            case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:      /* Output device isn't ready, drop it. */ /** @todo check state? */
            case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:    /* No output device yet. */
            case DRVAUDIOPLAYSTATE_NOPLAY:
            case DRVAUDIOPLAYSTATE_INVALID:
            case DRVAUDIOPLAYSTATE_END:
                /* no default, want warnings. */
                break;
            case DRVAUDIOPLAYSTATE_PREBUF:
            case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
                if (pStreamEx->Out.cbPreBuffered > 0)
                {
                    /* Must check the backend state here first and only try commit the
                       pre-buffered samples if the backend is in working order. */
                    uint32_t const fBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx); /* (checks pThis->pHostDrvAudio too) */
                    if (   (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
                        && (fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED))
                    {
                        uint32_t cbIgnored = 0;
                        drvAudioStreamPreBufComitting(pThis, pStreamEx, NULL, 0, &cbIgnored);
                        cFramesLive = PDMAudioPropsBytesToFrames(&pStreamEx->Core.Props, pStreamEx->Out.cbPreBuffered);
                    }
                    else
                        Log3Func(("[%s] Skipping committing pre-buffered samples, backend not initialized (%#x)!\n",
                                  pStreamEx->Core.szName, fBackendStatus));
                }
                break;
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
            /** @todo r=bird: We could probably just skip this next check, as if drainig
             *        failes, we should definitely try disable the stream.  Maybe the
             *        host audio device was unplugged and we're leaving this stream in a
             *        bogus state. */
            if (RT_SUCCESS(rc))
            {
                /*
                 * Before we disable the stream, check if the backend has finished playing the buffered data.
                 */
                uint32_t cbPending;
                if (!pThis->pHostDrvAudio || !pThis->pHostDrvAudio->pfnStreamGetPending) /* Optional. */
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
                    LogFunc(("[%s] Disabling pending stream\n", pStreamEx->Core.szName));
                    rc = drvAudioStreamControlInternalBackend(pThis, pStreamEx, PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_SUCCESS(rc))
                    {
                        pStreamEx->fStatus &= ~(PDMAUDIOSTREAM_STS_ENABLED | PDMAUDIOSTREAM_STS_PENDING_DISABLE);
                        PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
                        drvAudioStreamDropInternal(pStreamEx); /* Not a DROP command, just a stream reset. */
                    }
                    /** @todo r=bird: This log entry sounds a rather fishy to be honest...  Any
                     *        backend which would do that, or genuinely need this?  */
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

    rc = drvAudioStreamIterateInternal(pThis, pStreamEx);

    RTCritSectLeave(&pThis->CritSect);

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);
    return rc;
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
        && (   PDMAudioStrmStatusCanRead(pStreamEx->fStatus)
            || fDisabled)
       )
    {
        uint32_t const fBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx);

        if (pStreamEx->fNoMixBufs)
            cbReadable =    pThis->pHostDrvAudio
                         && (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
                         && (fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED)
                         && !fDisabled
                       ? pThis->pHostDrvAudio->pfnStreamGetReadable(pThis->pHostDrvAudio, pStreamEx->pBackend)
                       : 0;
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
            if (   !PDMAudioStrmStatusBackendCanRead(fBackendStatus)
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
                                pStreamEx->Core.szName, pThis->szName, fBackendStatus));

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
     *
     * Note: We don't propagate the backend stream's status to the outside -- it's the job of this
     *       audio connector to make sense of it.
     */
    uint32_t cbWritable = 0;
    if (   PDMAudioStrmStatusCanWrite(pStreamEx->fStatus)
        && pThis->pHostDrvAudio != NULL)
    {
        switch (pStreamEx->Out.enmPlayState)
        {
            /*
             * Whatever the backend can hold.
             */
            case DRVAUDIOPLAYSTATE_PLAY:
            case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY);
                Assert(drvAudioStreamGetBackendStatus(pThis, pStreamEx) & PDMAUDIOSTREAM_STS_INITIALIZED);
                cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
                break;

            /*
             * Whatever we've got of available space in the pre-buffer.
             * Note! For the last round when we pass the pre-buffering threshold, we may
             *       report fewer bytes than what a DMA timer period for the guest device
             *       typically produces, however that should be transfered in the following
             *       round that goes directly to the backend buffer.
             */
            case DRVAUDIOPLAYSTATE_PREBUF:
                cbWritable = pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBuffered;
                if (!cbWritable)
                    cbWritable = PDMAudioPropsFramesToBytes(&pStreamEx->Core.Props, 2);
                break;

            /*
             * These are slightly more problematic and can go wrong if the pre-buffer is
             * manually configured to be smaller than the output of a typeical DMA timer
             * period for the guest device.  So, to overcompensate, we just report back
             * the backend buffer size (the pre-buffer is circular, so no overflow issue).
             */
            case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
            case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                cbWritable = PDMAudioPropsFramesToBytes(&pStreamEx->Core.Props,
                                                        RT_MAX(pStreamEx->Host.Cfg.Backend.cFramesBufferSize,
                                                               pStreamEx->Host.Cfg.Backend.cFramesPreBuffering));
                break;

            case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
            {
                /* Buggy backend: We weren't able to copy all the pre-buffered data to it
                   when reaching the threshold.  Try escape this situation, or at least
                   keep the extra buffering to a minimum.  We must try write something
                   as long as there is space for it, as we need the pfnStreamWrite call
                   to move the data. */
                Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY);
                Assert(drvAudioStreamGetBackendStatus(pThis, pStreamEx) & PDMAUDIOSTREAM_STS_INITIALIZED);
                uint32_t const cbMin = PDMAudioPropsFramesToBytes(&pStreamEx->Core.Props, 8);
                cbWritable = pThis->pHostDrvAudio->pfnStreamGetWritable(pThis->pHostDrvAudio, pStreamEx->pBackend);
                if (cbWritable >= pStreamEx->Out.cbPreBuffered + cbMin)
                    cbWritable -= pStreamEx->Out.cbPreBuffered + cbMin / 2;
                else
                    cbWritable = RT_MIN(cbMin, pStreamEx->Out.cbPreBufAlloc - pStreamEx->Out.cbPreBuffered);
                AssertLogRel(cbWritable);
                break;
            }

            case DRVAUDIOPLAYSTATE_NOPLAY:
                break;
            case DRVAUDIOPLAYSTATE_INVALID:
            case DRVAUDIOPLAYSTATE_END:
                AssertFailed();
                break;
        }

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
static DECLCALLBACK(uint32_t) drvAudioStreamGetStatus(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IAudioConnector);
    AssertPtr(pThis);

    /** @todo r=bird: It is not documented that we ignore NULL streams...  Why is
     *        this necessary? */
    if (!pStream)
        return PDMAUDIOSTREAM_STS_NONE;
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamEx, PDMAUDIOSTREAM_STS_NONE);
    AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, PDMAUDIOSTREAM_STS_NONE);
    AssertReturn(pStreamEx->uMagic      == DRVAUDIOSTREAM_MAGIC, PDMAUDIOSTREAM_STS_NONE);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, PDMAUDIOSTREAM_STS_NONE);

    uint32_t fStrmStatus = pStreamEx->fStatus;

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
 * Processes backend status change.
 *
 * @todo bird: I'm more and more of the opinion that the backend should
 *       explicitly notify us about these changes, rather that we polling them
 *       via PDMIHOSTAUDIO::pfnStreamGetStatus...
 */
static void drvAudioStreamPlayProcessBackendStateChange(PDRVAUDIOSTREAM pStreamEx, uint32_t fNewState, uint32_t fOldState)
{
    DRVAUDIOPLAYSTATE const enmPlayState = pStreamEx->Out.enmPlayState;

    /*
     * Did PDMAUDIOSTREAM_STS_INITIALIZED change?
     */
    if ((fOldState ^ fNewState) & PDMAUDIOSTREAM_STS_INITIALIZED)
    {
        if (fOldState & PDMAUDIOSTREAM_STS_INITIALIZED)
        {
            /* Pulse audio clear INITIALIZED. */
            switch (enmPlayState)
            {
                case DRVAUDIOPLAYSTATE_PLAY:
                case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_NOPLAY;
                    /** @todo We could enter PREBUF here and hope for the device to re-appear in
                     *        initialized state... */
                    break;
                case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF_SWITCHING;
                    break;
                case DRVAUDIOPLAYSTATE_PREBUF:
                    break;
                case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
                case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                    AssertFailedBreak();
                /* no default */
                case DRVAUDIOPLAYSTATE_NOPLAY:
                case DRVAUDIOPLAYSTATE_END:
                case DRVAUDIOPLAYSTATE_INVALID:
                    break;
            }
            LogFunc(("PDMAUDIOSTREAM_STS_INITIALIZED was cleared: %s -> %s\n",
                     drvAudioPlayStateName(enmPlayState), drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
        }
        else
        {
            if (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
            {
    /** @todo We need to resync the stream status somewhere...  */
                switch (enmPlayState)
                {
                    case DRVAUDIOPLAYSTATE_PREBUF:
                    case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                        break;
                    case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
                        pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF_COMMITTING;
                        break;
                    case DRVAUDIOPLAYSTATE_NOPLAY:
                        pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF;
                        break;
                    case DRVAUDIOPLAYSTATE_PLAY:
                    case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                    case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
                        AssertFailedBreak();
                    /* no default */
                    case DRVAUDIOPLAYSTATE_END:
                    case DRVAUDIOPLAYSTATE_INVALID:
                        break;
                }
                LogFunc(("PDMAUDIOSTREAM_STS_INITIALIZED was set: %s -> %s\n",
                         drvAudioPlayStateName(enmPlayState), drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
            }
            else
                LogFunc(("PDMAUDIOSTREAM_STS_INITIALIZED was set (enmPlayState=%s), but PDMAUDIOSTREAM_STS_BACKEND_READY is not.\n",
                         drvAudioPlayStateName(enmPlayState) ));
        }
    }

    /*
     * Deal with PDMAUDIOSTREAM_STS_PREPARING_SWITCH being set.
     *
     * Note! We don't care if it's cleared as the backend will call
     *       PDMIHOSTAUDIOPORT::pfnStreamNotifyDeviceChanged when that takes place.
     */
    if (   !(fOldState & PDMAUDIOSTREAM_STS_PREPARING_SWITCH)
        && (fNewState  & PDMAUDIOSTREAM_STS_PREPARING_SWITCH))
    {
        if (pStreamEx->Out.cbPreBufThreshold > 0)
        {
            switch (enmPlayState)
            {
                case DRVAUDIOPLAYSTATE_PREBUF:
                case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
                case DRVAUDIOPLAYSTATE_NOPLAY:
                case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING: /* simpler */
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF_SWITCHING;
                    break;
                case DRVAUDIOPLAYSTATE_PLAY:
                    pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PLAY_PREBUF;
                    break;
                case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                    break;
                /* no default */
                case DRVAUDIOPLAYSTATE_END:
                case DRVAUDIOPLAYSTATE_INVALID:
                    break;
            }
            LogFunc(("PDMAUDIOSTREAM_STS_INITIALIZED was set: %s -> %s\n",
                     drvAudioPlayStateName(enmPlayState), drvAudioPlayStateName(pStreamEx->Out.enmPlayState) ));
        }
        else
            LogFunc(("PDMAUDIOSTREAM_STS_PREPARING_SWITCH was set, but no pre-buffering configured.\n"));
    }
}


/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioStreamPlay(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
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
    AssertMsgReturn(pStreamEx->Core.enmDir == PDMAUDIODIR_OUT,
                    ("Stream '%s' is not an output stream and therefore cannot be written to (direction is '%s')\n",
                     pStreamEx->Core.szName, PDMAudioDirGetName(pStreamEx->Core.enmDir)), VERR_ACCESS_DENIED);
    Assert(pStreamEx->fNoMixBufs);

    AssertMsg(PDMAudioPropsIsSizeAligned(&pStreamEx->Guest.Cfg.Props, cbBuf),
              ("Stream '%s' got a non-frame-aligned write (%RU32 bytes)\n", pStreamEx->Core.szName, cbBuf));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * First check that we can write to the stream, and if not,
     * whether to just drop the input into the bit bucket.
     */
    if (PDMAudioStrmStatusIsReady(pStreamEx->fStatus))
    {
        if (   pThis->Out.fEnabled /* (see @bugref{9882}) */
            && pThis->pHostDrvAudio != NULL)
        {
#ifdef LOG_ENABLED
            char szState[DRVAUDIO_STATUS_STR_MAX];
#endif
            /*
             * Get the backend state and check if we've changed to "initialized" or
             * to switch prep mode.
             *
             * We don't really need to watch ENABLE or PAUSE here as these should be
             * in sync between our state and the backend (there is a tiny tiny chance
             * that we are resumed before the backend driver, but AIO threads really
             * shouldn't be getting here that fast I hope).
             */
            /** @todo
             * The PENDING_DISABLE (== draining) is not reported by most backend and it's an
             * open question whether we should still allow writes even when the backend
             * is draining anyway.  We currently don't.  Maybe we just re-define it as
             * a non-backend status flag.
             */
            uint32_t const fBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx);
            Assert(      (fBackendStatus     & (PDMAUDIOSTREAM_STS_ENABLED | PDMAUDIOSTREAM_STS_PAUSED))
                      == (pStreamEx->fStatus & (PDMAUDIOSTREAM_STS_ENABLED | PDMAUDIOSTREAM_STS_PAUSED))
                   || !(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY)
                   || !(fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED) );

            if (!(  (pStreamEx->fLastBackendStatus ^ fBackendStatus)
                  & (PDMAUDIOSTREAM_STS_INITIALIZED | PDMAUDIOSTREAM_STS_PREPARING_SWITCH)))
            { /* no relevant change - likely */ }
            else
            {
                drvAudioStreamPlayProcessBackendStateChange(pStreamEx, fBackendStatus, pStreamEx->fLastBackendStatus);
                pStreamEx->fLastBackendStatus = fBackendStatus;
            }

            /*
             * Do the transfering.
             */
            switch (pStreamEx->Out.enmPlayState)
            {
                case DRVAUDIOPLAYSTATE_PLAY:
                    Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY);
                    Assert(fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED);
                    rc = drvAudioStreamPlayLocked(pThis, pStreamEx, (uint8_t const *)pvBuf, cbBuf, pcbWritten);
                    break;

                case DRVAUDIOPLAYSTATE_PLAY_PREBUF:
                    Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY);
                    Assert(fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED);
                    rc = drvAudioStreamPlayLocked(pThis, pStreamEx, (uint8_t const *)pvBuf, cbBuf, pcbWritten);
                    drvAudioStreamPreBuffer(pStreamEx, (uint8_t const *)pvBuf, *pcbWritten, pStreamEx->Out.cbPreBufThreshold);
                    break;

                case DRVAUDIOPLAYSTATE_PREBUF:
                    if (cbBuf + pStreamEx->Out.cbPreBuffered < pStreamEx->Out.cbPreBufThreshold)
                        rc = drvAudioStreamPlayToPreBuffer(pStreamEx, pvBuf, cbBuf, pStreamEx->Out.cbPreBufThreshold, pcbWritten);
                    else if (fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED)
                    {
                        Log3Func(("[%s] Pre-buffering completing: cbBuf=%#x cbPreBuffered=%#x => %#x vs cbPreBufThreshold=%#x\n",
                                  pStreamEx->Core.szName, cbBuf, pStreamEx->Out.cbPreBuffered,
                                  cbBuf + pStreamEx->Out.cbPreBuffered, pStreamEx->Out.cbPreBufThreshold));
                        rc = drvAudioStreamPreBufComitting(pThis, pStreamEx, (uint8_t const *)pvBuf, cbBuf, pcbWritten);
                    }
                    else
                    {
                        Log3Func(("[%s] Pre-buffering completing but device not ready: cbBuf=%#x cbPreBuffered=%#x => %#x vs cbPreBufThreshold=%#x; PREBUF -> PREBUF_OVERDUE\n",
                                  pStreamEx->Core.szName, cbBuf, pStreamEx->Out.cbPreBuffered,
                                  cbBuf + pStreamEx->Out.cbPreBuffered, pStreamEx->Out.cbPreBufThreshold));
                        pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF_OVERDUE;
                        rc = drvAudioStreamPlayToPreBuffer(pStreamEx, pvBuf, cbBuf, pStreamEx->Out.cbPreBufThreshold, pcbWritten);
                    }
                    break;

                case DRVAUDIOPLAYSTATE_PREBUF_OVERDUE:
                    Assert(!(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY));
                    Assert(!(fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED));
                    RT_FALL_THRU();
                case DRVAUDIOPLAYSTATE_PREBUF_SWITCHING:
                    rc = drvAudioStreamPlayToPreBuffer(pStreamEx, pvBuf, cbBuf, pStreamEx->Out.cbPreBufThreshold, pcbWritten);
                    break;

                case DRVAUDIOPLAYSTATE_PREBUF_COMMITTING:
                    Assert(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY);
                    Assert(fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED);
                    rc = drvAudioStreamPreBufComitting(pThis, pStreamEx, (uint8_t const *)pvBuf, cbBuf, pcbWritten);
                    break;

                case DRVAUDIOPLAYSTATE_NOPLAY:
                    *pcbWritten = cbBuf;
                    pStreamEx->offInternal += cbBuf;
                    Log3Func(("[%s] Discarding the data, backend state: %s\n", pStreamEx->Core.szName,
                              dbgAudioStreamStatusToStr(szState, fBackendStatus) ));
                    break;

                default:
                    *pcbWritten = cbBuf;
                    AssertMsgFailedBreak(("%d; cbBuf=%#x\n", pStreamEx->Out.enmPlayState, cbBuf));
            }

            if (!pThis->Out.Cfg.Dbg.fEnabled || RT_FAILURE(rc))
            { /* likely */ }
            else
                AudioHlpFileWrite(pStreamEx->Out.Dbg.pFilePlayNonInterleaved, pvBuf, *pcbWritten, 0 /* fFlags */);
        }
        else
        {
            *pcbWritten = cbBuf;
            pStreamEx->offInternal += cbBuf;
            Log3Func(("[%s] Backend stream %s, discarding the data\n", pStreamEx->Core.szName,
                      !pThis->Out.fEnabled ? "disabled" : !pThis->pHostDrvAudio ? "not attached" : "not ready yet"));
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    RTCritSectLeave(&pThis->CritSect);
    return rc;
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
            if (!PDMAudioStrmStatusCanRead(pStreamEx->fStatus))
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
 *
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
    Log3Func(("[%s] fStatus=%s\n", pStreamEx->Core.szName, dbgAudioStreamStatusToStr(szStreamSts, pStreamEx->fStatus)));

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
            || !PDMAudioStrmStatusCanRead(pStreamEx->fStatus)
            || !(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_BACKEND_READY))
        {
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        uint32_t const fBackendStatus = drvAudioStreamGetBackendStatus(pThis, pStreamEx);
        if (fBackendStatus & PDMAUDIOSTREAM_STS_INITIALIZED)
        {
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
        }
        else
            rc = VERR_AUDIO_STREAM_NOT_READY;
    } while (0);

    RTCritSectLeave(&pThis->CritSect);

    if (pcFramesCaptured)
        *pcFramesCaptured = cfCaptured;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   PDMIHOSTAUDIOPORT interface implementation.                                                                                  *
*********************************************************************************************************************************/

/**
 * Worker for drvAudioHostPort_DoOnWorkerThread with stream argument, called on
 * worker thread.
 */
static DECLCALLBACK(void) drvAudioHostPort_DoOnWorkerThreadStreamWorker(PDRVAUDIO pThis, PDRVAUDIOSTREAM pStreamEx,
                                                                        uintptr_t uUser, void *pvUser)
{
    LogFlowFunc(("pThis=%p uUser=%#zx pvUser=%p\n", pThis, uUser, pvUser));
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStreamEx);
    AssertReturnVoid(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC);
    PPDMIHOSTAUDIO const pIHostDrvAudio = pThis->pHostDrvAudio;
    AssertPtrReturnVoid(pIHostDrvAudio);
    AssertPtrReturnVoid(pIHostDrvAudio->pfnDoOnWorkerThread);

    pIHostDrvAudio->pfnDoOnWorkerThread(pIHostDrvAudio, pStreamEx->pBackend, uUser, pvUser);

    drvAudioStreamReleaseInternal(pThis, pStreamEx, true /*fMayDestroy*/);
    LogFlowFunc(("returns\n"));
}


/**
 * Worker for drvAudioHostPort_DoOnWorkerThread without stream argument, called
 * on worker thread.
 *
 * This wrapper isn't technically required, but it helps with logging and a few
 * extra sanity checks.
 */
static DECLCALLBACK(void) drvAudioHostPort_DoOnWorkerThreadWorker(PDRVAUDIO pThis, uintptr_t uUser, void *pvUser)
{
    LogFlowFunc(("pThis=%p uUser=%#zx pvUser=%p\n", pThis, uUser, pvUser));
    AssertPtrReturnVoid(pThis);
    PPDMIHOSTAUDIO const pIHostDrvAudio = pThis->pHostDrvAudio;
    AssertPtrReturnVoid(pIHostDrvAudio);
    AssertPtrReturnVoid(pIHostDrvAudio->pfnDoOnWorkerThread);

    pIHostDrvAudio->pfnDoOnWorkerThread(pIHostDrvAudio, NULL, uUser, pvUser);

    LogFlowFunc(("returns\n"));
}


/**
 * @interface_method_impl{PDMIHOSTAUDIOPORT,pfnDoOnWorkerThread}
 */
static DECLCALLBACK(int) drvAudioHostPort_DoOnWorkerThread(PPDMIHOSTAUDIOPORT pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           uintptr_t uUser, void *pvUser)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IHostAudioPort);
    LogFlowFunc(("pStream=%p uUser=%#zx pvUser=%p\n", pStream, uUser, pvUser));

    /*
     * Assert some sanity and do the work.
     */
    AssertReturn(pThis->pHostDrvAudio, VERR_INTERNAL_ERROR_3);
    AssertReturn(pThis->pHostDrvAudio->pfnDoOnWorkerThread, VERR_INVALID_FUNCTION);
    AssertReturn(pThis->hReqPool != NIL_RTREQPOOL, VERR_INVALID_FUNCTION);
    int rc;
    if (!pStream)
    {
        rc = RTReqPoolCallEx(pThis->hReqPool, 0 /*cMillies*/, NULL /*phReq*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                             (PFNRT)drvAudioHostPort_DoOnWorkerThreadWorker, 3, pThis, uUser, pvUser);
        AssertRC(rc);
    }
    else
    {
        AssertPtrReturn(pStream, VERR_INVALID_POINTER);
        AssertReturn(pStream->uMagic == PDMAUDIOBACKENDSTREAM_MAGIC, VERR_INVALID_MAGIC);
        PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream->pStream;
        AssertPtrReturn(pStreamEx, VERR_INVALID_POINTER);
        AssertReturn(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);
        AssertReturn(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC, VERR_INVALID_MAGIC);

        uint32_t cRefs = drvAudioStreamRetainInternal(pStreamEx);
        if (cRefs != UINT32_MAX)
        {
            rc = RTReqPoolCallEx(pThis->hReqPool, 0 /*cMillies*/, NULL, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                                 (PFNRT)drvAudioHostPort_DoOnWorkerThreadStreamWorker,
                                 4, pThis, pStreamEx, uUser, pvUser);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                drvAudioStreamReleaseInternal(pThis, pStreamEx, true /*fMayDestroy*/);
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Marks a stream for re-init.
 */
static void drvAudioStreamMarkNeedReInit(PDRVAUDIOSTREAM pStreamEx, const char *pszCaller)
{
    LogFlow((LOG_FN_FMT ": Flagging %s for re-init.\n", pszCaller, pStreamEx->Core.szName)); RT_NOREF(pszCaller);
    pStreamEx->fStatus      |= PDMAUDIOSTREAM_STS_NEED_REINIT;
    PDMAUDIOSTREAM_STS_ASSERT_VALID(pStreamEx->fStatus);
    pStreamEx->cTriesReInit  = 0;
    pStreamEx->nsLastReInit  = 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIOPORT,pfnNotifyDeviceChanged}
 */
static DECLCALLBACK(void) drvAudioHostPort_NotifyDeviceChanged(PPDMIHOSTAUDIOPORT pInterface, PDMAUDIODIR enmDir, void *pvUser)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IHostAudioPort);
    AssertReturnVoid(enmDir == PDMAUDIODIR_IN || enmDir == PDMAUDIODIR_OUT);
    LogRel(("Audio: The %s device for %s is changing.\n", enmDir == PDMAUDIODIR_IN ? "input" : "output", pThis->szName));

    RTCritSectEnter(&pThis->CritSect);
    PDRVAUDIOSTREAM pStreamEx;
    RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
    {
        if (pStreamEx->Core.enmDir == enmDir)
        {
            if (pThis->pHostDrvAudio->pfnStreamNotifyDeviceChanged)
            {
                LogFlowFunc(("Calling pfnStreamNotifyDeviceChanged on %s, old backend status: %#x...\n", pStreamEx->Core.szName,
                              drvAudioStreamGetBackendStatus(pThis, pStreamEx) ));
                pThis->pHostDrvAudio->pfnStreamNotifyDeviceChanged(pThis->pHostDrvAudio, pStreamEx->pBackend, pvUser);
                LogFlowFunc(("New stream backend status: %#x.\n", drvAudioStreamGetBackendStatus(pThis, pStreamEx) ));
            }
            else
                drvAudioStreamMarkNeedReInit(pStreamEx, __PRETTY_FUNCTION__);
        }
    }
    RTCritSectLeave(&pThis->CritSect);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIOPORT,pfnStreamNotifyDeviceChanged}
 */
static DECLCALLBACK(void) drvAudioHostPort_StreamNotifyDeviceChanged(PPDMIHOSTAUDIOPORT pInterface,
                                                                     PPDMAUDIOBACKENDSTREAM pStream, bool fReInit)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IHostAudioPort);

    /*
     * Backend stream to validated DrvAudio stream:
     */
    AssertPtrReturnVoid(pStream);
    AssertReturnVoid(pStream->uMagic == PDMAUDIOBACKENDSTREAM_MAGIC);
    PDRVAUDIOSTREAM pStreamEx = (PDRVAUDIOSTREAM)pStream->pStream;
    AssertPtrReturnVoid(pStreamEx);
    AssertReturnVoid(pStreamEx->Core.uMagic == PDMAUDIOSTREAM_MAGIC);
    AssertReturnVoid(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC);

    /*
     * Grab the lock and do the requested work.
     */
    RTCritSectEnter(&pThis->CritSect);
    AssertReturnVoidStmt(pStreamEx->uMagic == DRVAUDIOSTREAM_MAGIC, RTCritSectLeave(&pThis->CritSect)); /* paranoia */

    if (fReInit)
        drvAudioStreamMarkNeedReInit(pStreamEx, __PRETTY_FUNCTION__);
    else
    {
        /*
         * Adjust the stream state now that the device has (perhaps finally) been switched.
         *
         * For enabled output streams, we must update the play state.  We could try commit
         * pre-buffered data here, but it's really not worth the hazzle and risk (don't
         * know which thread we're on, do we now).
         */
        AssertStmt(!(pStreamEx->fStatus & PDMAUDIOSTREAM_STS_NEED_REINIT),
                   pStreamEx->fStatus &= ~PDMAUDIOSTREAM_STS_NEED_REINIT);

        if (   pStreamEx->Core.enmDir == PDMAUDIODIR_OUT
            && (pStreamEx->fStatus & PDMAUDIOSTREAM_STS_ENABLED))
        {
            DRVAUDIOPLAYSTATE const enmPlayState = pStreamEx->Out.enmPlayState;
            pStreamEx->Out.enmPlayState = DRVAUDIOPLAYSTATE_PREBUF;
            LogFunc(("%s: %s -> %s\n", pStreamEx->Core.szName, drvAudioPlayStateName(enmPlayState),
                     drvAudioPlayStateName(pStreamEx->Out.enmPlayState) )); RT_NOREF(enmPlayState);
        }
    }

    RTCritSectLeave(&pThis->CritSect);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIOPORT,pfnNotifyDevicesChanged}
 */
static DECLCALLBACK(void) drvAudioHostPort_NotifyDevicesChanged(PPDMIHOSTAUDIOPORT pInterface)
{
    PDRVAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVAUDIO, IHostAudioPort);
    LogRel(("Audio: Device configuration of driver '%s' has changed\n", pThis->szName));

    /** @todo Remove legacy behaviour: */
    if (!pThis->pHostDrvAudio->pfnStreamNotifyDeviceChanged)
    {
/** @todo r=bird: Locking?   */
        /* Mark all host streams to re-initialize. */
        PDRVAUDIOSTREAM pStreamEx;
        RTListForEach(&pThis->lstStreams, pStreamEx, DRVAUDIOSTREAM, ListEntry)
        {
            drvAudioStreamMarkNeedReInit(pStreamEx, __PRETTY_FUNCTION__);
        }
    }

# ifdef VBOX_WITH_AUDIO_ENUM
    /* Re-enumerate all host devices as soon as possible. */
    pThis->fEnumerateDevices = true;
# endif
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
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIOPORT, &pThis->IHostAudioPort);

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
#if 0 /* This leads to double destruction. Also in the backend when we don't set pHostDrvAudio to NULL below. */
            drvAudioStreamDestroyInternalBackend(pThis, pStreamEx);
#endif
        }

#if 0 /* Messes up using drvAudioHostPort_DoOnWorkerThread from the backend drivers' power off callback.  */
        pThis->pHostDrvAudio = NULL;
#endif
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

    LogFunc(("%s (detached %p, hReqPool=%p)\n", pThis->szName, pThis->pHostDrvAudio, pThis->hReqPool));

    /*
     * Must first destroy the thread pool first so we are certain no threads
     * are still using the instance being detached.  Release lock while doing
     * this as the thread functions may need to take it to complete.
     */
    if (pThis->pHostDrvAudio && pThis->hReqPool != NIL_RTREQPOOL)
    {
        RTREQPOOL hReqPool = pThis->hReqPool;
        pThis->hReqPool = NIL_RTREQPOOL;
        RTCritSectLeave(&pThis->CritSect);

        RTReqPoolRelease(hReqPool);

        RTCritSectEnter(&pThis->CritSect);
    }

    /*
     * Now we can safely set pHostDrvAudio to NULL.
     */
    pThis->pHostDrvAudio = NULL;

    RTCritSectLeave(&pThis->CritSect);
}


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
    PPDMIHOSTAUDIO pIHostDrvAudio = pThis->pHostDrvAudio;
    AssertPtrReturn(pIHostDrvAudio, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnGetConfig, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnGetDevices, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnGetStatus, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnDoOnWorkerThread, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnStreamConfigHint, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamCreate, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnStreamInitAsync, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamDestroy, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnStreamNotifyDeviceChanged, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamControl, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamGetReadable, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamGetWritable, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pIHostDrvAudio->pfnStreamGetPending, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamGetStatus, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamPlay, VERR_INVALID_POINTER);
    AssertPtrReturn(pIHostDrvAudio->pfnStreamCapture, VERR_INVALID_POINTER);

    /*
     * Get the backend configuration.
     */
    int rc = pIHostDrvAudio->pfnGetConfig(pIHostDrvAudio, &pThis->BackendCfg);
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
    /* Ignore rc2. */
#endif

    /*
     * Create a thread pool if stream creation can be asynchronous.
     *
     * The pool employs no pushback as the caller is typically EMT and
     * shouldn't be delayed.
     *
     * The number of threads limits and the device implementations use
     * of pfnStreamDestroy limits the number of streams pending async
     * init.  We use RTReqCancel in drvAudioStreamDestroy to allow us
     * to release extra reference held by the pfnStreamInitAsync call
     * if successful.  Cancellation will only be possible if the call
     * hasn't been picked up by a worker thread yet, so the max number
     * of threads in the pool defines how many destroyed streams that
     * can be lingering.  (We must keep this under control, otherwise
     * an evil guest could just rapidly trigger stream creation and
     * destruction to consume host heap and hog CPU resources for
     * configuring audio backends.)
     */
    if (   pThis->hReqPool == NIL_RTREQPOOL
        && (   pIHostDrvAudio->pfnStreamInitAsync
            || pIHostDrvAudio->pfnDoOnWorkerThread
            || (pThis->BackendCfg.fFlags & PDMAUDIOBACKEND_F_ASYNC_HINT) ))
    {
        char szName[16];
        RTStrPrintf(szName, sizeof(szName), "Aud%uWr", pThis->pDrvIns->iInstance);
        RTREQPOOL hReqPool = NIL_RTREQPOOL;
        rc = RTReqPoolCreate(3 /*cMaxThreads*/, RT_MS_30SEC /*cMsMinIdle*/, UINT32_MAX /*cThreadsPushBackThreshold*/,
                             1 /*cMsMaxPushBack*/, szName, &hReqPool);
        LogFlowFunc(("Creating thread pool '%s': %Rrc, hReqPool=%p\n", szName, rc, hReqPool));
        AssertRCReturn(rc, rc);

        rc = RTReqPoolSetCfgVar(hReqPool, RTREQPOOLCFGVAR_THREAD_FLAGS, RTTHREADFLAGS_COM_MTA);
        AssertRCReturnStmt(rc, RTReqPoolRelease(hReqPool), rc);

        rc = RTReqPoolSetCfgVar(hReqPool, RTREQPOOLCFGVAR_MIN_THREADS, 1);
        AssertRC(rc); /* harmless */

        pThis->hReqPool = hReqPool;
    }
    else
        LogFlowFunc(("No thread pool.\n"));

    LogFlowFuncLeave();
    return VINF_SUCCESS;
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
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalFramesIn);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->Stats.TotalBytesRead);
#endif

    if (pThis->hReqPool != NIL_RTREQPOOL)
    {
        uint32_t cRefs = RTReqPoolRelease(pThis->hReqPool);
        Assert(cRefs == 0); RT_NOREF(cRefs);
        pThis->hReqPool = NIL_RTREQPOOL;
    }

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
    pThis->hReqPool = NIL_RTREQPOOL;

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
    pThis->IAudioConnector.pfnStreamReInit      = drvAudioStreamReInit;
    pThis->IAudioConnector.pfnStreamRetain      = drvAudioStreamRetain;
    pThis->IAudioConnector.pfnStreamRelease     = drvAudioStreamRelease;
    pThis->IAudioConnector.pfnStreamControl     = drvAudioStreamControl;
    pThis->IAudioConnector.pfnStreamIterate     = drvAudioStreamIterate;
    pThis->IAudioConnector.pfnStreamGetReadable = drvAudioStreamGetReadable;
    pThis->IAudioConnector.pfnStreamGetWritable = drvAudioStreamGetWritable;
    pThis->IAudioConnector.pfnStreamGetStatus   = drvAudioStreamGetStatus;
    pThis->IAudioConnector.pfnStreamSetVolume   = drvAudioStreamSetVolume;
    pThis->IAudioConnector.pfnStreamPlay        = drvAudioStreamPlay;
    pThis->IAudioConnector.pfnStreamRead        = drvAudioStreamRead;
    pThis->IAudioConnector.pfnStreamCapture     = drvAudioStreamCapture;
    /* IHostAudioPort */
    pThis->IHostAudioPort.pfnDoOnWorkerThread           = drvAudioHostPort_DoOnWorkerThread;
    pThis->IHostAudioPort.pfnNotifyDeviceChanged        = drvAudioHostPort_NotifyDeviceChanged;
    pThis->IHostAudioPort.pfnStreamNotifyDeviceChanged  = drvAudioHostPort_StreamNotifyDeviceChanged;
    pThis->IHostAudioPort.pfnNotifyDevicesChanged       = drvAudioHostPort_NotifyDevicesChanged;

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
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalFramesIn,        "TotalFramesIn",
                              STAMUNIT_COUNT, "Total frames captured by backend.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalBytesRead,       "TotalBytesRead",
                              STAMUNIT_BYTES, "Total bytes read.");
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

