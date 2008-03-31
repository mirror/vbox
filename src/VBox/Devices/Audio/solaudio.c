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

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
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
typedef struct solaudioVoiceOut {
    HWVoiceOut Hw;
    int        AudioDev;
    int        AudioCtl;
    int        cBuffersPlayed;
    void      *pPCMBuf;
} solaudioVoiceOut;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
struct
{
    int cbBuffer;
} conf =
{
    INIT_FIELD (cbBuffer =) 4352,
};


static void GCC_FMT_ATTR (2, 3) solaudio_logerr (int err, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    AUD_vlog(AUDIO_CAP, fmt, ap);
    va_end(ap);

    AUD_log(AUDIO_CAP, "Reason: %s\n", strerror(err));
}


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
            solaudio_logerr (-1, "Bad audio format %d\n", fmt);
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
            solaudio_logerr (-1, "Bad audio format %d\n", fmt);
            return AUD_FMT_S8;
    }
}


static char *solaudio_getdevice (void)
{
    /* This is for multiple audio devices where env. var determines current one,
     * otherwise else we fallback to default.
     */
    const char *pszAudioDev = RTEnvGet("AUDIODEV");
    if (pszAudioDev)
        return RTStrDup(pszAudioDev);

    return RTStrDup("/dev/audio");
}


static int solaudio_open (int in, audio_info_t *info, int *pfd, int *pctl_fd)
{
    int AudioDev;
    int AudioCtl;
    struct stat FileStat;
    char *pszAudioDev = NULL;
    char *pszAudioCtl = NULL;
    audio_info_t AudioInfo;

    pszAudioDev = solaudio_getdevice();
    if (!pszAudioDev)
    {
        LogRel(("solaudio: solaudio_getdevice() failed to return a valid device.\n"));
        return -1;
    }

    if (stat(pszAudioDev, &FileStat) < 0)
    {
        LogRel(("solaudio: failed to stat %s\n", pszAudioDev));
        goto err2;
    }

    if (!S_ISCHR(FileStat.st_mode))
    {
        LogRel(("solaudio: invalid mode for %s\n", pszAudioDev));
        goto err2;
    }

    AudioDev = open(pszAudioDev, O_WRONLY | O_NONBLOCK);
    if (AudioDev < 0)
    {
        LogRel(("solaudio: failed to open %s\n", pszAudioDev));
        goto err2;
    }

    RTStrAPrintf(&pszAudioCtl, "%sctl", pszAudioDev);
    AudioCtl = open(pszAudioCtl, O_WRONLY | O_NONBLOCK);
    if (AudioCtl < 0)
    {
        LogRel(("solaudio: failed to open %s\n", pszAudioCtl));
        close(AudioDev);
        goto err;
    }

    AUDIO_INITINFO(&AudioInfo);
    if (ioctl(AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_GETINFO failed\n"));
        close(AudioDev);
        close(AudioCtl);
        goto err;
    }
    AudioInfo.play.sample_rate = info->play.sample_rate;
    AudioInfo.play.channels = info->play.channels;
    AudioInfo.play.precision = info->play.precision;
    AudioInfo.play.encoding = info->play.encoding;
    AudioInfo.play.buffer_size = info->play.buffer_size;
    AudioInfo.play.gain = AUDIO_MAX_GAIN;
    if (ioctl(AudioDev, AUDIO_SETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed\n"));
        close(AudioDev);
        close(AudioCtl);
        goto err;
    }
    LogFlow(("solaudio: buffer_size=%d\n", AudioInfo.play.buffer_size));
    *pfd = AudioDev;
    *pctl_fd = AudioCtl;
    RTStrFree(pszAudioDev);
    RTStrFree(pszAudioCtl);
    return 0;

err:
    RTStrFree(pszAudioCtl);
err2:
    RTStrFree(pszAudioDev);
    return -1;
}


static int solaudio_init_out (HWVoiceOut *hw, audsettings_t *as)
{
    solaudioVoiceOut *pSol = (solaudioVoiceOut *)hw;
    audio_info_t AudioInfo;
    audsettings_t ObtAudioInfo;
    int AudioDev = -1;
    int AudioCtl = -1;

    AUDIO_INITINFO(&AudioInfo);
    AudioInfo.play.sample_rate = as->freq;
    AudioInfo.play.channels = as->nchannels;
    AudioInfo.play.precision = aud_to_solfmt(as->fmt);
#if 0
    /* Not really needed. */
    int cbPerSample = (AudioInfo.play.channels * AudioInfo.play.precision) / 8;
    int cbPerSecond = cbPerSample * AudioInfo.play.sample_rate;
    AudioInfo.play.buffer_size = cbPerSecond > 131072 ? conf.cbBuffer : conf.cbBuffer / 2;
#endif
    AudioInfo.play.buffer_size = conf.cbBuffer;

    if (as->fmt == AUD_FMT_U8)
        AudioInfo.play.encoding = AUDIO_ENCODING_LINEAR8;
    else
        AudioInfo.play.encoding = AUDIO_ENCODING_LINEAR;

    if (solaudio_open(0, &AudioInfo, &AudioDev, &AudioCtl))
    {
        LogRel(("solaudio: solaudio_open failed\n"));
        return -1;
    }

    pSol->AudioDev = AudioDev;
    pSol->AudioCtl = AudioCtl;
    ObtAudioInfo.freq = AudioInfo.play.sample_rate;
    ObtAudioInfo.nchannels = AudioInfo.play.channels;
    ObtAudioInfo.fmt = sol_to_audfmt(AudioInfo.play.precision, AudioInfo.play.encoding);
    ObtAudioInfo.endianness = as->endianness;

    audio_pcm_init_info(&hw->info, &ObtAudioInfo);
    pSol->cBuffersPlayed = AudioInfo.play.eof;

    hw->samples = AudioInfo.play.buffer_size >> hw->info.shift;
    pSol->pPCMBuf = RTMemAllocZ(AudioInfo.play.buffer_size);
    if (!pSol->pPCMBuf)
    {
        LogRel(("solaudio: failed to alloc %d %d bytes to pPCMBuf\n", hw->samples << hw->info.shift, hw->samples));
        return -1;
    }
    LogFlow(("solaudio: hw->samples=%d play.buffer_size=%d\n", hw->samples, AudioInfo.play.buffer_size));
    return 0;
}


static void solaudio_stop (solaudioVoiceOut *sol)
{
    audio_info_t AudioInfo;
    LogFlow(("solaudio: stop\n"));
    if (sol->AudioDev < 0 || sol->AudioCtl < 0)
    {
        Log(("solaudio: invalid file descriptors\n"));
        return;
    }

    if (ioctl(sol->AudioCtl, I_SETSIG, 0) < 0)
    {
        Log(("solaudio: failed to stop signalling\n"));
        return;
    }

    if (ioctl(sol->AudioDev, I_FLUSH, FLUSHW) < 0)
    {
        LogRel(("solaudio: failed to drop unplayed buffers\n"));
        return;
    }

    AUDIO_INITINFO(&AudioInfo);
    AudioInfo.play.samples = 0;
    AudioInfo.play.pause = 0;
    AudioInfo.play.eof = 0;
    AudioInfo.play.error = 0;
    sol->cBuffersPlayed = 0;
    if (ioctl(sol->AudioDev, AUDIO_SETINFO, &AudioInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed during stop.\n"));
        return;
    }
}


static void solaudio_fini_out (HWVoiceOut *hw)
{
    solaudioVoiceOut *sol = (solaudioVoiceOut *) hw;
    solaudio_stop (sol);

    close(sol->AudioDev);
    sol->AudioDev = -1;
    close(sol->AudioCtl);
    sol->AudioCtl = -1;
    if (sol->pPCMBuf)
    {
        RTMemFree(sol->pPCMBuf);
        sol->pPCMBuf = NULL;
    }

    LogFlow(("solaudio: fini_out done\n"));
}


static int solaudio_availbuf (solaudioVoiceOut *sol)
{
    audio_info_t AudioInfo;
    int cbBuffer = 0;

    AUDIO_INITINFO(&AudioInfo);
    if (ioctl(sol->AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
    {
        Log(("solaudio: AUDIO_GETINFO ioctl failed\n"));
        return -1;
    }

    if (sol->cBuffersPlayed - AudioInfo.play.eof <= 2)
        cbBuffer = AudioInfo.play.buffer_size;

    LogFlow(("avail: eof=%d samples=%d bufsize=%d bufplayed=%d avail=%d\n", AudioInfo.play.eof, AudioInfo.play.samples,
        AudioInfo.play.buffer_size, sol->cBuffersPlayed, cbBuffer));
    return cbBuffer;
}

#if 0
static void solaudio_yield (solaudioVoiceOut *pSol)
{
    audio_info_t AudioInfo;
    timespec_t WaitTimeSpec;
    if (ioctl(pSol->AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
        return;

    WaitTimeSpec.tv_sec = 0;
    WaitTimeSpec.tv_nsec = 100000000;

    while (AudioInfo.play.eof + 10 < pSol->cBuffersPlayed)
    {
        nanosleep(&WaitTimeSpec, NULL);
        if (ioctl(pSol->AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
            break;
    }
}
#endif

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

        cbWritten = write(pSol->AudioDev, pu8Dst, cbToWrite);
        if (cbWritten < 0)
            break;

        hw->rpos   = (hw->rpos + csToWrite) % hw->samples;
        csSamples -= csToWrite;
    }

    /* Increment eof marker for synchronous buffer processed */
    write (pSol->AudioDev, NULL, 0);
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
            /* reset the eof marker and samples markers */
            audio_info_t AudioInfo;
            LogFlow(("solaudio: voice_enable\n"));
            AUDIO_INITINFO(&AudioInfo);
            ioctl(pSol->AudioDev, AUDIO_GETINFO, &AudioInfo);
            AudioInfo.play.eof = 0;
            AudioInfo.play.samples = 0;
            ioctl(pSol->AudioDev, AUDIO_SETINFO, &AudioInfo);
            pSol->cBuffersPlayed = 0;

            audio_pcm_info_clear_buf(&hw->info, pSol->pPCMBuf, hw->samples);
            break;
        }

        case VOICE_DISABLE:
        {
            LogFlow(("solaudio: voice_disable\n"));
            solaudio_stop(pSol);
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
    return &conf;
}


static void solaudio_audio_fini (void *opaque)
{
    NOREF(opaque);
}


static struct audio_pcm_ops solaudio_pcm_ops =
{
    solaudio_init_out,
    solaudio_fini_out,
    solaudio_run_out,
    solaudio_write,
    solaudio_ctl_out,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static struct audio_option solaudio_options[] =
{
    {"BUFFER_SIZE", AUD_OPT_INT, &conf.cbBuffer,
     "Size of the buffer in bytes", NULL, 0},
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
    INIT_FIELD (max_voices_out = ) 1,
    INIT_FIELD (max_voices_in  = ) 0,
    INIT_FIELD (voice_size_out = ) sizeof (solaudioVoiceOut),
    INIT_FIELD (voice_size_in  = ) 0
};

