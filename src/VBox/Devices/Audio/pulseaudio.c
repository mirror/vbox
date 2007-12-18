/** @file
 *
 * VBox PulseAudio backend
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#include "../../vl_vbox.h"
#include "audio.h"
#define AUDIO_CAP "pulse"
#include "audio_int.h"
#include <stdio.h>

/*
 * We use a g_pMainLoop in a separate thread g_pContext. We have to call functions for
 * manipulating objects either from callback functions or we have to protect
 * these functions by pa_threaded_mainloop_lock() / pa_threaded_mainloop_unlock().
 */
static struct pa_threaded_mainloop *g_pMainLoop;
static struct pa_context           *g_pContext;

static void   pulse_audio_fini (void *);

typedef struct PulseVoice
{
    HWVoiceOut  hw;
    void       *pPCMBuf;
    pa_stream  *pStream;
    int         fOpSuccess;
} PulseVoice;

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

struct pulse_params_req
{
    int                 freq;
    pa_sample_format_t  pa_format;
    int                 nchannels;
};

struct pulse_params_obt
{
    int                 freq;
    pa_sample_format_t  pa_format;
    int                 nchannels;
    unsigned long       buffer_size;
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

static void context_state_callback(pa_context *c, void *userdata)
{
    switch (pa_context_get_state(c))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(g_pMainLoop, 0);
            break;
        default:
            break;
    }
}

static void stream_state_callback(pa_stream *s, void *userdata)
{
    switch (pa_stream_get_state(s))
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

static void stream_latency_update_callback(pa_stream *s, void *userdata)
{
    pa_threaded_mainloop_signal(g_pMainLoop, 0);
}

static int pulse_open (int fIn, struct pulse_params_req *req,
                       struct pulse_params_obt *obt, pa_stream **ppStream)
{
    pa_sample_spec        sspec;
    pa_channel_map        cmap;
    pa_stream            *pStream = NULL;
    pa_buffer_attr        bufAttr;
    const pa_buffer_attr *pBufAttr;
    const pa_sample_spec *pSampSpec;
    const char           *pchPCMName = fIn ? "pcm_in" : "pcm_out";
    pa_stream_flags_t     flags;
    int                   ms = fIn ? conf.buffer_msecs_in : conf.buffer_msecs_out;

    sspec.rate     = req->freq;
    sspec.channels = req->nchannels;
    sspec.format   = req->pa_format;

    LogRel(("Pulse: open %s rate=%dHz channels=%d format=%s\n",
                fIn ? "PCM_IN" : "PCM_OUT", req->freq, req->nchannels,
                pa_sample_format_to_string(req->pa_format)));

    if (!pa_sample_spec_valid(&sspec))
    {
        LogRel(("Pulse: Unsupported sample specification\n"));
        goto fail;
    }

    pa_channel_map_init_auto(&cmap, sspec.channels, PA_CHANNEL_MAP_ALSA);

#if 0
    pa_cvolume_reset(&volume, sspec.channels);
#endif

    pa_threaded_mainloop_lock(g_pMainLoop);

    if (!(pStream = pa_stream_new(g_pContext, pchPCMName, &sspec, &cmap)))
    {
        LogRel(("Pulse: Cannot create stream %s\n", pchPCMName));
        goto unlock_and_fail;
    }

    pSampSpec      = pa_stream_get_sample_spec(pStream);
    obt->pa_format = pSampSpec->format;
    obt->nchannels = pSampSpec->channels;
    obt->freq      = pSampSpec->rate;

    pa_stream_set_state_callback(pStream, stream_state_callback, NULL);
    pa_stream_set_latency_update_callback(pStream, stream_latency_update_callback, NULL);

    memset(&bufAttr, 0, sizeof(bufAttr));
    bufAttr.tlength   = (pa_bytes_per_second(pSampSpec) * ms) / 1000;
    bufAttr.maxlength = (bufAttr.tlength*3) / 2;
    bufAttr.minreq    = pa_bytes_per_second(pSampSpec) / 100;    /* 10ms */
    bufAttr.prebuf    = bufAttr.tlength - bufAttr.minreq;
    bufAttr.fragsize  = pa_bytes_per_second(pSampSpec) / 100;    /* 10ms */

    flags = PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
    if (fIn)
    {
        if (pa_stream_connect_record(pStream, /*dev=*/NULL, &bufAttr, flags) < 0)
        {
            LogRel(("Pulse: Cannot connect record stream : %s\n",
                    pa_strerror(pa_context_errno(g_pContext))));
            goto disconnect_unlock_and_fail;
        }
    }
    else
    {
        if (pa_stream_connect_playback(pStream, /*dev=*/NULL, &bufAttr, flags,
                                       NULL, NULL) < 0)
        {
            LogRel(("Pulse: Cannot connect playback stream: %s\n",
                    pa_strerror(pa_context_errno(g_pContext))));
            goto disconnect_unlock_and_fail;
        }
    }

    /* Wait until the stream is ready */
    pa_threaded_mainloop_wait(g_pMainLoop);

    if (pa_stream_get_state(pStream) != PA_STREAM_READY)
    {
        LogRel(("Pulse: Wrong stream state %d\n", pa_stream_get_state(pStream)));
        goto disconnect_unlock_and_fail;
    }

    pa_threaded_mainloop_unlock(g_pMainLoop);

    pBufAttr = pa_stream_get_buffer_attr(pStream);
    obt->buffer_size = pBufAttr->maxlength;

    LogRel(("Pulse: buffer settings: max=%d tlength=%d prebuf=%d minreq=%d\n",
            pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

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
    PulseVoice *pulse = (PulseVoice *) hw;
    struct pulse_params_req req;
    struct pulse_params_obt obt;
    audfmt_e effective_fmt;
    int endianness;
    audsettings_t obt_as;

    req.pa_format   = aud_to_pulsefmt (as->fmt);
    req.freq        = as->freq;
    req.nchannels   = as->nchannels;

    if (pulse_open (/*fIn=*/0, &req, &obt, &pulse->pStream))
        return -1;

    if (pulse_to_audfmt (obt.pa_format, &effective_fmt, &endianness))
    {
        LogRel(("Pulse: Cannot find audio format %d\n", obt.pa_format));
        return -1;
    }

    obt_as.freq       = obt.freq;
    obt_as.nchannels  = obt.nchannels;
    obt_as.fmt        = effective_fmt;
    obt_as.endianness = endianness;

    audio_pcm_init_info (&hw->info, &obt_as);
    hw->samples = obt.buffer_size >> hw->info.shift;

    pulse->pPCMBuf = RTMemAllocZ(obt.buffer_size);
    if (!pulse->pPCMBuf)
    {
        LogRel(("Pulse: Could not allocate DAC buffer of %d bytes\n", obt.buffer_size));
        return -1;
    }

    return 0;
}

static void pulse_fini_out (HWVoiceOut *hw)
{
    PulseVoice *pulse = (PulseVoice *)hw;
    if (pulse->pStream)
    {
        pa_stream_disconnect(pulse->pStream);
        pa_stream_unref(pulse->pStream);
        pulse->pStream = NULL;
    }
    if (pulse->pPCMBuf)
    {
        RTMemFree (pulse->pPCMBuf);
        pulse->pPCMBuf = NULL;
    }
}

static int pulse_run_out (HWVoiceOut *hw)
{
    PulseVoice *pulse = (PulseVoice *) hw;
    int          csLive, csDecr, csSamples, csToWrite, csAvail;
    size_t       cbAvail, cbToWrite;
    uint8_t     *pu8Dst;
    st_sample_t *psSrc;

    csLive = audio_pcm_hw_get_live_out (hw);
    if (!csLive)
        return 0;

    pa_threaded_mainloop_lock(g_pMainLoop);

    cbAvail = pa_stream_writable_size (pulse->pStream);
    if (cbAvail == -1)
    {
        LogRel(("Pulse: Failed to determine the writable size: %s\n",
                pa_strerror(pa_context_errno(g_pContext))));
        return 0;
    }
    
    csAvail   = cbAvail >> hw->info.shift; /* bytes => samples */
    csDecr    = audio_MIN (csLive, csAvail);
    csSamples = csDecr;

    while (csSamples)
    {
        /* split request at the end of our samples buffer */
        csToWrite = audio_MIN (csSamples, hw->samples - hw->rpos);
        cbToWrite = csToWrite << hw->info.shift;
        psSrc     = hw->mix_buf + hw->rpos;
        pu8Dst    = advance (pulse->pPCMBuf, hw->rpos << hw->info.shift);

        hw->clip (pu8Dst, psSrc, csToWrite);

        if (pa_stream_write (pulse->pStream, pu8Dst, cbToWrite, 
                             /*cleanup_callback=*/NULL, 0, PA_SEEK_RELATIVE) < 0)
        {
            LogRel(("Pulse: Failed to write %d samples: %s\n",
                    csToWrite, pa_strerror(pa_context_errno(g_pContext))));
            break;
        }
        hw->rpos   = (hw->rpos + csToWrite) % hw->samples;
        csSamples -= csToWrite;
    }

    pa_threaded_mainloop_unlock(g_pMainLoop);

    return csDecr;
}

static int pulse_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static void stream_success_callback(pa_stream *pStream, int success, void *userdata)
{
    PulseVoice *pulse = (PulseVoice *) userdata;
    pulse->fOpSuccess = success;
    pa_threaded_mainloop_signal(g_pMainLoop, 0);
}

typedef enum
{
    Unpause  = 0,
    Pause    = 1,
    Flush    = 2,
    Trigger  = 3
} pulse_cmd_t;

static int pulse_ctrl (HWVoiceOut *hw, pulse_cmd_t cmd)
{
    PulseVoice *pulse = (PulseVoice *) hw;
    pa_operation *op = NULL;

    if (!pulse->pStream)
        return 0;

    pa_threaded_mainloop_lock(g_pMainLoop);
    switch (cmd)
    {
        case Pause:
            op = pa_stream_cork(pulse->pStream, 1, stream_success_callback, pulse);
            break;
        case Unpause:
            op = pa_stream_cork(pulse->pStream, 0, stream_success_callback, pulse);
            break;
        case Flush:
            op = pa_stream_flush(pulse->pStream, stream_success_callback, pulse);
            break;
        case Trigger:
            op = pa_stream_trigger(pulse->pStream, stream_success_callback, pulse);
            break;
        default:
            goto fail;
    }
    if (!op)
        LogRel(("Pulse: Failed ctrl cmd=%d to stream: %s\n",
                cmd, pa_strerror(pa_context_errno(g_pContext))));
    else
        pa_operation_unref(op);

fail:
    pa_threaded_mainloop_unlock(g_pMainLoop);
    return 0;
}

static int pulse_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    switch (cmd)
    {
        case VOICE_ENABLE:
            pulse_ctrl(hw, Unpause);
            pulse_ctrl(hw, Trigger);
            break;
        case VOICE_DISABLE:
            pulse_ctrl(hw, Flush);
            break;
        default:
            return -1;
    }
    return 0;
}

static int pulse_init_in (HWVoiceIn *hw, audsettings_t *as)
{
    PulseVoice *pulse = (PulseVoice *) hw;
    struct pulse_params_req req;
    struct pulse_params_obt obt;
    audfmt_e effective_fmt;
    int endianness;
    audsettings_t obt_as;

    req.pa_format   = aud_to_pulsefmt (as->fmt);
    req.freq        = as->freq;
    req.nchannels   = as->nchannels;

    if (pulse_open (/*fIn=*/1, &req, &obt, &pulse->pStream))
        return -1;

    if (pulse_to_audfmt (obt.pa_format, &effective_fmt, &endianness))
    {
        LogRel(("Pulse: Cannot find audio format %d\n", obt.pa_format));
        return -1;
    }

    obt_as.freq       = obt.freq;
    obt_as.nchannels  = obt.nchannels;
    obt_as.fmt        = effective_fmt;
    obt_as.endianness = endianness;

    audio_pcm_init_info (&hw->info, &obt_as);

    /* pcm_in: reserve twice as the maximum buffer length because of peek()/drop(). */
    hw->samples = 2 * (obt.buffer_size >> hw->info.shift);

    /* no buffer for input */
    pulse->pPCMBuf = NULL;

    return 0;
}

static void pulse_fini_in (HWVoiceIn *hw)
{
    PulseVoice *pulse = (PulseVoice *)hw;
    if (pulse->pStream)
    {
        pa_stream_disconnect(pulse->pStream);
        pa_stream_unref(pulse->pStream);
        pulse->pStream = NULL;
    }
    if (pulse->pPCMBuf)
    {
        RTMemFree (pulse->pPCMBuf);
        pulse->pPCMBuf = NULL;
    }
}

static int pulse_run_in (HWVoiceIn *hw)
{
    PulseVoice *pulse = (PulseVoice *) hw;
    int    csDead, csDecr = 0, csSamples, csRead, csAvail;
    size_t cbAvail;
    const void  *pu8Src;
    st_sample_t *psDst;

    csDead = hw->samples - audio_pcm_hw_get_live_in (hw);

    if (!csDead)
        return 0; /* no buffer available */

    pa_threaded_mainloop_lock(g_pMainLoop);

    if (pa_stream_peek(pulse->pStream, &pu8Src, &cbAvail) < 0)
    {
        LogRel(("Pulse: Peek failed: %s\n",
                pa_strerror(pa_context_errno(g_pContext))));
        goto exit;
    }
    if (!pu8Src)
        goto exit;

    csAvail = cbAvail >> hw->info.shift;
    csDecr  = audio_MIN (csDead, csAvail);

    csSamples = csDecr;

    while (csSamples)
    {
        /* split request at the end of our samples buffer */
        psDst      = hw->conv_buf + hw->wpos;
        csRead     = audio_MIN (csSamples, hw->samples - hw->wpos);
        hw->conv (psDst, pu8Src, csRead, &nominal_volume);
        hw->wpos   = (hw->wpos + csRead) % hw->samples;
        csSamples -= csRead;
        pu8Src     = (const void*)((uint8_t*)pu8Src + (csRead << hw->info.shift));
    }

    pa_stream_drop(pulse->pStream);

exit:
    pa_threaded_mainloop_unlock(g_pMainLoop);

    return csDecr;
}

static int pulse_read (SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}

static int pulse_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
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
    if (pa_context_connect(g_pContext, /*server=*/NULL, 0, NULL) < 0)
    {
        LogRel(("Pulse: Failed to connect to server: %s\n",
                 pa_strerror(pa_context_errno(g_pContext))));
        goto fail;
    }

    /* Wait until the g_pContext is ready */
    pa_threaded_mainloop_lock(g_pMainLoop);
    pa_threaded_mainloop_wait(g_pMainLoop);
    if (pa_context_get_state(g_pContext) != PA_CONTEXT_READY)
    {
        LogRel(("Pulse: Wrong context state %d\n", pa_context_get_state(g_pContext)));
        goto unlock_and_fail;
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
