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
    int bufsize_in;
    int bufsize_out;
    int latency_millis;
    char *device_guid_out;
    char *device_guid_in;
} conf = {
    1,
    1,
    1,
    16384,
    16384,
    10,
    NULL,
    NULL
};

typedef struct {
    RTUUID devguid_play;
    LPCGUID devguidp_play;
    RTUUID devguid_capture;
    LPCGUID devguidp_capture;
} dsound;

static dsound glob_dsound;

typedef struct {
    HWVoiceOut hw;
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER dsound_buffer;
    DWORD old_pos;
    int first_time;
    int playback_buffer_size;
    audsettings_t as;
} DSoundVoiceOut;

typedef struct {
    HWVoiceIn hw;
    int last_read_pos;
    int capture_buffer_size;
    LPDIRECTSOUNDCAPTURE dsound_capture;
    LPDIRECTSOUNDCAPTUREBUFFER dsound_capture_buffer;
    audsettings_t as;
    HRESULT hr_last_run_in;
} DSoundVoiceIn;

static void dsound_clear_sample (DSoundVoiceOut *ds);

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
            DSLOGREL(("DSound: restore playback buffer %Rhrc\n", hr));
            return -1;
        }
    }

    DSLOGF(("DSound: %d attempts to restore playback buffer failed\n", i));
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

/*
 * DirectSound playback
 */

static void dsoundPlayInterfaceRelease (DSoundVoiceOut *ds)
{
    if (ds->dsound) {
        IDirectSound_Release (ds->dsound);
        ds->dsound = NULL;
    }
}

static int dsoundPlayInterfaceCreate (DSoundVoiceOut *ds)
{
    dsound *s = &glob_dsound;

    HRESULT hr;

    if (ds->dsound != NULL) {
        DSLOGF(("DSound: DirectSound instance already exists\n"));
        return 0;
    }

    hr = CoCreateInstance (&CLSID_DirectSound, NULL, CLSCTX_ALL,
                           &IID_IDirectSound, (void **) &ds->dsound);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: DirectSound create instance %Rhrc\n", hr));
    }
    else {
        hr = IDirectSound_Initialize (ds->dsound, s->devguidp_play);
        if (SUCCEEDED(hr)) {
            HWND hwnd = GetDesktopWindow ();
            hr = IDirectSound_SetCooperativeLevel (ds->dsound, hwnd, DSSCL_PRIORITY);
            if (FAILED (hr)) {
                DSLOGREL(("DSound: set cooperative level for window %p %Rhrc\n", hwnd, hr));
            }
        }
        if (FAILED (hr)) {
            if (hr == DSERR_NODRIVER) {
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            }
            else {
                DSLOGREL(("DSound: DirectSound initialize %Rhrc\n", hr));
            }
            dsoundPlayInterfaceRelease (ds);
        }
    }

    return SUCCEEDED (hr)? 0: -1;
}

static void dsoundPlayClose (DSoundVoiceOut *ds)
{
    dsound *s = &glob_dsound;

    HRESULT hr;

    DSLOGF(("DSound: playback close %p buffer %p\n", ds, ds->dsound_buffer));

    if (ds->dsound_buffer) {
        hr = IDirectSoundBuffer_Stop (ds->dsound_buffer);
        if (FAILED (hr)) {
            DSLOGREL(("DSound: playback close Stop %Rhrc\n", hr));
        }

        IDirectSoundBuffer_Release (ds->dsound_buffer);
        ds->dsound_buffer = NULL;
    }

    dsoundPlayInterfaceRelease (ds);
}

static int dsoundPlayOpen (DSoundVoiceOut *ds)
{
    dsound *s = &glob_dsound;

    int err;
    HRESULT hr;
    WAVEFORMATEX wfx;
    DSBUFFERDESC bd;
    DSBCAPS bc;

    DSLOGF(("DSound: playback open %p size %d samples, freq %d, chan %d, bits %d, sign %d\n",
            ds,
            ds->hw.samples,
            ds->hw.info.freq,
            ds->hw.info.nchannels,
            ds->hw.info.bits,
            ds->hw.info.sign));

    if (ds->dsound_buffer != NULL) {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundBuffer already exists\n"));
        dsoundPlayClose (ds);
    }

    err = waveformat_from_audio_settings (&wfx, &ds->as);
    if (err) {
        return err;
    }

    err = dsoundPlayInterfaceCreate (ds);
    if (err) {
        return err;
    }

    memset (&bd, 0, sizeof (bd));
    bd.dwSize = sizeof (bd);
    bd.lpwfxFormat = &wfx;
    bd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
    bd.dwBufferBytes = ds->hw.samples << ds->hw.info.shift;
    hr = IDirectSound_CreateSoundBuffer (ds->dsound,
                                         &bd, &ds->dsound_buffer, NULL);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: playback CreateSoundBuffer %Rhrc\n", hr));
        goto fail0;
    }

    /* Query the actual parameters. */

    hr = IDirectSoundBuffer_GetFormat (ds->dsound_buffer, &wfx, sizeof (wfx), NULL);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: playback GetFormat %Rhrc\n", hr));
        goto fail0;
    }

    memset (&bc, 0, sizeof (bc));
    bc.dwSize = sizeof (bc);
    hr = IDirectSoundBuffer_GetCaps (ds->dsound_buffer, &bc);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: playback GetCaps %Rhrc\n", hr));
        goto fail0;
    }

    DSLOGF(("DSound: playback format: size %d bytes\n"
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
        DSLOGREL(("DSound: playback GetCaps returned misaligned buffer size %ld, alignment %d\n",
                  bc.dwBufferBytes, ds->hw.info.align + 1));
    }

    if (ds->hw.samples != 0 && ds->hw.samples != (bc.dwBufferBytes >> ds->hw.info.shift)) {
        DSLOGREL(("DSound: playback buffer size mismatch dsound %d, hw %d bytes\n",
                  bc.dwBufferBytes, ds->hw.samples << ds->hw.info.shift));
    }

    /* Initial state.
     * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
     * playback buffer position.
     */
    ds->playback_buffer_size = bc.dwBufferBytes >> ds->hw.info.shift;
    DSLOGF(("DSound: playback open playback_buffer_size %d\n", ds->playback_buffer_size));

    return 0;

 fail0:
    dsoundPlayClose (ds);
    return -1;
}

static int dsoundPlayGetStatus (DSoundVoiceOut *ds, DWORD *statusp)
{
    HRESULT hr;
    DWORD status;
    int i;

    if (ds->dsound_buffer != NULL) {
        for (i = 0; i < RT_MAX(conf.getstatus_retries, 1); ++i) {
            hr = IDirectSoundBuffer_GetStatus (ds->dsound_buffer, &status);
            if (FAILED (hr)) {
                DSLOGF(("DSound: playback start GetStatus %Rhrc\n", hr));
                break;
            }

            if ((status & DSBSTATUS_BUFFERLOST) == 0) {
                break;
            }

            if (dsound_restore_out (ds->dsound_buffer)) {
                hr = E_FAIL;
                break;
            }
        }
    }
    else
    {
        hr = E_FAIL;
    }

    if (SUCCEEDED (hr)) {
        *statusp = status;
        return 0;
    }
    return -1;
}

static void dsoundPlayStop (DSoundVoiceOut *ds)
{
    HRESULT hr;
    DWORD status;

    if (ds->dsound_buffer != NULL) {
        /* This performs some restore, so call it anyway and ignore result. */
        dsoundPlayGetStatus (ds, &status);

        hr = IDirectSoundBuffer_Stop (ds->dsound_buffer);
        if (FAILED (hr)) {
            DSLOGF(("DSound: stop playback buffer %Rhrc\n", hr));
        }
    }
}

static int dsoundPlayStart (DSoundVoiceOut *ds)
{
    HRESULT hr;
    DWORD status;

    if (ds->dsound_buffer != NULL) {
        if (dsoundPlayGetStatus (ds, &status)) {
            DSLOGF(("DSound: playback start GetStatus failed\n"));
            hr = E_FAIL;
        }
        else {
            if (status & DSBSTATUS_PLAYING) {
                DSLOGF(("DSound: already playing\n"));
            }
            else {
                dsound_clear_sample (ds);

                /* Reinit the playback buffer position. */
                ds->first_time = 1;

                DSLOGF(("DSound: playback start\n"));

                hr = IDirectSoundBuffer_Play (ds->dsound_buffer, 0, 0, DSBPLAY_LOOPING);
                if (FAILED (hr)) {
                    DSLOGREL(("DSound: playback start %Rhrc\n", hr));
                }
            }
        }
    }
    else {
        hr = E_FAIL;
    }

    return SUCCEEDED (hr)? 0: -1;
}

/*
 * DirectSoundCapture
 */

static void dsoundCaptureInterfaceRelease (DSoundVoiceIn *ds)
{
    if (ds->dsound_capture) {
        IDirectSoundCapture_Release (ds->dsound_capture);
        ds->dsound_capture = NULL;
    }
}

static int dsoundCaptureInterfaceCreate (DSoundVoiceIn *ds)
{
    dsound *s = &glob_dsound;

    HRESULT hr;

    if (ds->dsound_capture != NULL) {
        DSLOGF(("DSound: DirectSoundCapture instance already exists\n"));
        return 0;
    }

    hr = CoCreateInstance (&CLSID_DirectSoundCapture, NULL, CLSCTX_ALL,
                           &IID_IDirectSoundCapture, (void **) &ds->dsound_capture);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: DirectSoundCapture create instance %Rhrc\n", hr));
    }
    else {
        hr = IDirectSoundCapture_Initialize (ds->dsound_capture, s->devguidp_capture);
        if (FAILED (hr)) {
            if (hr == DSERR_NODRIVER) {
                DSLOGREL(("DSound: DirectSound capture is currently unavailable\n"));
            }
            else {
                DSLOGREL(("DSound: DirectSoundCapture initialize %Rhrc\n", hr));
            }
            dsoundCaptureInterfaceRelease (ds);
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

        IDirectSoundCaptureBuffer_Release (ds->dsound_capture_buffer);
        ds->dsound_capture_buffer = NULL;
    }

    dsoundCaptureInterfaceRelease (ds);
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

    err = dsoundCaptureInterfaceCreate (ds);
    if (err) {
        return err;
    }

    memset (&bd, 0, sizeof (bd));
    bd.dwSize = sizeof (bd);
    bd.lpwfxFormat = &wfx;
    bd.dwBufferBytes = ds->hw.samples << ds->hw.info.shift;
    hr = IDirectSoundCapture_CreateCaptureBuffer (ds->dsound_capture,
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
    DSLOGF(("DSound: capture open last_read_pos %d, capture_buffer_size %d\n", ds->last_read_pos, ds->capture_buffer_size));

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
                /** @todo Fill the capture beffer with silence here. */

                DSLOGF(("DSound: capture start\n"));
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
    }

    if (src_len2) {
        dst = advance (dst, src_len1 << hw->info.shift);
        hw->clip (dst, src2, src_len2);
    }

    hw->rpos = pos % hw->samples;
}

static void dsound_clear_sample (DSoundVoiceOut *ds)
{
    int err;
    LPVOID p1, p2;
    DWORD blen1, blen2, len1, len2;

    err = dsound_lock_out (
        ds->dsound_buffer,
        &ds->hw.info,
        0,
        ds->playback_buffer_size << ds->hw.info.shift,
        &p1, &p2,
        &blen1, &blen2,
        1
        );
    if (err) {
        return;
    }

    len1 = blen1 >> ds->hw.info.shift;
    len2 = blen2 >> ds->hw.info.shift;

    if (p1 && len1) {
        audio_pcm_info_clear_buf (&ds->hw.info, p1, len1);
    }

    if (p2 && len2) {
        audio_pcm_info_clear_buf (&ds->hw.info, p2, len2);
    }

    dsound_unlock_out (ds->dsound_buffer, p1, p2, blen1, blen2);
}

static int dsound_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    DSoundVoiceOut *ds = (DSoundVoiceOut *) hw;

    switch (cmd) {
    case VOICE_ENABLE:
        /* Try to start playback. If it fails, then reopen and try again. */
        if (dsoundPlayStart (ds)) {
            dsoundPlayClose (ds);
            dsoundPlayOpen (ds);

            if (dsoundPlayStart (ds)) {
                return -1;
            }
        }
        break;

    case VOICE_DISABLE:
        dsoundPlayStop (ds);
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
        DSLOGF(("DSound: run_out no playback buffer\n"));
        return 0;
    }

    hwshift = hw->info.shift;
    bufsize = ds->playback_buffer_size << hwshift;

    live = audio_pcm_hw_get_live_out (hw);

    hr = IDirectSoundBuffer_GetCurrentPosition (
        dsb,
        &ppos,
        &wpos
        );
    if (hr == DSERR_BUFFERLOST) {
        if (dsound_restore_out(dsb))
            return 0;
        hr = IDirectSoundBuffer_GetCurrentPosition(dsb, &ppos, &wpos);
        if (hr == DSERR_BUFFERLOST)
            return 0;   // Avoid log flooding if the error is still there.
    }
    if (FAILED (hr)) {
        DSLOGF(("DSound: get playback buffer position %Rhrc\n", hr));
        return 0;
    }

    len = live << hwshift;

    if (ds->first_time) {
        if (conf.latency_millis) {
            DWORD cur_blat;
            DWORD conf_blat;

            conf_blat = millis_to_bytes (&hw->info, conf.latency_millis);
            cur_blat = audio_ring_dist (wpos, ppos, bufsize);
            old_pos = wpos;
            if (conf_blat > cur_blat) /* Do not write before wpos. */
               old_pos += conf_blat - cur_blat;
            old_pos %= bufsize;
            old_pos &= ~hw->info.align;
        }
        else {
            old_pos = wpos;
        }
        ds->first_time = 0;
    }
    else {
        if (ds->old_pos == ppos) {
            /* Full buffer. */
            return 0;
        }

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
        DSLOGF(("DSound: error len=%d bufsize=%d old_pos=%ld ppos=%ld\n",
               len, bufsize, old_pos, ppos));
        return 0;
    }

    len &= ~hw->info.align;
    if (!len) {
        return 0;
    }

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

static int dsoundIsAvailable (void)
{
    LPDIRECTSOUND dsound;
    HRESULT hr = CoCreateInstance (&CLSID_DirectSound, NULL, CLSCTX_ALL,
                                   &IID_IDirectSound, (void **) &dsound);
    if (SUCCEEDED(hr)) {
        IDirectSound_Release (dsound);
        return 1;
    }

    DSLOGREL(("DSound: is unavailable %Rhrc\n", hr));
    return 0;
}

static void dsound_audio_fini (void *opaque)
{
    dsound *s = opaque;
    NOREF(s);
    CoUninitialize();
}

static void *dsound_audio_init (void)
{
    HRESULT hr;
    dsound *s = &glob_dsound;

    hr = CoInitializeEx (NULL, COINIT_MULTITHREADED);
    if (FAILED (hr)) {
        DSLOGREL(("DSound: COM initialize %Rhrc\n", hr));
        return NULL;
    }

    if (conf.device_guid_out) {
        int rc = RTUuidFromStr(&s->devguid_play, conf.device_guid_out);
        if (FAILED (rc)) {
            LogRel(("DSound: Could not parse DirectSound output device GUID\n"));
        }
        s->devguidp_play = (LPCGUID)&s->devguid_play;
    } else {
        s->devguidp_play = NULL;
    }

    if (conf.device_guid_in) {
        int rc = RTUuidFromStr(&s->devguid_capture, conf.device_guid_in);
        if (RT_FAILURE(rc)) {
            LogRel(("DSound: Could not parse DirectSound input device GUID\n"));
        }
        s->devguidp_capture = (LPCGUID)&s->devguid_capture;
    } else {
        s->devguidp_capture = NULL;
    }

    /* Check that DSound interface is available. */
    if (dsoundIsAvailable ())
        return s;

    dsound_audio_fini (s);
    return NULL;
}

static struct audio_option dsound_options[] = {
    {"LockRetries", AUD_OPT_INT, &conf.lock_retries,
     "Number of times to attempt locking the buffer", NULL, 0},
    {"RestoreRetries", AUD_OPT_INT, &conf.restore_retries,
     "Number of times to attempt restoring the buffer", NULL, 0},
    {"GetStatusRetries", AUD_OPT_INT, &conf.getstatus_retries,
     "Number of times to attempt getting status of the buffer", NULL, 0},
    {"LatencyMillis", AUD_OPT_INT, &conf.latency_millis,
     "(undocumented)", NULL, 0},
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
