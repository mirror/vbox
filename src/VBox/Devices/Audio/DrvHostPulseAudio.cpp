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

#define VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS 32 /** @todo Make this configurable thru driver options. */

#ifndef PA_STREAM_NOFLAGS
# define PA_STREAM_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

#ifndef PA_CONTEXT_NOFLAGS
# define PA_CONTEXT_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

/*
 * We use a g_pMainLoop in a separate thread g_pContext. We have to call functions for
 * manipulating objects either from callback functions or we have to protect
 * these functions by pa_threaded_mainloop_lock() / pa_threaded_mainloop_unlock().
 */
static struct pa_threaded_mainloop *g_pMainLoop;
static struct pa_context           *g_pContext;
static volatile bool                g_fAbortMainLoop;

/**
 * Host Pulse audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTPULSEAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS         pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO      IHostAudio;
    /** Error count for not flooding the release log.
     *  UINT32_MAX for unlimited logging. */
    uint32_t           cLogErrors;
    /** Configuration option: stream name. */
    char               *pszStreamName;
} DRVHOSTPULSEAUDIO, *PDRVHOSTPULSEAUDIO;

typedef struct PULSEAUDIOSTREAM
{
    /** Must come first, as this struct might be
     *  casted to one of these structs. */
    union
    {
        PDMAUDIOHSTSTRMIN  In;
        PDMAUDIOHSTSTRMOUT Out;
    };
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

/** Makes DRVHOSTPULSEAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface) \
    ( (PDRVHOSTPULSEAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPULSEAUDIO, IHostAudio)) )

static int  drvHostPulseAudioError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg);
static void drvHostPulseAudioCbSuccess(pa_stream *pStream, int fSuccess, void *pvContext);

/**
 * Signal the main loop to abort. Just signalling isn't sufficient as the
 * mainloop might not have been entered yet.
 */
static void drvHostPulseAudioAbortMainLoop(void)
{
    g_fAbortMainLoop = true;
    pa_threaded_mainloop_signal(g_pMainLoop, 0);
}

static pa_sample_format_t drvHostPulseAudioFmtToPulse(PDMAUDIOFMT fmt)
{
    switch (fmt)
    {
        case AUD_FMT_U8:
            return PA_SAMPLE_U8;

        case AUD_FMT_S16:
            return PA_SAMPLE_S16LE;

#ifdef PA_SAMPLE_S32LE
        case AUD_FMT_S32:
            return PA_SAMPLE_S32LE;
#endif
        default:
            break;
    }

    AssertMsgFailed(("Format %ld not supported\n", fmt));
    return PA_SAMPLE_U8;
}

static int drvHostPulseAudioPulseToFmt(pa_sample_format_t pulsefmt,
                                       PDMAUDIOFMT *pFmt, PDMAUDIOENDIANNESS *pEndianness)
{
    switch (pulsefmt)
    {
        case PA_SAMPLE_U8:
            *pFmt = AUD_FMT_U8;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case PA_SAMPLE_S16LE:
            *pFmt = AUD_FMT_S16;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case PA_SAMPLE_S16BE:
            *pFmt = AUD_FMT_S16;
            *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

#ifdef PA_SAMPLE_S32LE
        case PA_SAMPLE_S32LE:
            *pFmt = AUD_FMT_S32;
            *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;
#endif

#ifdef PA_SAMPLE_S32BE
        case PA_SAMPLE_S32BE:
            *pFmt = AUD_FMT_S32;
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
static int drvHostPulseAudioWaitFor(pa_operation *pOP, RTMSINTERVAL cMsTimeout)
{
    AssertPtrReturn(pOP, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    if (pOP)
    {
        uint64_t u64StartMs = RTTimeMilliTS();
        while (pa_operation_get_state(pOP) == PA_OPERATION_RUNNING)
        {
            if (!g_fAbortMainLoop)
                pa_threaded_mainloop_wait(g_pMainLoop);
            g_fAbortMainLoop = false;

            uint64_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
            if (u64ElapsedMs >= cMsTimeout)
            {
                rc = VERR_TIMEOUT;
                break;
            }
        }

        pa_operation_unref(pOP);
    }

    return rc;
}

/**
 * Context status changed.
 */
static void drvHostPulseAudioCbCtxState(pa_context *pContext, void *pvUser)
{
    AssertPtrReturnVoid(pContext);
    NOREF(pvUser);

    switch (pa_context_get_state(pContext))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
            drvHostPulseAudioAbortMainLoop();
            break;

        case PA_CONTEXT_FAILED:
            LogRel(("PulseAudio: Audio input/output stopped!\n"));
            drvHostPulseAudioAbortMainLoop();
            break;

        default:
            break;
    }
}

/**
 * Callback called when our pa_stream_drain operation was completed.
 */
static void drvHostPulseAudioCbStreamDrain(pa_stream *pStream, int fSuccess, void *pvContext)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvContext;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;
    if (fSuccess)
    {
        pa_operation_unref(pa_stream_cork(pStream, 1,
                                          drvHostPulseAudioCbSuccess, pvContext));
    }
    else
        drvHostPulseAudioError(pStrm->pDrv, "Failed to drain stream");

    pa_operation_unref(pStrm->pDrainOp);
    pStrm->pDrainOp = NULL;
}

/**
 * Stream status changed.
 */
static void drvHostPulseAudioCbStreamState(pa_stream *pStream, void *pvContext)
{
    AssertPtrReturnVoid(pStream);
    NOREF(pvContext);

    switch (pa_stream_get_state(pStream))
    {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            drvHostPulseAudioAbortMainLoop();
            break;

        default:
            break;
    }
}

static void drvHostPulseAudioCbSuccess(pa_stream *pStream, int fSuccess, void *pvContext)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvContext;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;

    if (fSuccess)
        drvHostPulseAudioAbortMainLoop();
    else
        drvHostPulseAudioError(pStrm->pDrv, "Failed to finish stream operation");
}

static int drvHostPulseAudioOpen(bool fIn, const char *pszName,
                                 pa_sample_spec *pSampleSpec, pa_buffer_attr *pBufAttr,
                                 pa_stream **ppStream)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pSampleSpec, VERR_INVALID_POINTER);
    AssertPtrReturn(pBufAttr, VERR_INVALID_POINTER);
    AssertPtrReturn(ppStream, VERR_INVALID_POINTER);

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

    pa_threaded_mainloop_lock(g_pMainLoop);

    do
    {
        if (!(pStream = pa_stream_new(g_pContext, pszName, pSampleSpec,
                                      NULL /* pa_channel_map */)))
        {
            LogRel(("PulseAudio: Could not create stream \"%s\"\n", pszName));
            rc = VERR_NO_MEMORY;
            break;
        }

        pa_stream_set_state_callback(pStream, drvHostPulseAudioCbStreamState, NULL);

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
                        pszName, pa_strerror(pa_context_errno(g_pContext))));
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
                        pszName, pa_strerror(pa_context_errno(g_pContext))));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }

        /* Wait until the stream is ready. */
        for (;;)
        {
            if (!g_fAbortMainLoop)
                pa_threaded_mainloop_wait(g_pMainLoop);
            g_fAbortMainLoop = false;

            pa_stream_state_t sstate = pa_stream_get_state(pStream);
            if (sstate == PA_STREAM_READY)
                break;
            else if (   sstate == PA_STREAM_FAILED
                     || sstate == PA_STREAM_TERMINATED)
            {
                LogRel(("PulseAudio: Failed to initialize stream \"%s\" (state %ld)\n",
                        pszName, sstate));
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

    pa_threaded_mainloop_unlock(g_pMainLoop);

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
    NOREF(pInterface);

    LogFlowFuncEnter();

    int rc = audioLoadPulseLib();
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Failed to load the PulseAudio shared library! Error %Rrc\n", rc));
        return rc;
    }

    bool fLocked = false;

    do
    {
        if (!(g_pMainLoop = pa_threaded_mainloop_new()))
        {
            LogRel(("PulseAudio: Failed to allocate main loop: %s\n",
                     pa_strerror(pa_context_errno(g_pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (!(g_pContext = pa_context_new(pa_threaded_mainloop_get_api(g_pMainLoop), "VirtualBox")))
        {
            LogRel(("PulseAudio: Failed to allocate context: %s\n",
                     pa_strerror(pa_context_errno(g_pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (pa_threaded_mainloop_start(g_pMainLoop) < 0)
        {
            LogRel(("PulseAudio: Failed to start threaded mainloop: %s\n",
                     pa_strerror(pa_context_errno(g_pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        g_fAbortMainLoop = false;
        pa_context_set_state_callback(g_pContext, drvHostPulseAudioCbCtxState, NULL);
        pa_threaded_mainloop_lock(g_pMainLoop);
        fLocked = true;

        if (pa_context_connect(g_pContext, NULL /* pszServer */,
                               PA_CONTEXT_NOFLAGS, NULL) < 0)
        {
            LogRel(("PulseAudio: Failed to connect to server: %s\n",
                     pa_strerror(pa_context_errno(g_pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        /* Wait until the g_pContext is ready */
        for (;;)
        {
            if (!g_fAbortMainLoop)
                pa_threaded_mainloop_wait(g_pMainLoop);
            g_fAbortMainLoop = false;

            pa_context_state_t cstate = pa_context_get_state(g_pContext);
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
        pa_threaded_mainloop_unlock(g_pMainLoop);

    if (RT_FAILURE(rc))
    {
        if (g_pMainLoop)
            pa_threaded_mainloop_stop(g_pMainLoop);

        if (g_pContext)
        {
            pa_context_disconnect(g_pContext);
            pa_context_unref(g_pContext);
            g_pContext = NULL;
        }

        if (g_pMainLoop)
        {
            pa_threaded_mainloop_free(g_pMainLoop);
            g_pMainLoop = NULL;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioInitOut(PPDMIHOSTAUDIO pInterface,
                                                  PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                                  uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    /* pcSamples is optional. */

    PDRVHOSTPULSEAUDIO pDrv = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM pThisStrmOut = (PPULSEAUDIOSTREAM)pHstStrmOut;

    LogFlowFuncEnter();

    pThisStrmOut->pDrainOp            = NULL;

    pThisStrmOut->SampleSpec.format   = drvHostPulseAudioFmtToPulse(pCfg->enmFormat);
    pThisStrmOut->SampleSpec.rate     = pCfg->uHz;
    pThisStrmOut->SampleSpec.channels = pCfg->cChannels;

    /* Note that setting maxlength to -1 does not work on PulseAudio servers
     * older than 0.9.10. So use the suggested value of 3/2 of tlength */
    pThisStrmOut->BufAttr.tlength     =   (pa_bytes_per_second(&pThisStrmOut->SampleSpec)
                                        * s_pulseCfg.buffer_msecs_out) / 1000;
    pThisStrmOut->BufAttr.maxlength   = (pThisStrmOut->BufAttr.tlength * 3) / 2;
    pThisStrmOut->BufAttr.prebuf      = -1; /* Same as tlength */
    pThisStrmOut->BufAttr.minreq      = -1; /* Pulse should set something sensible for minreq on it's own */

    /* Note that the struct BufAttr is updated to the obtained values after this call! */
    char achName[64];
    RTStrPrintf(achName, sizeof(achName), "%.32s (out)", pDrv->pszStreamName);
    int rc = drvHostPulseAudioOpen(false /* fIn */, achName, &pThisStrmOut->SampleSpec, &pThisStrmOut->BufAttr,
                                   &pThisStrmOut->pStream);
    if (RT_FAILURE(rc))
        return rc;

    PDMAUDIOSTREAMCFG streamCfg;
    rc = drvHostPulseAudioPulseToFmt(pThisStrmOut->SampleSpec.format,
                                     &streamCfg.enmFormat, &streamCfg.enmEndianness);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio output format %ld\n", pThisStrmOut->SampleSpec.format));
        return rc;
    }

    streamCfg.uHz       = pThisStrmOut->SampleSpec.rate;
    streamCfg.cChannels = pThisStrmOut->SampleSpec.channels;

    rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmOut->Props);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbBuf  = RT_MIN(pThisStrmOut->BufAttr.tlength * 2,
                                 pThisStrmOut->BufAttr.maxlength); /** @todo Make this configurable! */
        if (cbBuf)
        {
            pThisStrmOut->pvPCMBuf = RTMemAllocZ(cbBuf);
            if (pThisStrmOut->pvPCMBuf)
            {
                pThisStrmOut->cbPCMBuf = cbBuf;

                uint32_t cSamples = cbBuf >> pHstStrmOut->Props.cShift;
                if (pcSamples)
                    *pcSamples = cSamples;

                /* Save pointer to driver instance. */
                pThisStrmOut->pDrv = pDrv;

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

static DECLCALLBACK(bool) drvHostPulseAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostPulseAudioInitIn(PPDMIHOSTAUDIO pInterface,
                                                 PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                                 PDMAUDIORECSOURCE enmRecSource,
                                                 uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    /* pcSamples is optional. */

    PDRVHOSTPULSEAUDIO pDrv = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM pThisStrmIn = (PPULSEAUDIOSTREAM)pHstStrmIn;

    LogFunc(("enmRecSrc=%ld\n", enmRecSource));

    pThisStrmIn->SampleSpec.format   = drvHostPulseAudioFmtToPulse(pCfg->enmFormat);
    pThisStrmIn->SampleSpec.rate     = pCfg->uHz;
    pThisStrmIn->SampleSpec.channels = pCfg->cChannels;

    /* XXX check these values */
    pThisStrmIn->BufAttr.fragsize    = (pa_bytes_per_second(&pThisStrmIn->SampleSpec)
                                   * s_pulseCfg.buffer_msecs_in) / 1000;
    pThisStrmIn->BufAttr.maxlength   = (pThisStrmIn->BufAttr.fragsize * 3) / 2;
    /* Note: Other members of pa_buffer_attr are ignored for record streams. */

    char achName[64];
    RTStrPrintf(achName, sizeof(achName), "%.32s (in)", pDrv->pszStreamName);
    int rc = drvHostPulseAudioOpen(true /* fIn */, achName, &pThisStrmIn->SampleSpec, &pThisStrmIn->BufAttr,
                                   &pThisStrmIn->pStream);
    if (RT_FAILURE(rc))
        return rc;

    PDMAUDIOSTREAMCFG streamCfg;
    rc = drvHostPulseAudioPulseToFmt(pThisStrmIn->SampleSpec.format, &streamCfg.enmFormat,
                                     &streamCfg.enmEndianness);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio capture format %ld\n", pThisStrmIn->SampleSpec.format));
        return rc;
    }

    streamCfg.uHz       = pThisStrmIn->SampleSpec.rate;
    streamCfg.cChannels = pThisStrmIn->SampleSpec.channels;

    rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmIn->Props);
    if (RT_SUCCESS(rc))
    {
        uint32_t cSamples = RT_MIN(pThisStrmIn->BufAttr.fragsize * 10, pThisStrmIn->BufAttr.maxlength)
                            >> pHstStrmIn->Props.cShift;
        LogFunc(("cShift=%RU8, cSamples=%RU32\n", pHstStrmIn->Props.cShift, cSamples));

        if (pcSamples)
            *pcSamples = cSamples;

        /* Save pointer to driver instance. */
        pThisStrmIn->pDrv = pDrv;

        pThisStrmIn->pu8PeekBuf = NULL;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                    uint32_t *pcSamplesCaptured)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PPULSEAUDIOSTREAM pThisStrmIn = (PPULSEAUDIOSTREAM)pHstStrmIn;

    /* We should only call pa_stream_readable_size() once and trust the first value. */
    pa_threaded_mainloop_lock(g_pMainLoop);
    size_t cbAvail = pa_stream_readable_size(pThisStrmIn->pStream);
    pa_threaded_mainloop_unlock(g_pMainLoop);

    if (cbAvail == (size_t)-1)
        return drvHostPulseAudioError(pThisStrmIn->pDrv, "Failed to determine input data size");

    /* If the buffer was not dropped last call, add what remains. */
    if (pThisStrmIn->pu8PeekBuf)
    {
        Assert(pThisStrmIn->cbPeekBuf >= pThisStrmIn->offPeekBuf);
        cbAvail += (pThisStrmIn->cbPeekBuf - pThisStrmIn->offPeekBuf);
    }

    if (!cbAvail) /* No data? Bail out. */
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    size_t cbToRead = RT_MIN(cbAvail, AudioMixBufFreeBytes(&pHstStrmIn->MixBuf));

    LogFlowFunc(("cbToRead=%zu, cbAvail=%zu, offPeekBuf=%zu, cbPeekBuf=%zu\n",
                 cbToRead, cbAvail, pThisStrmIn->offPeekBuf, pThisStrmIn->cbPeekBuf));

    size_t offWrite = 0;
    uint32_t cWrittenTotal = 0;

    while (cbToRead)
    {
        /* If there is no data, do another peek. */
        if (!pThisStrmIn->pu8PeekBuf)
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            pa_stream_peek(pThisStrmIn->pStream,
                           (const void**)&pThisStrmIn->pu8PeekBuf, &pThisStrmIn->cbPeekBuf);
            pa_threaded_mainloop_unlock(g_pMainLoop);

            pThisStrmIn->offPeekBuf = 0;

            /* No data anymore?
             * Note: If there's a data hole (cbPeekBuf then contains the length of the hole)
             *       we need to drop the stream lateron. */
            if (   !pThisStrmIn->pu8PeekBuf
                && !pThisStrmIn->cbPeekBuf)
            {
                break;
            }
        }

        Assert(pThisStrmIn->cbPeekBuf >= pThisStrmIn->offPeekBuf);
        size_t cbToWrite = RT_MIN(pThisStrmIn->cbPeekBuf - pThisStrmIn->offPeekBuf, cbToRead);

        LogFlowFunc(("cbToRead=%zu, cbToWrite=%zu, offPeekBuf=%zu, cbPeekBuf=%zu, pu8PeekBuf=%p\n",
                     cbToRead, cbToWrite,
                     pThisStrmIn->offPeekBuf, pThisStrmIn->cbPeekBuf, pThisStrmIn->pu8PeekBuf));

        if (cbToWrite)
        {
            uint32_t cWritten;
            rc = AudioMixBufWriteCirc(&pHstStrmIn->MixBuf,
                                      pThisStrmIn->pu8PeekBuf + pThisStrmIn->offPeekBuf,
                                      cbToWrite, &cWritten);
            if (RT_FAILURE(rc))
                break;

            uint32_t cbWritten = AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cWritten);

            Assert(cbToRead >= cbWritten);
            cbToRead -= cbWritten;
            cWrittenTotal += cWritten;
            pThisStrmIn->offPeekBuf += cbWritten;
        }

        if (/* Nothing to write anymore? Drop the buffer. */
               !cbToWrite
            /* Was there a hole in the peeking buffer? Drop it. */
            || !pThisStrmIn->pu8PeekBuf
            /* If the buffer is done, drop it. */
            || pThisStrmIn->offPeekBuf == pThisStrmIn->cbPeekBuf)
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            pa_stream_drop(pThisStrmIn->pStream);
            pa_threaded_mainloop_unlock(g_pMainLoop);

            pThisStrmIn->pu8PeekBuf = NULL;
        }
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t cProcessed = 0;
        if (cWrittenTotal)
            rc = AudioMixBufMixToParent(&pHstStrmIn->MixBuf, cWrittenTotal,
                                        &cProcessed);

        if (pcSamplesCaptured)
            *pcSamplesCaptured = cWrittenTotal;

        LogFlowFunc(("cWrittenTotal=%RU32 (%RU32 processed), rc=%Rrc\n",
                     cWrittenTotal, cProcessed, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                  uint32_t *pcSamplesPlayed)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PPULSEAUDIOSTREAM pThisStrmOut = (PPULSEAUDIOSTREAM)pHstStrmOut;

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;

    uint32_t cLive = AudioMixBufAvail(&pHstStrmOut->MixBuf);
    if (!cLive)
    {
        LogFlowFunc(("%p: No live samples, skipping\n", pHstStrmOut));

        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    pa_threaded_mainloop_lock(g_pMainLoop);

    do
    {
        size_t cbWriteable = pa_stream_writable_size(pThisStrmOut->pStream);
        if (cbWriteable == (size_t)-1)
        {
            rc = drvHostPulseAudioError(pThisStrmOut->pDrv, "Failed to determine output data size");
            break;
        }

        size_t cbLive   = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cLive);
        size_t cbToRead = RT_MIN(cbWriteable, cbLive);

        LogFlowFunc(("cbToRead=%zu, cbWriteable=%zu, cbLive=%zu\n",
                     cbToRead, cbWriteable, cbLive));

        uint32_t cRead, cbRead;
        while (cbToRead)
        {
            rc = AudioMixBufReadCirc(&pHstStrmOut->MixBuf, pThisStrmOut->pvPCMBuf,
                                     RT_MIN(cbToRead, pThisStrmOut->cbPCMBuf), &cRead);
            if (   !cRead
                || RT_FAILURE(rc))
            {
                break;
            }

            cbRead = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cRead);
            if (pa_stream_write(pThisStrmOut->pStream, pThisStrmOut->pvPCMBuf, cbRead, NULL /* Cleanup callback */,
                                0, PA_SEEK_RELATIVE) < 0)
            {
                rc = drvHostPulseAudioError(pThisStrmOut->pDrv, "Failed to write to output stream");
                break;
            }

            Assert(cbToRead >= cbRead);
            cbToRead    -= cbRead;
            cbReadTotal += cbRead;

            LogFlowFunc(("\tcRead=%RU32 (%zu bytes) cbReadTotal=%RU32, cbToRead=%RU32\n",
                         cRead, AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cRead), cbReadTotal, cbToRead));
        }

    } while (0);

    pa_threaded_mainloop_unlock(g_pMainLoop);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes), rc=%Rrc\n", cReadTotal, cbReadTotal, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** @todo Implement va handling. */
static int drvHostPulseAudioError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(szMsg, VERR_INVALID_POINTER);

    if (pThis->cLogErrors++ < VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS)
    {
        int rc2 = pa_context_errno(g_pContext);
        LogRel(("PulseAudio: %s: %s\n", szMsg, pa_strerror(rc2)));
    }

    /** @todo Implement some PulseAudio -> IPRT mapping here. */
    return VERR_GENERAL_FAILURE;
}

static DECLCALLBACK(int) drvHostPulseAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PPULSEAUDIOSTREAM pThisStrmIn = (PPULSEAUDIOSTREAM)pHstStrmIn;
    if (pThisStrmIn->pStream)
    {
        pa_threaded_mainloop_lock(g_pMainLoop);
        pa_stream_disconnect(pThisStrmIn->pStream);
        pa_stream_unref(pThisStrmIn->pStream);
        pa_threaded_mainloop_unlock(g_pMainLoop);

        pThisStrmIn->pStream = NULL;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostPulseAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PPULSEAUDIOSTREAM pThisStrmOut = (PPULSEAUDIOSTREAM)pHstStrmOut;
    if (pThisStrmOut->pStream)
    {
        pa_threaded_mainloop_lock(g_pMainLoop);
        pa_stream_disconnect(pThisStrmOut->pStream);
        pa_stream_unref(pThisStrmOut->pStream);
        pa_threaded_mainloop_unlock(g_pMainLoop);

        pThisStrmOut->pStream = NULL;
    }

    if (pThisStrmOut->pvPCMBuf)
    {
        RTMemFree(pThisStrmOut->pvPCMBuf);
        pThisStrmOut->pvPCMBuf = NULL;

        pThisStrmOut->cbPCMBuf = 0;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostPulseAudioControlOut(PPDMIHOSTAUDIO pInterface,
                                                     PPDMAUDIOHSTSTRMOUT pHstStrmOut, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    PPULSEAUDIOSTREAM pThisStrmOut = (PPULSEAUDIOSTREAM)pHstStrmOut;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(g_pMainLoop);

            if (   pThisStrmOut->pDrainOp
                && pa_operation_get_state(pThisStrmOut->pDrainOp) != PA_OPERATION_DONE)
            {
                pa_operation_cancel(pThisStrmOut->pDrainOp);
                pa_operation_unref(pThisStrmOut->pDrainOp);

                pThisStrmOut->pDrainOp = NULL;
            }
            else
            {
                /* This should return immediately. */
                rc = drvHostPulseAudioWaitFor(pa_stream_cork(pThisStrmOut->pStream, 0,
                                                             drvHostPulseAudioCbSuccess, pThisStrmOut),
                                              15 * 1000 /* 15s timeout */);
            }

            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /* Pause audio output (the Pause bit of the AC97 x_CR register is set).
             * Note that we must return immediately from here! */
            pa_threaded_mainloop_lock(g_pMainLoop);
            if (!pThisStrmOut->pDrainOp)
            {
                /* This should return immediately. */
                rc = drvHostPulseAudioWaitFor(pa_stream_trigger(pThisStrmOut->pStream,
                                                                drvHostPulseAudioCbSuccess, pThisStrmOut),
                                              15 * 1000 /* 15s timeout */);
                if (RT_LIKELY(RT_SUCCESS(rc)))
                    pThisStrmOut->pDrainOp = pa_stream_drain(pThisStrmOut->pStream,
                                                             drvHostPulseAudioCbStreamDrain, pThisStrmOut);
            }
            pa_threaded_mainloop_unlock(g_pMainLoop);
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

static DECLCALLBACK(int) drvHostPulseAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                    PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    PPULSEAUDIOSTREAM pThisStrmIn = (PPULSEAUDIOSTREAM)pHstStrmIn;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            /* This should return immediately. */
            rc = drvHostPulseAudioWaitFor(pa_stream_cork(pThisStrmIn->pStream, 0 /* Play / resume */,
                                                         drvHostPulseAudioCbSuccess, pThisStrmIn),
                                          15 * 1000 /* 15s timeout */);
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            if (pThisStrmIn->pu8PeekBuf) /* Do we need to drop the peek buffer?*/
            {
                pa_stream_drop(pThisStrmIn->pStream);
                pThisStrmIn->pu8PeekBuf = NULL;
            }
            /* This should return immediately. */
            rc = drvHostPulseAudioWaitFor(pa_stream_cork(pThisStrmIn->pStream, 1 /* Stop / pause */,
                                                         drvHostPulseAudioCbSuccess, pThisStrmIn),
                                          15 * 1000 /* 15s timeout */);
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostPulseAudioGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    pCfg->cbStreamOut     = sizeof(PULSEAUDIOSTREAM);
    pCfg->cbStreamIn      = sizeof(PULSEAUDIOSTREAM);
    pCfg->cMaxHstStrmsOut = UINT32_MAX;
    pCfg->cMaxHstStrmsIn  = UINT32_MAX;

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvHostPulseAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    LogFlowFuncEnter();

    if (g_pMainLoop)
        pa_threaded_mainloop_stop(g_pMainLoop);

    if (g_pContext)
    {
        pa_context_disconnect(g_pContext);
        pa_context_unref(g_pContext);
        g_pContext = NULL;
    }

    if (g_pMainLoop)
    {
        pa_threaded_mainloop_free(g_pMainLoop);
        g_pMainLoop = NULL;
    }

    LogFlowFuncLeave();
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

    CFGMR3QueryStringAlloc(pCfg, "StreamName", &pThis->pszStreamName);

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
    if (pThis->pszStreamName)
    {
        MMR3HeapFree(pThis->pszStreamName);
        pThis->pszStreamName = NULL;
    }
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
     "ADC period size in milliseconds", NULL, 0},

    NULL
};

