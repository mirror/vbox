/* $Id$ */
/** @file
 * VBox audio devices: Pulse Audio audio driver.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <stdio.h>

#include <iprt/alloc.h>
#include <iprt/mem.h>
#include <iprt/uuid.h>

RT_C_DECLS_BEGIN
 #include "pulse_mangling.h"
 #include "pulse_stubs.h"
RT_C_DECLS_END

#include <pulse/pulseaudio.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"

/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
#define VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS 32 /** @todo Make this configurable thru driver options. */

#ifndef PA_STREAM_NOFLAGS
# define PA_STREAM_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

#ifndef PA_CONTEXT_NOFLAGS
# define PA_CONTEXT_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

/** No flags specified. */
#define PULSEAUDIOENUMCBFLAGS_NONE          0
/** (Release) log found devices. */
#define PULSEAUDIOENUMCBFLAGS_LOG           RT_BIT(0)

/** Makes DRVHOSTPULSEAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface) \
    ( (PDRVHOSTPULSEAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPULSEAUDIO, IHostAudio)) )

/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/

/**
 * Host Pulse audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTPULSEAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS            pDrvIns;
    /** Pointer to PulseAudio's threaded main loop. */
    pa_threaded_mainloop *pMainLoop;
    /**
    * Pointer to our PulseAudio context.
    * Note: We use a pMainLoop in a separate thread (pContext).
    *       So either use callback functions or protect these functions
    *       by pa_threaded_mainloop_lock() / pa_threaded_mainloop_unlock().
    */
    pa_context           *pContext;
    /** Shutdown indicator. */
    bool                  fLoopWait;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO         IHostAudio;
    /** Error count for not flooding the release log.
     *  Specify UINT32_MAX for unlimited logging. */
    uint32_t              cLogErrors;
} DRVHOSTPULSEAUDIO, *PDRVHOSTPULSEAUDIO;

typedef struct PULSEAUDIOSTREAM
{
    /** Associated host input/output stream.
     *  Note: Always must come first! */
    PDMAUDIOSTREAM         Stream;
    /** Pointer to driver instance. */
    PDRVHOSTPULSEAUDIO     pDrv;
    /** DAC/ADC buffer. */
    void                  *pvPCMBuf;
    /** Size (in bytes) of DAC/ADC buffer. */
    uint32_t               cbPCMBuf;
    /** Pointer to opaque PulseAudio stream. */
    pa_stream             *pStream;
    /** Pulse sample format and attribute specification. */
    pa_sample_spec         SampleSpec;
    /** Pulse playback and buffer metrics. */
    pa_buffer_attr         BufAttr;
    int                    fOpSuccess;
    /** Pointer to Pulse sample peeking buffer. */
    const uint8_t         *pu8PeekBuf;
    /** Current size (in bytes) of peeking data in
     *  buffer. */
    size_t                 cbPeekBuf;
    /** Our offset (in bytes) in peeking buffer. */
    size_t                 offPeekBuf;
    pa_operation          *pDrainOp;
} PULSEAUDIOSTREAM, *PPULSEAUDIOSTREAM;

/* The desired buffer length in milliseconds. Will be the target total stream
 * latency on newer version of pulse. Apparent latency can be less (or more.)
 */
typedef struct PULSEAUDIOCFG
{
    RTMSINTERVAL buffer_msecs_out;
    RTMSINTERVAL buffer_msecs_in;
} PULSEAUDIOCFG, *PPULSEAUDIOCFG;

static PULSEAUDIOCFG s_pulseCfg =
{
    100, /* buffer_msecs_out */
    100  /* buffer_msecs_in */
};

/**
 * Callback context for server enumeration callbacks.
 */
typedef struct PULSEAUDIOENUMCBCTX
{
    /** Pointer to host backend driver. */
    PDRVHOSTPULSEAUDIO  pDrv;
    /** Enumeration flags. */
    uint32_t            fFlags;
    /** Number of found input devices. */
    uint8_t             cDevIn;
    /** Number of found output devices. */
    uint8_t             cDevOut;
    /** Name of default sink being used. Must be free'd using RTStrFree(). */
    char               *pszDefaultSink;
    /** Name of default source being used. Must be free'd using RTStrFree(). */
    char               *pszDefaultSource;
} PULSEAUDIOENUMCBCTX, *PPULSEAUDIOENUMCBCTX;

/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/

static int  paEnumerate(PDRVHOSTPULSEAUDIO pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum);
static int  paError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg);
static void paStreamCbSuccess(pa_stream *pStream, int fSuccess, void *pvContext);

/**
 * Signal the main loop to abort. Just signalling isn't sufficient as the
 * mainloop might not have been entered yet.
 */
static void paSignalWaiter(PDRVHOSTPULSEAUDIO pThis)
{
    pThis->fLoopWait = true;
    pa_threaded_mainloop_signal(pThis->pMainLoop, 0);
}

static pa_sample_format_t paFmtToPulse(PDMAUDIOFMT fmt)
{
    switch (fmt)
    {
        case PDMAUDIOFMT_U8:
            return PA_SAMPLE_U8;

        case PDMAUDIOFMT_S16:
            return PA_SAMPLE_S16LE;

#ifdef PA_SAMPLE_S32LE
        case PDMAUDIOFMT_S32:
            return PA_SAMPLE_S32LE;
#endif
        default:
            break;
    }

    AssertMsgFailed(("Format %ld not supported\n", fmt));
    return PA_SAMPLE_U8;
}

static int paPulseToFmt(pa_sample_format_t pulsefmt,
                        PDMAUDIOFMT *pFmt, PDMAUDIOENDIANNESS *pEndianness)
{
    switch (pulsefmt)
    {
        case PA_SAMPLE_U8:
            *pFmt = PDMAUDIOFMT_U8;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case PA_SAMPLE_S16LE:
            *pFmt = PDMAUDIOFMT_S16;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case PA_SAMPLE_S16BE:
            *pFmt = PDMAUDIOFMT_S16;
            *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

#ifdef PA_SAMPLE_S32LE
        case PA_SAMPLE_S32LE:
            *pFmt = PDMAUDIOFMT_S32;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;
#endif

#ifdef PA_SAMPLE_S32BE
        case PA_SAMPLE_S32BE:
            *pFmt = PDMAUDIOFMT_S32;
            *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;
#endif

        default:
            AssertMsgFailed(("Format %ld not supported\n", pulsefmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

/**
 * Synchronously wait until an operation completed.
 */
static int paWaitForEx(PDRVHOSTPULSEAUDIO pThis, pa_operation *pOP, RTMSINTERVAL cMsTimeout)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pOP,   VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    uint64_t u64StartMs = RTTimeMilliTS();
    while (pa_operation_get_state(pOP) == PA_OPERATION_RUNNING)
    {
        if (!pThis->fLoopWait)
        {
            AssertPtr(pThis->pMainLoop);
            pa_threaded_mainloop_wait(pThis->pMainLoop);
        }
        pThis->fLoopWait = false;

        uint64_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
        if (u64ElapsedMs >= cMsTimeout)
        {
            rc = VERR_TIMEOUT;
            break;
        }
    }

    pa_operation_unref(pOP);

    return rc;
}

static int paWaitFor(PDRVHOSTPULSEAUDIO pThis, pa_operation *pOP)
{
    return paWaitForEx(pThis, pOP, 10 * 1000 /* 10s timeout */);
}

/**
 * Context status changed.
 */
static void paContextCbStateChanged(pa_context *pCtx, void *pvUser)
{
    AssertPtrReturnVoid(pCtx);

    PDRVHOSTPULSEAUDIO pThis = (PDRVHOSTPULSEAUDIO)pvUser;
    AssertPtrReturnVoid(pThis);

    switch (pa_context_get_state(pCtx))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
            paSignalWaiter(pThis);
            break;

        case PA_CONTEXT_FAILED:
            LogRel(("PulseAudio: Audio context has failed, stopping\n"));
            paSignalWaiter(pThis);
            break;

        default:
            break;
    }
}

/**
 * Callback called when our pa_stream_drain operation was completed.
 */
static void paStreamCbDrain(pa_stream *pStream, int fSuccess, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvUser;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;
    if (fSuccess)
    {
        pa_operation_unref(pa_stream_cork(pStream, 1,
                                          paStreamCbSuccess, pvUser));
    }
    else
        paError(pStrm->pDrv, "Failed to drain stream");

    pa_operation_unref(pStrm->pDrainOp);
    pStrm->pDrainOp = NULL;
}

/**
 * Stream status changed.
 */
static void paStreamCbStateChanged(pa_stream *pStream, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PDRVHOSTPULSEAUDIO pThis = (PDRVHOSTPULSEAUDIO)pvUser;
    AssertPtrReturnVoid(pThis);

    switch (pa_stream_get_state(pStream))
    {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            paSignalWaiter(pThis);
            break;

        default:
            break;
    }
}

static void paStreamCbSuccess(pa_stream *pStream, int fSuccess, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvUser;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;

    if (fSuccess)
        paSignalWaiter(pStrm->pDrv);
    else
        paError(pStrm->pDrv, "Failed to finish stream operation");
}

static int paStreamOpen(PDRVHOSTPULSEAUDIO pThis, bool fIn, const char *pszName,
                        pa_sample_spec *pSampleSpec, pa_buffer_attr *pBufAttr,
                        pa_stream **ppStream)
{
    AssertPtrReturn(pThis,       VERR_INVALID_POINTER);
    AssertPtrReturn(pszName,     VERR_INVALID_POINTER);
    AssertPtrReturn(pSampleSpec, VERR_INVALID_POINTER);
    AssertPtrReturn(pBufAttr,    VERR_INVALID_POINTER);
    AssertPtrReturn(ppStream,    VERR_INVALID_POINTER);

    if (!pa_sample_spec_valid(pSampleSpec))
    {
        LogRel(("PulseAudio: Unsupported sample specification for stream \"%s\"\n",
                pszName));
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    pa_stream *pStream = NULL;
    uint32_t   flags = PA_STREAM_NOFLAGS;

    LogFunc(("Opening \"%s\", rate=%dHz, channels=%d, format=%s\n",
             pszName, pSampleSpec->rate, pSampleSpec->channels,
             pa_sample_format_to_string(pSampleSpec->format)));

    pa_threaded_mainloop_lock(pThis->pMainLoop);

    do
    {
        /** @todo r=andy Use pa_stream_new_with_proplist instead. */
        if (!(pStream = pa_stream_new(pThis->pContext, pszName, pSampleSpec,
                                      NULL /* pa_channel_map */)))
        {
            LogRel(("PulseAudio: Could not create stream \"%s\"\n", pszName));
            rc = VERR_NO_MEMORY;
            break;
        }

        pa_stream_set_state_callback(pStream, paStreamCbStateChanged, pThis);

#if PA_API_VERSION >= 12
        /* XXX */
        flags |= PA_STREAM_ADJUST_LATENCY;
#endif

#if 0
        /* Not applicable as we don't use pa_stream_get_latency() and pa_stream_get_time(). */
        flags |= PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
#endif
        /* No input/output right away after the stream was started. */
        flags |= PA_STREAM_START_CORKED;

        if (fIn)
        {
            LogFunc(("Input stream attributes: maxlength=%d fragsize=%d\n",
                     pBufAttr->maxlength, pBufAttr->fragsize));

            if (pa_stream_connect_record(pStream, /*dev=*/NULL, pBufAttr, (pa_stream_flags_t)flags) < 0)
            {
                LogRel(("PulseAudio: Could not connect input stream \"%s\": %s\n",
                        pszName, pa_strerror(pa_context_errno(pThis->pContext))));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }
        else
        {
            LogFunc(("Output buffer attributes: maxlength=%d tlength=%d prebuf=%d minreq=%d\n",
                     pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

            if (pa_stream_connect_playback(pStream, /*dev=*/NULL, pBufAttr, (pa_stream_flags_t)flags,
                                           /*cvolume=*/NULL, /*sync_stream=*/NULL) < 0)
            {
                LogRel(("PulseAudio: Could not connect playback stream \"%s\": %s\n",
                        pszName, pa_strerror(pa_context_errno(pThis->pContext))));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }

        /* Wait until the stream is ready. */
        for (;;)
        {
            if (!pThis->fLoopWait)
                pa_threaded_mainloop_wait(pThis->pMainLoop);
            pThis->fLoopWait = false;

            pa_stream_state_t streamSt = pa_stream_get_state(pStream);
            if (streamSt == PA_STREAM_READY)
                break;
            else if (   streamSt == PA_STREAM_FAILED
                     || streamSt == PA_STREAM_TERMINATED)
            {
                LogRel(("PulseAudio: Failed to initialize stream \"%s\" (state %ld)\n", pszName, streamSt));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }

        if (RT_FAILURE(rc))
            break;

        const pa_buffer_attr *pBufAttrObtained = pa_stream_get_buffer_attr(pStream);
        AssertPtr(pBufAttrObtained);
        memcpy(pBufAttr, pBufAttrObtained, sizeof(pa_buffer_attr));

        if (fIn)
            LogFunc(("Obtained record buffer attributes: maxlength=%RU32, fragsize=%RU32\n",
                     pBufAttr->maxlength, pBufAttr->fragsize));
        else
            LogFunc(("Obtained playback buffer attributes: maxlength=%d, tlength=%d, prebuf=%d, minreq=%d\n",
                     pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

    }
    while (0);

    if (   RT_FAILURE(rc)
        && pStream)
        pa_stream_disconnect(pStream);

    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_FAILURE(rc))
    {
        if (pStream)
            pa_stream_unref(pStream);
    }
    else
        *ppStream = pStream;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioInit(PPDMIHOSTAUDIO pInterface)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    LogFlowFuncEnter();

    int rc = audioLoadPulseLib();
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Failed to load the PulseAudio shared library! Error %Rrc\n", rc));
        return rc;
    }

    pThis->fLoopWait = false;
    pThis->pMainLoop = NULL;

    bool fLocked = false;

    do
    {
        if (!(pThis->pMainLoop = pa_threaded_mainloop_new()))
        {
            LogRel(("PulseAudio: Failed to allocate main loop: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (!(pThis->pContext = pa_context_new(pa_threaded_mainloop_get_api(pThis->pMainLoop), "VirtualBox")))
        {
            LogRel(("PulseAudio: Failed to allocate context: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (pa_threaded_mainloop_start(pThis->pMainLoop) < 0)
        {
            LogRel(("PulseAudio: Failed to start threaded mainloop: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        /* Install a global callback to known if something happens to our acquired context. */
        pa_context_set_state_callback(pThis->pContext, paContextCbStateChanged, pThis /* pvUserData */);

        pa_threaded_mainloop_lock(pThis->pMainLoop);
        fLocked = true;

        if (pa_context_connect(pThis->pContext, NULL /* pszServer */,
                               PA_CONTEXT_NOFLAGS, NULL) < 0)
        {
            LogRel(("PulseAudio: Failed to connect to server: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        /* Wait until the pThis->pContext is ready. */
        for (;;)
        {
            if (!pThis->fLoopWait)
                pa_threaded_mainloop_wait(pThis->pMainLoop);
            pThis->fLoopWait = false;

            pa_context_state_t cstate = pa_context_get_state(pThis->pContext);
            if (cstate == PA_CONTEXT_READY)
                break;
            else if (   cstate == PA_CONTEXT_TERMINATED
                     || cstate == PA_CONTEXT_FAILED)
            {
                LogRel(("PulseAudio: Failed to initialize context (state %d)\n", cstate));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }
    }
    while (0);

    if (fLocked)
        pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_FAILURE(rc))
    {
        if (pThis->pMainLoop)
            pa_threaded_mainloop_stop(pThis->pMainLoop);

        if (pThis->pContext)
        {
            pa_context_disconnect(pThis->pContext);
            pa_context_unref(pThis->pContext);
            pThis->pContext = NULL;
        }

        if (pThis->pMainLoop)
        {
            pa_threaded_mainloop_free(pThis->pMainLoop);
            pThis->pMainLoop = NULL;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int paCreateStreamOut(PPDMIHOSTAUDIO pInterface,
                             PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg, uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);
    /* pcSamples is optional. */

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    LogFlowFuncEnter();

    pStrm->pDrainOp            = NULL;

    pStrm->SampleSpec.format   = paFmtToPulse(pCfg->enmFormat);
    pStrm->SampleSpec.rate     = pCfg->uHz;
    pStrm->SampleSpec.channels = pCfg->cChannels;

    /* Note that setting maxlength to -1 does not work on PulseAudio servers
     * older than 0.9.10. So use the suggested value of 3/2 of tlength */
    pStrm->BufAttr.tlength     =   (pa_bytes_per_second(&pStrm->SampleSpec)
                                        * s_pulseCfg.buffer_msecs_out) / 1000;
    pStrm->BufAttr.maxlength   = (pStrm->BufAttr.tlength * 3) / 2;
    pStrm->BufAttr.prebuf      = -1; /* Same as tlength */

    /* Set minreq to 0, as we want to control ourselves when to start/stop the stream. */
    pStrm->BufAttr.minreq      = 0;

    /* Note that the struct BufAttr is updated to the obtained values after this call! */
    int rc = paStreamOpen(pThis, false /* fIn */, "PulseAudio (Out)", &pStrm->SampleSpec, &pStrm->BufAttr, &pStrm->pStream);
    if (RT_FAILURE(rc))
        return rc;

    PDMAUDIOSTREAMCFG streamCfg;
    rc = paPulseToFmt(pStrm->SampleSpec.format,
                      &streamCfg.enmFormat, &streamCfg.enmEndianness);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio output format %ld\n", pStrm->SampleSpec.format));
        return rc;
    }

    streamCfg.uHz       = pStrm->SampleSpec.rate;
    streamCfg.cChannels = pStrm->SampleSpec.channels;

    rc = DrvAudioHlpStreamCfgToProps(&streamCfg, &pStream->Props);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbBuf  = RT_MIN(pStrm->BufAttr.tlength * 2,
                                 pStrm->BufAttr.maxlength); /** @todo Make this configurable! */
        if (cbBuf)
        {
            pStrm->pvPCMBuf = RTMemAllocZ(cbBuf);
            if (pStrm->pvPCMBuf)
            {
                pStrm->cbPCMBuf = cbBuf;

                uint32_t cSamples = cbBuf >> pStream->Props.cShift;
                if (pcSamples)
                    *pcSamples = cSamples;

                /* Save pointer to driver instance. */
                pStrm->pDrv = pThis;

                LogFunc(("cbBuf=%RU32, cSamples=%RU32\n", cbBuf, cSamples));
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int paCreateStreamIn(PPDMIHOSTAUDIO pInterface,
                            PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg, uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);
    /* pcSamples is optional. */

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    pStrm->SampleSpec.format   = paFmtToPulse(pCfg->enmFormat);
    pStrm->SampleSpec.rate     = pCfg->uHz;
    pStrm->SampleSpec.channels = pCfg->cChannels;

    /* XXX check these values */
    pStrm->BufAttr.fragsize    = (pa_bytes_per_second(&pStrm->SampleSpec)
                                   * s_pulseCfg.buffer_msecs_in) / 1000;
    pStrm->BufAttr.maxlength   = (pStrm->BufAttr.fragsize * 3) / 2;

    /* Note: Other members of BufAttr are ignored for record streams. */
    int rc = paStreamOpen(pThis, true /* fIn */, "PulseAudio (In)", &pStrm->SampleSpec, &pStrm->BufAttr,
                          &pStrm->pStream);
    if (RT_FAILURE(rc))
        return rc;

    PDMAUDIOSTREAMCFG streamCfg;
    rc = paPulseToFmt(pStrm->SampleSpec.format, &streamCfg.enmFormat,
                      &streamCfg.enmEndianness);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio capture format %ld\n", pStrm->SampleSpec.format));
        return rc;
    }

    streamCfg.uHz       = pStrm->SampleSpec.rate;
    streamCfg.cChannels = pStrm->SampleSpec.channels;

    rc = DrvAudioHlpStreamCfgToProps(&streamCfg, &pStream->Props);
    if (RT_SUCCESS(rc))
    {
        uint32_t cSamples = RT_MIN(pStrm->BufAttr.fragsize * 10, pStrm->BufAttr.maxlength)
                            >> pStream->Props.cShift;

        LogFunc(("uHz=%RU32, cChannels=%RU8, cShift=%RU8, cSamples=%RU32\n",
                 pStream->Props.uHz, pStream->Props.cChannels, pStream->Props.cShift, cSamples));

        if (pcSamples)
            *pcSamples = cSamples;

        /* Save pointer to driver instance. */
        pStrm->pDrv = pThis;

        pStrm->pu8PeekBuf = NULL;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioStreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                        uint32_t *pcSamplesCaptured)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    /* We should only call pa_stream_readable_size() once and trust the first value. */
    pa_threaded_mainloop_lock(pThis->pMainLoop);
    size_t cbAvail = pa_stream_readable_size(pStrm->pStream);
    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (cbAvail == (size_t)-1)
        return paError(pStrm->pDrv, "Failed to determine input data size");

    /* If the buffer was not dropped last call, add what remains. */
    if (pStrm->pu8PeekBuf)
    {
        Assert(pStrm->cbPeekBuf >= pStrm->offPeekBuf);
        cbAvail += (pStrm->cbPeekBuf - pStrm->offPeekBuf);
    }

    if (!cbAvail) /* No data? Bail out. */
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;

    size_t cbToRead = RT_MIN(cbAvail, AudioMixBufFreeBytes(&pStream->MixBuf));

    LogFlowFunc(("cbToRead=%zu, cbAvail=%zu, offPeekBuf=%zu, cbPeekBuf=%zu\n",
                 cbToRead, cbAvail, pStrm->offPeekBuf, pStrm->cbPeekBuf));

    size_t   offWrite      = 0;
    uint32_t cWrittenTotal = 0;

    while (cbToRead)
    {
        /* If there is no data, do another peek. */
        if (!pStrm->pu8PeekBuf)
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            pa_stream_peek(pStrm->pStream,
                           (const void**)&pStrm->pu8PeekBuf, &pStrm->cbPeekBuf);
            pa_threaded_mainloop_unlock(pThis->pMainLoop);

            pStrm->offPeekBuf = 0;

            /* No data anymore?
             * Note: If there's a data hole (cbPeekBuf then contains the length of the hole)
             *       we need to drop the stream lateron. */
            if (   !pStrm->pu8PeekBuf
                && !pStrm->cbPeekBuf)
            {
                break;
            }
        }

        Assert(pStrm->cbPeekBuf >= pStrm->offPeekBuf);
        size_t cbToWrite = RT_MIN(pStrm->cbPeekBuf - pStrm->offPeekBuf, cbToRead);

        LogFlowFunc(("cbToRead=%zu, cbToWrite=%zu, offPeekBuf=%zu, cbPeekBuf=%zu, pu8PeekBuf=%p\n",
                     cbToRead, cbToWrite,
                     pStrm->offPeekBuf, pStrm->cbPeekBuf, pStrm->pu8PeekBuf));

        if (cbToWrite)
        {
            uint32_t cWritten;
            rc = AudioMixBufWriteCirc(&pStream->MixBuf,
                                      pStrm->pu8PeekBuf + pStrm->offPeekBuf,
                                      cbToWrite, &cWritten);
            if (RT_FAILURE(rc))
                break;

            uint32_t cbWritten = AUDIOMIXBUF_S2B(&pStream->MixBuf, cWritten);

            Assert(cbToRead >= cbWritten);
            cbToRead -= cbWritten;
            cWrittenTotal += cWritten;
            pStrm->offPeekBuf += cbWritten;
        }

        if (/* Nothing to write anymore? Drop the buffer. */
               !cbToWrite
            /* Was there a hole in the peeking buffer? Drop it. */
            || !pStrm->pu8PeekBuf
            /* If the buffer is done, drop it. */
            || pStrm->offPeekBuf == pStrm->cbPeekBuf)
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            pa_stream_drop(pStrm->pStream);
            pa_threaded_mainloop_unlock(pThis->pMainLoop);

            pStrm->pu8PeekBuf = NULL;
        }
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t cProcessed = 0;
        if (cWrittenTotal)
            rc = AudioMixBufMixToParent(&pStream->MixBuf, cWrittenTotal,
                                        &cProcessed);

        if (pcSamplesCaptured)
            *pcSamplesCaptured = cWrittenTotal;

        LogFlowFunc(("cWrittenTotal=%RU32 (%RU32 processed), rc=%Rrc\n",
                     cWrittenTotal, cProcessed, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioStreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                     uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pPAStream = (PPULSEAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;

    uint32_t cLive = AudioMixBufAvail(&pStream->MixBuf);
    if (!cLive)
    {
        LogFlowFunc(("No live samples, skipping\n"));
        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    pa_threaded_mainloop_lock(pThis->pMainLoop);

    do
    {
        size_t cbWriteable = pa_stream_writable_size(pPAStream->pStream);
        if (cbWriteable == (size_t)-1)
        {
            rc = paError(pPAStream->pDrv, "Failed to determine output data size");
            break;
        }

        size_t cbLive   = AUDIOMIXBUF_S2B(&pStream->MixBuf, cLive);
        size_t cbToRead = RT_MIN(cbWriteable, cbLive);

        LogFlowFunc(("cbToRead=%zu, cbWriteable=%zu, cbLive=%zu\n",
                     cbToRead, cbWriteable, cbLive));

        uint32_t cRead, cbRead;
        while (cbToRead)
        {
            rc = AudioMixBufReadCirc(&pStream->MixBuf, pPAStream->pvPCMBuf,
                                     RT_MIN(cbToRead, pPAStream->cbPCMBuf), &cRead);
            if (   !cRead
                || RT_FAILURE(rc))
            {
                break;
            }

            cbRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead);
            if (pa_stream_write(pPAStream->pStream, pPAStream->pvPCMBuf, cbRead, NULL /* Cleanup callback */,
                                0, PA_SEEK_RELATIVE) < 0)
            {
                rc = paError(pPAStream->pDrv, "Failed to write to output stream");
                break;
            }

            Assert(cbToRead >= cbRead);
            cbToRead    -= cbRead;
            cbReadTotal += cbRead;

            LogFlowFunc(("\tcRead=%RU32 (%zu bytes) cbReadTotal=%RU32, cbToRead=%RU32\n",
                         cRead, AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead), cbReadTotal, cbToRead));
        }

    } while (0);

    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pStream->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pStream->MixBuf, cReadTotal);

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes), rc=%Rrc\n", cReadTotal, cbReadTotal, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @todo Implement va handling. */
static int paError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(szMsg, VERR_INVALID_POINTER);

    if (pThis->cLogErrors++ < VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS)
    {
        int rc2 = pa_context_errno(pThis->pContext);
        LogRel(("PulseAudio: %s: %s\n", szMsg, pa_strerror(rc2)));
    }

    /** @todo Implement some PulseAudio -> IPRT mapping here. */
    return VERR_GENERAL_FAILURE;
}

static void paEnumSinkCb(pa_context *pCtx, const pa_sink_info *pInfo, int eol, void *pvUserData)
{
    if (eol != 0)
        return;

    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pInfo);

    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);
    AssertPtrReturnVoid(pCbCtx->pDrv);

    LogRel2(("PulseAudio: Using output sink '%s'\n", pInfo->name));

    /** @todo Store sinks + channel mapping in callback context as soon as we have surround support. */
    pCbCtx->cDevOut++;

    pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
}

static void paEnumSourceCb(pa_context *pCtx, const pa_source_info *pInfo, int eol, void *pvUserData)
{
    if (eol != 0)
        return;

    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pInfo);

    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);
    AssertPtrReturnVoid(pCbCtx->pDrv);

    LogRel2(("PulseAudio: Using input source '%s'\n", pInfo->name));

    /** @todo Store sources + channel mapping in callback context as soon as we have surround support. */
    pCbCtx->cDevIn++;

    pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
}

static void paEnumServerCb(pa_context *pCtx, const pa_server_info *pInfo, void *pvUserData)
{
    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pInfo);

    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);

    PDRVHOSTPULSEAUDIO pThis    = pCbCtx->pDrv;
    AssertPtrReturnVoid(pThis);

    if (pInfo->default_sink_name)
    {
        Assert(RTStrIsValidEncoding(pInfo->default_sink_name));
        pCbCtx->pszDefaultSink   = RTStrDup(pInfo->default_sink_name);
    }

    if (pInfo->default_sink_name)
    {
        Assert(RTStrIsValidEncoding(pInfo->default_source_name));
        pCbCtx->pszDefaultSource = RTStrDup(pInfo->default_source_name);
    }

    pa_threaded_mainloop_signal(pThis->pMainLoop, 0);
}

static int paEnumerate(PDRVHOSTPULSEAUDIO pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    PDMAUDIOBACKENDCFG Cfg;
    RT_ZERO(Cfg);

    Cfg.cbStreamOut    = sizeof(PULSEAUDIOSTREAM);
    Cfg.cbStreamIn     = sizeof(PULSEAUDIOSTREAM);
    Cfg.cMaxStreamsOut = UINT32_MAX;
    Cfg.cMaxStreamsIn  = UINT32_MAX;

    PULSEAUDIOENUMCBCTX cbCtx;
    RT_ZERO(cbCtx);

    cbCtx.pDrv   = pThis;
    cbCtx.fFlags = fEnum;

    bool fLog = (fEnum & PULSEAUDIOENUMCBFLAGS_LOG);

    int rc = paWaitFor(pThis, pa_context_get_server_info(pThis->pContext, paEnumServerCb, &cbCtx));
    if (RT_SUCCESS(rc))
    {
        if (cbCtx.pszDefaultSink)
        {
            if (fLog)
                LogRel2(("PulseAudio: Default output sink is '%s'\n", cbCtx.pszDefaultSink));

            rc = paWaitFor(pThis, pa_context_get_sink_info_by_name(pThis->pContext, cbCtx.pszDefaultSink,
                                                                   paEnumSinkCb, &cbCtx));
            if (   RT_FAILURE(rc)
                && fLog)
            {
                LogRel(("PulseAudio: Error enumerating properties for default output sink '%s'\n", cbCtx.pszDefaultSink));
            }
        }
        else if (fLog)
            LogRel2(("PulseAudio: No default output sink found\n"));

        if (RT_SUCCESS(rc))
        {
            if (cbCtx.pszDefaultSource)
            {
                if (fLog)
                    LogRel2(("PulseAudio: Default input source is '%s'\n", cbCtx.pszDefaultSource));

                rc = paWaitFor(pThis, pa_context_get_source_info_by_name(pThis->pContext, cbCtx.pszDefaultSource,
                                                                         paEnumSourceCb, &cbCtx));
                if (   RT_FAILURE(rc)
                    && fLog)
                {
                    LogRel(("PulseAudio: Error enumerating properties for default input source '%s'\n", cbCtx.pszDefaultSource));
                }
            }
            else if (fLog)
                LogRel2(("PulseAudio: No default input source found\n"));
        }

        if (RT_SUCCESS(rc))
        {
            Cfg.cSinks   = cbCtx.cDevOut;
            Cfg.cSources = cbCtx.cDevIn;

            if (fLog)
            {
                LogRel2(("PulseAudio: Found %RU8 host playback device(s)\n",  cbCtx.cDevOut));
                LogRel2(("PulseAudio: Found %RU8 host capturing device(s)\n", cbCtx.cDevIn));
            }

            if (pCfg)
                memcpy(pCfg, &Cfg, sizeof(PDMAUDIOBACKENDCFG));
        }

        if (cbCtx.pszDefaultSink)
        {
            RTStrFree(cbCtx.pszDefaultSink);
            cbCtx.pszDefaultSink = NULL;
        }

        if (cbCtx.pszDefaultSource)
        {
            RTStrFree(cbCtx.pszDefaultSource);
            cbCtx.pszDefaultSource = NULL;
        }
    }
    else if (fLog)
        LogRel(("PulseAudio: Error enumerating PulseAudio server properties\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int paDestroyStreamIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    LogFlowFuncEnter();

    if (pStrm->pStream)
    {
        pa_threaded_mainloop_lock(pThis->pMainLoop);

        pa_stream_disconnect(pStrm->pStream);
        pa_stream_unref(pStrm->pStream);

        pa_threaded_mainloop_unlock(pThis->pMainLoop);

        pStrm->pStream = NULL;
    }

    return VINF_SUCCESS;
}

static int paDestroyStreamOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    LogFlowFuncEnter();

    if (pStrm->pStream)
    {
        pa_threaded_mainloop_lock(pThis->pMainLoop);

        pa_stream_disconnect(pStrm->pStream);
        pa_stream_unref(pStrm->pStream);

        pa_threaded_mainloop_unlock(pThis->pMainLoop);

        pStrm->pStream = NULL;
    }

    if (pStrm->pvPCMBuf)
    {
        RTMemFree(pStrm->pvPCMBuf);
        pStrm->pvPCMBuf = NULL;
        pStrm->cbPCMBuf = 0;
    }

    return VINF_SUCCESS;
}

static int paControlStreamOut(PPDMIHOSTAUDIO pInterface,
                              PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface , VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);

            if (   pStrm->pDrainOp
                && pa_operation_get_state(pStrm->pDrainOp) != PA_OPERATION_DONE)
            {
                pa_operation_cancel(pStrm->pDrainOp);
                pa_operation_unref(pStrm->pDrainOp);

                pStrm->pDrainOp = NULL;
            }
            else
            {
                rc = paWaitFor(pThis, pa_stream_cork(pStrm->pStream, 0, paStreamCbSuccess, pStrm));
            }

            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /* Pause audio output (the Pause bit of the AC97 x_CR register is set).
             * Note that we must return immediately from here! */
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            if (!pStrm->pDrainOp)
            {
                rc = paWaitFor(pThis, pa_stream_trigger(pStrm->pStream, paStreamCbSuccess, pStrm));
                if (RT_LIKELY(RT_SUCCESS(rc)))
                    pStrm->pDrainOp = pa_stream_drain(pStrm->pStream, paStreamCbDrain, pStrm);
            }
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int paControlStreamIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                             PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStrm = (PPULSEAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            rc = paWaitFor(pThis, pa_stream_cork(pStrm->pStream, 0 /* Play / resume */, paStreamCbSuccess, pStrm));
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            if (pStrm->pu8PeekBuf) /* Do we need to drop the peek buffer?*/
            {
                pa_stream_drop(pStrm->pStream);
                pStrm->pu8PeekBuf = NULL;
            }

            rc = paWaitFor(pThis, pa_stream_cork(pStrm->pStream, 1 /* Stop / pause */, paStreamCbSuccess, pStrm));
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}

static DECLCALLBACK(void) drvHostPulseAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    AssertPtrReturnVoid(pInterface);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    LogFlowFuncEnter();

    if (pThis->pMainLoop)
        pa_threaded_mainloop_stop(pThis->pMainLoop);

    if (pThis->pContext)
    {
        pa_context_disconnect(pThis->pContext);
        pa_context_unref(pThis->pContext);
        pThis->pContext = NULL;
    }

    if (pThis->pMainLoop)
    {
        pa_threaded_mainloop_free(pThis->pMainLoop);
        pThis->pMainLoop = NULL;
    }

    LogFlowFuncLeave();
}

static DECLCALLBACK(int) drvHostPulseAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    return paEnumerate(pThis, pCfg, PULSEAUDIOENUMCBFLAGS_LOG /* fEnum */);
}

static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostPulseAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}

static DECLCALLBACK(int) drvHostPulseAudioStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg, uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    int rc;
    if (pCfg->enmDir == PDMAUDIODIR_IN)
        rc = paCreateStreamIn(pInterface,  pStream, pCfg, pcSamples);
    else
        rc = paCreateStreamOut(pInterface, pStream, pCfg, pcSamples);

    LogFlowFunc(("%s: rc=%Rrc\n", pStream->szName, rc));
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = paDestroyStreamIn(pInterface,  pStream);
    else
        rc = paDestroyStreamOut(pInterface, pStream);

    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioStreamControl(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = paControlStreamIn(pInterface,  pStream, enmStreamCmd);
    else
        rc = paControlStreamOut(pInterface, pStream, enmStreamCmd);

    return rc;
}

static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostPulseAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    return (PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED);
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostPulseAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    AssertPtrReturn(pInterface, NULL);
    AssertPtrReturn(pszIID, NULL);

    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTPULSEAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPULSEAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}

/**
 * Constructs a PulseAudio Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostPulseAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPULSEAUDIO);
    LogRel(("Audio: Initializing PulseAudio driver\n"));

    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostPulseAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostPulseAudio);

    return VINF_SUCCESS;
}

/**
 * Destructs a PulseAudio Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(void) drvHostPulseAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTPULSEAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPULSEAUDIO);
    LogFlowFuncEnter();
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostPulseAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "PulseAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Pulse Audio host driver",
    /* fFlags */
     PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTPULSEAUDIO),
    /* pfnConstruct */
    drvHostPulseAudioConstruct,
    /* pfnDestruct */
    drvHostPulseAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

static struct audio_option pulse_options[] =
{
    {"DAC_MS", AUD_OPT_INT, &s_pulseCfg.buffer_msecs_out,
     "DAC period size in milliseconds", NULL, 0},
    {"ADC_MS", AUD_OPT_INT, &s_pulseCfg.buffer_msecs_in,
     "ADC period size in milliseconds", NULL, 0}
};

