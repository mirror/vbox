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
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

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
    HWVoiceOut hw;
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
    int buffer_size;
    int nbuffers;
} conf =
{
    INIT_FIELD (buffer_size =) 8192,
    INIT_FIELD (nbuffers =) 4,
};


static void GCC_FMT_ATTR (2, 3) solaudio_logerr (int err, const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    AUD_vlog (AUDIO_CAP, fmt, ap);
    va_end (ap);

    AUD_log (AUDIO_CAP, "Reason: %s\n", strerror (err));
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
            return AUD_FMT_U16;

        default:
            solaudio_logerr (-1, "Bad audio format %d\n", fmt);
            return AUD_FMT_S8;
    }
}


static int solaudio_open (int in, audio_info_t *info, int *pfd, int *pctl_fd)
{
    int fd;
    int ctl_fd;
    struct stat st;
    const char *deviceName = "/dev/audio";
    const char *ctlDeviceName = "/dev/audioctl";
    audio_info_t audInfo;

    /* @todo Use AUDIO_GETDEV instead of hardcoding /dev/audio */
    if (stat(deviceName, &st) < 0)
    {
        LogRel(("solaudio: failed to stat %s\n", deviceName));
        return -1;
    }

    if (!S_ISCHR(st.st_mode))
    {
        LogRel(("solaudio: invalid mode for %s\n", deviceName));
        return -1;
    }

    fd = open(deviceName, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        LogRel(("solaudio: failed to open %s\n", deviceName));
        return -1;
    }

    ctl_fd = open(ctlDeviceName, O_WRONLY | O_NONBLOCK);
    if (ctl_fd < 0)
    {
        LogRel(("solaudio: failed to open %s\n", ctlDeviceName));
        close(fd);
        return -1;
    }

    AUDIO_INITINFO(&audInfo);
    if (ioctl(fd, AUDIO_GETINFO, &audInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_GETINFO failed\n"));
        close(fd);
        return -1;
    }
    audInfo.play.sample_rate = info->play.sample_rate;
    audInfo.play.channels = info->play.channels;
    audInfo.play.precision = info->play.precision;
    audInfo.play.encoding = info->play.encoding;
    audInfo.play.buffer_size = info->play.buffer_size;
    audInfo.play.gain = AUDIO_MID_GAIN;
    if (ioctl(fd, AUDIO_SETINFO, &audInfo) < 0)
    {
        LogRel(("solaudio: AUDIO_SETINFO failed\n"));
        close(fd);
        return -1;
    }
    LogFlow(("solaudio: system buffer_size=%d\n", audInfo.play.buffer_size));
    *pfd = fd;
    *pctl_fd = ctl_fd;
    return 0;
}


static int solaudio_init_out (HWVoiceOut *hw, audsettings_t *as)
{
    solaudioVoiceOut *sol = (solaudioVoiceOut *) hw;
    audio_info_t audioInfo;
    audsettings_t obt_as;
    int fd = -1;
    int ctl_fd = -1;

    AUDIO_INITINFO(&audioInfo);
    audioInfo.play.sample_rate = as->freq;
    audioInfo.play.channels = as->nchannels;
    audioInfo.play.precision = aud_to_solfmt(as->fmt);
    audioInfo.play.buffer_size = conf.buffer_size;
    if (as->fmt == AUD_FMT_U8)
        audioInfo.play.encoding = AUDIO_ENCODING_LINEAR8;
    else
        audioInfo.play.encoding = AUDIO_ENCODING_LINEAR;

    if (solaudio_open(0, &audioInfo, &fd, &ctl_fd))
    {
        LogRel(("solaudio: solaudio_open failed\n"));
        return -1;
    }

    sol->AudioDev = fd;
    sol->AudioCtl = ctl_fd;
    obt_as.freq = audioInfo.play.sample_rate;
    obt_as.nchannels = audioInfo.play.channels;
    obt_as.fmt = sol_to_audfmt(audioInfo.play.precision, audioInfo.play.encoding);
    obt_as.endianness = as->endianness;

    audio_pcm_init_info (&hw->info, &obt_as);
    sol->cBuffersPlayed = audioInfo.play.eof;

    hw->samples = audioInfo.play.buffer_size >> hw->info.shift;
    sol->pPCMBuf = RTMemAllocZ(audioInfo.play.buffer_size);
    if (!sol->pPCMBuf)
    {
        LogRel(("solaudio: failed to alloc %d %d bytes to pPCMBuf\n", hw->samples << hw->info.shift, hw->samples));
        return -1;
    }
    LogFlow(("solaudio: hw->samples=%d play.buffer_size=%d\n", hw->samples, audioInfo.play.buffer_size));
    return 0;
}


static void solaudio_stop (solaudioVoiceOut *solvw)
{
    LogFlow(("solaudio: stop\n"));
    if (solvw->AudioDev < 0 || solvw->AudioCtl < 0)
    {
        Log(("solaudio: invalid file descriptors\n"));
        return;
    }

    if (ioctl(solvw->AudioCtl, I_SETSIG, 0) < 0)
    {
        LogRel(("solaudio: failed to stop signalling\n"));
        return;
    }

    if (ioctl(solvw->AudioDev, I_FLUSH, FLUSHW) < 0)
    {
        Log(("solaudio: failed to drop unplayed buffers\n"));
        return;
    }

    close(solvw->AudioDev);
    solvw->AudioDev = -1;
    close(solvw->AudioCtl);
    solvw->AudioCtl = -1;
    solvw->cBuffersPlayed = 0;
    if (solvw->pPCMBuf)
    {
        RTMemFree(solvw->pPCMBuf);
        solvw->pPCMBuf = NULL;
    }
}


static void solaudio_fini_out (HWVoiceOut *hw)
{
    solaudioVoiceOut *sol = (solaudioVoiceOut *) hw;
    solaudio_stop (sol);
    LogFlow(("solaudio: fini_out done\n"));
}


static int solaudio_availbuf (solaudioVoiceOut *solvw)
{
    audio_info_t audioInfo;
    int buffers = 0;

    AUDIO_INITINFO(&audioInfo);
    if (ioctl(solvw->AudioDev, AUDIO_GETINFO, &audioInfo) < 0)
    {
        Log(("solaudio: AUDIO_GETINFO ioctl failed\n"));
        return -1;
    }

    buffers = audioInfo.play.buffer_size * (2 + audioInfo.play.eof - solvw->cBuffersPlayed);

    LogFlow(("avail: eof=%d samples=%d bufsize=%d bufplayed=%d avail=%d\n", audioInfo.play.eof, audioInfo.play.samples,
        audioInfo.play.buffer_size, solvw->cBuffersPlayed, buffers));
    return buffers;
}

static void solaudio_yield (solaudioVoiceOut *pSol)
{
    audio_info_t AudioInfo;
    timespec_t WaitTimeSpec;
    if (ioctl (pSol->AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
        return;

    WaitTimeSpec.tv_sec = 0;
    WaitTimeSpec.tv_nsec = 10000000;

    while (AudioInfo.play.eof < pSol->cBuffersPlayed - 2)
    {
        nanosleep(&WaitTimeSpec, NULL);
        if (ioctl(pSol->AudioDev, AUDIO_GETINFO, &AudioInfo) < 0)
            break;
    }
}


static int solaudio_run_out (HWVoiceOut *hw)
{
    solaudioVoiceOut *pSol = (solaudioVoiceOut *) hw;
    int          csLive, csDecr, csSamples, csToWrite, csAvail;
    size_t       cbAvail, cbToWrite, cbWritten;
    uint8_t     *pu8Dst;
    st_sample_t *psSrc;

    csLive = audio_pcm_hw_get_live_out (hw);
    if (!csLive)
        return 0;

    for (;;)
    {
        cbAvail = solaudio_availbuf (pSol);
        if (cbAvail > 0)
            break;
        solaudio_yield(pSol);
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
        pu8Dst    = advance (pSol->pPCMBuf, hw->rpos << hw->info.shift);

        hw->clip (pu8Dst, psSrc, csToWrite);

        cbWritten = write (pSol->AudioDev, pu8Dst, cbToWrite);
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
    solaudioVoiceOut *sol = (solaudioVoiceOut *) hw;
    switch (cmd)
    {
        case VOICE_ENABLE:
        {
            /* reset the eof marker and samples markers */
            audio_info_t audioInfo;
            AUDIO_INITINFO(&audioInfo);
            ioctl(sol->AudioDev, AUDIO_GETINFO, &audioInfo);
            audioInfo.play.eof = 0;
            audioInfo.play.samples = 0;
            ioctl(sol->AudioDev, AUDIO_SETINFO, &audioInfo);

            sol->cBuffersPlayed = 0;
            audio_pcm_info_clear_buf (&hw->info, sol->pPCMBuf, hw->samples);
            LogFlow(("solaudio: voice_enable\n"));
            break;
        }

        case VOICE_DISABLE:
        {
            LogFlow(("solaudio: voice_disable\n"));
            solaudio_stop(sol);
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
    (void) opaque;
}


static struct audio_pcm_ops solaudio_pcm_ops = {
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

static struct audio_option solaudio_options[] = {
    {"BUFFER_SIZE", AUD_OPT_INT, &conf.buffer_size,
     "Size of the buffer in frames", NULL, 0},
    {"BUFFER_COUNT", AUD_OPT_INT, &conf.nbuffers,
     "Number of buffers", NULL, 0},
    {NULL, 0, NULL, NULL, NULL, 0}
};

struct audio_driver solaudio_audio_driver = {
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

