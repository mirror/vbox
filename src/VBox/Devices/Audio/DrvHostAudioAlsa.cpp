/* $Id$ */
/** @file
 * Host audio driver - Advanced Linux Sound Architecture (ALSA).
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <VBox/vmm/pdmaudioinline.h>

RT_C_DECLS_BEGIN
#include "DrvHostAudioAlsaStubs.h"
#include "DrvHostAudioAlsaStubsMangling.h"
RT_C_DECLS_END

#include <alsa/asoundlib.h>
#include <alsa/control.h> /* For device enumeration. */

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/
/**
 * Structure for maintaining an ALSA audio stream.
 */
typedef struct ALSAAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG pCfg;
    /** Pointer to allocated ALSA PCM configuration to use. */
    snd_pcm_t          *phPCM;
    /** Internal stream offset (for debugging). */
    uint64_t            offInternal;
} ALSAAUDIOSTREAM, *PALSAAUDIOSTREAM;

/* latency = period_size * periods / (rate * bytes_per_frame) */

static int alsaStreamRecover(snd_pcm_t *phPCM);

/**
 * Host Alsa audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTALSAAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
    /** Error count for not flooding the release log.
     *  UINT32_MAX for unlimited logging. */
    uint32_t            cLogErrors;
    /** Default input device name.   */
    char                szDefaultIn[256];
    /** Default output device name. */
    char                szDefaultOut[256];
} DRVHOSTALSAAUDIO, *PDRVHOSTALSAAUDIO;

/** Maximum number of tries to recover a broken pipe. */
#define ALSA_RECOVERY_TRIES_MAX    5

/**
 * Structure for maintaining an ALSA audio stream configuration.
 */
typedef struct ALSAAUDIOSTREAMCFG
{
    unsigned int        freq;
    /** PCM sound format. */
    snd_pcm_format_t    fmt;
    /** PCM data access type. */
    snd_pcm_access_t    access;
    /** Whether resampling should be performed by alsalib or not. */
    int                 resample;
    /** Number of audio channels. */
    int                 nchannels;
    /** Buffer size (in audio frames). */
    unsigned long       buffer_size;
    /** Periods (in audio frames). */
    unsigned long       period_size;
    /** For playback: Starting to play threshold (in audio frames).
     *  For Capturing: Starting to capture threshold (in audio frames). */
    unsigned long       threshold;
} ALSAAUDIOSTREAMCFG, *PALSAAUDIOSTREAMCFG;



/**
 * Closes an ALSA stream
 *
 * @returns VBox status code.
 * @param   pphPCM              ALSA stream to close.
 */
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

    LogFlowFuncLeaveRC(rc);
    return rc;
}


#ifdef DEBUG
static void alsaDbgErrorHandler(const char *file, int line, const char *function,
                                int err, const char *fmt, ...)
{
    /** @todo Implement me! */
    RT_NOREF(file, line, function, err, fmt);
}
#endif


/**
 * Tries to recover an ALSA stream.
 *
 * @returns VBox status code.
 * @param   phPCM               ALSA stream handle.
 */
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

/**
 * Resumes an ALSA stream.
 *
 * @returns VBox status code.
 * @param   phPCM               ALSA stream to resume.
 */
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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RTStrPrintf2(pBackendCfg->szName, sizeof(pBackendCfg->szName), "ALSA");

    pBackendCfg->cbStreamIn  = sizeof(ALSAAUDIOSTREAM);
    pBackendCfg->cbStreamOut = sizeof(ALSAAUDIOSTREAM);

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
#if 0
                if (!RTStrICmp("input", pszIOID))

                else if (!RTStrICmp("output", pszIOID))
#endif
            }
            else /* NULL means bidirectional, input + output. */
            {
            }

            LogRel2(("ALSA: Found %s device: %s\n", pszIOID ?  RTStrToLower(pszIOID) : "bidirectional", pszDev));

            /* Special case for ALSAAudio. */
            if (   pszDev
                && RTStrIStr("pulse", pszDev) != NULL)
                LogRel2(("ALSA: ALSAAudio plugin in use\n"));

            if (pszIOID)
                free(pszIOID);

            if (pszDev)
                free(pszDev);

            pszHintCur++;
        }

        snd_device_name_free_hint((void **)pszHints);
        pszHints = NULL;
    }
    else
        LogRel2(("ALSA: Error enumerating PCM devices: %Rrc (%d)\n", RTErrConvertFromErrno(err), err));

    /* ALSA allows exactly one input and one output used at a time for the selected device(s). */
    pBackendCfg->cMaxStreamsIn  = 1;
    pBackendCfg->cMaxStreamsOut = 1;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostAlsaAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * Converts internal audio PCM properties to an ALSA PCM format.
 *
 * @returns Converted ALSA PCM format.
 * @param   pProps              Internal audio PCM configuration to convert.
 */
static snd_pcm_format_t alsaAudioPropsToALSA(PPDMAUDIOPCMPROPS pProps)
{
    switch (PDMAudioPropsSampleSize(pProps))
    {
        case 1:
            return pProps->fSigned ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_U8;

        case 2:
            if (PDMAudioPropsIsLittleEndian(pProps))
                return pProps->fSigned ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U16_LE;
            return pProps->fSigned ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_U16_BE;

        case 4:
            if (PDMAudioPropsIsLittleEndian(pProps))
                return pProps->fSigned ? SND_PCM_FORMAT_S32_LE : SND_PCM_FORMAT_U32_LE;
            return pProps->fSigned ? SND_PCM_FORMAT_S32_BE : SND_PCM_FORMAT_U32_BE;

        default:
            AssertMsgFailed(("%RU8 bytes not supported\n", PDMAudioPropsSampleSize(pProps)));
            return SND_PCM_FORMAT_U8;
    }
}


/**
 * Converts an ALSA PCM format to internal PCM properties.
 *
 * @returns VBox status code.
 * @param   pProps      Where to store the converted PCM properties on success.
 * @param   fmt         ALSA PCM format to convert.
 * @param   cChannels   Number of channels.
 * @param   uHz         Frequency.
 */
static int alsaALSAToAudioProps(PPDMAUDIOPCMPROPS pProps, snd_pcm_format_t fmt, int cChannels, unsigned uHz)
{
    AssertReturn(cChannels > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cChannels < 16, VERR_INVALID_PARAMETER);
    switch (fmt)
    {
        case SND_PCM_FORMAT_S8:
            PDMAudioPropsInit(pProps, 1 /*8-bit*/,  true /*signed*/, cChannels, uHz);
            break;

        case SND_PCM_FORMAT_U8:
            PDMAudioPropsInit(pProps, 1 /*8-bit*/, false /*signed*/, cChannels, uHz);
            break;

        case SND_PCM_FORMAT_S16_LE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/,  true /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_U16_LE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, false /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_S16_BE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/,  true /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_U16_BE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, false /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_S32_LE:
            PDMAudioPropsInitEx(pProps, 4 /*32-bit*/,  true /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_U32_LE:
            PDMAudioPropsInitEx(pProps, 4 /*32-bit*/, false /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_S32_BE:
            PDMAudioPropsInitEx(pProps, 4 /*32-bit*/,  true /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        case SND_PCM_FORMAT_U32_BE:
            PDMAudioPropsInitEx(pProps, 4 /*32-bit*/, false /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        default:
            AssertMsgFailedReturn(("Format %d not supported\n", fmt), VERR_NOT_SUPPORTED);
    }
    return VINF_SUCCESS;
}


/**
 * Sets the software parameters of an ALSA stream.
 *
 * @returns 0 on success, negative errno on failure.
 * @param   phPCM               ALSA stream to set software parameters for.
 * @param   fIn                 Whether this is an input stream or not.
 * @param   pCfgReq             Requested configuration to set.
 * @param   pCfgObt             Obtained configuration on success. Might differ from requested configuration.
 */
static int alsaStreamSetSWParams(snd_pcm_t *phPCM, bool fIn, PALSAAUDIOSTREAMCFG pCfgReq, PALSAAUDIOSTREAMCFG pCfgObt)
{
    if (fIn) /* For input streams there's nothing to do in here right now. */
        return VINF_SUCCESS;

    snd_pcm_sw_params_t *pSWParms = NULL;
    snd_pcm_sw_params_alloca(&pSWParms);
    AssertReturn(pSWParms, -ENOMEM);

    int err = snd_pcm_sw_params_current(phPCM, pSWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to get current software parameters: %s\n", snd_strerror(err)), err);

    /* Must make sure we don't require ALSA to prebuffer more than
       it has buffer space for, because that means output will
       never start. */
    unsigned long cFramesPreBuffer = pCfgReq->threshold;
    if (cFramesPreBuffer >= pCfgObt->buffer_size - pCfgObt->buffer_size / 16)
    {
        cFramesPreBuffer = pCfgObt->buffer_size - pCfgObt->buffer_size / 16;
        LogRel2(("ALSA: Reducing threshold from %lu to %lu due to buffer size of %lu.\n",
                 pCfgReq->threshold, cFramesPreBuffer, pCfgObt->buffer_size));
    }
    err = snd_pcm_sw_params_set_start_threshold(phPCM, pSWParms, cFramesPreBuffer);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set software threshold to %lu: %s\n", cFramesPreBuffer, snd_strerror(err)), err);

    err = snd_pcm_sw_params_set_avail_min(phPCM, pSWParms, pCfgReq->period_size);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set available minimum to %lu: %s\n", pCfgReq->period_size, snd_strerror(err)), err);

    /* Commit the software parameters: */
    err = snd_pcm_sw_params(phPCM, pSWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set new software parameters: %s\n", snd_strerror(err)), err);

    /* Get the actual parameters: */
    err = snd_pcm_sw_params_get_start_threshold(pSWParms, &pCfgObt->threshold);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to get start threshold: %s\n", snd_strerror(err)), err);

    LogRel2(("ALSA: SW params: %ul frames threshold, %ul frame avail minimum\n",
             pCfgObt->threshold, pCfgReq->period_size));
    return 0;
}


/**
 * Sets the hardware parameters of an ALSA stream.
 *
 * @returns 0 on success, negative errno on failure.
 * @param   phPCM   ALSA stream to set software parameters for.
 * @param   pCfgReq Requested configuration to set.
 * @param   pCfgObt Obtained configuration on success. Might differ from
 *                  requested configuration.
 */
static int alsaStreamSetHwParams(snd_pcm_t *phPCM, PALSAAUDIOSTREAMCFG pCfgReq, PALSAAUDIOSTREAMCFG pCfgObt)
{
    /*
     * Get the current hardware parameters.
     */
    snd_pcm_hw_params_t *pHWParms = NULL;
    snd_pcm_hw_params_alloca(&pHWParms);
    AssertReturn(pHWParms, -ENOMEM);

    int err = snd_pcm_hw_params_any(phPCM, pHWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to initialize hardware parameters: %s\n", snd_strerror(err)), err);

    /*
     * Modify them according to pCfgReq.
     * We update pCfgObt as we go for parameters set by "near" methods.
     */
    /* We'll use snd_pcm_writei/snd_pcm_readi: */
    err = snd_pcm_hw_params_set_access(phPCM, pHWParms, SND_PCM_ACCESS_RW_INTERLEAVED);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set access type: %s\n", snd_strerror(err)), err);

    /* Set the format, frequency and channel count. */
    err = snd_pcm_hw_params_set_format(phPCM, pHWParms, pCfgReq->fmt);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set audio format to %d: %s\n", pCfgReq->fmt, snd_strerror(err)), err);

    unsigned int uFreq = pCfgReq->freq;
    err = snd_pcm_hw_params_set_rate_near(phPCM, pHWParms, &uFreq, NULL /*dir*/);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set frequency to %uHz: %s\n", pCfgReq->freq, snd_strerror(err)), err);
    pCfgObt->freq      = uFreq;

    unsigned int cChannels = pCfgReq->nchannels;
    err = snd_pcm_hw_params_set_channels_near(phPCM, pHWParms, &cChannels);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set number of channels to %d\n", pCfgReq->nchannels), err);
    AssertLogRelMsgReturn(cChannels == 1 || cChannels == 2, ("ALSA: Number of audio channels (%u) not supported\n", cChannels), -1);
    pCfgObt->nchannels = cChannels;

    /* The period size (reportedly frame count per hw interrupt): */
    int               dir    = 0;
    snd_pcm_uframes_t minval = pCfgReq->period_size;
    err = snd_pcm_hw_params_get_period_size_min(pHWParms, &minval, &dir);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Could not determine minimal period size: %s\n", snd_strerror(err)), err);

    snd_pcm_uframes_t period_size_f = pCfgReq->period_size;
    if (period_size_f < minval)
        period_size_f = minval;
    err = snd_pcm_hw_params_set_period_size_near(phPCM, pHWParms, &period_size_f, 0);
    LogRel2(("ALSA: Period size is: %lu frames (min %lu, requested %lu)\n", period_size_f, minval, pCfgReq->period_size));
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set period size %d (%s)\n", period_size_f, snd_strerror(err)), err);

    /* The buffer size: */
    minval = pCfgReq->buffer_size;
    err = snd_pcm_hw_params_get_buffer_size_min(pHWParms, &minval);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Could not retrieve minimal buffer size: %s\n", snd_strerror(err)), err);

    snd_pcm_uframes_t buffer_size_f = pCfgReq->buffer_size;
    if (buffer_size_f < minval)
        buffer_size_f = minval;
    err = snd_pcm_hw_params_set_buffer_size_near(phPCM, pHWParms, &buffer_size_f);
    LogRel2(("ALSA: Buffer size is: %lu frames (min %lu, requested %lu)\n", buffer_size_f, minval, pCfgReq->buffer_size));
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set near buffer size %RU32: %s\n", buffer_size_f, snd_strerror(err)), err);

    /*
     * Set the hardware parameters.
     */
    err = snd_pcm_hw_params(phPCM, pHWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to apply audio parameters: %s\n", snd_strerror(err)), err);

    /*
     * Get relevant parameters and put them in the pCfgObt structure.
     */
    snd_pcm_uframes_t obt_buffer_size = buffer_size_f;
    err = snd_pcm_hw_params_get_buffer_size(pHWParms, &obt_buffer_size);
    AssertLogRelMsgStmt(err >= 0, ("ALSA: Failed to get buffer size: %s\n", snd_strerror(err)), obt_buffer_size = buffer_size_f);
    pCfgObt->buffer_size = obt_buffer_size;

    snd_pcm_uframes_t obt_period_size = period_size_f;
    err = snd_pcm_hw_params_get_period_size(pHWParms, &obt_period_size, &dir);
    AssertLogRelMsgStmt(err >= 0, ("ALSA: Failed to get period size: %s\n", snd_strerror(err)), obt_period_size = period_size_f);
    pCfgObt->period_size = obt_period_size;

    pCfgObt->access  = pCfgReq->access;
    pCfgObt->fmt     = pCfgReq->fmt;

    LogRel2(("ALSA: HW params: %u Hz, %lu frames period, %lu frames buffer, %u channel(s), fmt=%d, access=%d\n",
             pCfgObt->freq, pCfgObt->period_size, pCfgObt->buffer_size, pCfgObt->nchannels, pCfgObt->fmt, pCfgObt->access));
    return 0;
}


/**
 * Opens (creates) an ALSA stream.
 *
 * @returns VBox status code.
 * @param   pszDev  The name of the device to open.
 * @param   fIn     Whether this is an input stream to create or not.
 * @param   pCfgReq Requested configuration to create stream with.
 * @param   pCfgObt Obtained configuration the stream got created on success.
 * @param   pphPCM  Where to store the ALSA stream handle on success.
 */
static int alsaStreamOpen(const char *pszDev, bool fIn, PALSAAUDIOSTREAMCFG pCfgReq,
                          PALSAAUDIOSTREAMCFG pCfgObt,  snd_pcm_t **pphPCM)
{
    AssertLogRelMsgReturn(pszDev && *pszDev,
                          ("ALSA: Invalid or no %s device name set\n", fIn ? "input" : "output"),
                          VERR_INVALID_NAME);

    /*
     * Open the stream.
     */
    int rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    snd_pcm_t *phPCM = NULL;
    LogRel(("ALSA: Using %s device \"%s\"\n", fIn ? "input" : "output", pszDev));
    int err = snd_pcm_open(&phPCM, pszDev,
                           fIn ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                           SND_PCM_NONBLOCK);
    if (err >= 0)
    {
        err = snd_pcm_nonblock(phPCM, 1);
        if (err >= 0)
        {
            /*
             * Configure hardware stream parameters.
             */
            err = alsaStreamSetHwParams(phPCM, pCfgReq, pCfgObt);
            if (err >= 0)
            {
                /*
                 * Prepare it.
                 */
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                err = snd_pcm_prepare(phPCM);
                if (err >= 0)
                {
                    /*
                     * Configure software stream parameters and we're done.
                     */
                    rc = alsaStreamSetSWParams(phPCM, fIn, pCfgReq, pCfgObt);
                    if (RT_SUCCESS(rc))
                    {
                        *pphPCM = phPCM;
                        return VINF_SUCCESS;
                    }
                }
                else
                    LogRel(("ALSA: snd_pcm_prepare failed: %s\n", snd_strerror(err)));
            }
        }
        else
            LogRel(("ALSA: Error setting output non-blocking mode: %s\n", snd_strerror(err)));
        alsaStreamClose(&phPCM);
    }
    else
        LogRel(("ALSA: Failed to open \"%s\" as %s device: %s\n", pszDev, fIn ? "input" : "output", snd_strerror(err)));
    return rc;
}


/**
 * Creates an ALSA output stream.
 *
 * @returns VBox status code.
 * @param   pThis       The ALSA driver instance data.
 * @param   pStreamALSA ALSA output stream to create.
 * @param   pCfgReq     Requested configuration to create stream with.
 * @param   pCfgAcq     Obtained configuration the stream got created
 *                      with on success.
 */
static int alsaCreateStreamOut(PDRVHOSTALSAAUDIO pThis, PALSAAUDIOSTREAM pStreamALSA,
                               PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    snd_pcm_t *phPCM = NULL;

    int rc;

    do
    {
        ALSAAUDIOSTREAMCFG req;
        req.fmt         = alsaAudioPropsToALSA(&pCfgReq->Props);
        req.freq        = PDMAudioPropsHz(&pCfgReq->Props);
        req.nchannels   = PDMAudioPropsChannels(&pCfgReq->Props);
        req.period_size = pCfgReq->Backend.cFramesPeriod;
        req.buffer_size = pCfgReq->Backend.cFramesBufferSize;
        req.threshold   = pCfgReq->Backend.cFramesPreBuffering;

        ALSAAUDIOSTREAMCFG obt;
        rc = alsaStreamOpen(pThis->szDefaultOut, false /* fIn */, &req, &obt, &phPCM);
        if (RT_FAILURE(rc))
            break;

        rc = alsaALSAToAudioProps(&pCfgAcq->Props, obt.fmt, obt.nchannels, obt.freq);
        if (RT_FAILURE(rc))
            break;

        pCfgAcq->Backend.cFramesPeriod     = obt.period_size;
        pCfgAcq->Backend.cFramesBufferSize = obt.buffer_size;
        pCfgAcq->Backend.cFramesPreBuffering     = obt.threshold;

        pStreamALSA->phPCM = phPCM;
    }
    while (0);

    if (RT_FAILURE(rc))
        alsaStreamClose(&phPCM);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Creates an ALSA input stream.
 *
 * @returns VBox status code.
 * @param   pThis       The ALSA driver instance data.
 * @param   pStreamALSA ALSA input stream to create.
 * @param   pCfgReq     Requested configuration to create stream with.
 * @param   pCfgAcq     Obtained configuration the stream got created
 *                      with on success.
 */
static int alsaCreateStreamIn(PDRVHOSTALSAAUDIO pThis, PALSAAUDIOSTREAM pStreamALSA,
                              PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc;

    snd_pcm_t *phPCM = NULL;

    do
    {
        ALSAAUDIOSTREAMCFG req;
        req.fmt         = alsaAudioPropsToALSA(&pCfgReq->Props);
        req.freq        = PDMAudioPropsHz(&pCfgReq->Props);
        req.nchannels   = PDMAudioPropsChannels(&pCfgReq->Props);
        req.period_size = PDMAudioPropsMilliToFrames(&pCfgReq->Props, 50 /*ms*/); /** @todo Make this configurable. */
        req.buffer_size = req.period_size * 2; /** @todo Make this configurable. */
        req.threshold   = req.period_size;

        ALSAAUDIOSTREAMCFG obt;
        rc = alsaStreamOpen(pThis->szDefaultIn, true /* fIn */, &req, &obt, &phPCM);
        if (RT_FAILURE(rc))
            break;

        rc = alsaALSAToAudioProps(&pCfgAcq->Props, obt.fmt, obt.nchannels, obt.freq);
        if (RT_FAILURE(rc))
            break;

        pCfgAcq->Backend.cFramesPeriod     = obt.period_size;
        pCfgAcq->Backend.cFramesBufferSize = obt.buffer_size;
        pCfgAcq->Backend.cFramesPreBuffering = 0; /* No pre-buffering. */

        pStreamALSA->phPCM = phPCM;
    }
    while (0);

    if (RT_FAILURE(rc))
        alsaStreamClose(&phPCM);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTALSAAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTALSAAUDIO, IHostAudio);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = alsaCreateStreamIn( pThis, pStreamALSA, pCfgReq, pCfgAcq);
    else
        rc = alsaCreateStreamOut(pThis, pStreamALSA, pCfgReq, pCfgAcq);
    if (RT_SUCCESS(rc))
    {
        pStreamALSA->pCfg = PDMAudioStrmCfgDup(pCfgAcq);
        if (!pStreamALSA->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    if (!pStreamALSA->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc = alsaStreamClose(&pStreamALSA->phPCM);
    /** @todo r=bird: It's not like we can do much with a bad status... Check
     *        what the caller does... */
    if (RT_SUCCESS(rc))
    {
        PDMAudioStrmCfgFree(pStreamALSA->pCfg);
        pStreamALSA->pCfg = NULL;
    }

    return rc;
}


/**
 * Controls an ALSA input stream.
 *
 * @returns VBox status code.
 * @param   pStreamALSA         ALSA input stream to control.
 * @param   enmStreamCmd        Stream command to issue.
 */
static int alsaControlStreamIn(PALSAAUDIOSTREAM pStreamALSA, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    int rc = VINF_SUCCESS;

    int err;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            err = snd_pcm_prepare(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error preparing input stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            else
            {
                Assert(snd_pcm_state(pStreamALSA->phPCM) == SND_PCM_STATE_PREPARED);

                /* Only start the PCM stream for input streams. */
                err = snd_pcm_start(pStreamALSA->phPCM);
                if (err < 0)
                {
                    LogRel(("ALSA: Error starting input stream: %s\n", snd_strerror(err)));
                    rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
                }
            }

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            err = snd_pcm_drop(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error disabling input stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            err = snd_pcm_drop(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error pausing input stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Controls an ALSA output stream.
 *
 * @returns VBox status code.
 * @param   pStreamALSA         ALSA output stream to control.
 * @param   enmStreamCmd        Stream command to issue.
 */
static int alsaControlStreamOut(PALSAAUDIOSTREAM pStreamALSA, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    int rc = VINF_SUCCESS;

    int err;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            err = snd_pcm_prepare(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error preparing output stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            else
            {
                Assert(snd_pcm_state(pStreamALSA->phPCM) == SND_PCM_STATE_PREPARED);
            }

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            err = snd_pcm_drop(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error disabling output stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /** @todo shouldn't this try snd_pcm_pause first? */
            err = snd_pcm_drop(pStreamALSA->phPCM);
            if (err < 0)
            {
                LogRel(("ALSA: Error pausing output stream: %s\n", snd_strerror(err)));
                rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DRAIN:
        {
            snd_pcm_state_t streamState = snd_pcm_state(pStreamALSA->phPCM);
            Log2Func(("Stream state is: %d\n", streamState));

            if (   streamState == SND_PCM_STATE_PREPARED
                || streamState == SND_PCM_STATE_RUNNING)
            {
                /** @todo r=bird: You want EMT to block here for potentially 200-300ms worth
                 *        of buffer to be drained?  That's a certifiably bad idea.  */
                err = snd_pcm_nonblock(pStreamALSA->phPCM, 0);
                if (err < 0)
                {
                    LogRel(("ALSA: Error disabling output non-blocking mode: %s\n", snd_strerror(err)));
                    rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
                    break;
                }

                err = snd_pcm_drain(pStreamALSA->phPCM);
                if (err < 0)
                {
                    LogRel(("ALSA: Error draining output: %s\n", snd_strerror(err)));
                    rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
                    break;
                }

                err = snd_pcm_nonblock(pStreamALSA->phPCM, 1);
                if (err < 0)
                {
                    LogRel(("ALSA: Error re-enabling output non-blocking mode: %s\n", snd_strerror(err)));
                    rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
                }
            }
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                          PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    if (!pStreamALSA->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamALSA->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = alsaControlStreamIn (pStreamALSA, enmStreamCmd);
    else
        rc = alsaControlStreamOut(pStreamALSA, enmStreamCmd);

    return rc;
}


/**
 * Returns the available audio frames queued.
 *
 * @returns VBox status code.
 * @param   phPCM           ALSA stream handle.
 * @param   pcFramesAvail   Where to store the available frames.
 */
static int alsaStreamGetAvail(snd_pcm_t *phPCM, snd_pcm_sframes_t *pcFramesAvail)
{
    AssertPtr(phPCM);
    AssertPtr(pcFramesAvail);

    int rc;
    snd_pcm_sframes_t cFramesAvail = snd_pcm_avail_update(phPCM);
    if (cFramesAvail > 0)
    {
        LogFunc(("cFramesAvail=%ld\n", cFramesAvail));
        *pcFramesAvail = cFramesAvail;
        return VINF_SUCCESS;
    }

    if (cFramesAvail == -EPIPE)
    {
        rc = alsaStreamRecover(phPCM);
        if (RT_SUCCESS(rc))
        {
            cFramesAvail = snd_pcm_avail_update(phPCM);
            if (cFramesAvail >= 0)
            {
                LogFunc(("cFramesAvail=%ld\n", cFramesAvail));
                *pcFramesAvail = cFramesAvail;
                return VINF_SUCCESS;
            }
        }
        else
        {
            *pcFramesAvail = 0;
            return rc;
        }
    }

    rc = RTErrConvertFromErrno(-cFramesAvail);
    LogFunc(("failed - cFramesAvail=%ld rc=%Rrc\n", cFramesAvail, rc));
    *pcFramesAvail = 0;
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostAlsaAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);

    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    uint32_t cbAvail = 0;

    snd_pcm_sframes_t cFramesAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->phPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
        cbAvail = PDMAUDIOSTREAMCFG_F2B(pStreamALSA->pCfg, cFramesAvail);

    return cbAvail;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostAlsaAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);

    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    uint32_t cbAvail = 0;

    snd_pcm_sframes_t cFramesAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->phPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
        cbAvail = PDMAUDIOSTREAMCFG_F2B(pStreamALSA->pCfg, cFramesAvail);

    return cbAvail;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetPending}
 */
static DECLCALLBACK(uint32_t) drvHostAlsaAudioHA_StreamGetPending(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamALSA, 0);
    AssertPtr(pStreamALSA->pCfg);

    /*
     * This is only relevant to output streams (input streams can't have
     * any pending, unplayed data).
     */
    uint32_t cbPending = 0;
    if (pStreamALSA->pCfg->enmDir == PDMAUDIODIR_OUT)
    {
        /*
         * Getting the delay (in audio frames) reports the time it will take
         * to hear a new sample after all queued samples have been played out.
         *
         * We use snd_pcm_avail_delay instead of snd_pcm_delay here as it will
         * update the buffer positions, and we can use the extra value against
         * the buffer size to double check since the delay value may include
         * fixed built-in delays in the processing chain and hardware.
         */
        snd_pcm_sframes_t cFramesAvail = 0;
        snd_pcm_sframes_t cFramesDelay = 0;
        int rc = snd_pcm_avail_delay(pStreamALSA->phPCM, &cFramesAvail, &cFramesDelay);

        /*
         * We now also get the state as the pending value should be zero when
         * we're not in a playing state.
         */
        snd_pcm_state_t enmState = snd_pcm_state(pStreamALSA->phPCM);
        switch (enmState)
        {
            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_DRAINING:
                if (rc >= 0)
                {
                    if (cFramesAvail >= pStreamALSA->pCfg->Backend.cFramesBufferSize)
                        cbPending = 0;
                    else
                        cbPending = PDMAudioPropsFramesToBytes(&pStreamALSA->pCfg->Props, cFramesDelay);
                }
                break;

            default:
                break;
        }
        Log2Func(("returns %u (%#x) - cFramesBufferSize=%RU32 cFramesAvail=%ld cFramesDelay=%ld rc=%d; enmState=%s (%d) \n",
                  cbPending, cbPending, pStreamALSA->pCfg->Backend.cFramesBufferSize, cFramesAvail, cFramesDelay, rc,
                  snd_pcm_state_name(enmState), enmState));
    }
    return cbPending;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostAlsaAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);

    return PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED | PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF_PV(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamALSA, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);
    PPDMAUDIOSTREAMCFG pCfg = pStreamALSA->pCfg;
    AssertPtr(pCfg);

    /*
     * Figure out how much we can read without trouble (we're doing
     * non-blocking reads, but whatever).
     */
    snd_pcm_sframes_t cAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->phPCM, &cAvail);
    if (RT_SUCCESS(rc))
    {
        if (!cAvail) /* No data yet? */
        {
            snd_pcm_state_t enmState = snd_pcm_state(pStreamALSA->phPCM);
            switch (enmState)
            {
                case SND_PCM_STATE_PREPARED:
                    /** @todo r=bird: explain the logic here...    */
                    cAvail = PDMAudioPropsBytesToFrames(&pCfg->Props, cbBuf);
                    break;

                case SND_PCM_STATE_SUSPENDED:
                    rc = alsaStreamResume(pStreamALSA->phPCM);
                    if (RT_SUCCESS(rc))
                    {
                        LogFlowFunc(("Resumed suspended input stream.\n"));
                        break;
                    }
                    LogFunc(("Failed resuming suspended input stream: %Rrc\n", rc));
                    return rc;

                default:
                    LogFlow(("No frames available: state=%s (%d)\n", snd_pcm_state_name(enmState), enmState));
                    break;
            }
            if (!cAvail)
            {
                *pcbRead = 0;
                return VINF_SUCCESS;
            }
        }
    }
    else
    {
        LogFunc(("Error getting number of captured frames, rc=%Rrc\n", rc));
        return rc;
    }

    size_t cbToRead = PDMAudioPropsFramesToBytes(&pCfg->Props, cAvail);
    cbToRead = RT_MIN(cbToRead, cbBuf);
    LogFlowFunc(("cbToRead=%zu, cAvail=%RI32\n", cbToRead, cAvail));

    /*
     * Read loop.
     */
    uint32_t cbReadTotal = 0;
    while (cbToRead > 0)
    {
        /*
         * Do the reading.
         */
        snd_pcm_uframes_t const cFramesToRead = PDMAudioPropsBytesToFrames(&pCfg->Props, cbToRead);
        AssertBreakStmt(cFramesToRead > 0, rc = VERR_NO_DATA);

        snd_pcm_sframes_t cFramesRead = snd_pcm_readi(pStreamALSA->phPCM, pvBuf, cFramesToRead);
        if (cFramesRead > 0)
        {
            /*
             * We should not run into a full mixer buffer or we lose samples and
             * run into an endless loop if ALSA keeps producing samples ("null"
             * capture device for example).
             */
            uint32_t const cbRead = PDMAudioPropsFramesToBytes(&pCfg->Props, cFramesRead);
            Assert(cbRead <= cbToRead);

            cbToRead    -= cbRead;
            cbReadTotal += cbRead;
            pvBuf        = (uint8_t *)pvBuf + cbRead;
        }
        else
        {
            /*
             * Try recover from overrun and re-try.
             * Other conditions/errors we cannot and will just quit the loop.
             */
            if (cFramesRead == -EPIPE)
            {
                rc = alsaStreamRecover(pStreamALSA->phPCM);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("Successfully recovered from overrun\n"));
                    continue;
                }
                LogFunc(("Failed to recover from overrun: %Rrc\n", rc));
            }
            else if (cFramesRead == -EAGAIN)
                LogFunc(("No input frames available (EAGAIN)\n"));
            else if (cFramesRead == 0)
                LogFunc(("No input frames available (0)\n"));
            else
            {
                rc = RTErrConvertFromErrno(-(int)cFramesRead);
                LogFunc(("Failed to read input frames: %s (%ld, %Rrc)\n", snd_strerror(cFramesRead), cFramesRead, rc));
            }

            /* If we've read anything, suppress the error. */
            if (RT_FAILURE(rc) && cbReadTotal > 0)
            {
                LogFunc(("Suppressing %Rrc because %#x bytes has been read already\n", rc, cbReadTotal));
                rc = VINF_SUCCESS;
            }
            break;
        }
    }

    LogFlowFunc(("returns %Rrc and %#x (%d) bytes (%u bytes left)\n", rc, cbReadTotal, cbReadTotal, cbToRead));
    pStreamALSA->offInternal += cbReadTotal;
    *pcbRead = cbReadTotal;
    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);
    Log4Func(("@%#RX64: pvBuf=%p cbBuf=%#x (%u) state=%s - %s\n", pStreamALSA->offInternal, pvBuf, cbBuf, cbBuf,
              snd_pcm_state_name(snd_pcm_state(pStreamALSA->phPCM)), pStreamALSA->pCfg->szName));

    /*
     * Determine how much we can write (caller actually did this
     * already, but we repeat it just to be sure or something).
     */
    snd_pcm_sframes_t cFramesAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->phPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
    {
        Assert(cFramesAvail);
        if (cFramesAvail)
        {
            PCPDMAUDIOPCMPROPS pProps    = &pStreamALSA->pCfg->Props;
            uint32_t           cbToWrite = PDMAudioPropsFramesToBytes(pProps, (uint32_t)cFramesAvail);
            if (cbToWrite)
            {
                if (cbToWrite > cbBuf)
                    cbToWrite = cbBuf;

                /*
                 * Try write the data.
                 */
                uint32_t cFramesToWrite = PDMAudioPropsBytesToFrames(pProps, cbToWrite);
                snd_pcm_sframes_t cFramesWritten = snd_pcm_writei(pStreamALSA->phPCM, pvBuf, cFramesToWrite);
                if (cFramesWritten > 0)
                {
                    Log4Func(("snd_pcm_writei w/ cbToWrite=%u -> %ld (frames) [cFramesAvail=%ld]\n",
                              cbToWrite, cFramesWritten, cFramesAvail));
                    *pcbWritten = PDMAudioPropsFramesToBytes(pProps, cFramesWritten);
                    pStreamALSA->offInternal += *pcbWritten;
                    return VINF_SUCCESS;
                }
                LogFunc(("snd_pcm_writei w/ cbToWrite=%u -> %ld [cFramesAvail=%ld]\n", cbToWrite, cFramesWritten, cFramesAvail));


                /*
                 * There are a couple of error we can recover from, try to do so.
                 * Only don't try too many times.
                 */
                for (unsigned iTry = 0;
                     (cFramesWritten == -EPIPE || cFramesWritten == -ESTRPIPE) && iTry < ALSA_RECOVERY_TRIES_MAX;
                     iTry++)
                {
                    if (cFramesWritten == -EPIPE)
                    {
                        /* Underrun occurred. */
                        rc = alsaStreamRecover(pStreamALSA->phPCM);
                        if (RT_FAILURE(rc))
                            break;
                        LogFlowFunc(("Recovered from playback (iTry=%u)\n", iTry));
                    }
                    else
                    {
                        /* An suspended event occurred, needs resuming. */
                        rc = alsaStreamResume(pStreamALSA->phPCM);
                        if (RT_FAILURE(rc))
                        {
                            LogRel(("ALSA: Failed to resume output stream (iTry=%u, rc=%Rrc)\n", iTry, rc));
                            break;
                        }
                        LogFlowFunc(("Resumed suspended output stream (iTry=%u)\n", iTry));
                    }

                    cFramesWritten = snd_pcm_writei(pStreamALSA->phPCM, pvBuf, cFramesToWrite);
                    if (cFramesWritten > 0)
                    {
                        Log4Func(("snd_pcm_writei w/ cbToWrite=%u -> %ld (frames) [cFramesAvail=%ld]\n",
                                  cbToWrite, cFramesWritten, cFramesAvail));
                        *pcbWritten = PDMAudioPropsFramesToBytes(pProps, cFramesWritten);
                        pStreamALSA->offInternal += *pcbWritten;
                        return VINF_SUCCESS;
                    }
                    LogFunc(("snd_pcm_writei w/ cbToWrite=%u -> %ld [cFramesAvail=%ld, iTry=%d]\n", cbToWrite, cFramesWritten, cFramesAvail, iTry));
                }

                /* Make sure we return with an error status. */
                if (RT_SUCCESS_NP(rc))
                {
                    if (cFramesWritten == 0)
                        rc = VERR_ACCESS_DENIED;
                    else
                    {
                        rc = RTErrConvertFromErrno(-cFramesWritten);
                        LogFunc(("Failed to write %RU32 bytes: %ld (%Rrc)\n", cbToWrite, cFramesWritten, rc));
                    }
                }
            }
        }
    }
    else
        LogFunc(("Error getting number of playback frames, rc=%Rrc\n", rc));
    *pcbWritten = 0;
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostAlsaAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTALSAAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTALSAAUDIO);
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
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTALSAAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTALSAAUDIO);
    LogRel(("Audio: Initializing ALSA driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostAlsaAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig          = drvHostAlsaAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices         = NULL;
    pThis->IHostAudio.pfnGetStatus          = drvHostAlsaAudioHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostAlsaAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostAlsaAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostAlsaAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostAlsaAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostAlsaAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending   = drvHostAlsaAudioHA_StreamGetPending;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostAlsaAudioHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamPlay         = drvHostAlsaAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostAlsaAudioHA_StreamCapture;

    /*
     * Read configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "StreamName|DefaultOutput|DefaultInput", "");

    int rc = CFGMR3QueryStringDef(pCfg, "DefaultInput", pThis->szDefaultIn, sizeof(pThis->szDefaultIn), "default");
    AssertRCReturn(rc, rc);
    rc = CFGMR3QueryStringDef(pCfg, "DefaultOutput", pThis->szDefaultOut, sizeof(pThis->szDefaultOut), "default");
    AssertRCReturn(rc, rc);

    /*
     * Init the alsa library.
     */
    rc = audioLoadAlsaLib();
    if (RT_FAILURE(rc))
    {
        LogRel(("ALSA: Failed to load the ALSA shared library: %Rrc\n", rc));
        return rc;
    }
#ifdef DEBUG
    snd_lib_error_set_handler(alsaDbgErrorHandler);
#endif
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

