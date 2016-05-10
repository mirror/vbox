/* $Id$ */
/** @file
 * VBox audio devices: ALSA audio driver.
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
 * --------------------------------------------------------------------
 *
 * This code is based on: alsaaudio.c
 *
 * QEMU ALSA audio driver
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */
#include <VBox/vmm/pdmaudioifs.h>

RT_C_DECLS_BEGIN
 #include "alsa_stubs.h"
 #include "alsa_mangling.h"
RT_C_DECLS_END

#include <alsa/asoundlib.h>
#include <alsa/control.h> /* For device enumeration. */

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"

/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/

/** Makes DRVHOSTALSAAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTALSAAUDIO(pInterface) \
    ( (PDRVHOSTALSAAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTALSAAUDIO, IHostAudio)) )

/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/

typedef struct ALSAAUDIOSTREAMIN
{
    PDMAUDIOHSTSTRMIN   pStreamIn;
    snd_pcm_t          *phPCM;
    void               *pvBuf;
    size_t              cbBuf;
} ALSAAUDIOSTREAMIN, *PALSAAUDIOSTREAMIN;

typedef struct ALSAAUDIOSTREAMOUT
{
    PDMAUDIOHSTSTRMOUT  pStreamOut;
    snd_pcm_t          *phPCM;
    void               *pvBuf;
    size_t              cbBuf;
} ALSAAUDIOSTREAMOUT, *PALSAAUDIOSTREAMOUT;

/* latency = period_size * periods / (rate * bytes_per_frame) */

typedef struct ALSAAUDIOCFG
{
    int size_in_usec_in;
    int size_in_usec_out;
    const char *pcm_name_in;
    const char *pcm_name_out;
    unsigned int buffer_size_in;
    unsigned int period_size_in;
    unsigned int buffer_size_out;
    unsigned int period_size_out;
    unsigned int threshold;

    int buffer_size_in_overriden;
    int period_size_in_overriden;

    int buffer_size_out_overriden;
    int period_size_out_overriden;

} ALSAAUDIOCFG, *PALSAAUDIOCFG;

static int alsaStreamRecover(snd_pcm_t *phPCM);

static ALSAAUDIOCFG s_ALSAConf =
{
#ifdef HIGH_LATENCY
    1,
    1,
#else
    0,
    0,
#endif
    "default",
    "default",
#ifdef HIGH_LATENCY
    400000,
    400000 / 4,
    400000,
    400000 / 4,
#else
# define DEFAULT_BUFFER_SIZE 1024
# define DEFAULT_PERIOD_SIZE 256
    DEFAULT_BUFFER_SIZE * 4,
    DEFAULT_PERIOD_SIZE * 4,
    DEFAULT_BUFFER_SIZE,
    DEFAULT_PERIOD_SIZE,
#endif
    0,
    0,
    0,
    0,
    0
};

/**
 * Host Alsa audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTALSAAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS         pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO      IHostAudio;
    /** Error count for not flooding the release log.
     *  UINT32_MAX for unlimited logging. */
    uint32_t           cLogErrors;
} DRVHOSTALSAAUDIO, *PDRVHOSTALSAAUDIO;

/** Maximum number of tries to recover a broken pipe. */
#define ALSA_RECOVERY_TRIES_MAX    5

typedef struct ALSAAUDIOSTREAMCFG
{
    unsigned int freq;
    snd_pcm_format_t fmt;
    int nchannels;
    unsigned long buffer_size;
    unsigned long period_size;
    snd_pcm_uframes_t samples;
} ALSAAUDIOSTREAMCFG, *PALSAAUDIOSTREAMCFG;



static snd_pcm_format_t alsaAudioFmtToALSA(PDMAUDIOFMT fmt)
{
    switch (fmt)
    {
        case PDMAUDIOFMT_S8:
            return SND_PCM_FORMAT_S8;

        case PDMAUDIOFMT_U8:
            return SND_PCM_FORMAT_U8;

        case PDMAUDIOFMT_S16:
            return SND_PCM_FORMAT_S16_LE;

        case PDMAUDIOFMT_U16:
            return SND_PCM_FORMAT_U16_LE;

        case PDMAUDIOFMT_S32:
            return SND_PCM_FORMAT_S32_LE;

        case PDMAUDIOFMT_U32:
            return SND_PCM_FORMAT_U32_LE;

        default:
            break;
    }

    AssertMsgFailed(("Format %ld not supported\n", fmt));
    return SND_PCM_FORMAT_U8;
}

static int alsaALSAToAudioFmt(snd_pcm_format_t fmt,
                              PDMAUDIOFMT *pFmt, PDMAUDIOENDIANNESS *pEndianness)
{
    AssertPtrReturn(pFmt, VERR_INVALID_POINTER);
    /* pEndianness is optional. */

    switch (fmt)
    {
        case SND_PCM_FORMAT_S8:
            *pFmt = PDMAUDIOFMT_S8;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_U8:
            *pFmt = PDMAUDIOFMT_U8;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_S16_LE:
            *pFmt = PDMAUDIOFMT_S16;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_U16_LE:
            *pFmt = PDMAUDIOFMT_U16;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_S16_BE:
            *pFmt = PDMAUDIOFMT_S16;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

        case SND_PCM_FORMAT_U16_BE:
            *pFmt = PDMAUDIOFMT_U16;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

        case SND_PCM_FORMAT_S32_LE:
            *pFmt = PDMAUDIOFMT_S32;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_U32_LE:
            *pFmt = PDMAUDIOFMT_U32;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case SND_PCM_FORMAT_S32_BE:
            *pFmt = PDMAUDIOFMT_S32;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

        case SND_PCM_FORMAT_U32_BE:
            *pFmt = PDMAUDIOFMT_U32;
            if (pEndianness)
                *pEndianness = PDMAUDIOENDIANNESS_BIG;
            break;

        default:
            AssertMsgFailed(("Format %ld not supported\n", fmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static int alsaGetSampleShift(snd_pcm_format_t fmt, unsigned *puShift)
{
    AssertPtrReturn(puShift, VERR_INVALID_POINTER);

    switch (fmt)
    {
        case SND_PCM_FORMAT_S8:
        case SND_PCM_FORMAT_U8:
            *puShift = 0;
            break;

        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_U16_LE:
        case SND_PCM_FORMAT_S16_BE:
        case SND_PCM_FORMAT_U16_BE:
            *puShift = 1;
            break;

        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_U32_LE:
        case SND_PCM_FORMAT_S32_BE:
        case SND_PCM_FORMAT_U32_BE:
            *puShift = 2;
            break;

        default:
            AssertMsgFailed(("Format %ld not supported\n", fmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static int alsaStreamSetThreshold(snd_pcm_t *phPCM, snd_pcm_uframes_t threshold)
{
    snd_pcm_sw_params_t *pSWParms = NULL;
    snd_pcm_sw_params_alloca(&pSWParms);
    if (!pSWParms)
        return VERR_NO_MEMORY;

    int rc;
    do
    {
        int err = snd_pcm_sw_params_current(phPCM, pSWParms);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to get current software parameters for threshold: %s\n",
                    snd_strerror(err)));
            rc = VERR_ACCESS_DENIED;
            break;
        }

        err = snd_pcm_sw_params_set_start_threshold(phPCM, pSWParms, threshold);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set software threshold to %ld: %s\n",
                    threshold, snd_strerror(err)));
            rc = VERR_ACCESS_DENIED;
            break;
        }

        err = snd_pcm_sw_params(phPCM, pSWParms);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set new software parameters for threshold: %s\n",
                    snd_strerror(err)));
            rc = VERR_ACCESS_DENIED;
            break;
        }

        LogFlowFunc(("Setting threshold to %RU32\n", threshold));
        rc = VINF_SUCCESS;
    }
    while (0);

    return rc;
}

static int alsaStreamClose(snd_pcm_t **pphPCM)
{
    if (!pphPCM || !*pphPCM)
        return VINF_SUCCESS;

    int rc;
    int rc2 = snd_pcm_close(*pphPCM);
    if (rc2)
    {
        LogRel(("ALSA: Closing PCM descriptor failed: %s\n", snd_strerror(rc2)));
        rc = VERR_GENERAL_FAILURE; /** @todo */
    }
    else
    {
        *pphPCM = NULL;
        rc = VINF_SUCCESS;
    }

    return rc;
}

static int alsaStreamOpen(bool fIn, PALSAAUDIOSTREAMCFG pCfgReq, PALSAAUDIOSTREAMCFG pCfgObt, snd_pcm_t **pphPCM)
{
    snd_pcm_t *phPCM = NULL;
    int rc;

    unsigned int cChannels = pCfgReq->nchannels;
    unsigned int uFreq = pCfgReq->freq;
    snd_pcm_uframes_t obt_buffer_size;

    do
    {
        const char *pszDev = fIn ? s_ALSAConf.pcm_name_in : s_ALSAConf.pcm_name_out;
        if (!pszDev)
        {
            LogRel(("ALSA: Invalid or no %s device name set\n", fIn ? "input" : "output"));
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        int err = snd_pcm_open(&phPCM, pszDev,
                               fIn ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                               SND_PCM_NONBLOCK);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to open \"%s\" as %s device: %s\n", pszDev, fIn ? "input" : "output", snd_strerror(err)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        LogRel(("ALSA: Using %s device \"%s\"\n", fIn ? "input" : "output", pszDev));

        snd_pcm_hw_params_t *pHWParms;
        snd_pcm_hw_params_alloca(&pHWParms); /** @todo Check for successful allocation? */
        err = snd_pcm_hw_params_any(phPCM, pHWParms);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to initialize hardware parameters: %s\n", snd_strerror(err)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        err = snd_pcm_hw_params_set_access(phPCM, pHWParms,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set access type: %s\n", snd_strerror(err)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        err = snd_pcm_hw_params_set_format(phPCM, pHWParms, pCfgReq->fmt);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set audio format to %d: %s\n", pCfgReq->fmt, snd_strerror(err)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        err = snd_pcm_hw_params_set_rate_near(phPCM, pHWParms, &uFreq, 0);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set frequency to %uHz: %s\n", pCfgReq->freq, snd_strerror(err)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        err = snd_pcm_hw_params_set_channels_near(phPCM, pHWParms, &cChannels);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to set number of channels to %d\n", pCfgReq->nchannels));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        if (   cChannels != 1
            && cChannels != 2)
        {
            LogRel(("ALSA: Number of audio channels (%u) not supported\n", cChannels));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        unsigned int period_size = pCfgReq->period_size;
        unsigned int buffer_size = pCfgReq->buffer_size;

        if (   !((fIn && s_ALSAConf.size_in_usec_in)
            ||  (!fIn && s_ALSAConf.size_in_usec_out)))
        {
            if (!buffer_size)
            {
                buffer_size = DEFAULT_BUFFER_SIZE;
                period_size = DEFAULT_PERIOD_SIZE;
            }
        }

        if (buffer_size)
        {
            if (   ( fIn && s_ALSAConf.size_in_usec_in)
                || (!fIn && s_ALSAConf.size_in_usec_out))
            {
                if (period_size)
                {
                    err = snd_pcm_hw_params_set_period_time_near(phPCM, pHWParms,
                                                                 &period_size, 0);
                    if (err < 0)
                    {
                        LogRel(("ALSA: Failed to set period time %d\n", pCfgReq->period_size));
                        rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                        break;
                    }
                }

                err = snd_pcm_hw_params_set_buffer_time_near(phPCM, pHWParms,
                                                             &buffer_size, 0);
                if (err < 0)
                {
                    LogRel(("ALSA: Failed to set buffer time %d\n", pCfgReq->buffer_size));
                    rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                    break;
                }
            }
            else
            {
                snd_pcm_uframes_t period_size_f = (snd_pcm_uframes_t)period_size;
                snd_pcm_uframes_t buffer_size_f = (snd_pcm_uframes_t)buffer_size;

                snd_pcm_uframes_t minval;

                if (period_size_f)
                {
                    minval = period_size_f;

                    int dir = 0;
                    err = snd_pcm_hw_params_get_period_size_min(pHWParms,
                                                                &minval, &dir);
                    if (err < 0)
                    {
                        LogRel(("ALSA: Could not determine minimal period size\n"));
                        rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                        break;
                    }
                    else
                    {
                        LogFunc(("Minimal period size is: %ld\n", minval));
                        if (period_size_f < minval)
                        {
                            if (   ( fIn && s_ALSAConf.period_size_in_overriden)
                                || (!fIn && s_ALSAConf.period_size_out_overriden))
                            {
                                LogFunc(("Period size %RU32 is less than minimal period size %RU32\n",
                                         period_size_f, minval));
                            }

                            period_size_f = minval;
                        }
                    }

                    err = snd_pcm_hw_params_set_period_size_near(phPCM, pHWParms,
                                                                 &period_size_f, 0);
                    LogFunc(("Period size is: %RU32\n", period_size_f));
                    if (err < 0)
                    {
                        LogRel(("ALSA: Failed to set period size %d (%s)\n",
                                period_size_f, snd_strerror(err)));
                        rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                        break;
                    }
                }

                /* Calculate default buffer size here since it might have been changed
                 * in the _near functions */
                buffer_size_f = 4 * period_size_f;

                minval = buffer_size_f;
                err = snd_pcm_hw_params_get_buffer_size_min(pHWParms, &minval);
                if (err < 0)
                {
                    LogRel(("ALSA: Could not retrieve minimal buffer size\n"));
                    rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                    break;
                }
                else
                {
                    LogFunc(("Minimal buffer size is: %RU32\n", minval));
                    if (buffer_size_f < minval)
                    {
                        if (   ( fIn && s_ALSAConf.buffer_size_in_overriden)
                            || (!fIn && s_ALSAConf.buffer_size_out_overriden))
                        {
                            LogFunc(("Buffer size %RU32 is less than minimal buffer size %RU32\n",
                                     buffer_size_f, minval));
                        }

                        buffer_size_f = minval;
                    }
                }

                err = snd_pcm_hw_params_set_buffer_size_near(phPCM,
                                                             pHWParms, &buffer_size_f);
                LogFunc(("Buffer size is: %RU32\n", buffer_size_f));
                if (err < 0)
                {
                    LogRel(("ALSA: Failed to set buffer size %d: %s\n",
                            buffer_size_f, snd_strerror(err)));
                    rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                    break;
                }
            }
        }
        else
            LogFunc(("Warning: Buffer size is not set\n"));

        err = snd_pcm_hw_params(phPCM, pHWParms);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to apply audio parameters\n"));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        err = snd_pcm_hw_params_get_buffer_size(pHWParms, &obt_buffer_size);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to get buffer size\n"));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        snd_pcm_uframes_t obt_period_size;
        int dir = 0;
        err = snd_pcm_hw_params_get_period_size(pHWParms, &obt_period_size, &dir);
        if (err < 0)
        {
            LogRel(("ALSA: Failed to get period size\n"));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        LogFunc(("Freq=%dHz, period size=%RU32, buffer size=%RU32\n",
                 pCfgReq->freq, obt_period_size, obt_buffer_size));

        err = snd_pcm_prepare(phPCM);
        if (err < 0)
        {
            LogRel(("ALSA: Could not prepare hPCM %p\n", (void *)phPCM));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        if (   !fIn
            && s_ALSAConf.threshold)
        {
            unsigned uShift;
            rc = alsaGetSampleShift(pCfgReq->fmt, &uShift);
            if (RT_SUCCESS(rc))
            {
                int bytes_per_sec = uFreq
                    << (cChannels == 2)
                    << uShift;

                snd_pcm_uframes_t threshold
                    = (s_ALSAConf.threshold * bytes_per_sec) / 1000;

                rc = alsaStreamSetThreshold(phPCM, threshold);
            }
        }
        else
            rc = VINF_SUCCESS;
    }
    while (0);

    if (RT_SUCCESS(rc))
    {
        pCfgObt->fmt       = pCfgReq->fmt;
        pCfgObt->nchannels = cChannels;
        pCfgObt->freq      = uFreq;
        pCfgObt->samples   = obt_buffer_size;

        *pphPCM = phPCM;
    }
    else
        alsaStreamClose(&phPCM);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef DEBUG
static void alsaDbgErrorHandler(const char *file, int line, const char *function,
                                int err, const char *fmt, ...)
{
    /** @todo Implement me! */
}
#endif

static int alsaStreamGetAvail(snd_pcm_t *phPCM, snd_pcm_sframes_t *pFramesAvail)
{
    AssertPtrReturn(phPCM, VERR_INVALID_POINTER);
    AssertPtrReturn(pFramesAvail, VERR_INVALID_POINTER);

    int rc;

    snd_pcm_sframes_t framesAvail;
    framesAvail = snd_pcm_avail_update(phPCM);
    if (framesAvail < 0)
    {
        if (framesAvail == -EPIPE)
        {
            rc = alsaStreamRecover(phPCM);
            if (RT_SUCCESS(rc))
                framesAvail = snd_pcm_avail_update(phPCM);
        }
        else
            rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
    }
    else
        rc = VINF_SUCCESS;

    if (framesAvail >= 0)
        *pFramesAvail = framesAvail;

    return rc;
}

static int alsaStreamRecover(snd_pcm_t *phPCM)
{
    AssertPtrReturn(phPCM, VERR_INVALID_POINTER);

    int err = snd_pcm_prepare(phPCM);
    if (err < 0)
    {
        LogFunc(("Failed to recover stream %p: %s\n", phPCM, snd_strerror(err)));
        return VERR_ACCESS_DENIED; /** @todo Find a better rc. */
    }

    return VINF_SUCCESS;
}

static int alsaStreamResume(snd_pcm_t *phPCM)
{
    AssertPtrReturn(phPCM, VERR_INVALID_POINTER);

    int err = snd_pcm_resume(phPCM);
    if (err < 0)
    {
        LogFunc(("Failed to resume stream %p: %s\n", phPCM, snd_strerror(err)));
        return VERR_ACCESS_DENIED; /** @todo Find a better rc. */
    }

    return VINF_SUCCESS;
}

static int drvHostALSAAudioStreamCtl(snd_pcm_t *phPCM, bool fPause)
{
    int err;
    if (fPause)
    {
        err = snd_pcm_drop(phPCM);
        if (err < 0)
        {
            LogRel(("ALSA: Error stopping stream %p: %s\n", phPCM, snd_strerror(err)));
            return VERR_ACCESS_DENIED;
        }
    }
    else
    {
        err = snd_pcm_prepare(phPCM);
        if (err < 0)
        {
            LogRel(("ALSA: Error preparing stream %p: %s\n", phPCM, snd_strerror(err)));
            return VERR_ACCESS_DENIED;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostALSAAudioInit(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    LogFlowFuncEnter();

    int rc = audioLoadAlsaLib();
    if (RT_FAILURE(rc))
        LogRel(("ALSA: Failed to load the ALSA shared library, rc=%Rrc\n", rc));
    else
    {
#ifdef DEBUG
        snd_lib_error_set_handler(alsaDbgErrorHandler);
#endif
    }

    return rc;
}

static DECLCALLBACK(int) drvHostALSAAudioCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   uint32_t *pcSamplesCaptured)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    PALSAAUDIOSTREAMIN pThisStrmIn = (PALSAAUDIOSTREAMIN)pHstStrmIn;

    snd_pcm_sframes_t cAvail;
    int rc = alsaStreamGetAvail(pThisStrmIn->phPCM, &cAvail);
    if (RT_FAILURE(rc))
    {
        LogFunc(("Error getting number of captured frames, rc=%Rrc\n", rc));
        return rc;
    }

    if (!cAvail) /* No data yet? */
    {
        snd_pcm_state_t state = snd_pcm_state(pThisStrmIn->phPCM);
        switch (state)
        {
            case SND_PCM_STATE_PREPARED:
                cAvail = AudioMixBufFree(&pHstStrmIn->MixBuf);
                break;

            case SND_PCM_STATE_SUSPENDED:
            {
                rc = alsaStreamResume(pThisStrmIn->phPCM);
                if (RT_FAILURE(rc))
                    break;

                LogFlow(("Resuming suspended input stream\n"));
                break;
            }

            default:
                LogFlow(("No frames available, state=%d\n", state));
                break;
        }

        if (!cAvail)
        {
            if (pcSamplesCaptured)
                *pcSamplesCaptured = 0;
            return VINF_SUCCESS;
        }
    }

    /*
     * Check how much we can read from the capture device without overflowing
     * the mixer buffer.
     */
    Assert(cAvail);
    size_t cbMixFree = AudioMixBufFreeBytes(&pHstStrmIn->MixBuf);
    size_t cbToRead = RT_MIN((size_t)AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cAvail), cbMixFree);

    LogFlowFunc(("cbToRead=%zu, cAvail=%RI32\n", cbToRead, cAvail));

    uint32_t cWrittenTotal = 0;
    snd_pcm_uframes_t cToRead;
    snd_pcm_sframes_t cRead;

    while (   cbToRead
           && RT_SUCCESS(rc))
    {
        cToRead = RT_MIN(AUDIOMIXBUF_B2S(&pHstStrmIn->MixBuf, cbToRead),
                         AUDIOMIXBUF_B2S(&pHstStrmIn->MixBuf, pThisStrmIn->cbBuf));
        AssertBreakStmt(cToRead, rc = VERR_NO_DATA);
        cRead = snd_pcm_readi(pThisStrmIn->phPCM, pThisStrmIn->pvBuf, cToRead);
        if (cRead <= 0)
        {
            switch (cRead)
            {
                case 0:
                {
                    LogFunc(("No input frames available\n"));
                    rc = VERR_ACCESS_DENIED;
                    break;
                }

                case -EAGAIN:
                {
                    /*
                     * Don't set error here because EAGAIN means there are no further frames
                     * available at the moment, try later. As we might have read some frames
                     * already these need to be processed instead.
                     */
                    cbToRead = 0;
                    break;
                }

                case -EPIPE:
                {
                    rc = alsaStreamRecover(pThisStrmIn->phPCM);
                    if (RT_FAILURE(rc))
                        break;

                    LogFlowFunc(("Recovered from capturing\n"));
                    continue;
                }

                default:
                {
                    LogFunc(("Failed to read input frames: %s\n", snd_strerror(cRead)));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
                }
            }
        }
        else
        {
            uint32_t cWritten;
            rc = AudioMixBufWriteCirc(&pHstStrmIn->MixBuf,
                                      pThisStrmIn->pvBuf, AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cRead),
                                      &cWritten);
            if (RT_FAILURE(rc))
                break;

            /*
             * We should not run into a full mixer buffer or we loose samples and
             * run into an endless loop if ALSA keeps producing samples ("null"
             * capture device for example).
             */
            AssertLogRelMsgBreakStmt(cWritten > 0, ("Mixer buffer shouldn't be full at this point!\n"),
                                     rc = VERR_INTERNAL_ERROR);
            uint32_t cbWritten = AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cWritten);

            Assert(cbToRead >= cbWritten);
            cbToRead -= cbWritten;
            cWrittenTotal += cWritten;
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

static DECLCALLBACK(int) drvHostALSAAudioPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                 uint32_t *pcSamplesPlayed)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    PALSAAUDIOSTREAMOUT pThisStrmOut = (PALSAAUDIOSTREAMOUT)pHstStrmOut;

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;

    do
    {
        snd_pcm_sframes_t cAvail;
        rc = alsaStreamGetAvail(pThisStrmOut->phPCM, &cAvail);
        if (RT_FAILURE(rc))
        {
            LogFunc(("Error getting number of playback frames, rc=%Rrc\n", rc));
            break;
        }

        size_t cbToRead = RT_MIN(AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf,
                                                 (uint32_t)cAvail), /* cAvail is always >= 0 */
                                 AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf,
                                                 AudioMixBufAvail(&pHstStrmOut->MixBuf)));
        LogFlowFunc(("cbToRead=%zu, cbAvail=%zu\n",
                     cbToRead, AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cAvail)));

        uint32_t cRead, cbRead;
        snd_pcm_sframes_t cWritten;
        while (cbToRead)
        {
            rc = AudioMixBufReadCirc(&pHstStrmOut->MixBuf, pThisStrmOut->pvBuf, cbToRead, &cRead);
            if (RT_FAILURE(rc))
                break;

            cbRead = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cRead);
            AssertBreak(cbRead);

            /* Don't try infinitely on recoverable errors. */
            unsigned iTry;
            for (iTry = 0; iTry < ALSA_RECOVERY_TRIES_MAX; iTry++)
            {
                cWritten = snd_pcm_writei(pThisStrmOut->phPCM, pThisStrmOut->pvBuf, cRead);
                if (cWritten <= 0)
                {
                    switch (cWritten)
                    {
                        case 0:
                        {
                            LogFunc(("Failed to write %RI32 frames\n", cRead));
                            rc = VERR_ACCESS_DENIED;
                            break;
                        }

                        case -EPIPE:
                        {
                            rc = alsaStreamRecover(pThisStrmOut->phPCM);
                            if (RT_FAILURE(rc))
                                break;

                            LogFlowFunc(("Recovered from playback\n"));
                            continue;
                        }

                        case -ESTRPIPE:
                        {
                            /* Stream was suspended and waiting for a recovery. */
                            rc = alsaStreamResume(pThisStrmOut->phPCM);
                            if (RT_FAILURE(rc))
                            {
                                LogRel(("ALSA: Failed to resume output stream\n"));
                                break;
                            }

                            LogFlowFunc(("Resumed suspended output stream\n"));
                            continue;
                        }

                        default:
                            LogFlowFunc(("Failed to write %RI32 output frames, rc=%Rrc\n",
                                         cRead, rc));
                            rc = VERR_GENERAL_FAILURE; /** @todo */
                            break;
                    }
                }
                else
                    break;
            } /* For number of tries. */

            if (   iTry == ALSA_RECOVERY_TRIES_MAX
                && cWritten <= 0)
                rc = VERR_BROKEN_PIPE;

            if (RT_FAILURE(rc))
                break;

            Assert(cbToRead >= cbRead);
            cbToRead -= cbRead;
            cbReadTotal += cbRead;
        }
    }
    while (0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes), rc=%Rrc\n",
                     cReadTotal, cbReadTotal, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostALSAAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    PALSAAUDIOSTREAMIN pThisStrmIn = (PALSAAUDIOSTREAMIN)pHstStrmIn;

    alsaStreamClose(&pThisStrmIn->phPCM);

    if (pThisStrmIn->pvBuf)
    {
        RTMemFree(pThisStrmIn->pvBuf);
        pThisStrmIn->pvBuf = NULL;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostALSAAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    PALSAAUDIOSTREAMOUT pThisStrmOut = (PALSAAUDIOSTREAMOUT)pHstStrmOut;

    alsaStreamClose(&pThisStrmOut->phPCM);

    if (pThisStrmOut->pvBuf)
    {
        RTMemFree(pThisStrmOut->pvBuf);
        pThisStrmOut->pvBuf = NULL;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostALSAAudioInitOut(PPDMIHOSTAUDIO pInterface,
                                                 PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                                 uint32_t *pcSamples)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    PALSAAUDIOSTREAMOUT pThisStrmOut = (PALSAAUDIOSTREAMOUT)pHstStrmOut;
    snd_pcm_t *phPCM = NULL;

    int rc;

    do
    {
        ALSAAUDIOSTREAMCFG req;
        req.fmt         = alsaAudioFmtToALSA(pCfg->enmFormat);
        req.freq        = pCfg->uHz;
        req.nchannels   = pCfg->cChannels;
        req.period_size = s_ALSAConf.period_size_out;
        req.buffer_size = s_ALSAConf.buffer_size_out;

        ALSAAUDIOSTREAMCFG obt;
        rc = alsaStreamOpen(false /* fIn */, &req, &obt, &phPCM);
        if (RT_FAILURE(rc))
            break;

        PDMAUDIOFMT enmFormat;
        PDMAUDIOENDIANNESS enmEnd;
        rc = alsaALSAToAudioFmt(obt.fmt, &enmFormat, &enmEnd);
        if (RT_FAILURE(rc))
            break;

        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = obt.freq;
        streamCfg.cChannels     = obt.nchannels;
        streamCfg.enmFormat     = enmFormat;
        streamCfg.enmEndianness = enmEnd;

        rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmOut->Props);
        if (RT_FAILURE(rc))
            break;

        AssertBreakStmt(obt.samples, rc = VERR_INVALID_PARAMETER);
        size_t cbBuf = obt.samples * (1 << pHstStrmOut->Props.cShift);
        AssertBreakStmt(cbBuf, rc = VERR_INVALID_PARAMETER);
        pThisStrmOut->pvBuf = RTMemAlloc(cbBuf);
        if (!pThisStrmOut->pvBuf)
        {
            LogRel(("ALSA: Not enough memory for output DAC buffer (%RU32 samples, each %d bytes)\n",
                    obt.samples, 1 << pHstStrmOut->Props.cShift));
            rc = VERR_NO_MEMORY;
            break;
        }

        pThisStrmOut->cbBuf       = cbBuf;
        pThisStrmOut->phPCM       = phPCM;

        if (pcSamples)
            *pcSamples = obt.samples;
    }
    while (0);

    if (RT_FAILURE(rc))
        alsaStreamClose(&phPCM);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostALSAAudioInitIn(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                                PDMAUDIORECSOURCE enmRecSource,
                                                uint32_t *pcSamples)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    int rc;

    PALSAAUDIOSTREAMIN pThisStrmIn = (PALSAAUDIOSTREAMIN)pHstStrmIn;
    snd_pcm_t *phPCM = NULL;

    do
    {
        ALSAAUDIOSTREAMCFG req;
        req.fmt         = alsaAudioFmtToALSA(pCfg->enmFormat);
        req.freq        = pCfg->uHz;
        req.nchannels   = pCfg->cChannels;
        req.period_size = s_ALSAConf.period_size_in;
        req.buffer_size = s_ALSAConf.buffer_size_in;

        ALSAAUDIOSTREAMCFG obt;
        rc = alsaStreamOpen(true /* fIn */, &req, &obt, &phPCM);
        if (RT_FAILURE(rc))
            break;

        PDMAUDIOFMT enmFormat;
        PDMAUDIOENDIANNESS enmEnd;
        rc = alsaALSAToAudioFmt(obt.fmt, &enmFormat, &enmEnd);
        if (RT_FAILURE(rc))
            break;

        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = obt.freq;
        streamCfg.cChannels     = obt.nchannels;
        streamCfg.enmFormat     = enmFormat;
        streamCfg.enmEndianness = enmEnd;

        rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmIn->Props);
        if (RT_FAILURE(rc))
            break;

        AssertBreakStmt(obt.samples, rc = VERR_INVALID_PARAMETER);
        size_t cbBuf = obt.samples * (1 << pHstStrmIn->Props.cShift);
        AssertBreakStmt(cbBuf, rc = VERR_INVALID_PARAMETER);
        pThisStrmIn->pvBuf = RTMemAlloc(cbBuf);
        if (!pThisStrmIn->pvBuf)
        {
            LogRel(("ALSA: Not enough memory for input ADC buffer (%RU32 samples, each %d bytes)\n",
                    obt.samples, 1 << pHstStrmIn->Props.cShift));
            rc = VERR_NO_MEMORY;
            break;
        }

        pThisStrmIn->cbBuf       = cbBuf;
        pThisStrmIn->phPCM       = phPCM;

        if (pcSamples)
            *pcSamples = obt.samples;
    }
    while (0);

    if (RT_FAILURE(rc))
        alsaStreamClose(&phPCM);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(bool) drvHostALSAAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostALSAAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    PALSAAUDIOSTREAMIN pThisStrmIn = (PALSAAUDIOSTREAMIN)pHstStrmIn;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    int rc;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
            rc = drvHostALSAAudioStreamCtl(pThisStrmIn->phPCM, false /* fStop */);
            break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
            rc = drvHostALSAAudioStreamCtl(pThisStrmIn->phPCM, true /* fStop */);
            break;

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostALSAAudioControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                    PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    PALSAAUDIOSTREAMOUT pThisStrmOut = (PALSAAUDIOSTREAMOUT)pHstStrmOut;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    int rc;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
            rc = drvHostALSAAudioStreamCtl(pThisStrmOut->phPCM, false /* fStop */);
            break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
            rc = drvHostALSAAudioStreamCtl(pThisStrmOut->phPCM, true /* fStop */);
            break;

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostALSAAudioGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    pCfg->cbStreamIn  = sizeof(ALSAAUDIOSTREAMIN);
    pCfg->cbStreamOut = sizeof(ALSAAUDIOSTREAMOUT);

    pCfg->cSources    = 0;
    pCfg->cSinks      = 0;

    /* Enumerate sound devices. */
    char **pszHints;
    int err = snd_device_name_hint(-1 /* All cards */, "pcm", (void***)&pszHints);
    if (err == 0)
    {
        char** pszHintCur = pszHints;
        while (*pszHintCur != NULL)
        {
            char *pszDev = snd_device_name_get_hint(*pszHintCur, "NAME");
            bool fSkip =    !pszDev
                         || !RTStrICmp("null", pszDev);
            if (fSkip)
            {
                if (pszDev)
                    free(pszDev);
                pszHintCur++;
                continue;
            }

            char *pszIOID = snd_device_name_get_hint(*pszHintCur, "IOID");
            if (pszIOID)
            {
                if (!RTStrICmp("input", pszIOID))
                    pCfg->cSources++;
                else if (!RTStrICmp("output", pszIOID))
                    pCfg->cSinks++;
            }
            else /* NULL means bidirectional, input + output. */
            {
                pCfg->cSources++;
                pCfg->cSinks++;
            }

            LogRel2(("ALSA: Found %s device: %s\n", pszIOID ?  RTStrToLower(pszIOID) : "bidirectional", pszDev));

            /* Special case for PulseAudio. */
            if (   pszDev
                && RTStrIStr("pulse", pszDev) != NULL)
                LogRel2(("ALSA: PulseAudio plugin in use\n"));

            if (pszIOID)
                free(pszIOID);

            if (pszDev)
                free(pszDev);

            pszHintCur++;
        }

        LogRel2(("ALSA: Found %RU8 host playback devices\n",  pCfg->cSinks));
        LogRel2(("ALSA: Found %RU8 host capturing devices\n", pCfg->cSources));

        snd_device_name_free_hint((void **)pszHints);
        pszHints = NULL;
    }
    else
        LogRel2(("ALSA: Error enumerating PCM devices: %Rrc (%d)\n", RTErrConvertFromErrno(err), err));

    /* ALSA allows exactly one input and one output used at a time for the selected device(s). */
    pCfg->cMaxStreamsIn  = 1;
    pCfg->cMaxStreamsOut = 1;

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvHostALSAAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostALSAAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTALSAAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTALSAAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}

/**
 * Construct a DirectSound Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostAlsaAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTALSAAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTALSAAUDIO);
    LogRel(("Audio: Initializing ALSA driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostALSAAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostALSAAudio);

    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostALSAAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "ALSAAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "ALSA host audio driver",
    /* fFlags */
     PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTALSAAUDIO),
    /* pfnConstruct */
    drvHostAlsaAudioConstruct,
    /* pfnDestruct */
    NULL,
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

static struct audio_option alsa_options[] =
{
    {"DACSizeInUsec", AUD_OPT_BOOL, &s_ALSAConf.size_in_usec_out,
     "DAC period/buffer size in microseconds (otherwise in frames)", NULL, 0},
    {"DACPeriodSize", AUD_OPT_INT, &s_ALSAConf.period_size_out,
     "DAC period size", &s_ALSAConf.period_size_out_overriden, 0},
    {"DACBufferSize", AUD_OPT_INT, &s_ALSAConf.buffer_size_out,
     "DAC buffer size", &s_ALSAConf.buffer_size_out_overriden, 0},

    {"ADCSizeInUsec", AUD_OPT_BOOL, &s_ALSAConf.size_in_usec_in,
     "ADC period/buffer size in microseconds (otherwise in frames)", NULL, 0},
    {"ADCPeriodSize", AUD_OPT_INT, &s_ALSAConf.period_size_in,
     "ADC period size", &s_ALSAConf.period_size_in_overriden, 0},
    {"ADCBufferSize", AUD_OPT_INT, &s_ALSAConf.buffer_size_in,
     "ADC buffer size", &s_ALSAConf.buffer_size_in_overriden, 0},

    {"Threshold", AUD_OPT_INT, &s_ALSAConf.threshold,
     "(undocumented)", NULL, 0},

    {"DACDev", AUD_OPT_STR, &s_ALSAConf.pcm_name_out,
     "DAC device name (for instance dmix)", NULL, 0},

    {"ADCDev", AUD_OPT_STR, &s_ALSAConf.pcm_name_in,
     "ADC device name", NULL, 0}
};

