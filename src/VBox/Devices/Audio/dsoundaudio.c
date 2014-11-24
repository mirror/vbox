/* $Id$ */
/** @file
 * DirectSound Windows Host Audio Backend.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: dsoundaudio.c
 *
 * QEMU DirectSound audio driver
 *
 * Copyright (c) 2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * SEAL 1.07 by Carlos 'pel' Hasan was used as documentation
 */

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#define _WIN32_DCOM
#include <windows.h>
#include <objbase.h>
#include <dsound.h>

#include "VBoxDD.h"
#include "vl_vbox.h"
#include "audio.h"
#include <iprt/alloc.h>
#include <iprt/uuid.h>
#include <VBox/log.h>


#define AUDIO_CAP "dsound"
#include "audio_int.h"

/* #define DEBUG_DSOUND */

#define DSLOGF(a) do { LogRel2(a); } while(0)
#define DSLOGREL(a)                 \
    do {                            \
        static int8_t scLogged = 0; \
        if (scLogged < 8) {         \
            ++scLogged;             \
            LogRel(a);              \
        }                           \
        else {                      \
            DSLOGF(a);              \
        }                           \
    } while (0)

static struct {
    int lock_retries;
    int restore_retries;
    int getstatus_retries;
    int set_primary;
    int bufsize_in;
    int bufsize_out;
    audsettings_t settings;
    int latency_millis;
    char *device_guid_out;
    char *device_guid_in;
} conf = {
    1,
    1,
    1,
    0,
    16384,
    16384,
    {
        44100,
        2,
        AUD_FMT_S16
    },
    10,
    NULL,
    NULL
};

typedef struct {
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDCAPTURE dsound_capture;
    LPDIRECTSOUNDBUFFER dsound_primary_buffer;
    audsettings_t settings;
    LPCGUID devguidp;
    LPCGUID devguidp_capture;
} dsound;

static dsound glob_dsound;

typedef struct {
    HWVoiceOut hw;
    LPDIRECTSOUNDBUFFER dsound_buffer;
    DWORD old_pos;
    int first_time;
#ifdef DEBUG_DSOUND
    DWORD old_ppos;
    DWORD played;
    DWORD mixed;
#endif
} DSoundVoiceOut;

typedef struct {
    HWVoiceIn hw;
    int last_read_pos;
    int capture_buffer_size;
    LPDIRECTSOUNDCAPTUREBUFFER dsound_capture_buffer;
    audsettings_t as;
    HRESULT hr_last_run_in;
} DSoundVoiceIn;

static void dsound_log_hresult (HRESULT hr)
{
    const char *str = "BUG";

    switch (hr) {
    case DS_OK:
        str = "The method succeeded";
        break;
#ifdef DS_NO_VIRTUALIZATION
    case DS_NO_VIRTUALIZATION:
        str = "The buffer was created, but another 3D algorithm was substituted";
        break;
#endif
#ifdef DS_INCOMPLETE
    case DS_INCOMPLETE:
        str = "The method succeeded, but not all the optional effects were obtained";
        break;
#endif
#ifdef DSERR_ACCESSDENIED
    case DSERR_ACCESSDENIED:
        str = "The request failed because access was denied";
        break;
#endif
#ifdef DSERR_ALLOCATED
    case DSERR_ALLOCATED:
        str = "The request failed because resources, such as a priority level, were already in use by another caller";
        break;
#endif
#ifdef DSERR_ALREADYINITIALIZED
    case DSERR_ALREADYINITIALIZED:
        str = "The object is already initialized";
        break;
#endif
#ifdef DSERR_BADFORMAT
    case DSERR_BADFORMAT:
        str = "The specified wave format is not supported";
        break;
#endif
#ifdef DSERR_BADSENDBUFFERGUID
    case DSERR_BADSENDBUFFERGUID:
        str = "The GUID specified in an audiopath file does not match a valid mix-in buffer";
        break;
#endif
#ifdef DSERR_BUFFERLOST
    case DSERR_BUFFERLOST:
        str = "The buffer memory has been lost and must be restored";
        break;
#endif
#ifdef DSERR_BUFFERTOOSMALL
    case DSERR_BUFFERTOOSMALL:
        str = "The buffer size is not great enough to enable effects processing";
        break;
#endif
#ifdef DSERR_CONTROLUNAVAIL
    case DSERR_CONTROLUNAVAIL:
        str = "The buffer control (volume, pan, and so on) requested by the caller is not available. Controls must be specified when the buffer is created, using the dwFlags member of DSBUFFERDESC";
        break;
#endif
#ifdef DSERR_DS8_REQUIRED
    case DSERR_DS8_REQUIRED:
        str = "A DirectSound object of class CLSID_DirectSound8 or later is required for the requested functionality. For more information, see IDirectSound8 Interface";
        break;
#endif
#ifdef DSERR_FXUNAVAILABLE
    case DSERR_FXUNAVAILABLE:
        str = "The effects requested could not be found on the system, or they are in the wrong order or in the wrong location; for example, an effect expected in hardware was found in software";
        break;
#endif
#ifdef DSERR_GENERIC
    case DSERR_GENERIC :
        str = "An undetermined error occurred inside the DirectSound subsystem";
        break;
#endif
#ifdef DSERR_INVALIDCALL
    case DSERR_INVALIDCALL:
        str = "This function is not valid for the current state of this object";
        break;
#endif
#ifdef DSERR_INVALIDPARAM
    case DSERR_INVALIDPARAM:
        str = "An invalid parameter was passed to the returning function";
        break;
#endif
#ifdef DSERR_NOAGGREGATION
    case DSERR_NOAGGREGATION:
        str = "The object does not support aggregation";
        break;
#endif
#ifdef DSERR_NODRIVER
    case DSERR_NODRIVER:
        str = "No sound driver is available for use, or the given GUID is not a valid DirectSound device ID";
        break;
#endif
#ifdef DSERR_NOINTERFACE
    case DSERR_NOINTERFACE:
        str = "The requested COM interface is not available";
        break;
#endif
#ifdef DSERR_OBJECTNOTFOUND
    case DSERR_OBJECTNOTFOUND:
        str = "The requested object was not found";
        break;
#endif
#ifdef DSERR_OTHERAPPHASPRIO
    case DSERR_OTHERAPPHASPRIO:
        str = "Another application has a higher priority level, preventing this call from succeeding";
        break;
#endif
#ifdef DSERR_OUTOFMEMORY
    case DSERR_OUTOFMEMORY:
        str = "The DirectSound subsystem could not allocate sufficient memory to complete the caller's request";
        break;
#endif
#ifdef DSERR_PRIOLEVELNEEDED
    case DSERR_PRIOLEVELNEEDED:
        str = "A cooperative level of DSSCL_PRIORITY or higher is required";
        break;
#endif
#ifdef DSERR_SENDLOOP
    case DSERR_SENDLOOP:
        str = "A circular loop of send effects was detected";
        break;
#endif
#ifdef DSERR_UNINITIALIZED
    case DSERR_UNINITIALIZED:
        str = "The Initialize method has not been called or has not been called successfully before other methods were called";
        break;
#endif
#ifdef DSERR_UNSUPPORTED
    case DSERR_UNSUPPORTED:
        str = "The function called is not supported at this time";
        break;
#endif
    default:
        AUD_log (AUDIO_CAP, "Reason: Unknown (HRESULT %#lx)\n", hr);
        return;
    }

    AUD_log (AUDIO_CAP, "Reason: %s\n", str);
}

static void GCC_FMT_ATTR (2, 3) dsound_logerr (
    HRESULT hr,
    const char *fmt,
    ...
    )
{
    va_list ap;

    va_start (ap, fmt);
    AUD_vlog (AUDIO_CAP, fmt, ap);
    va_end (ap);

    dsound_log_hresult (hr);
}

static void GCC_FMT_ATTR (3, 4) dsound_logerr2 (
    HRESULT hr,
    const char *typ,
    const char *fmt,
    ...
    )
{
    va_list ap;

    AUD_log (AUDIO_CAP, "Could not initialize %s\n", typ);
    va_start (ap, fmt);
    AUD_vlog (AUDIO_CAP, fmt, ap);
    va_end (ap);

    dsound_log_hresult (hr);
}

static DWORD millis_to_bytes (struct audio_pcm_info *info, DWORD millis)
{
    return (millis * info->bytes_per_second) / 1000;
}

#ifdef DEBUG_DSOUND
static void print_wave_format (WAVEFORMATEX *wfx)
{
    dolog ("tag             = %d\n", wfx->wFormatTag);
    dolog ("nChannels       = %d\n", wfx->nChannels);
    dolog ("nSamplesPerSec  = %ld\n", wfx->nSamplesPerSec);
    dolog ("nAvgBytesPerSec = %ld\n", wfx->nAvgBytesPerSec);
    dolog ("nBlockAlign     = %d\n", wfx->nBlockAlign);
    dolog ("wBitsPerSample  = %d\n", wfx->wBitsPerSample);
    dolog ("cbSize          = %d\n", wfx->cbSize);
}
#endif

static int dsound_restore_out (LPDIRECTSOUNDBUFFER dsb)
{
    HRESULT hr;
    int i;

    for (i = 0; i < conf.restore_retries; ++i) {
        hr = IDirectSoundBuffer_Restore (dsb);

        switch (hr) {
        case DS_OK:
            return 0;

        case DSERR_BUFFERLOST:
            continue;

        default:
            dsound_logerr (hr, "Could not restore playback buffer\n");
            return -1;
        }
    }

    dolog ("%d attempts to restore playback buffer failed\n", i);
    return -1;
}

static int waveformat_from_audio_settings (WAVEFORMATEX *wfx, audsettings_t *as)
{
    memset (wfx, 0, sizeof (*wfx));

    wfx->wFormatTag = WAVE_FORMAT_PCM;
    wfx->nChannels = as->nchannels;
    wfx->nSamplesPerSec = as->freq;
    wfx->nAvgBytesPerSec = as->freq << (as->nchannels == 2);
    wfx->nBlockAlign = 1 << (as->nchannels == 2);
    wfx->cbSize = 0;

    switch (as->fmt) {
    case AUD_FMT_S8:
    case AUD_FMT_U8:
        wfx->wBitsPerSample = 8;
        break;

    case AUD_FMT_S16:
    case AUD_FMT_U16:
        wfx->wBitsPerSample = 16;
        wfx->nAvgBytesPerSec <<= 1;
        wfx->nBlockAlign <<= 1;
        break;

    case AUD_FMT_S32:
    case AUD_FMT_U32:
        wfx->wBitsPerSample = 32;
        wfx->nAvgBytesPerSec <<= 2;
        wfx->nBlockAlign <<= 2;
        break;

    default:
        dolog ("Internal logic error: Bad audio format %d\n", as->freq);
        return -1;
    }

    return 0;
}

static int waveformat_to_audio_settings (WAVEFORMATEX *wfx, audsettings_t *as)
{
    if (wfx->wFormatTag != WAVE_FORMAT_PCM) {
        dolog ("Invalid wave format, tag is not PCM, but %d\n",
               wfx->wFormatTag);
        return -1;
    }

    if (!wfx->nSamplesPerSec) {
        dolog ("Invalid wave format, frequency is zero\n");
        return -1;
    }
    as->freq = wfx->nSamplesPerSec;

    switch (wfx->nChannels) {
    case 1:
        as->nchannels = 1;
        break;

    case 2:
        as->nchannels = 2;
        break;

    default:
        dolog (
            "Invalid wave format, number of channels is not 1 or 2, but %d\n",
            wfx->nChannels
            );
        return -1;
    }

    switch (wfx->wBitsPerSample) {
    case 8:
        as->fmt = AUD_FMT_U8;
        break;

    case 16:
        as->fmt = AUD_FMT_S16;
        break;

    case 32:
        as->fmt = AUD_FMT_S32;
        break;

    default:
        dolog ("Invalid wave format, bits per sample is not "
               "8, 16 or 32, but %d\n",
               wfx->wBitsPerSample);
        return -1;
    }

    return 0;
}

static void dsoundCaptureInterfaceRelease (dsound *s)
{
    if (s->dsound_capture) {
        HRESULT hr = IDirectSoundCapture_Release (s->dsound_capture);
        if (FAILED (hr)) {
            DSLOGF(("DSound: DirectSoundCapture release %Rhrc\n", hr));
        }
        s->dsound_capture = NULL;
    }
}

static int dsoundCaptureInterfaceCreate (dsound *s)
{
    HRESULT hr;

    if (s->dsound_capture != NULL) {
        DSLOGF(("DSound: DirectSoundCapture instance already exists\n"));
        return -1;
    }

    hr = CoCreateInstance (&CLSID_DirectSoundCapture, NULL, CLSCTX_ALL,
                           &IID_IDirectSoundCapture, (void **) &s->dsound_capture);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: DirectSoundCapture create instance %Rhrc\n", hr));
    }
    else {
        hr = IDirectSoundCapture_Initialize (s->dsound_capture, s->devguidp_capture);
        if (FAILED (hr)) {
            DSLOGREL(("DSound: DirectSoundCapture initialize %Rhrc\n", hr));
            dsoundCaptureInterfaceRelease (s);
        }
    }

    return SUCCEEDED (hr)? 0: -1;
}

static void dsoundCaptureClose (DSoundVoiceIn *ds)
{
    dsound *s = &glob_dsound;

    DSLOGF(("DSound: capture close %p buffer %p\n", ds, ds->dsound_capture_buffer));

    if (ds->dsound_capture_buffer) {
        HRESULT hr = IDirectSoundCaptureBuffer_Stop (ds->dsound_capture_buffer);
        if (FAILED (hr)) {
            DSLOGF(("DSound: close capture buffer stop %Rhrc\n", hr));
        }

        hr = IDirectSoundCaptureBuffer_Release (ds->dsound_capture_buffer);
        if (FAILED (hr)) {
            DSLOGF(("DSound: close capture buffer release %Rhrc\n", hr));
        }
        ds->dsound_capture_buffer = NULL;
    }

    dsoundCaptureInterfaceRelease (s);
}

static int dsoundCaptureOpen (DSoundVoiceIn *ds)
{
    dsound *s = &glob_dsound;

    int err;
    HRESULT hr;
    WAVEFORMATEX wfx;
    DSCBUFFERDESC bd;
    DSCBCAPS bc;
    DWORD cpos;

    DSLOGF(("DSound: capture open %p size %d samples, freq %d, chan %d, bits %d, sign %d\n",
            ds,
            ds->hw.samples,
            ds->hw.info.freq,
            ds->hw.info.nchannels,
            ds->hw.info.bits,
            ds->hw.info.sign));

    if (ds->dsound_capture_buffer != NULL) {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundCaptureBuffer already exists\n"));
        dsoundCaptureClose (ds);
    }

    err = waveformat_from_audio_settings (&wfx, &ds->as);
    if (err) {
        return err;
    }

    err = dsoundCaptureInterfaceCreate (s);
    if (err) {
        return err;
    }

    memset (&bd, 0, sizeof (bd));
    bd.dwSize = sizeof (bd);
    bd.lpwfxFormat = &wfx;
    bd.dwBufferBytes = ds->hw.samples << ds->hw.info.shift;
    hr = IDirectSoundCapture_CreateCaptureBuffer (s->dsound_capture,
                                                  &bd, &ds->dsound_capture_buffer, NULL);

    if (FAILED (hr)) {
        DSLOGREL(("DSound: create capture buffer %Rhrc\n", hr));
        ds->dsound_capture_buffer = NULL;
        goto fail0;
    }

    /* Query the actual parameters. */

    hr = IDirectSoundCaptureBuffer_GetCurrentPosition (ds->dsound_capture_buffer, &cpos, NULL);
    if (FAILED (hr)) {
        cpos = 0;
        DSLOGF(("DSound: open GetCurrentPosition %Rhrc\n", hr));
    }

    memset (&wfx, 0, sizeof (wfx));
    hr = IDirectSoundCaptureBuffer_GetFormat (ds->dsound_capture_buffer, &wfx, sizeof (wfx), NULL);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: capture buffer GetFormat %Rhrc\n", hr));
        goto fail0;
    }

    memset (&bc, 0, sizeof (bc));
    bc.dwSize = sizeof (bc);
    hr = IDirectSoundCaptureBuffer_GetCaps (ds->dsound_capture_buffer, &bc);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: capture buffer GetCaps %Rhrc\n", hr));
        goto fail0;
    }

    DSLOGF(("DSound: capture buffer format: size %d bytes\n"
            "  tag             = %d\n"
            "  nChannels       = %d\n"
            "  nSamplesPerSec  = %d\n"
            "  nAvgBytesPerSec = %d\n"
            "  nBlockAlign     = %d\n"
            "  wBitsPerSample  = %d\n"
            "  cbSize          = %d\n",
            bc.dwBufferBytes,
            wfx.wFormatTag,
            wfx.nChannels,
            wfx.nSamplesPerSec,
            wfx.nAvgBytesPerSec,
            wfx.nBlockAlign,
            wfx.wBitsPerSample,
            wfx.cbSize));

    if (bc.dwBufferBytes & ds->hw.info.align) {
        DSLOGREL(("DSound: GetCaps returned misaligned buffer size %ld, alignment %d\n",
                  bc.dwBufferBytes, ds->hw.info.align + 1));
    }

    if (ds->hw.samples != 0 && ds->hw.samples != (bc.dwBufferBytes >> ds->hw.info.shift)) {
        DSLOGREL(("DSound: buffer size mismatch dsound %d, hw %d bytes\n",
                  bc.dwBufferBytes, ds->hw.samples << ds->hw.info.shift));
    }

    /* Initial state: reading at the initial capture position. */
    ds->hw.wpos = 0;
    ds->last_read_pos = cpos >> ds->hw.info.shift;
    ds->capture_buffer_size = bc.dwBufferBytes >> ds->hw.info.shift;
    DSLOGF(("DSound: open last_read_pos %d, capture_buffer_size %d\n", ds->last_read_pos, ds->capture_buffer_size));

    ds->hr_last_run_in = S_OK;

    return 0;

 fail0:
    dsoundCaptureClose (ds);
    return -1;
}

static void dsoundCaptureStop (DSoundVoiceIn *ds)
{
    if (ds->dsound_capture_buffer) {
        HRESULT hr = IDirectSoundCaptureBuffer_Stop (ds->dsound_capture_buffer);
        if (FAILED (hr)) {
            DSLOGF(("DSound: stop capture buffer %Rhrc\n", hr));
        }
    }
}

static int dsoundCaptureStart (DSoundVoiceIn *ds)
{
    HRESULT hr;
    DWORD status;

    if (ds->dsound_capture_buffer != NULL) {
        hr = IDirectSoundCaptureBuffer_GetStatus (ds->dsound_capture_buffer, &status);
        if (FAILED (hr)) {
            DSLOGF(("DSound: start GetStatus %Rhrc\n", hr));
        }
        else {
            if (status & DSCBSTATUS_CAPTURING) {
                DSLOGF(("DSound: already capturing\n"));
            }
            else {
                hr = IDirectSoundCaptureBuffer_Start (ds->dsound_capture_buffer, DSCBSTART_LOOPING);
                if (FAILED (hr)) {
                    DSLOGREL(("DSound: start %Rhrc\n", hr));
                }
            }
        }
    }
    else {
        hr = E_FAIL;
    }

    return SUCCEEDED (hr)? 0: -1;
}

#include "dsound_template.h"
#define DSBTYPE_IN
#include "dsound_template.h"
#undef DSBTYPE_IN

static int dsound_get_status_out (LPDIRECTSOUNDBUFFER dsb, DWORD *statusp)
{
    HRESULT hr;
    int i;

    for (i = 0; i < conf.getstatus_retries; ++i) {
        hr = IDirectSoundBuffer_GetStatus (dsb, statusp);
        if (FAILED (hr)) {
            dsound_logerr (hr, "Could not get playback buffer status\n");
            return -1;
        }

        if (*statusp & DSERR_BUFFERLOST) {
            if (dsound_restore_out (dsb)) {
                return -1;
            }
            continue;
        }
        break;
    }

    return 0;
}

static int dsound_get_status_in (LPDIRECTSOUNDCAPTUREBUFFER dscb,
                                 DWORD *statusp)
{
    HRESULT hr;

    hr = IDirectSoundCaptureBuffer_GetStatus (dscb, statusp);
    if (FAILED (hr)) {
        dsound_logerr (hr, "Could not get capture buffer status\n");
        return -1;
    }

    return 0;
}

static void dsound_write_sample (HWVoiceOut *hw, uint8_t *dst, int dst_len)
{
    int src_len1 = dst_len;
    int src_len2 = 0;
    int pos = hw->rpos + dst_len;
    st_sample_t *src1 = hw->mix_buf + hw->rpos;
    st_sample_t *src2 = NULL;

    if (pos > hw->samples) {
        src_len1 = hw->samples - hw->rpos;
        src2 = hw->mix_buf;
        src_len2 = dst_len - src_len1;
        pos = src_len2;
    }

    if (src_len1) {
        hw->clip (dst, src1, src_len1);
//        mixeng_sniff_and_clear (hw, src1, dst, src_len1);
    }

    if (src_len2) {
        dst = advance (dst, src_len1 << hw->info.shift);
        hw->clip (dst, src2, src_len2);
//        mixeng_sniff_and_clear (hw, src2, dst, src_len2);
    }

    hw->rpos = pos % hw->samples;
}

static void dsound_clear_sample (HWVoiceOut *hw, LPDIRECTSOUNDBUFFER dsb)
{
    int err;
    LPVOID p1, p2;
    DWORD blen1, blen2, len1, len2;

    err = dsound_lock_out (
        dsb,
        &hw->info,
        0,
        hw->samples << hw->info.shift,
        &p1, &p2,
        &blen1, &blen2,
        1
        );
    if (err) {
        return;
    }

    len1 = blen1 >> hw->info.shift;
    len2 = blen2 >> hw->info.shift;

#ifdef DEBUG_DSOUND
    dolog ("clear %p,%ld,%ld %p,%ld,%ld\n",
           p1, blen1, len1,
           p2, blen2, len2);
#endif

    if (p1 && len1) {
        audio_pcm_info_clear_buf (&hw->info, p1, len1);
    }

    if (p2 && len2) {
        audio_pcm_info_clear_buf (&hw->info, p2, len2);
    }

    dsound_unlock_out (dsb, p1, p2, blen1, blen2);
}

static void dsound_close (dsound *s)
{
    HRESULT hr;

    if (s->dsound_primary_buffer) {
        hr = IDirectSoundBuffer_Release (s->dsound_primary_buffer);
        if (FAILED (hr)) {
            dsound_logerr (hr, "Could not release primary buffer\n");
        }
        s->dsound_primary_buffer = NULL;
    }
}

static int dsound_open (dsound *s)
{
    int err;
    HRESULT hr;
    WAVEFORMATEX wfx;
    DSBUFFERDESC dsbd;
    HWND hwnd;

    hwnd = GetDesktopWindow ();
    hr = IDirectSound_SetCooperativeLevel (
        s->dsound,
        hwnd,
        DSSCL_PRIORITY
        );

    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not set cooperative level for window %p\n",
                       hwnd);
#else
        LogRel(("DSound: Could not set cooperative level for window %p\n", hwnd));
        dsound_log_hresult(hr);
#endif
        return -1;
    }

    if (!conf.set_primary) {
        return 0;
    }

    err = waveformat_from_audio_settings (&wfx, &conf.settings);
    if (err) {
        return -1;
    }

    memset (&dsbd, 0, sizeof (dsbd));
    dsbd.dwSize = sizeof (dsbd);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    dsbd.dwBufferBytes = 0;
    dsbd.lpwfxFormat = NULL;

    hr = IDirectSound_CreateSoundBuffer (
        s->dsound,
        &dsbd,
        &s->dsound_primary_buffer,
        NULL
        );
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not create primary playback buffer\n");
#else
        LogRel(("DSound: Could not create primary playback buffer\n"));
        dsound_log_hresult(hr);
#endif
        return -1;
    }

    hr = IDirectSoundBuffer_SetFormat (s->dsound_primary_buffer, &wfx);
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not set primary playback buffer format\n");
#else
        LogRel(("DSound: Could not set primary playback buffer format\n"));
        dsound_log_hresult(hr);
#endif
    }

    hr = IDirectSoundBuffer_GetFormat (
        s->dsound_primary_buffer,
        &wfx,
        sizeof (wfx),
        NULL
        );
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not get primary playback buffer format\n");
#else
        LogRel(("DSound: Could not get primary playback buffer format\n"));
        dsound_log_hresult(hr);
#endif
        goto fail0;
    }

#ifdef DEBUG_DSOUND
    dolog ("Primary\n");
    print_wave_format (&wfx);
#endif

    err = waveformat_to_audio_settings (&wfx, &s->settings);
    if (err) {
        goto fail0;
    }

    return 0;

 fail0:
    dsound_close (s);
    return -1;
}

static int dsound_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    HRESULT hr;
    DWORD status;
    DSoundVoiceOut *ds = (DSoundVoiceOut *) hw;
    LPDIRECTSOUNDBUFFER dsb = ds->dsound_buffer;

    if (!dsb) {
        dolog ("Attempt to control voice without a buffer\n");
        return 0;
    }

    switch (cmd) {
    case VOICE_ENABLE:
        if (dsound_get_status_out (dsb, &status)) {
            return -1;
        }

        if (status & DSBSTATUS_PLAYING) {
            dolog ("warning: Voice is already playing\n");
            return 0;
        }

        dsound_clear_sample (hw, dsb);

        hr = IDirectSoundBuffer_Play (dsb, 0, 0, DSBPLAY_LOOPING);
        if (FAILED (hr)) {
            dsound_logerr (hr, "Could not start playing buffer\n");
            return -1;
        }
        break;

    case VOICE_DISABLE:
        if (dsound_get_status_out (dsb, &status)) {
            return -1;
        }

        if (status & DSBSTATUS_PLAYING) {
            hr = IDirectSoundBuffer_Stop (dsb);
            if (FAILED (hr)) {
                dsound_logerr (hr, "Could not stop playing buffer\n");
                return -1;
            }
        }
        else {
            dolog ("warning: Voice is not playing\n");
        }
        break;
    }
    return 0;
}

static int dsound_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int dsound_run_out (HWVoiceOut *hw)
{
    int err;
    HRESULT hr;
    DSoundVoiceOut *ds = (DSoundVoiceOut *) hw;
    LPDIRECTSOUNDBUFFER dsb = ds->dsound_buffer;
    int live, len, hwshift;
    DWORD blen1, blen2;
    DWORD len1, len2;
    DWORD decr;
    DWORD wpos, ppos, old_pos;
    LPVOID p1, p2;
    int bufsize;

    if (!dsb) {
        dolog ("Attempt to run empty with playback buffer\n");
        return 0;
    }

    hwshift = hw->info.shift;
    bufsize = hw->samples << hwshift;

    live = audio_pcm_hw_get_live_out (hw);

    hr = IDirectSoundBuffer_GetCurrentPosition (
        dsb,
        &ppos,
        ds->first_time ? &wpos : NULL
        );
    if (hr == DSERR_BUFFERLOST) {
        if (dsound_restore_out(dsb))
            return 0;
        hr = IDirectSoundBuffer_GetCurrentPosition(dsb, &ppos, ds->first_time ? &wpos : NULL);
        if (hr == DSERR_BUFFERLOST)
            return 0;   // Avoid log flooding if the error is still there.
    }
    if (FAILED (hr)) {
        dsound_logerr (hr, "Could not get playback buffer position\n");
        return 0;
    }

    len = live << hwshift;

    if (ds->first_time) {
        if (conf.latency_millis) {
            DWORD cur_blat;

            cur_blat = audio_ring_dist (wpos, ppos, bufsize);
            ds->first_time = 0;
            old_pos = wpos;
            old_pos +=
                millis_to_bytes (&hw->info, conf.latency_millis) - cur_blat;
            old_pos %= bufsize;
            old_pos &= ~hw->info.align;
        }
        else {
            old_pos = wpos;
        }
#ifdef DEBUG_DSOUND
        ds->played = 0;
        ds->mixed = 0;
#endif
    }
    else {
        if (ds->old_pos == ppos) {
#ifdef DEBUG_DSOUND
            dolog ("old_pos == ppos\n");
#endif
            return 0;
        }

#ifdef DEBUG_DSOUND
        ds->played += audio_ring_dist (ds->old_pos, ppos, bufsize);
#endif
        old_pos = ds->old_pos;
    }

    if ((old_pos < ppos) && ((old_pos + len) > ppos)) {
        len = ppos - old_pos;
    }
    else {
        if ((old_pos > ppos) && ((old_pos + len) > (ppos + bufsize))) {
            len = bufsize - old_pos + ppos;
        }
    }

    if (audio_bug (AUDIO_FUNC, len < 0 || len > bufsize)) {
        dolog ("len=%d bufsize=%d old_pos=%ld ppos=%ld\n",
               len, bufsize, old_pos, ppos);
        return 0;
    }

    len &= ~hw->info.align;
    if (!len) {
        return 0;
    }

#ifdef DEBUG_DSOUND
    ds->old_ppos = ppos;
#endif
    err = dsound_lock_out (
        dsb,
        &hw->info,
        old_pos,
        len,
        &p1, &p2,
        &blen1, &blen2,
        0
        );
    if (err) {
        return 0;
    }

    len1 = blen1 >> hwshift;
    len2 = blen2 >> hwshift;
    decr = len1 + len2;

    if (p1 && len1) {
        dsound_write_sample (hw, p1, len1);
    }

    if (p2 && len2) {
        dsound_write_sample (hw, p2, len2);
    }

    dsound_unlock_out (dsb, p1, p2, blen1, blen2);
    ds->old_pos = (old_pos + (decr << hwshift)) % bufsize;

#ifdef DEBUG_DSOUND
    ds->mixed += decr << hwshift;

    dolog ("played %lu mixed %lu diff %ld sec %f\n",
           ds->played,
           ds->mixed,
           ds->mixed - ds->played,
           abs (ds->mixed - ds->played) / (double) hw->info.bytes_per_second);
#endif
    return decr;
}

static int dsound_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    DSoundVoiceIn *ds = (DSoundVoiceIn *) hw;

    switch (cmd) {
    case VOICE_ENABLE:
        /* Try to start capture. If it fails, then reopen and try again. */
        if (dsoundCaptureStart (ds)) {
            dsoundCaptureClose (ds);
            dsoundCaptureOpen (ds);

            if (dsoundCaptureStart (ds)) {
                return -1;
            }
        }
        break;

    case VOICE_DISABLE:
        dsoundCaptureStop (ds);
        break;
    }
    return 0;
}

static int dsound_read (SWVoiceIn *sw, void *buf, int len)
{
    return audio_pcm_sw_read (sw, buf, len);
}

static int dsound_run_in (HWVoiceIn *hw)
{
    int err;
    HRESULT hr;
    DSoundVoiceIn *ds = (DSoundVoiceIn *) hw;
    LPDIRECTSOUNDCAPTUREBUFFER dscb = ds->dsound_capture_buffer;
    int live, len, dead;
    int ltmp;
    DWORD blen1, blen2;
    int len1, len2;
    int decr;
    DWORD cpos;
    LPVOID p1, p2;
    int hwshift;

    if (!dscb) {
        DSLOGF(("DSound: run_in no capture buffer\n"));
        return 0;
    }

    hwshift = hw->info.shift;

    live = audio_pcm_hw_get_live_in (hw);
    dead = hw->samples - live;
    if (!dead) {
        return 0;
    }

    hr = IDirectSoundCaptureBuffer_GetCurrentPosition (
        dscb,
        &cpos,
        NULL
        );
    if (FAILED (hr)) {
        if (hr != ds->hr_last_run_in) {
            DSLOGREL(("DSound: run_in GetCurrentPosition %Rhrc\n", hr));
        }
        ds->hr_last_run_in = hr;
        return 0;
    }
    ds->hr_last_run_in = hr;

    if (cpos & hw->info.align) {
        DSLOGF(("DSound: run_in misaligned capture position %ld(%d)\n", cpos, hw->info.align));
    }

    cpos >>= hwshift;

    /* Number of samples available in the capture buffer. */
    len = audio_ring_dist (cpos, ds->last_read_pos, ds->capture_buffer_size);
    if (!len) {
        return 0;
    }
    len = audio_MIN (len, dead);

    err = dsound_lock_in (
        dscb,
        &hw->info,
        ds->last_read_pos << hwshift,
        len << hwshift,
        &p1,
        &p2,
        &blen1,
        &blen2,
        0
        );
    if (err) {
        return 0;
    }

    len1 = blen1 >> hwshift;
    len2 = blen2 >> hwshift;
    decr = len1 + len2;

    if (p1 && len1) {
        ltmp = audio_MIN(len1, hw->samples - hw->wpos);
        hw->conv (hw->conv_buf + hw->wpos, p1, ltmp, &pcm_in_volume);
        if (len1 > ltmp) {
            hw->conv (hw->conv_buf, (void *)((uintptr_t)p1 + (ltmp << hwshift)), len1 - ltmp, &pcm_in_volume);
        }
        hw->wpos = (hw->wpos + len1) % hw->samples;
    }

    if (p2 && len2) {
        ltmp = audio_MIN(len2, hw->samples - hw->wpos);
        hw->conv (hw->conv_buf + hw->wpos, p2, ltmp, &pcm_in_volume);
        if (len2 > ltmp) {
            hw->conv (hw->conv_buf, (void *)((uintptr_t)p2 + (ltmp << hwshift)), len2 - ltmp, &pcm_in_volume);
        }
        hw->wpos = (hw->wpos + len2) % hw->samples;
    }

    dsound_unlock_in (dscb, p1, p2, blen1, blen2);
    ds->last_read_pos = (ds->last_read_pos + decr) % ds->capture_buffer_size;
    return decr;
}

static void dsound_audio_fini (void *opaque)
{
    HRESULT hr;
    dsound *s = opaque;

    if (!s->dsound) {
        goto dsound_audio_fini_exit;
    }

    hr = IDirectSound_Release (s->dsound);
    if (FAILED (hr)) {
        dsound_logerr (hr, "Could not release DirectSound\n");
    }
    s->dsound = NULL;

    if (!s->dsound_capture) {
        goto dsound_audio_fini_exit;
    }

    hr = IDirectSoundCapture_Release (s->dsound_capture);
    if (FAILED (hr)) {
        dsound_logerr (hr, "Could not release DirectSoundCapture\n");
    }
    s->dsound_capture = NULL;

dsound_audio_fini_exit:
    CoUninitialize();
}

static void *dsound_audio_init (void)
{
    int err;
    HRESULT hr;
    dsound *s = &glob_dsound;
    RTUUID devguid;
    LPCGUID devguidp;

    hr = CoInitializeEx (NULL, COINIT_MULTITHREADED);
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not initialize COM\n");
#else
        LogRel(("DSound: Could not initialize COM\n"));
        dsound_log_hresult(hr);
#endif
        return NULL;
    }

    hr = CoCreateInstance (
        &CLSID_DirectSound,
        NULL,
        CLSCTX_ALL,
        &IID_IDirectSound,
        (void **) &s->dsound
        );
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not create DirectSound instance\n");
#else
        LogRel(("DSound: Could not create DirectSound instance\n"));
        dsound_log_hresult(hr);
#endif
        CoUninitialize();
        return NULL;
    }

    if (conf.device_guid_out) {
        hr = RTUuidFromStr(&devguid, conf.device_guid_out);
        if (FAILED (hr)) {
            LogRel(("DSound: Could not parse DirectSound output device GUID\n"));
        }
        devguidp = (LPCGUID)&devguid;
    } else {
        devguidp = NULL;
    }
    hr = IDirectSound_Initialize (s->dsound, devguidp);
    if (FAILED (hr)) {
#ifndef VBOX
        dsound_logerr (hr, "Could not initialize DirectSound\n");
#else
        LogRel(("DSound: Could not initialize DirectSound\n"));
        dsound_log_hresult(hr);
#endif

        hr = IDirectSound_Release (s->dsound);
        if (FAILED (hr)) {
            dsound_logerr (hr, "Could not release DirectSound\n");
        }
        s->dsound = NULL;
        CoUninitialize();
        return NULL;
    }

    if (conf.device_guid_in) {
        int rc = RTUuidFromStr(&devguid, conf.device_guid_in);
        if (RT_FAILURE(rc)) {
            LogRel(("DSound: Could not parse DirectSound input device GUID\n"));
        }
        s->devguidp = (LPCGUID)&devguid;
    } else {
        s->devguidp = NULL;
    }

    err = dsound_open (s);
    if (err) {
        dsound_audio_fini (s);
        return NULL;
    }

    return s;
}

static struct audio_option dsound_options[] = {
    {"LockRetries", AUD_OPT_INT, &conf.lock_retries,
     "Number of times to attempt locking the buffer", NULL, 0},
    {"RestoreRetries", AUD_OPT_INT, &conf.restore_retries,
     "Number of times to attempt restoring the buffer", NULL, 0},
    {"GetStatusRetries", AUD_OPT_INT, &conf.getstatus_retries,
     "Number of times to attempt getting status of the buffer", NULL, 0},
    {"SetPrimary", AUD_OPT_BOOL, &conf.set_primary,
     "Set the parameters of primary buffer", NULL, 0},
    {"LatencyMillis", AUD_OPT_INT, &conf.latency_millis,
     "(undocumented)", NULL, 0},
    {"PrimaryFreq", AUD_OPT_INT, &conf.settings.freq,
     "Primary buffer frequency", NULL, 0},
    {"PrimaryChannels", AUD_OPT_INT, &conf.settings.nchannels,
     "Primary buffer number of channels (1 - mono, 2 - stereo)", NULL, 0},
    {"PrimaryFmt", AUD_OPT_FMT, &conf.settings.fmt,
     "Primary buffer format", NULL, 0},
    {"BufsizeOut", AUD_OPT_INT, &conf.bufsize_out,
     "(undocumented)", NULL, 0},
    {"BufsizeIn", AUD_OPT_INT, &conf.bufsize_in,
     "(undocumented)", NULL, 0},
    {"DeviceGuidOut", AUD_OPT_STR, &conf.device_guid_out,
     "DirectSound output device GUID", NULL, 0},
    {"DeviceGuidIn", AUD_OPT_STR, &conf.device_guid_in,
     "DirectSound input device GUID", NULL, 0},
    {NULL, 0, NULL, NULL, NULL, 0}
};

static struct audio_pcm_ops dsound_pcm_ops = {
    dsound_init_out,
    dsound_fini_out,
    dsound_run_out,
    dsound_write,
    dsound_ctl_out,

    dsound_init_in,
    dsound_fini_in,
    dsound_run_in,
    dsound_read,
    dsound_ctl_in
};

struct audio_driver dsound_audio_driver = {
    INIT_FIELD (name           = ) "dsound",
    INIT_FIELD (descr          = )
    "DirectSound http://wikipedia.org/wiki/DirectSound",
    INIT_FIELD (options        = ) dsound_options,
    INIT_FIELD (init           = ) dsound_audio_init,
    INIT_FIELD (fini           = ) dsound_audio_fini,
    INIT_FIELD (pcm_ops        = ) &dsound_pcm_ops,
    INIT_FIELD (can_be_default = ) 1,
    INIT_FIELD (max_voices_out = ) INT_MAX,
    INIT_FIELD (max_voices_in  = ) 1,
    INIT_FIELD (voice_size_out = ) sizeof (DSoundVoiceOut),
    INIT_FIELD (voice_size_in  = ) sizeof (DSoundVoiceIn)
};
