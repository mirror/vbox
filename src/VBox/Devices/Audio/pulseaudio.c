/** @file
 *
 * VBox PulseAudio backend
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>
#include <iprt/mem.h>

#include <pulse/pulseaudio.h>
#include "pulse_stubs.h"

#include "vl_vbox.h"
#include "audio.h"
#define AUDIO_CAP "pulse"
#include "audio_int.h"
#include <stdio.h>

#define MAX_LOG_REL_ERRORS 32

/*
 * We use a g_pMainLoop in a separate thread g_pContext. We have to call functions for
 * manipulating objects either from callback functions or we have to protect
 * these functions by pa_threaded_mainloop_lock() / pa_threaded_mainloop_unlock().
 */
static struct pa_threaded_mainloop *g_pMainLoop;
static struct pa_context           *g_pContext;

typedef struct PulseVoice
{
    /** not accessed from within this context */
    union
    {
        HWVoiceOut     In;
        HWVoiceIn      Out;
    } hw;
    /** DAC buffer */
    void           *pPCMBuf;
    /** Pulse stream */
    pa_stream      *pStream;
    /** Pulse sample format and attribute specification */
    pa_sample_spec SampleSpec;
    /** Pulse playback and buffer metrics */
    pa_buffer_attr BufAttr;
    int            fOpSuccess;
    /** number of logged errors */
    unsigned       cErrors;
    /** Pulse record peek buffer */
    const uint8_t  *pu8PeekBuf;
    size_t         cbPeekBuf;
    size_t         offPeekBuf;
    pa_operation   *pDrainOp;
} PulseVoice;

/* The desired buffer length in milliseconds. Will be the target total stream
 * latency on newer version of pulse. Apparent latency can be less (or more.)
 */
static struct
{
    int         buffer_msecs_out;
    int         buffer_msecs_in;
} conf
=
{
    INIT_FIELD (.buffer_msecs_out = ) 100,
    INIT_FIELD (.buffer_msecs_in  = ) 100,
};

static pa_sample_format_t aud_to_pulsefmt (audfmt_e fmt)
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
            dolog ("Bad audio format %d\n", fmt);
            return PA_SAMPLE_U8;
    }
}


static int pulse_to_audfmt (pa_sample_format_t pulsefmt, audfmt_e *fmt, int *endianess)
{
    switch (pulsefmt)
    {
        case PA_SAMPLE_U8:
            *endianess = 0;
            *fmt = AUD_FMT_U8;
            break;

        case PA_SAMPLE_S16LE:
            *fmt = AUD_FMT_S16;
            *endianess = 0;
            break;

        case PA_SAMPLE_S16BE:
            *fmt = AUD_FMT_S16;
            *endianess = 1;
            break;

#ifdef PA_SAMPLE_S32LE
        case PA_SAMPLE_S32LE:
            *fmt = AUD_FMT_S32;
            *endianess = 0;
            break;
#endif

#ifdef PA_SAMPLE_S32BE
        case PA_SAMPLE_S32BE:
            *fmt = AUD_FMT_S32;
            *endianess = 1;
            break;
#endif

        default:
            return -1;
    }
    return 0;
}

static void stream_success_callback(pa_stream *pStream, int fSuccess, void *userdata)
{
    PulseVoice *pPulse = (PulseVoice *)userdata;
    pPulse->fOpSuccess = fSuccess;
    if (!fSuccess)
    {
        if (pPulse->cErrors < MAX_LOG_REL_ERRORS)
        {
            int rc = pa_context_errno(g_pContext);
            pPulse->cErrors++;
            LogRel(("Pulse: Failed stream operation: %s\n", pa_strerror(rc)));
        }
    }
    pa_threaded_mainloop_signal(g_pMainLoop, 0);
}

/**
 * Synchronously wait until an operation completed.
 */
static int pulse_wait_for_operation (pa_operation *op)
{
    if (op)
    {
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(g_pMainLoop);
        pa_operation_unref(op);
    }

    return 1;
}

/**
 * Context status changed.
 */
static void context_state_callback(pa_context *pContext, void *userdata)
{
    PulseVoice *pPulse = (PulseVoice *)userdata;
    switch (pa_context_get_state(pContext))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(g_pMainLoop, 0);
            break;

        case PA_CONTEXT_FAILED:
            LogRel(("Pulse: Audio input/output stopped!\n"));
            if (pPulse)
                pPulse->cErrors = MAX_LOG_REL_ERRORS;
            pa_threaded_mainloop_signal(g_pMainLoop, 0);
            break;

        default:
            break;
    }
}

/**
 * Stream status changed.
 */
static void stream_state_callback(pa_stream *pStream, void *userdata)
{
    switch (pa_stream_get_state(pStream))
    {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(g_pMainLoop, 0);
            break;

        default:
            break;
    }
}

/**
 * Callback called when our pa_stream_drain operation was completed.
 */
static void stream_drain_callback(pa_stream *pStream, int fSuccess, void *userdata)
{
    PulseVoice *pPulse = (PulseVoice *)userdata;
    pPulse->fOpSuccess = fSuccess;
    if (!fSuccess)
    {
        if (pPulse->cErrors < MAX_LOG_REL_ERRORS)
        {
            int rc = pa_context_errno(g_pContext);
            pPulse->cErrors++;
            LogRel(("Pulse: Failed stream operation: %s\n", pa_strerror(rc)));
        }
    }
    else
        pa_operation_unref(pa_stream_cork(pStream, 1, stream_success_callback, userdata));

    pa_operation_unref(pPulse->pDrainOp);
    pPulse->pDrainOp = NULL;
}

static int pulse_open (int fIn, pa_stream **ppStream, pa_sample_spec *pSampleSpec,
                       pa_buffer_attr *pBufAttr)
{
    const pa_buffer_attr *pBufAttrObtained;
    pa_stream            *pStream = NULL;
    char                 achPCMName[64];
    pa_stream_flags_t    flags = 0;
    const char           *stream_name = audio_get_stream_name();

    RTStrPrintf(achPCMName, sizeof(achPCMName), "%.32s%s%s%s",
                stream_name ? stream_name : "",
                stream_name ? " (" : "",
                fIn ? "pcm_in" : "pcm_out",
                stream_name ? ")" : "");

    LogRel(("Pulse: open %s rate=%dHz channels=%d format=%s\n",
                fIn ? "PCM_IN" : "PCM_OUT", pSampleSpec->rate, pSampleSpec->channels,
                pa_sample_format_to_string(pSampleSpec->format)));

    if (!pa_sample_spec_valid(pSampleSpec))
    {
        LogRel(("Pulse: Unsupported sample specification\n"));
        goto fail;
    }

    pa_threaded_mainloop_lock(g_pMainLoop);

    if (!(pStream = pa_stream_new(g_pContext, achPCMName, pSampleSpec, /*channel_map=*/NULL)))
    {
        LogRel(("Pulse: Cannot create stream %s\n", achPCMName));
        goto unlock_and_fail;
    }

    pa_stream_set_state_callback(pStream, stream_state_callback, NULL);

#if PA_API_VERSION >= 12
    /* XXX */
    flags |= PA_STREAM_ADJUST_LATENCY;
#endif

#if 0
    /* not applicable as we don't use pa_stream_get_latency() and pa_stream_get_time() */
    flags |= PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
#endif

    /* no input/output right away after the stream was started */
    flags |= PA_STREAM_START_CORKED;

    if (fIn)
    {
        LogRel(("Pulse: Requested record buffer attributes: maxlength=%d fragsize=%d\n",
                pBufAttr->maxlength, pBufAttr->fragsize));

        if (pa_stream_connect_record(pStream, /*dev=*/NULL, pBufAttr, flags) < 0)
        {
            LogRel(("Pulse: Cannot connect record stream: %s\n",
                    pa_strerror(pa_context_errno(g_pContext))));
            goto disconnect_unlock_and_fail;
        }
    }
    else
    {
        LogRel(("Pulse: Requested playback buffer attributes: maxlength=%d tlength=%d prebuf=%d minreq=%d\n",
                pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

        if (pa_stream_connect_playback(pStream, /*dev=*/NULL, pBufAttr, flags,
                                       /*cvolume=*/NULL, /*sync_stream=*/NULL) < 0)
        {
            LogRel(("Pulse: Cannot connect playback stream: %s\n",
                    pa_strerror(pa_context_errno(g_pContext))));
            goto disconnect_unlock_and_fail;
        }
    }

    /* Wait until the stream is ready */
    for (;;)
    {
        pa_stream_state_t sstate;
        pa_threaded_mainloop_wait(g_pMainLoop);

        sstate = pa_stream_get_state(pStream);
        if (sstate == PA_STREAM_READY)
            break;
        else if (sstate == PA_STREAM_FAILED || sstate == PA_STREAM_TERMINATED)
        {
            LogRel(("Pulse: Failed to initialize stream (state %d)\n", sstate));
            goto disconnect_unlock_and_fail;
        }
    }

    pBufAttrObtained = pa_stream_get_buffer_attr(pStream);
    memcpy(pBufAttr, pBufAttrObtained, sizeof(pa_buffer_attr));

    if (fIn)
    {
        LogRel(("Pulse:  Obtained record buffer attributes: maxlength=%d fragsize=%d\n",
            pBufAttr->maxlength, pBufAttr->fragsize));
    }
    else
    {
        LogRel(("Pulse:  Obtained playback buffer attributes: maxlength=%d tlength=%d prebuf=%d minreq=%d\n",
            pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));
    }

    pa_threaded_mainloop_unlock(g_pMainLoop);
    *ppStream = pStream;
    return 0;

disconnect_unlock_and_fail:
    pa_stream_disconnect(pStream);

unlock_and_fail:
    pa_threaded_mainloop_unlock(g_pMainLoop);

fail:
    if (pStream)
        pa_stream_unref(pStream);

    *ppStream = NULL;
    return -1;
}

static int pulse_init_out (HWVoiceOut *hw, audsettings_t *as)
{
    PulseVoice *pPulse = (PulseVoice *) hw;
    audsettings_t obt_as;
    int cbBuf;

    pPulse->pDrainOp            = NULL;

    pPulse->SampleSpec.format   = aud_to_pulsefmt (as->fmt);
    pPulse->SampleSpec.rate     = as->freq;
    pPulse->SampleSpec.channels = as->nchannels;

    /* Note that setting maxlength to -1 does not work on PulseAudio servers
     * older than 0.9.10. So use the suggested value of 3/2 of tlength */
    pPulse->BufAttr.tlength     = (pa_bytes_per_second(&pPulse->SampleSpec)
                                  * conf.buffer_msecs_out) / 1000;
    pPulse->BufAttr.maxlength   = (pPulse->BufAttr.tlength * 3) / 2;
    pPulse->BufAttr.prebuf      = -1; /* Same as tlength */
    pPulse->BufAttr.minreq      = -1; /* Pulse should set something sensible for minreq on it's own */

    /* Notice that the struct BufAttr is updated to the obtained values after this call */
    if (pulse_open (0, &pPulse->pStream, &pPulse->SampleSpec, &pPulse->BufAttr))
        return -1;

    if (pulse_to_audfmt (pPulse->SampleSpec.format, &obt_as.fmt, &obt_as.endianness))
    {
        LogRel(("Pulse: Cannot find audio format %d\n", pPulse->SampleSpec.format));
        return -1;
    }

    obt_as.freq       = pPulse->SampleSpec.rate;
    obt_as.nchannels  = pPulse->SampleSpec.channels;

    audio_pcm_init_info (&hw->info, &obt_as);
    cbBuf = audio_MIN(pPulse->BufAttr.tlength * 2, pPulse->BufAttr.maxlength);

    pPulse->pPCMBuf = RTMemAllocZ(cbBuf);
    if (!pPulse->pPCMBuf)
    {
        LogRel(("Pulse: Could not allocate DAC buffer of %d bytes\n", cbBuf));
        return -1;
    }

    /* Convert from bytes to frames (aka samples) */
    hw->samples = cbBuf >> hw->info.shift;

    return 0;
}

static void pulse_fini_out (HWVoiceOut *hw)
{
    PulseVoice *pPulse = (PulseVoice *)hw;

    if (pPulse->pStream)
    {
        pa_threaded_mainloop_lock(g_pMainLoop);
        pa_stream_disconnect(pPulse->pStream);
        pa_stream_unref(pPulse->pStream);
        pa_threaded_mainloop_unlock(g_pMainLoop);
        pPulse->pStream = NULL;
    }

    if (pPulse->pPCMBuf)
    {
        RTMemFree (pPulse->pPCMBuf);
        pPulse->pPCMBuf = NULL;
    }
}

static int pulse_run_out (HWVoiceOut *hw)
{
    PulseVoice  *pPulse = (PulseVoice *) hw;
    int          cFramesLive;
    int          cFramesWritten = 0;
    int          csSamples;
    int          cFramesToWrite;
    int          cFramesAvail;
    size_t       cbAvail;
    size_t       cbToWrite;
    uint8_t     *pu8Dst;
    st_sample_t *psSrc;

    cFramesLive = audio_pcm_hw_get_live_out (hw);
    if (!cFramesLive)
        return 0;

    pa_threaded_mainloop_lock(g_pMainLoop);

    cbAvail = pa_stream_writable_size (pPulse->pStream);
    if (cbAvail == (size_t)-1)
    {
        if (pPulse->cErrors < MAX_LOG_REL_ERRORS)
        {
            int rc = pa_context_errno(g_pContext);
            pPulse->cErrors++;
            LogRel(("Pulse: Failed to determine the writable size: %s\n",
                     pa_strerror(rc)));
        }
        goto unlock_and_exit;
    }

    cFramesAvail   = cbAvail >> hw->info.shift; /* bytes => samples */
    cFramesWritten = audio_MIN (cFramesLive, cFramesAvail);
    csSamples      = cFramesWritten;

    while (csSamples)
    {
        /* split request at the end of our samples buffer */
        cFramesToWrite = audio_MIN (csSamples, hw->samples - hw->rpos);
        cbToWrite      = cFramesToWrite << hw->info.shift;
        psSrc          = hw->mix_buf + hw->rpos;
        pu8Dst         = advance (pPulse->pPCMBuf, hw->rpos << hw->info.shift);

        hw->clip (pu8Dst, psSrc, cFramesToWrite);

        if (pa_stream_write (pPulse->pStream, pu8Dst, cbToWrite,
                             /*cleanup_callback=*/NULL, 0, PA_SEEK_RELATIVE) < 0)
        {
            LogRel(("Pulse: Failed to write %d samples: %s\n",
                    cFramesToWrite, pa_strerror(pa_context_errno(g_pContext))));
            break;
        }
        hw->rpos   = (hw->rpos + cFramesToWrite) % hw->samples;
        csSamples -= cFramesToWrite;
    }

unlock_and_exit:
    pa_threaded_mainloop_unlock(g_pMainLoop);

    return cFramesWritten;
}

static int pulse_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int pulse_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    PulseVoice *pPulse = (PulseVoice *) hw;

    switch (cmd)
    {
        case VOICE_ENABLE:
            /* Start audio output. */
            pa_threaded_mainloop_lock(g_pMainLoop);
            if (   pPulse->pDrainOp
                && pa_operation_get_state(pPulse->pDrainOp) != PA_OPERATION_DONE)
            {
                pa_operation_cancel(pPulse->pDrainOp);
                pa_operation_unref(pPulse->pDrainOp);
                pPulse->pDrainOp = NULL;
            }
            else
            {
                /* should return immediately */
                pulse_wait_for_operation(pa_stream_cork(pPulse->pStream, 0,
                                                        stream_success_callback, pPulse));
            }
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;

        case VOICE_DISABLE:
            /* Pause audio output (the Pause bit of the AC97 x_CR register is set).
             * Note that we must return immediately from here! */
            pa_threaded_mainloop_lock(g_pMainLoop);
            if (!pPulse->pDrainOp)
            {
                /* should return immediately */
                pulse_wait_for_operation(pa_stream_trigger(pPulse->pStream,
                                                           stream_success_callback, pPulse));
                pPulse->pDrainOp = pa_stream_drain(pPulse->pStream,
                                                   stream_drain_callback, pPulse);
            }
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;

        default:
            return -1;
    }
    return 0;
}

static int pulse_init_in (HWVoiceIn *hw, audsettings_t *as)
{
    PulseVoice *pPulse = (PulseVoice *) hw;
    audsettings_t obt_as;

    pPulse->SampleSpec.format   = aud_to_pulsefmt (as->fmt);
    pPulse->SampleSpec.rate     = as->freq;
    pPulse->SampleSpec.channels = as->nchannels;

    /* XXX check these values */
    pPulse->BufAttr.fragsize    = (pa_bytes_per_second(&pPulse->SampleSpec)
                                  * conf.buffer_msecs_in) / 1000;
    pPulse->BufAttr.maxlength   = (pPulse->BufAttr.fragsize * 3) / 2;
    /* Other members of pa_buffer_attr are ignored for record streams */

    if (pulse_open (1, &pPulse->pStream, &pPulse->SampleSpec, &pPulse->BufAttr))
        return -1;

    if (pulse_to_audfmt (pPulse->SampleSpec.format, &obt_as.fmt, &obt_as.endianness))
    {
        LogRel(("Pulse: Cannot find audio format %d\n", pPulse->SampleSpec.format));
        return -1;
    }

    obt_as.freq       = pPulse->SampleSpec.rate;
    obt_as.nchannels  = pPulse->SampleSpec.channels;
    audio_pcm_init_info (&hw->info, &obt_as);
    hw->samples       = audio_MIN(pPulse->BufAttr.fragsize * 10, pPulse->BufAttr.maxlength)
                          >> hw->info.shift;
    pPulse->pu8PeekBuf = NULL;

    return 0;
}

static void pulse_fini_in (HWVoiceIn *hw)
{
    PulseVoice *pPulse = (PulseVoice *)hw;

    if (pPulse->pStream)
    {
        pa_threaded_mainloop_lock(g_pMainLoop);
        pa_stream_disconnect(pPulse->pStream);
        pa_stream_unref(pPulse->pStream);
        pa_threaded_mainloop_unlock(g_pMainLoop);
        pPulse->pStream = NULL;
    }
}

static int pulse_run_in (HWVoiceIn *hw)
{
    PulseVoice *pPulse = (PulseVoice *) hw;
    const int hwshift = hw->info.shift;
    int       cFramesRead = 0;    /* total frames which have been read this call */
    int       cFramesAvail;       /* total frames available from pulse at start of call */
    int       cFramesToRead;      /* the largest amount we want/can get this call */
    int       cFramesToPeek;      /* the largest amount we want/can get this peek */

    /* We should only call pa_stream_readable_size() once and trust the first value */
    pa_threaded_mainloop_lock(g_pMainLoop);
    cFramesAvail = pa_stream_readable_size(pPulse->pStream) >> hwshift;
    pa_threaded_mainloop_unlock(g_pMainLoop);

    if (cFramesAvail == -1)
    {
        if (pPulse->cErrors < MAX_LOG_REL_ERRORS)
        {
            int rc = pa_context_errno(g_pContext);
            pPulse->cErrors++;
            LogRel(("Pulse: Failed to determine the readable size: %s\n",
                     pa_strerror(rc)));
        }
        return 0;
    }

    /* If the buffer was not dropped last call, add what remains */
    if (pPulse->pu8PeekBuf)
        cFramesAvail += (pPulse->cbPeekBuf - pPulse->offPeekBuf) >> hwshift;

    cFramesToRead = audio_MIN(cFramesAvail, hw->samples - audio_pcm_hw_get_live_in(hw));
    for (; cFramesToRead; cFramesToRead -= cFramesToPeek)
    {
        /* If there is no data, do another peek */
        if (!pPulse->pu8PeekBuf)
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            pa_stream_peek(pPulse->pStream, (const void**)&pPulse->pu8PeekBuf, &pPulse->cbPeekBuf);
            pa_threaded_mainloop_unlock(g_pMainLoop);
            pPulse->offPeekBuf = 0;
            if (   !pPulse->pu8PeekBuf
                || !pPulse->cbPeekBuf)
                break;
        }

        cFramesToPeek = audio_MIN((signed)(  pPulse->cbPeekBuf
                                           - pPulse->offPeekBuf) >> hwshift,
                                  cFramesToRead);

        /* Check for wrapping around the buffer end */
        if (hw->wpos + cFramesToPeek > hw->samples)
        {
            int cFramesDelta = hw->samples - hw->wpos;

            hw->conv(hw->conv_buf + hw->wpos,
                     pPulse->pu8PeekBuf + pPulse->offPeekBuf,
                     cFramesDelta,
                     &nominal_volume);

            hw->conv(hw->conv_buf,
                     pPulse->pu8PeekBuf + pPulse->offPeekBuf + (cFramesDelta << hwshift),
                     cFramesToPeek - cFramesDelta,
                     &nominal_volume);
        }
        else
        {
            hw->conv(hw->conv_buf + hw->wpos,
                     pPulse->pu8PeekBuf + pPulse->offPeekBuf,
                     cFramesToPeek,
                     &nominal_volume);
        }

        cFramesRead += cFramesToPeek;
        hw->wpos = (hw->wpos + cFramesToPeek) % hw->samples;
        pPulse->offPeekBuf += cFramesToPeek << hwshift;

        /* If the buffer is done, drop it */
        if (pPulse->offPeekBuf == pPulse->cbPeekBuf)
        {
            pa_threaded_mainloop_lock(g_pMainLoop);
            pa_stream_drop(pPulse->pStream);
            pa_threaded_mainloop_unlock(g_pMainLoop);
            pPulse->pu8PeekBuf = NULL;
        }
    }

    return cFramesRead;
}

static int pulse_read (SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}

static int pulse_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    PulseVoice *pPulse = (PulseVoice *)hw;

    switch (cmd)
    {
        case VOICE_ENABLE:
            pa_threaded_mainloop_lock(g_pMainLoop);
            /* should return immediately */
            pulse_wait_for_operation(pa_stream_cork(pPulse->pStream, 0,
                                                    stream_success_callback, pPulse));
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;

        case VOICE_DISABLE:
            pa_threaded_mainloop_lock(g_pMainLoop);
            if (pPulse->pu8PeekBuf)
            {
                pa_stream_drop(pPulse->pStream);
                pPulse->pu8PeekBuf = NULL;
            }
            /* should return immediately */
            pulse_wait_for_operation(pa_stream_cork(pPulse->pStream, 1,
                                                    stream_success_callback, pPulse));
            pa_threaded_mainloop_unlock(g_pMainLoop);
            break;

        default:
            return -1;
    }
    return 0;
}

static void *pulse_audio_init (void)
{
    int rc;

    rc = audioLoadPulseLib();
    if (RT_FAILURE(rc))
    {
        LogRel(("Pulse: Failed to load the PulseAudio shared library! Error %Rrc\n", rc));
        return NULL;
    }

    if (!(g_pMainLoop = pa_threaded_mainloop_new()))
    {
        LogRel(("Pulse: Failed to allocate main loop: %s\n",
                 pa_strerror(pa_context_errno(g_pContext))));
        goto fail;
    }

    if (!(g_pContext = pa_context_new(pa_threaded_mainloop_get_api(g_pMainLoop), "VBox")))
    {
        LogRel(("Pulse: Failed to allocate context: %s\n",
                 pa_strerror(pa_context_errno(g_pContext))));
        goto fail;
    }

    if (pa_threaded_mainloop_start(g_pMainLoop) < 0)
    {
        LogRel(("Pulse: Failed to start threaded mainloop: %s\n",
                 pa_strerror(pa_context_errno(g_pContext))));
        goto fail;
    }

    pa_context_set_state_callback(g_pContext, context_state_callback, NULL);
    pa_threaded_mainloop_lock(g_pMainLoop);

    if (pa_context_connect(g_pContext, /*server=*/NULL, 0, NULL) < 0)
    {
        LogRel(("Pulse: Failed to connect to server: %s\n",
                 pa_strerror(pa_context_errno(g_pContext))));
        goto unlock_and_fail;
    }

    /* Wait until the g_pContext is ready */
    for (;;)
    {
        pa_context_state_t cstate;
        pa_threaded_mainloop_wait(g_pMainLoop);
        cstate = pa_context_get_state(g_pContext);
        if (cstate == PA_CONTEXT_READY)
            break;
        else if (cstate == PA_CONTEXT_TERMINATED || cstate == PA_CONTEXT_FAILED)
        {
            LogRel(("Pulse: Failed to initialize context (state %d)\n", cstate));
            goto unlock_and_fail;
        }
    }
    pa_threaded_mainloop_unlock(g_pMainLoop);

    return &conf;

unlock_and_fail:
    if (g_pMainLoop)
        pa_threaded_mainloop_unlock(g_pMainLoop);

fail:
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

    return NULL;
}

static void pulse_audio_fini (void *opaque)
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

    (void) opaque;
}

static struct audio_option pulse_options[] =
{
    {"DAC_MS", AUD_OPT_INT, &conf.buffer_msecs_out,
     "DAC period size in milliseconds", NULL, 0},
    {"ADC_MS", AUD_OPT_INT, &conf.buffer_msecs_in,
     "ADC period size in milliseconds", NULL, 0},
    {NULL, 0, NULL, NULL, NULL, 0}
};

static struct audio_pcm_ops pulse_pcm_ops =
{
    pulse_init_out,
    pulse_fini_out,
    pulse_run_out,
    pulse_write,
    pulse_ctl_out,

    pulse_init_in,
    pulse_fini_in,
    pulse_run_in,
    pulse_read,
    pulse_ctl_in
};

struct audio_driver pulse_audio_driver =
{
    INIT_FIELD (name           = ) "pulse",
    INIT_FIELD (descr          = ) "PulseAudio http://www.pulseaudio.org",
    INIT_FIELD (options        = ) pulse_options,
    INIT_FIELD (init           = ) pulse_audio_init,
    INIT_FIELD (fini           = ) pulse_audio_fini,
    INIT_FIELD (pcm_ops        = ) &pulse_pcm_ops,
    INIT_FIELD (can_be_default = ) 1,
    INIT_FIELD (max_voices_out = ) INT_MAX,
    INIT_FIELD (max_voices_in  = ) INT_MAX,
    INIT_FIELD (voice_size_out = ) sizeof (PulseVoice),
    INIT_FIELD (voice_size_in  = ) sizeof (PulseVoice)
};
