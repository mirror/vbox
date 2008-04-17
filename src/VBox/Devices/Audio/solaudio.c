/* $Id$ */
/** @file
 * VirtualBox Audio Driver - Solaris host.
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
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#include <fcntl.h>
#include <sys/audio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mixer.h>

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#define LOG_ENABLED
#include <VBox/log.h>
#include <iprt/env.h>

#include "Builtins.h"
#include "vl_vbox.h"
#include "audio.h"
#include <iprt/alloc.h>

#define AUDIO_CAP "solaudio"
#include "audio_int.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct solaudioVoiceOut
{
    HWVoiceOut    Hw;
    audio_info_t  AudioInfo;
    uint_t        cBuffersPlayed;
    void         *pPCMBuf;
} solaudioVoiceOut;

typedef struct solaudioVoiceIn
{
    HWVoiceIn     Hw;
    audio_info_t  AudioInfo;
    void         *pPCMBuf;
} solaudioVoiceIn;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static struct
{
    int cbPlayBuffer;
    int cbRecordBuffer;
} conf =
{
    INIT_FIELD (cbPlayBuffer =) 4352,
    INIT_FIELD (cbRecordBuffer = ) 8192
};

static int   g_AudioDev = -1;
static int   g_RecordDev = -1;
static int   g_AudioCtl = -1;
static char *g_pszAudioDev = NULL;
static char *g_pszAudioCtl = NULL;

typedef enum
{
    enmPlay          = 6,
    enmRecord        = 9,
    enmRecordPassive = 15
} audio_dest_t;


static int aud_to_solfmt (audfmt_e fmt)
{
    switch (fmt)
    {
        case AUD_FMT_S8:
        case AUD_FMT_U8:
            return AUDIO_PRECISION_8;

        case AUD_FMT_S16:
        case AUD_FMT_U16:
            return AUDIO_PRECISION_16;

        default:
            LogRel(("solaudio: aud_to_solfmt: Bad audio format %d\n", fmt));
            return AUDIO_PRECISION_8;
    }
}


static int sol_to_audfmt (int fmt, int encoding)
{
    switch (fmt)
    {
        case AUDIO_PRECISION_8:
        {
            if (encoding == AUDIO_ENCODING_LINEAR8)
                return AUD_FMT_U8;
            else
                return AUD_FMT_S8;
            break;
        }

        case AUDIO_PRECISION_16:
        {
            if (encoding == AUDIO_ENCODING_LINEAR)
                return AUD_FMT_S16;
            else
                return AUD_FMT_U16;
            break;
        }

        default:
            LogRel(("solaudio: sol_to_audfmt: Bad audio format %d\n", fmt));
            return AUD_FMT_S8;
    }
}


static char *solaudio_getdevice (void)
{
    /*
     * This is for multiple audio devices where env. var determines current one,
     * otherwise else we fallback to default.
     */
    const char *pszAudioDev = RTEnvGet("AUDIODEV");
    if (pszAudioDev)
        return RTStrDup(pszAudioDev);

    return RTStrDup("/dev/audio");
}


static void solaudio_close_device (audio_dest_t dst)
{
    LogFlow(("solaudio: solaudio_close_device\n"));
    switch (dst)
    {
        case enmPlay:
        {
            close(g_AudioDev);
            g_AudioDev = -1;
            break;
        }

        case enmRecord:
        case enmRecordPassive:
        {
            close(g_RecordDev);
            g_RecordDev = -1;
            break;
        }

        default:
            LogRel(("solaudio: cannot close. invalid audio destination %d.\n", dst));
    }
}


static int solaudio_open_device (audio_dest_t dst)
{
    int rc = 0;

    LogFlow(("solaudio: solaudio_open_device dest=%d\n", dst));

    switch (dst)
    {
        case enmPlay:
        {
            LogFlow(("solaudio: open_device for enmPlay\n"));
            g_AudioDev = open(g_pszAudioDev, O_WRONLY | O_NONBLOCK);
            if (g_AudioDev < 0)
            {
                LogRel(("solaudio: failed to open device %s dst=%d\n", g_pszAudioDev, dst));
                rc = -1;
            }
            break;
        }

        case enmRecord:
        case enmRecordPassive:
        {
            LogFlow(("solaudio: open_device for enmRecord\n"));
            g_RecordDev = open(g_pszAudioDev, (dst == enmRecord ? O_RDONLY : O_WRONLY) | O_NONBLOCK);
            if (g_RecordDev < 0)
            {
                LogRel(("solaudio: failed to open device %s dst=%d\n", g_pszAudioDev, dst));
                rc = -1;
            }
            break;
        }

        default:
            LogRel(("solaudio: Invalid audio destination.\n"));
            break;
    }
    return rc;
}


static int solaudio_setattrs(audio_dest_t dst, audio_info_t *info)
{
    audio_info_t AudioInfo;
    audio_prinfo_t *pDstInfo;
    audio_prinfo_t *pSrcInfo;

    LogFlow(("solaudio: solaudio_setattrs dst=%d info=%p\n", dst, info));

    if (!info)
        return -1;

    AUDIO_INITINFO(&AudioInfo);
    if (ioctl(dst == enmPlay ? g_AudioDev : g_RecordDev, AUDIO_GETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_GETINFO failed\n"));
        return -1;
    }

    if (dst == enmPlay)
    {
        pDstInfo = &AudioInfo.play;
        pSrcInfo = &info->play;
    }
    else
    {
        pDstInfo = &AudioInfo.record;
        pSrcInfo = &info->record;
    }

    pDstInfo->sample_rate = pSrcInfo->sample_rate;
    pDstInfo->channels = pSrcInfo->channels;
    pDstInfo->precision = pSrcInfo->precision;
    pDstInfo->encoding = pSrcInfo->encoding;
    pDstInfo->buffer_size = pSrcInfo->buffer_size;
    pDstInfo->gain = AUDIO_MAX_GAIN;
    pDstInfo->open = 0;

    if (ioctl(dst == enmPlay ? g_AudioDev : g_RecordDev, AUDIO_SETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed\n"));
        return -1;
    }
    return 0;
}


static int solaudio_init_out (HWVoiceOut *hw, audsettings_t *as)
{
    solaudioVoiceOut *pSol = (solaudioVoiceOut *)hw;
    audsettings_t ObtAudioInfo;

    AUDIO_INITINFO(&pSol->AudioInfo);
    pSol->AudioInfo.play.sample_rate = as->freq;
    pSol->AudioInfo.play.channels = as->nchannels;
    pSol->AudioInfo.play.precision = aud_to_solfmt(as->fmt);
    pSol->AudioInfo.play.buffer_size = conf.cbPlayBuffer;

    if (as->fmt == AUD_FMT_U8)
        pSol->AudioInfo.play.encoding = AUDIO_ENCODING_LINEAR8;
    else
        pSol->AudioInfo.play.encoding = AUDIO_ENCODING_LINEAR;

    /* Open device for playback. */
    if (solaudio_open_device(enmPlay))
    {
        LogRel(("solaudio: solaudio_open failed\n"));
        return -1;
    }

    /* Specify playback attributes to device. */
    if (solaudio_setattrs(enmPlay, &pSol->AudioInfo))
    {
        LogRel(("solaudio: failed to set playback attributes.\n"));
        return -1;
    }

    /* Copy obtained playback attributes. */
    ObtAudioInfo.freq = pSol->AudioInfo.play.sample_rate;
    ObtAudioInfo.nchannels = pSol->AudioInfo.play.channels;
    ObtAudioInfo.fmt = sol_to_audfmt(pSol->AudioInfo.play.precision, pSol->AudioInfo.play.encoding);
    ObtAudioInfo.endianness = as->endianness;

    audio_pcm_init_info(&hw->info, &ObtAudioInfo);
    pSol->cBuffersPlayed = 0;

    hw->samples = pSol->AudioInfo.play.buffer_size >> hw->info.shift;
    pSol->pPCMBuf = RTMemAllocZ(pSol->AudioInfo.play.buffer_size);
    if (!pSol->pPCMBuf)
    {
        LogRel(("solaudio: failed to alloc %d %d bytes to pPCMBuf\n", hw->samples << hw->info.shift, hw->samples));
        return -1;
    }
    LogFlow(("solaudio: init_out hw->samples=%d play.buffer_size=%d\n", hw->samples, pSol->AudioInfo.play.buffer_size));
    return 0;
}


static void solaudio_fini_out (HWVoiceOut *hw)
{
    solaudioVoiceOut *sol = (solaudioVoiceOut *) hw;
    LogFlow(("solaudio: fini_out\n"));

    solaudio_close_device(enmPlay);
    if (sol->pPCMBuf)
    {
        RTMemFree(sol->pPCMBuf);
        sol->pPCMBuf = NULL;
    }
}


static void solaudio_start_out (HWVoiceOut *hw)
{
    audio_info_t AudioInfo;
    solaudioVoiceOut *pSol = (solaudioVoiceOut *)hw;
    LogFlow(("solaudio: voice_enable\n"));

    audio_pcm_info_clear_buf(&hw->info, pSol->pPCMBuf, hw->samples);

    AUDIO_INITINFO(&AudioInfo);
    ioctl(g_AudioDev, AUDIO_GETINFO, &AudioInfo);
    AudioInfo.play.pause = 0;
#if 0
    AudioInfo.play.eof = 0;
    AudioInfo.play.samples = 0;
    pSol->cBuffersPlayed = 0;
#endif
    ioctl(g_AudioDev, AUDIO_SETINFO, &AudioInfo);
}


static void solaudio_stop_out (solaudioVoiceOut *sol)
{
    audio_info_t AudioInfo;
    LogFlow(("solaudio: stop_out\n"));

    if (ioctl(g_AudioCtl, I_SETSIG, 0) < 0)
    {
        Log(("solaudio: failed to stop signalling\n"));
        return;
    }

    if (ioctl(g_AudioDev, I_FLUSH, FLUSHW) < 0)
    {
        LogRel(("solaudio: failed to drop unplayed buffers\n"));
        return;
    }

    AUDIO_INITINFO(&AudioInfo);
    AudioInfo.play.pause = 1;
#if 0
    AudioInfo.play.samples = 0;
    AudioInfo.play.eof = 0;
    AudioInfo.play.error = 0;
    sol->cBuffersPlayed = 0;
#endif
    if (ioctl(g_AudioDev, AUDIO_SETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed during stop_out.\n"));
        return;
    }
}


static int solaudio_availbuf (solaudioVoiceOut *sol)
{
    int cbPlayBuffer = 0;
    if (ioctl(g_AudioDev, AUDIO_GETINFO, &sol->AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_GETINFO ioctl failed\n"));
        return -1;
    }

    if (sol->cBuffersPlayed - sol->AudioInfo.play.eof <= 2)
        cbPlayBuffer = conf.cbPlayBuffer;

    /* Check for overflow */
    if (sol->cBuffersPlayed > UINT_MAX - 4)
    {
        sol->cBuffersPlayed -= UINT_MAX - 4;
        sol->AudioInfo.play.eof -= UINT_MAX - 4;
        ioctl(g_AudioDev, AUDIO_SETINFO, &sol->AudioInfo);
    }

    LogFlow(("avail: eof=%d samples=%d bufplayed=%d avail=%d\n", sol->AudioInfo.play.eof, sol->AudioInfo.play.samples,
            sol->cBuffersPlayed, cbPlayBuffer));
    return cbPlayBuffer;
}


static int solaudio_run_out (HWVoiceOut *hw)
{
    solaudioVoiceOut *pSol = (solaudioVoiceOut *) hw;
    int          csLive, csDecr, csSamples, csToWrite, csAvail;
    size_t       cbAvail, cbToWrite, cbWritten;
    uint8_t     *pu8Dst;
    st_sample_t *psSrc;

    csLive = audio_pcm_hw_get_live_out(hw);
    if (!csLive)
        return 0;

    cbAvail = solaudio_availbuf(pSol);
    if (cbAvail <= 0)
        return 0;

    csAvail   = cbAvail >> hw->info.shift; /* bytes => samples */
    csDecr    = audio_MIN(csLive, csAvail);
    csSamples = csDecr;

    while (csSamples)
    {
        /* split request at the end of our samples buffer */
        csToWrite = audio_MIN(csSamples, hw->samples - hw->rpos);
        cbToWrite = csToWrite << hw->info.shift;
        psSrc     = hw->mix_buf + hw->rpos;
        pu8Dst    = advance(pSol->pPCMBuf, hw->rpos << hw->info.shift);

        hw->clip(pu8Dst, psSrc, csToWrite);

        cbWritten = write(g_AudioDev, pu8Dst, cbToWrite);
        if (cbWritten < 0)
            break;

        hw->rpos   = (hw->rpos + csToWrite) % hw->samples;
        csSamples -= csToWrite;
    }

    /* Increment eof marker for synchronous buffer processed */
    write (g_AudioDev, NULL, 0);
    pSol->cBuffersPlayed++;
    return csDecr;
}


static int solaudio_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    solaudioVoiceOut *pSol = (solaudioVoiceOut *) hw;
    switch (cmd)
    {
        case VOICE_ENABLE:
        {
            LogFlow(("solaudio: voice_enable\n"));
            solaudio_start_out(hw);
            break;
        }

        case VOICE_DISABLE:
        {
            LogFlow(("solaudio: voice_disable\n"));
            solaudio_stop_out(pSol);
            break;
        }
    }
    return 0;
}


static int solaudio_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}


static void *solaudio_audio_init (void)
{
    struct stat FileStat;

    LogFlow(("solaudio_audio_init\n"));
    if (!g_pszAudioDev)
    {
        g_pszAudioDev = solaudio_getdevice();
        if (!g_pszAudioDev)
        {
            LogRel(("solaudio: solaudio_getdevice() failed to return a valid device.\n"));
            return NULL;
        }
    }

    if (stat(g_pszAudioDev, &FileStat) < 0)
    {
        LogRel(("solaudio: failed to stat %s\n", g_pszAudioDev));
        return NULL;
    }

    if (!S_ISCHR(FileStat.st_mode))
    {
        LogRel(("solaudio: invalid mode for %s\n", g_pszAudioDev));
        return NULL;
    }

    if (!g_pszAudioCtl)
        RTStrAPrintf(&g_pszAudioCtl, "%sctl", g_pszAudioDev);

    if (g_AudioCtl < 0)
    {
        g_AudioCtl = open(g_pszAudioCtl, O_RDWR | O_NONBLOCK);
        if (g_AudioCtl < 0)
        {
            LogRel(("solaudio: failed to open device %s\n", g_pszAudioCtl));
            return NULL;
        }
    }

    return &conf;
}


static void solaudio_audio_fini (void *opaque)
{
    LogFlow(("solaudio_audio_fini\n"));
    if (g_pszAudioDev)
    {
        RTStrFree(g_pszAudioDev);
        g_pszAudioDev = NULL;
    }
    if (g_pszAudioCtl)
    {
        RTStrFree(g_pszAudioCtl);
        g_pszAudioCtl = NULL;
    }
    close(g_AudioCtl);
    g_AudioCtl = -1;

    NOREF(opaque);
}


/* -=-=-=-=- Audio Input -=-=-=-=- */

static void solaudio_pause_record(void)
{
    audio_info_t AudioInfo;
    AUDIO_INITINFO(&AudioInfo);
    if (ioctl(g_RecordDev, AUDIO_GETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: failed to get info. to pause recording.\n"));
        return;
    }

    AudioInfo.record.pause = 1;
    if (ioctl(g_RecordDev, AUDIO_SETINFO, &AudioInfo))
        LogRel(("solaudio: failed to pause recording.\n"));
}


static void solaudio_resume_record(void)
{
    audio_info_t AudioInfo;
    AUDIO_INITINFO(&AudioInfo);
    if (ioctl(g_RecordDev, AUDIO_GETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: failed to get info. to resume recording.\n"));
        return;
    }

    AudioInfo.record.pause = 0;
    if (ioctl(g_RecordDev, AUDIO_SETINFO, &AudioInfo))
        LogRel(("solaudio: failed to resume recording.\n"));
}



static void solaudio_stop_in (solaudioVoiceIn *sol)
{
    audio_info_t AudioInfo;
    LogFlow(("solaudio: stop_in\n"));

    if (ioctl(g_AudioCtl, I_SETSIG, 0) < 0)
    {
        Log(("solaudio: failed to stop signalling\n"));
        return;
    }

    if (ioctl(g_RecordDev, I_FLUSH, FLUSHR) < 0)
    {
        LogRel(("solaudio: failed to drop record buffers\n"));
        return;
    }

    AUDIO_INITINFO(&AudioInfo);
    AudioInfo.record.samples = 0;
    AudioInfo.record.pause = 1;
    AudioInfo.record.eof = 0;
    AudioInfo.record.error = 0;
    if (ioctl(g_RecordDev, AUDIO_SETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed during stop_in.\n"));
        return;
    }

    solaudio_close_device(enmRecord);
}


static void solaudio_start_in (solaudioVoiceIn *sol)
{
    LogFlow(("solaudio: start_in\n"));
    if (solaudio_open_device(enmRecord))
    {
        LogRel(("solaudio: failed to open for recording.\n"));
    }

    if (solaudio_setattrs(enmRecord, &sol->AudioInfo))
    {
        LogRel(("solaudio: solaudio_setattrs for recording failed.\n"));
        return;
    }
    solaudio_resume_record();
}


static int solaudio_init_in (HWVoiceIn *hw, audsettings_t *as)
{
    solaudioVoiceIn *pSol = (solaudioVoiceIn *)hw;
    audsettings_t ObtAudioInfo;

    AUDIO_INITINFO(&pSol->AudioInfo);
    pSol->AudioInfo.record.sample_rate = as->freq;
    pSol->AudioInfo.record.channels = as->nchannels;
    pSol->AudioInfo.record.precision = aud_to_solfmt(as->fmt);
    pSol->AudioInfo.record.buffer_size = conf.cbRecordBuffer;

    if (as->fmt == AUD_FMT_U8)
        pSol->AudioInfo.record.encoding = AUDIO_ENCODING_LINEAR8;
    else
        pSol->AudioInfo.record.encoding = AUDIO_ENCODING_LINEAR;

    /*
     * Open device for recording in passive mode (O_WRONLY) as we do not
     * want to start buffering audio immediately. This is what is recommended.
     */
    if (solaudio_open_device(enmRecordPassive))
    {
        LogRel(("solaudio: solaudio_open failed.\n"));
        return -1;
    }

    /* Specify playback attributes to device. */
    if (solaudio_setattrs(enmRecord, &pSol->AudioInfo))
    {
        LogRel(("solaudio: failed to set playback attributes.\n"));
        return -1;
    }

    /* Copy obtained record attributes. */
    ObtAudioInfo.freq = pSol->AudioInfo.record.sample_rate;
    ObtAudioInfo.nchannels = pSol->AudioInfo.record.channels;
    ObtAudioInfo.fmt = sol_to_audfmt(pSol->AudioInfo.record.precision, pSol->AudioInfo.record.encoding);
    ObtAudioInfo.endianness = as->endianness;

    audio_pcm_init_info(&hw->info, &ObtAudioInfo);

    hw->samples = pSol->AudioInfo.record.buffer_size >> hw->info.shift;
    pSol->pPCMBuf = RTMemAllocZ(pSol->AudioInfo.record.buffer_size);
    if (!pSol->pPCMBuf)
    {
        LogRel(("solaudio: init_in: failed to alloc %d %d bytes to pPCMBuf\n", hw->samples << hw->info.shift, hw->samples));
        return -1;
    }
    solaudio_close_device(enmRecordPassive);
    LogFlow(("solaudio: init_in: hw->samples=%d record.buffer_size=%d rate=%d\n", hw->samples, pSol->AudioInfo.record.buffer_size,
            pSol->AudioInfo.record.sample_rate));
    return 0;
}


static void solaudio_fini_in (HWVoiceIn *hw)
{
    solaudioVoiceIn *sol = (solaudioVoiceIn *) hw;
    LogFlow(("solaudio: fini_in done\n"));

    if (sol->pPCMBuf)
    {
        RTMemFree(sol->pPCMBuf);
        sol->pPCMBuf = NULL;
    }
}


static int solaudio_run_in (HWVoiceIn *hw)
{
#if 0
    solaudioVoiceIn *pSol = (solaudioVoiceIn *) hw;
    int          csDead, csDecr = 0, csSamples, csRead, csAvail;
    size_t       cbAvail, cbRead;
    void        *pu8Src;
    st_sample_t *psDst;

    csDead = hw->samples - audio_pcm_hw_get_live_in (hw);

    if (!csDead)
        return 0;

    if (ioctl(g_AudioDev, I_NREAD, &cbAvail) < 0)
    {
        LogRel(("solaudio: I_NREAD failed\n"));
        return 0;
    }

    if (!cbAvail)
        return 0;

    cbAvail = audio_MIN(cbAvail, conf.cbRecordBuffer);
    pu8Src = pSol->pPCMBuf;
    cbRead = read(g_AudioDev, pu8Src, cbAvail);
    if (cbRead <= 0)
        return 0;

    csAvail = cbAvail >> hw->info.shift;
    csDecr  = audio_MIN (csDead, csAvail);
    csSamples = csDecr;

    while (csSamples)
    {
        /* split request at the end of our samples buffer */
        psDst      = hw->conv_buf + hw->wpos;
        csRead     = audio_MIN (csSamples, hw->samples - hw->wpos);
        hw->conv (psDst, pu8Src, csRead, &pcm_in_volume);
        hw->wpos   = (hw->wpos + csRead) % hw->samples;
        csSamples -= csRead;
        pu8Src     = (void *)((uint8_t*)pu8Src + (csRead << hw->info.shift));
    }
    return csDecr;
#else
    solaudioVoiceIn *sol = (solaudioVoiceIn *) hw;
    int hwshift = hw->info.shift;
    int i;
    int live = audio_pcm_hw_get_live_in (hw);
    int dead = hw->samples - live;
    size_t read_samples = 0;
    struct
    {
        int add;
        int len;
    } bufs[2];

    bufs[0].add = hw->wpos;
    bufs[0].len = 0;
    bufs[1].add = 0;
    bufs[1].len = 0;

    if (!dead) {
        return 0;
    }

    if (hw->wpos + dead > hw->samples)
    {
        bufs[0].len = (hw->samples - hw->wpos) << hwshift;
        bufs[1].len = (dead - (hw->samples - hw->wpos)) << hwshift;
    }
    else
        bufs[0].len = dead << hwshift;


    for (i = 0; i < 2; ++i)
    {
        ssize_t nread;

        if (bufs[i].len)
        {
            void *p = advance (sol->pPCMBuf, bufs[i].add << hwshift);
            nread = read (g_RecordDev, p, bufs[i].len);

            if (nread > 0)
            {
                read_samples += nread >> hwshift;
                hw->conv (hw->conv_buf + bufs[i].add, p, nread >> hwshift,
                          &pcm_in_volume);
            }

            if (bufs[i].len - nread)
                if (nread == -1)
                    break;
        }
    }

    hw->wpos = (hw->wpos + read_samples) % hw->samples;
    return read_samples;
#endif
}


static int solaudio_read (SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}


static int solaudio_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    solaudioVoiceIn *pSol = (solaudioVoiceIn *) hw;
    switch (cmd)
    {
        case VOICE_ENABLE:
        {
            LogRel(("solaudio: solaudio_ctl_in voice_enable\n"));
            solaudio_start_in(pSol);
            break;
        }

        case VOICE_DISABLE:
        {
            LogRel(("solaudio: solaudio_ctl_in voice_disable\n"));
            solaudio_stop_in(pSol);
            break;
        }
    }
    return 0;
}


static struct audio_pcm_ops solaudio_pcm_ops =
{
    solaudio_init_out,
    solaudio_fini_out,
    solaudio_run_out,
    solaudio_write,
    solaudio_ctl_out,

    solaudio_init_in,
    solaudio_fini_in,
    solaudio_run_in,
    solaudio_read,
    solaudio_ctl_in
};


static struct audio_option solaudio_options[] =
{
    {"PLAY_BUFFER_SIZE", AUD_OPT_INT, &conf.cbPlayBuffer,
     "Size of the buffer in bytes", NULL, 0},
#if 0
    {"RECORD_BUFFER_SIZE", AUD_OPT_INT, &conf.cbRecordBuffer,
     "Size of the record bufffer in bytes", NULL, 0},
#endif
    {NULL, 0, NULL, NULL, NULL, 0}
};


struct audio_driver solaudio_audio_driver =
{
    INIT_FIELD (name           = ) "solaudio",
    INIT_FIELD (descr          = ) "SolarisAudio http://sun.com",
    INIT_FIELD (options        = ) solaudio_options,
    INIT_FIELD (init           = ) solaudio_audio_init,
    INIT_FIELD (fini           = ) solaudio_audio_fini,
    INIT_FIELD (pcm_ops        = ) &solaudio_pcm_ops,
    INIT_FIELD (can_be_default = ) 1,
    INIT_FIELD (max_voices_out = ) INT_MAX,
    INIT_FIELD (max_voices_in  = ) 0,           /* Input not supported. */
    INIT_FIELD (voice_size_out = ) sizeof (solaudioVoiceOut),
    INIT_FIELD (voice_size_in  = ) 0
};

