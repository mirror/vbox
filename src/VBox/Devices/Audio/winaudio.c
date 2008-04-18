/** @file
 *
 * VBox audio device:
 * Windows audio driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <Windows.h>
#include <mmsystem.h>

#include <VBox/pdm.h>
#include <VBox/err.h>

#define LOG_GROUP LOG_GROUP_DEV_AC97
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include "Builtins.h"
#include "../../vl_vbox.h"

#include "audio.h"
#include "audio_int.h"

#define WINMM_BUFFER_SIZE       4096
#define WINMM_NUMBER_BUFFERS    32

typedef struct OSSVoiceOut {
    HWVoiceOut hw;
    void *pcm_buf;
    int fd;
    int nfrags;
    int fragsize;
    int old_optr;
    int default_bufsize;

    int        fStart;
    LPWAVEHDR  lpwh;
    int        cBuffers;
    int        idxBuffer;
    HWAVEOUT   hStream;
} OSSVoiceOut;

typedef struct OSSVoiceIn {
    HWVoiceIn hw;
    void *pcm_buf;
    int fd;
    int nfrags;
    int fragsize;
    int old_optr;

    WAVEHDR  wh;
    HWAVEIN  hStream;
} OSSVoiceIn;


static int winmm_write (SWVoiceOut *sw, void *buf, int len)
{
    Log(("winmm_write: %08x %x\n", buf, len));
    return pcm_sw_write (sw, buf, len);
}


static void winmm_run_out (HWVoiceOut *hw)
{
    OSSVoiceOut *oss = (OSSVoiceOut *) hw;
    int err, rpos, live, decr;
    int samples;
    uint8_t *dst;
    st_sample_t *src;
    MMTIME mmtime;

    live = pcm_hw_get_live (hw, NULL);
    Log(("winmm_run_out live=%x\n", live));
    if (live <= 0)
        return;

    mmtime.wType = TIME_SAMPLES;
    err = waveOutGetPosition(oss->hStream, &mmtime, sizeof(mmtime));
    if (err != MMSYSERR_NOERROR) {
        Log( ("WINMMAUD: waveOutGetPosition failed with %d\n", err));
        return;
    }
    if (mmtime.u.sample > 0 && mmtime.u.sample == oss->old_optr) {
        if (abs (hw->samples - live) < 64)
            Log( ("winmmaudio: overrun\n"));
        return;
    }

    samples = oss->default_bufsize >> hw->info.shift;

    decr = audio_MIN (samples, live);

    Log(("winmm_run_out: current pos %08X room left=%08X, decr=%08x\n", mmtime.u.sample, samples, decr));

    samples = decr;
    rpos = hw->rpos;
    while (samples)
    {
        int rc;
        int left_till_end_samples = hw->samples - rpos;
        int convert_samples = audio_MIN (samples, left_till_end_samples);

        Assert(oss->lpwh[oss->idxBuffer].dwFlags & WHDR_PREPARED);
        Assert( (oss->lpwh[oss->idxBuffer].dwFlags & WHDR_DONE)  ||
               !(oss->lpwh[oss->idxBuffer].dwFlags & WHDR_INQUEUE));

        if (!(oss->lpwh[oss->idxBuffer].dwFlags & WHDR_DONE) && (oss->lpwh[oss->idxBuffer].dwFlags & WHDR_INQUEUE))
        {
            Log(("winmm: buffer overrun -> current buffer=%d!!\n", oss->idxBuffer));
            break;
        }

        src = advance (hw->mix_buf, rpos * sizeof (st_sample_t));
        dst = oss->lpwh[oss->idxBuffer].lpData;

        Log(("winmm_run_out: buffer=%d dst=%08x src=%08x convert_samples %08X\n", oss->idxBuffer, dst, src, convert_samples));

        hw->clip (dst, src, convert_samples);
        sniffer_run_out (hw, dst, convert_samples);

        /* Update the size of the buffer */
        oss->lpwh[oss->idxBuffer].dwBufferLength = convert_samples << hw->info.shift;
        rc = waveOutWrite(oss->hStream, &oss->lpwh[oss->idxBuffer], sizeof(oss->lpwh[oss->idxBuffer]));
        if (rc != MMSYSERR_NOERROR)
        {
            Log( ("WINMMAUD: waveOutWrite failed with %d\n", rc));
            break;
        }

        memset (src, 0, convert_samples * sizeof (st_sample_t));

        rpos = (rpos + convert_samples) % hw->samples;
        samples -= convert_samples;

        oss->idxBuffer++;
        if (oss->idxBuffer >= oss->cBuffers)
            oss->idxBuffer = 0;
    }

    pcm_hw_dec_live (hw, decr);
    hw->rpos       = rpos;
    oss->old_optr  = mmtime.u.sample;
}

static void winmm_fini_out (HWVoiceOut *hw)
{
    MMRESULT err;
    OSSVoiceOut *oss = (OSSVoiceOut *) hw;

    Log( ("WINMMAUD: winmm_fini_out\n"));
    waveOutReset(oss->hStream);
    err = waveOutClose(oss->hStream);
    if (err != MMSYSERR_NOERROR) {
        Log( ("WINMMAUD: Failed to close OSS descriptor %d\n", err));
    }
    oss->fd = -1;

    if (oss->pcm_buf) {
        RTMemFree(oss->pcm_buf);
        oss->pcm_buf = NULL;
    }
}

#ifdef DEBUG
void CALLBACK winmmBufferDone(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    if (uMsg == WOM_DONE)
    {
        LPWAVEHDR lpwh = (LPWAVEHDR)dwParam1;
        OSSVoiceOut *oss = (OSSVoiceOut *)dwInstance;
        int bufidx;

        bufidx = (dwParam1 - (DWORD)oss->lpwh) / sizeof(WAVEHDR);
        Log(("winmm: WOM_DONE %08X %08X index=%d\n", lpwh->lpData, lpwh->dwBufferLength, bufidx));
    }
    return;
}

#endif

static int winmm_init_out (HWVoiceOut *hw, int freq, int nchannels, audfmt_e fmt)
{
    OSSVoiceOut *oss = (OSSVoiceOut *) hw;
    MMRESULT rc;
    WAVEFORMATEX waveInfo;
    int i;

    Log(("winmm_init_out %x freq=%d nchannels=%d\n", hw, freq, nchannels));

    waveInfo.cbSize          = sizeof(WAVEFORMATEX);
    waveInfo.nChannels       = nchannels;
    waveInfo.nSamplesPerSec  = freq;
    waveInfo.wFormatTag      = WAVE_FORMAT_PCM;

    switch (fmt)
    {
    case AUD_FMT_U8:
    case AUD_FMT_S8:
        waveInfo.wBitsPerSample = 8;
        break;
    case AUD_FMT_U16:
    case AUD_FMT_S16:
        waveInfo.wBitsPerSample = 16;
        break;
    default:
        AssertFailed();
        return -1;
    }
    waveInfo.nBlockAlign     = waveInfo.wBitsPerSample*waveInfo.nSamplesPerSec/8;
    waveInfo.nAvgBytesPerSec = waveInfo.wBitsPerSample*waveInfo.nSamplesPerSec;

#ifdef DEBUG
    rc = waveOutOpen(&oss->hStream, WAVE_MAPPER, &waveInfo, &winmmBufferDone, oss, CALLBACK_FUNCTION);
#else
    rc = waveOutOpen(&oss->hStream, WAVE_MAPPER, &waveInfo, 0, 0, CALLBACK_NULL);
#endif
    if (rc != MMSYSERR_NOERROR)
    {
        AssertMsgFailed(("waveOutOpen failed with %d\n", rc));
        return -1;
    }

    pcm_init_info (&hw->info, freq, nchannels, fmt);
    hw->bufsize = WINMM_NUMBER_BUFFERS * WINMM_BUFFER_SIZE;

    oss->pcm_buf = RTMemAllocZ (hw->bufsize);
    if (!oss->pcm_buf)
    {
        AssertFailed();
        rc = waveOutClose (oss->hStream);
        oss->hStream = (HWAVEOUT)-1;
        return -1;
    }

    Log(("PCM buffer %08X size %d\n", oss->pcm_buf, hw->bufsize));
    oss->cBuffers = hw->bufsize / WINMM_BUFFER_SIZE;
    oss->lpwh = RTMemAllocZ (oss->cBuffers * sizeof(*oss->lpwh));
    if (!oss->lpwh)
    {
        RTMemFree(oss->pcm_buf);
        AssertFailed();
        rc = waveOutClose (oss->hStream);
        oss->hStream = (HWAVEOUT)-1;
        return -1;
    }

    for (i=0;i<oss->cBuffers;i++)
    {
        oss->lpwh[i].lpData          = (char *)oss->pcm_buf + i*WINMM_BUFFER_SIZE;
        oss->lpwh[i].dwBufferLength  = WINMM_BUFFER_SIZE;

        rc = waveOutPrepareHeader(oss->hStream, &oss->lpwh[i], sizeof(oss->lpwh[i]));
        if (rc != MMSYSERR_NOERROR)
        {
            AssertMsgFailed(("waveOutPrepareHeader failed with %d\n", rc));
            return -1;
        }
    }

    oss->default_bufsize = WINMM_BUFFER_SIZE;

    waveOutSetVolume(oss->hStream, 0xffffffff);
    return 0;
}

static int winmm_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    OSSVoiceOut *oss = (OSSVoiceOut *) hw;
    MMRESULT rc;

    switch (cmd) {
    case VOICE_SETBUFSIZE:
    {
        int buflen, i;

        va_list ap;
        va_start (ap, cmd);
        buflen = va_arg (ap, int);
        va_end (ap);

        Assert(buflen);

        Log(("winmm_ctl_out: setbufsize to %x\n", buflen));

        if (buflen > oss->default_bufsize)
        {
            oss->default_bufsize = buflen;
            oss->cBuffers = hw->bufsize / buflen;

            for (i=0;i<oss->cBuffers;i++)
            {
                rc = waveOutUnprepareHeader(oss->hStream, &oss->lpwh[i], sizeof(oss->lpwh[i]));
                if (rc != MMSYSERR_NOERROR)
                {
                    AssertMsgFailed(("waveOutPrepareHeader failed with %d\n", rc));
                    return -1;
                }
            }

            for (i=0;i<oss->cBuffers;i++)
            {
                oss->lpwh[i].lpData          = (char *)oss->pcm_buf + i*oss->default_bufsize;
                oss->lpwh[i].dwBufferLength  = oss->default_bufsize;

                rc = waveOutPrepareHeader(oss->hStream, &oss->lpwh[i], sizeof(oss->lpwh[i]));
                if (rc != MMSYSERR_NOERROR)
                {
                    AssertMsgFailed(("waveOutPrepareHeader failed with %d\n", rc));
                    return -1;
                }
            }
        }
        break;
    }

    case VOICE_ENABLE:
    {
        Log( ("WINMMAUD: enabling voice\n"));
        oss->old_optr  = 0;
        oss->idxBuffer = 0;
        oss->fStart    = 1;
        pcm_info_clear (&hw->info, oss->pcm_buf, hw->samples);
        break;
    }

    case VOICE_DISABLE:
        Log( ("WINMMAUD: disabling voice\n"));
        rc = waveOutReset(oss->hStream);
        if (rc != MMSYSERR_NOERROR) {
            Log( ("WINMMAUD: waveOutPause failed with %d\n", rc));
            return -1;
        }
        break;
    }
    return 0;
}

static int winmm_init_in (HWVoiceIn *hw,
                        int freq, int nchannels, audfmt_e fmt)
{
    OSSVoiceIn *oss = (OSSVoiceIn *) hw;
    MMRESULT rc;
    WAVEFORMATEX waveInfo;

    return -1;

    waveInfo.cbSize          = sizeof(WAVEFORMATEX);
    waveInfo.nChannels       = nchannels;
    waveInfo.nSamplesPerSec  = freq;
    waveInfo.wFormatTag      = WAVE_FORMAT_PCM;

    switch (fmt)
    {
    case AUD_FMT_U8:
    case AUD_FMT_S8:
        waveInfo.wBitsPerSample = 8;
        break;
    case AUD_FMT_U16:
    case AUD_FMT_S16:
        waveInfo.wBitsPerSample = 16;
        break;
    default:
        AssertFailed();
        return -1;
    }
    waveInfo.nBlockAlign     = waveInfo.wBitsPerSample*waveInfo.nSamplesPerSec/8;
    waveInfo.nAvgBytesPerSec = waveInfo.wBitsPerSample*waveInfo.nSamplesPerSec;

    rc = waveInOpen(&oss->hStream, WAVE_MAPPER, &waveInfo, 0, 0, CALLBACK_NULL);
    if (rc != MMSYSERR_NOERROR)
    {
        Log(("waveInOpen failed with %d\n", rc));
        return -1;
    }

    pcm_init_info (&hw->info, freq, nchannels, fmt);
    hw->bufsize = waveInfo.nAvgBytesPerSec/2;

    oss->pcm_buf = RTMemAllocZ (hw->bufsize);
    if (!oss->pcm_buf) {
        rc = waveInClose(oss->hStream);
        oss->hStream = (HWAVEIN)-1;
        return -1;
    }

    return 0;
}

static void winmm_fini_in (HWVoiceIn *hw)
{
    MMRESULT err;
    OSSVoiceIn *oss = (OSSVoiceIn *) hw;
    err = waveInClose(oss->hStream);
    if (err) {
        Log( ("WINMMAUD: waveInClose failed with %d\n", err));
    }
    oss->fd = -1;
    if (oss->pcm_buf) {
        RTMemFree(oss->pcm_buf);
        oss->pcm_buf = NULL;
    }
}

static void winmm_run_in (HWVoiceIn *hw)
{
#if 0
    OSSVoiceIn *oss = (OSSVoiceIn *) hw;
    int hwshift = hw->info.shift;
    int i;
    int live = hw->total_samples_acquired - pcm_hw_find_min_samples_in (hw);
    int dead = hw->samples - live;
    size_t read_samples = 0;
    struct {
        int add;
        int len;
    } bufs[2] = {
        { hw->wpos },
        { 0, 0 }
    };

    if (!dead) {
        Log( ("WINMMAUD: empty tot=%d min=%d\n",
                hw->total_samples_acquired, pcm_hw_find_min_samples_in (hw)));
        return;
    }

    if (hw->wpos + dead > hw->samples) {
        bufs[0].len = (hw->samples - hw->wpos) << hwshift;
        bufs[1].len = (dead - (hw->samples - hw->wpos)) << hwshift;
    }
    else {
        bufs[0].len = dead << hwshift;
    }


    for (i = 0; i < 2; ++i) {
        ssize_t nread;

        if (bufs[i].len) {
            void *p = advance (oss->pcm_buf, bufs[i].add << hwshift);
            nread = read (oss->fd, p, bufs[i].len);

            if (nread > 0) {
                if (nread & hw->info.align) {
                    Log( ("WINMMAUD: Unaligned read %d\n", nread));
                }
                read_samples += nread >> hwshift;
                hw->conv (hw->conv_buf + bufs[i].add, p, nread >> hwshift,
                          &nominal_volume);
            }

            if (bufs[i].len - nread) {
                if (nread == -1) {
                    switch (errno) {
                    case EINTR:
                    case EAGAIN:
                        break;
                    default:
                        Log( ("WINMMAUD: Failed to read %d bytes (to %p): %s\n",
                               bufs[i].len, p, errstr ()));
                    }
                }
                break;
            }
        }
    }
    hw->total_samples_acquired += read_samples;
    hw->wpos = (hw->wpos + read_samples) % hw->samples;
#endif
}

static int winmm_read (SWVoiceIn *sw, void *buf, int size)
{
    return pcm_sw_read (sw, buf, size);
}

static int winmm_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    (void) hw;
    (void) cmd;
    return 0;
}

#if 0
static int winmm_read_recsrc (int fd)
{
    int recsrc;
    int err = ioctl (fd, SOUND_MIXER_READ_RECSRC, &recsrc);
    if (err) {
        Log( ("WINMMAUD: Failed to read record source mask\nReason: %s\n",
               errstr ()));
        return -1;
    }
    return recsrc;
}

static int winmm_write_recsrc (int fd, int *recsrc)
{
    int err = ioctl (fd, SOUND_MIXER_READ_RECSRC, &recsrc);
    if (err) {
        Log( ("WINMMAUD: Failed to write record source mask\nReason: %s\n",
               errstr ()));
        return -1;
    }
    return 0;
}

static const char *winmm_mixer_names[] = SOUND_DEVICE_NAMES;

static int winmm_read_volume (int fd, int ctl)
{
    int vol;
    MMRESULT err;
    DWORD dwVolume;

    err = waveOutGetVolume(oss->hStream, &dwVolume);
    if (err) {
        Log( ("WINMMAUD: Failed to read %s volume\nReason: %s\n",
               winmm_mixer_names[ctl], errstr ()));
        return -1;
    }
    return (int) (((dwVolume & 0xFFFF) * 100) / 0xFFFF);
}

static int winmm_write_volume (int fd, int ctl, int *vol)
{
    DWORD dwVolume;

    dwVolume = (vol * 0xFFFF / 100);
    dwVolume = dwVolume | (dwVolume << 16);

    err = waveOutSetVolume(oss->hStream, dwVolume);
    if (err) {
        Log( ("WINMMAUD: Failed to write %s volume\nReason: %s\n",
               winmm_mixer_names[ctl], errstr ()));
        return -1;
    }
    return 0;
}
#endif

static void *winmm_audio_init (void)
{
#if 0
    int fd, err, i;

    fd = open (conf.mixer_name, O_RDWR);
    if (fd < 0) {
        Log( ("WINMMAUD: Failed to open mixer `%s'\nReason: %s\n",
               conf.mixer_name, errstr ()));
        goto ret;
    }

    conf.recsrc = winmm_read_recsrc (fd);

    if (audio_state.allow_mixer_access) {
        for (i = 0; i < sizeof (conf.vol) / sizeof (conf.vol[0]); ++i) {
            conf.vol[0].vol = winmm_read_volume (fd, conf.vol[i].ctl);
        }
    }

    err = close (fd);
    if (err) {
        Log( ("WINMMAUD: Failed to close mixer device\nReason: %s\n", errstr ()));
    }
 ret:
    return &conf;
#endif
    return (void *)1;
}

static void winmm_audio_fini (void *opaque)
{
#if 0
    int fd, err, temp, i;

    fd = open (conf.mixer_name, O_RDWR);
    if (fd < 0) {
        Log( ("WINMMAUD: Failed to open mixer `%s'\nReason: %s\n",
               conf.mixer_name, errstr ()));
        return;
    }

    if (conf.recsrc != -1) {
        temp = conf.recsrc;
        winmm_write_recsrc (fd, &temp);
    }

    for (i = 0; i < sizeof (conf.vol) / sizeof (conf.vol[0]); ++i) {
        temp = conf.vol[i].vol;
        if (temp != -1)
            winmm_write_volume (fd, conf.vol[i].ctl, &temp);
    }

    err = close (fd);
    if (err) {
        Log( ("WINMMAUD: Failed to close mixer device\nReason: %s\n", errstr ()));
    }
#endif
}

#if 0
static int aud_to_winmm_record_source (audrecsource_t s)
{
    switch (s) {
    case AUD_REC_MIC: return SOUND_MASK_MIC;
    case AUD_REC_CD: return SOUND_MASK_CD;
    case AUD_REC_VIDEO: return SOUND_MASK_VIDEO;
    case AUD_REC_AUX: return SOUND_MASK_LINE1; /* ??? */
    case AUD_REC_LINE_IN: return SOUND_MASK_LINE;
    case AUD_REC_PHONE: return SOUND_MASK_PHONEIN;
    default:
        Log( ("WINMMAUD: Unknown recording source %d using MIC\n", s));
        return SOUND_MIXER_MIC;
    }
}

static int winmm_to_aud_record_source (int s)
{
    switch (s) {
    case SOUND_MASK_MIC: return AUD_REC_MIC;
    case SOUND_MASK_CD: return AUD_REC_CD;
    case SOUND_MASK_VIDEO: return AUD_REC_VIDEO;
    case SOUND_MASK_LINE1: return AUD_REC_AUX;
    case SOUND_MASK_LINE: return AUD_REC_LINE_IN;
    case SOUND_MASK_PHONEIN: return AUD_REC_PHONE;
    default:
        Log( ("WINMMAUD: Unknown OSS recording source %d using MIC\n", s));
        return AUD_REC_MIC;
    }
}
#endif

#if 0
static int winmm_set_record_source (audrecsource_t *lrs, audrecsource_t *rrs)
{
#if 0
    int source, fd, err;
    int ret = -1;

    source = aud_to_winmm_record_source (*lrs);
    if (source == -1) {
        Log( ("WINMMAUD: Unsupported recording source %s\n",
               AUD_record_source_name (*lrs)));
        return -1;
    }

    fd = open (conf.mixer_name, O_RDWR);

    if (fd == -1) {
        Log( ("WINMMAUD: Failed to open mixer device\nReason: %s\n", errstr ()));
        return -1;
    }

    err = ioctl (fd, SOUND_MIXER_WRITE_RECSRC, &source);
    if (err) {
        Log( ("WINMMAUD: Failed to set recording source to %s\nReason: %s\n",
               AUD_record_source_name (*lrs), errstr ()));
        goto err;
    }

    *rrs = *lrs = winmm_to_aud_record_source (source);
    ret = 0;

 err:
    if (close (fd)) {
        Log( ("WINMMAUD: Failed to close mixer device\nReason: %s\n",
               errstr ())));
    }
    return ret;
#else
    return 0;
#endif
}

static int winmm_set_volume (audmixerctl_t m, int *mute,
                           uint8_t *lvol, uint8_t *rvol)
{
    int vol;
    int winmm_mixerctl, err;
    int fd = open (conf.mixer_name, O_RDWR);
    int ret = -1;

    if (fd == -1) {
        Log( ("WINMMAUD: Failed to open mixer device\nReason: %s\n", errstr ()));
        return -1;
    }

    vol = *mute ? 0 : ((*lvol * 100) / 256) | (((*rvol * 100) / 256) << 8);

    switch (m) {
    case AUD_MIXER_VOLUME:
        winmm_mixerctl = SOUND_MIXER_VOLUME;
        break;
    case AUD_MIXER_PCM:
        winmm_mixerctl = SOUND_MIXER_PCM;
        break;
    case AUD_MIXER_LINE_IN:
        winmm_mixerctl = SOUND_MIXER_LINE;
        break;
    default:
        Log( ("WINMMAUD: Unknown mixer control %d\n", m));
        return -1;
    }

    err = ioctl (fd, MIXER_WRITE (winmm_mixerctl), &vol);
    if (err) {
        Log( ("WINMMAUD: Failed to update mixer\nReason: %s\n", errstr ()));
        goto err;
    }

    if (!*mute) {
        *lvol = ((vol & 0xff) * 100) / 256;
        *rvol = ((vol >> 8) * 100) / 256;
    }
    ret = 0;

 err:
    if (close (fd)) {
        Log( ("WINMMAUD: Failed to close mixer device\nReason: %s\n",
               errstr ()));
    }
    return ret;
}
#endif

static int winmm_audio_ctl (void *opaque, int cmd, ...)
{
    int ret = 0;
#if 0
    switch (cmd) {
    case SET_RECORD_SOURCE:
        {
            va_list ap;
            va_start (ap, cmd);
            audrecsource_t *lrs = va_arg (ap, audrecsource_t *);
            audrecsource_t *rrs = va_arg (ap, audrecsource_t *);
            va_end (ap);
            ret = winmm_set_record_source (lrs, rrs);
        }
        break;
    case SET_VOLUME:
        {
            va_list ap;
            va_start (ap, cmd);
            audmixerctl_t m = va_arg (ap, audmixerctl_t);
            int *mute = va_arg (ap, int *);
            uint8_t *lvol = va_arg (ap, uint8_t *);
            uint8_t *rvol = va_arg (ap, uint8_t *);
            va_end (ap);
            ret = winmm_set_volume (m, mute, lvol, rvol);
        }
        break;
    default:
        ret = -1;
        break;
    }
#endif
    return ret;
}

static struct pcm_ops winmm_pcm_ops = {
    winmm_init_out,
    winmm_fini_out,
    winmm_run_out,
    winmm_write,
    winmm_ctl_out,

    winmm_init_in,
    winmm_fini_in,
    winmm_run_in,
    winmm_read,
    winmm_ctl_in,
};

struct audio_driver winmm_audio_driver = {
    "WINMM",
    winmm_audio_init,
    winmm_audio_fini,
    winmm_audio_ctl,
    &winmm_pcm_ops,
    1,
    INT_MAX,
    INT_MAX,
    sizeof (OSSVoiceOut),
    sizeof (OSSVoiceIn)
};
