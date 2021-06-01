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
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include "DrvHostAudioAlsaStubsMangling.h"
#include <alsa/asoundlib.h>
#include <alsa/control.h> /* For device enumeration. */
#include <alsa/version.h>
#include "DrvHostAudioAlsaStubs.h"

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Maximum number of tries to recover a broken pipe. */
#define ALSA_RECOVERY_TRIES_MAX    5


/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/
/**
 * ALSA audio stream configuration.
 */
typedef struct ALSAAUDIOSTREAMCFG
{
    unsigned int        freq;
    /** PCM sound format. */
    snd_pcm_format_t    fmt;
#if 0 /* Unused. */
    /** PCM data access type. */
    snd_pcm_access_t    access;
    /** Whether resampling should be performed by alsalib or not. */
    int                 resample;
#endif
    /** Number of audio channels. */
    int                 cChannels;
    /** Buffer size (in audio frames). */
    unsigned long       buffer_size;
    /** Periods (in audio frames). */
    unsigned long       period_size;
    /** For playback:  Starting to play threshold (in audio frames).
     *  For Capturing: ~~Starting to capture threshold (in audio frames)~~ !nothing! */
    unsigned long       threshold;

    /* latency = period_size * periods / (rate * bytes_per_frame) */
} ALSAAUDIOSTREAMCFG;
/** Pointer to an ALSA audio stream config. */
typedef ALSAAUDIOSTREAMCFG *PALSAAUDIOSTREAMCFG;


/**
 * ALSA host audio specific stream data.
 */
typedef struct ALSAAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;

    /** Handle to the ALSA PCM stream. */
    snd_pcm_t              *hPCM;
    /** Internal stream offset (for debugging). */
    uint64_t                offInternal;

    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** The acquired ALSA stream config (same as Cfg). */
    ALSAAUDIOSTREAMCFG      AlsaCfg;
} ALSAAUDIOSTREAM;
/** Pointer to the ALSA host audio specific stream data. */
typedef ALSAAUDIOSTREAM *PALSAAUDIOSTREAM;


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
} DRVHOSTALSAAUDIO;
/** Pointer to the instance data of an ALSA host audio driver. */
typedef DRVHOSTALSAAUDIO *PDRVHOSTALSAAUDIO;



/**
 * Closes an ALSA stream
 *
 * @returns VBox status code.
 * @param   phPCM   Pointer to the ALSA stream handle to close.  Will be set to
 *                  NULL.
 */
static int alsaStreamClose(snd_pcm_t **phPCM)
{
    if (!phPCM || !*phPCM)
        return VINF_SUCCESS;

    int rc;
    int rc2 = snd_pcm_close(*phPCM);
    if (rc2 == 0)
    {
        *phPCM = NULL;
        rc = VINF_SUCCESS;
    }
    else
    {
        rc = RTErrConvertFromErrno(-rc2);
        LogRel(("ALSA: Closing PCM descriptor failed: %s (%d, %Rrc)\n", snd_strerror(rc2), rc2, rc));
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
 * @param   hPCM               ALSA stream handle.
 */
static int alsaStreamRecover(snd_pcm_t *hPCM)
{
    AssertPtrReturn(hPCM, VERR_INVALID_POINTER);

    int rc = snd_pcm_prepare(hPCM);
    if (rc >= 0)
    {
        LogFlowFunc(("Successfully recovered %p.\n", hPCM));
        return VINF_SUCCESS;
    }
    LogFunc(("Failed to recover stream %p: %s (%d)\n", hPCM, snd_strerror(rc), rc));
    return RTErrConvertFromErrno(-rc);
}


/**
 * Resumes an ALSA stream.
 *
 * @returns VBox status code.
 * @param   hPCM               ALSA stream to resume.
 */
static int alsaStreamResume(snd_pcm_t *hPCM)
{
    AssertPtrReturn(hPCM, VERR_INVALID_POINTER);

    int rc = snd_pcm_resume(hPCM);
    if (rc >= 0)
    {
        LogFlowFunc(("Successfuly resumed %p.\n", hPCM));
        return VINF_SUCCESS;
    }
    LogFunc(("Failed to resume stream %p: %s (%d)\n", hPCM, snd_strerror(rc), rc));
    return RTErrConvertFromErrno(-rc);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "ALSA");
    pBackendCfg->cbStream       = sizeof(ALSAAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    /* ALSA allows exactly one input and one output used at a time for the selected device(s). */
    pBackendCfg->cMaxStreamsIn  = 1;
    pBackendCfg->cMaxStreamsOut = 1;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetDevices}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_GetDevices(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum)
{
    RT_NOREF(pInterface);
    PDMAudioHostEnumInit(pDeviceEnum);

    char **papszHints = NULL;
    int rc = snd_device_name_hint(-1 /* All cards */, "pcm", (void***)&papszHints);
    if (rc == 0)
    {
        rc = VINF_SUCCESS;
        for (size_t iHint = 0; papszHints[iHint] != NULL && RT_SUCCESS(rc); iHint++)
        {
            /*
             * Retrieve the available info:
             */
            const char * const pszHint = papszHints[iHint];
            char * const pszDev     = snd_device_name_get_hint(pszHint, "NAME");
            char * const pszInOutId = snd_device_name_get_hint(pszHint, "IOID");
            char * const pszDesc    = snd_device_name_get_hint(pszHint, "DESC");

            if (pszDev && RTStrICmp(pszDev, "null") != 0)
            {
                /* Detect and log presence of pulse audio plugin. */
                if (RTStrIStr("pulse", pszDev) != NULL)
                    LogRel(("ALSA: The ALSAAudio plugin for pulse audio is being used (%s).\n", pszDev));

                /*
                 * Add an entry to the enumeration result.
                 */
                PPDMAUDIOHOSTDEV pDev = PDMAudioHostDevAlloc(sizeof(*pDev));
                if (pDev)
                {
                    pDev->fFlags  = PDMAUDIOHOSTDEV_F_NONE;
                    pDev->enmType = PDMAUDIODEVICETYPE_UNKNOWN;

                    if (pszInOutId == NULL)
                    {
                        pDev->enmUsage           = PDMAUDIODIR_DUPLEX;
                        pDev->cMaxInputChannels  = 2;
                        pDev->cMaxOutputChannels = 2;
                    }
                    else if (RTStrICmp(pszInOutId, "Input") == 0)
                    {
                        pDev->enmUsage           = PDMAUDIODIR_IN;
                        pDev->cMaxInputChannels  = 2;
                        pDev->cMaxOutputChannels = 0;
                    }
                    else
                    {
                        AssertMsg(RTStrICmp(pszInOutId, "Output") == 0, ("%s (%s)\n", pszInOutId, pszHint));
                        pDev->enmUsage = PDMAUDIODIR_OUT;
                        pDev->cMaxInputChannels  = 0;
                        pDev->cMaxOutputChannels = 2;
                    }

                    int rc2 = RTStrCopy(pDev->szName, sizeof(pDev->szName), pszDev);
                    AssertRC(rc2);

                    PDMAudioHostEnumAppend(pDeviceEnum, pDev);

                    LogRel2(("ALSA: Device #%u: '%s' enmDir=%s: %s\n", iHint, pszDev,
                             PDMAudioDirGetName(pDev->enmUsage), pszDesc));
                }
                else
                    rc = VERR_NO_MEMORY;
            }

            /*
             * Clean up.
             */
            if (pszInOutId)
                free(pszInOutId);
            if (pszDesc)
                free(pszDesc);
            if (pszDev)
                free(pszDev);
        }

        snd_device_name_free_hint((void **)papszHints);

        if (RT_FAILURE(rc))
        {
            PDMAudioHostEnumDelete(pDeviceEnum);
            PDMAudioHostEnumInit(pDeviceEnum);
        }
    }
    else
    {
        int rc2 = RTErrConvertFromErrno(-rc);
        LogRel2(("ALSA: Error enumerating PCM devices: %Rrc (%d)\n", rc2, rc));
        rc = rc2;
    }
    return rc;
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
    AssertReturn(cChannels < PDMAUDIO_MAX_CHANNELS, VERR_INVALID_PARAMETER);
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
 * @param   hPCM            ALSA stream to set software parameters for.
 * @param   fIn             Whether this is an input stream or not.
 * @param   pAlsaCfgReq     Requested configuration to set (ALSA).
 * @param   pAlsaCfgObt     Obtained configuration on success (ALSA).
 *                          Might differ from requested configuration.
 */
static int alsaStreamSetSWParams(snd_pcm_t *hPCM, bool fIn, PALSAAUDIOSTREAMCFG pAlsaCfgReq, PALSAAUDIOSTREAMCFG pAlsaCfgObt)
{
    if (fIn) /* For input streams there's nothing to do in here right now. */
        return VINF_SUCCESS;

    snd_pcm_sw_params_t *pSWParms = NULL;
    snd_pcm_sw_params_alloca(&pSWParms);
    AssertReturn(pSWParms, -ENOMEM);

    int err = snd_pcm_sw_params_current(hPCM, pSWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to get current software parameters: %s\n", snd_strerror(err)), err);

    /* Under normal circumstance, we don't need to set a playback threshold
       because DrvAudio will do the pre-buffering and hand us everything in
       one continuous chunk when we should start playing.  But since it is
       configurable, we'll set a reasonable minimum of two DMA periods or
       max 64 milliseconds (the pAlsaCfgReq->threshold value).

       Of course we also have to make sure the threshold is below the buffer
       size, or ALSA will never start playing. */
    unsigned long cFramesThreshold = RT_MIN(pAlsaCfgObt->period_size * 2, pAlsaCfgReq->threshold);
    if (cFramesThreshold >= pAlsaCfgObt->buffer_size - pAlsaCfgObt->buffer_size / 16)
        cFramesThreshold = pAlsaCfgObt->buffer_size - pAlsaCfgObt->buffer_size / 16;

    err = snd_pcm_sw_params_set_start_threshold(hPCM, pSWParms, cFramesThreshold);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set software threshold to %lu: %s\n", cFramesThreshold, snd_strerror(err)), err);

    err = snd_pcm_sw_params_set_avail_min(hPCM, pSWParms, pAlsaCfgReq->period_size);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set available minimum to %lu: %s\n",
                                     pAlsaCfgReq->period_size, snd_strerror(err)), err);

    /* Commit the software parameters: */
    err = snd_pcm_sw_params(hPCM, pSWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set new software parameters: %s\n", snd_strerror(err)), err);

    /* Get the actual parameters: */
    err = snd_pcm_sw_params_get_start_threshold(pSWParms, &pAlsaCfgObt->threshold);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to get start threshold: %s\n", snd_strerror(err)), err);

    LogRel2(("ALSA: SW params: %lu frames threshold, %lu frame avail minimum\n",
             pAlsaCfgObt->threshold, pAlsaCfgReq->period_size));
    return 0;
}


/**
 * Sets the hardware parameters of an ALSA stream.
 *
 * @returns 0 on success, negative errno on failure.
 * @param   hPCM        ALSA stream to set software parameters for.
 * @param   pAlsaCfgReq Requested configuration to set (ALSA).
 * @param   pAlsaCfgObt Obtained configuration on success (ALSA). Might differ
 *                      from requested configuration.
 */
static int alsaStreamSetHwParams(snd_pcm_t *hPCM, PALSAAUDIOSTREAMCFG pAlsaCfgReq, PALSAAUDIOSTREAMCFG pAlsaCfgObt)
{
    /*
     * Get the current hardware parameters.
     */
    snd_pcm_hw_params_t *pHWParms = NULL;
    snd_pcm_hw_params_alloca(&pHWParms);
    AssertReturn(pHWParms, -ENOMEM);

    int err = snd_pcm_hw_params_any(hPCM, pHWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to initialize hardware parameters: %s\n", snd_strerror(err)), err);

    /*
     * Modify them according to pAlsaCfgReq.
     * We update pAlsaCfgObt as we go for parameters set by "near" methods.
     */
    /* We'll use snd_pcm_writei/snd_pcm_readi: */
    err = snd_pcm_hw_params_set_access(hPCM, pHWParms, SND_PCM_ACCESS_RW_INTERLEAVED);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set access type: %s\n", snd_strerror(err)), err);

    /* Set the format, frequency and channel count. */
    err = snd_pcm_hw_params_set_format(hPCM, pHWParms, pAlsaCfgReq->fmt);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set audio format to %d: %s\n", pAlsaCfgReq->fmt, snd_strerror(err)), err);

    unsigned int uFreq = pAlsaCfgReq->freq;
    err = snd_pcm_hw_params_set_rate_near(hPCM, pHWParms, &uFreq, NULL /*dir*/);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set frequency to %uHz: %s\n", pAlsaCfgReq->freq, snd_strerror(err)), err);
    pAlsaCfgObt->freq = uFreq;

    unsigned int cChannels = pAlsaCfgReq->cChannels;
    err = snd_pcm_hw_params_set_channels_near(hPCM, pHWParms, &cChannels);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set number of channels to %d\n", pAlsaCfgReq->cChannels), err);
    pAlsaCfgObt->cChannels = cChannels;

    /* The period size (reportedly frame count per hw interrupt): */
    int               dir    = 0;
    snd_pcm_uframes_t minval = pAlsaCfgReq->period_size;
    err = snd_pcm_hw_params_get_period_size_min(pHWParms, &minval, &dir);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Could not determine minimal period size: %s\n", snd_strerror(err)), err);

    snd_pcm_uframes_t period_size_f = pAlsaCfgReq->period_size;
    if (period_size_f < minval)
        period_size_f = minval;
    err = snd_pcm_hw_params_set_period_size_near(hPCM, pHWParms, &period_size_f, 0);
    LogRel2(("ALSA: Period size is: %lu frames (min %lu, requested %lu)\n", period_size_f, minval, pAlsaCfgReq->period_size));
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set period size %d (%s)\n", period_size_f, snd_strerror(err)), err);

    /* The buffer size: */
    minval = pAlsaCfgReq->buffer_size;
    err = snd_pcm_hw_params_get_buffer_size_min(pHWParms, &minval);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Could not retrieve minimal buffer size: %s\n", snd_strerror(err)), err);

    snd_pcm_uframes_t buffer_size_f = pAlsaCfgReq->buffer_size;
    if (buffer_size_f < minval)
        buffer_size_f = minval;
    err = snd_pcm_hw_params_set_buffer_size_near(hPCM, pHWParms, &buffer_size_f);
    LogRel2(("ALSA: Buffer size is: %lu frames (min %lu, requested %lu)\n", buffer_size_f, minval, pAlsaCfgReq->buffer_size));
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to set near buffer size %RU32: %s\n", buffer_size_f, snd_strerror(err)), err);

    /*
     * Set the hardware parameters.
     */
    err = snd_pcm_hw_params(hPCM, pHWParms);
    AssertLogRelMsgReturn(err >= 0, ("ALSA: Failed to apply audio parameters: %s\n", snd_strerror(err)), err);

    /*
     * Get relevant parameters and put them in the pAlsaCfgObt structure.
     */
    snd_pcm_uframes_t obt_buffer_size = buffer_size_f;
    err = snd_pcm_hw_params_get_buffer_size(pHWParms, &obt_buffer_size);
    AssertLogRelMsgStmt(err >= 0, ("ALSA: Failed to get buffer size: %s\n", snd_strerror(err)), obt_buffer_size = buffer_size_f);
    pAlsaCfgObt->buffer_size = obt_buffer_size;

    snd_pcm_uframes_t obt_period_size = period_size_f;
    err = snd_pcm_hw_params_get_period_size(pHWParms, &obt_period_size, &dir);
    AssertLogRelMsgStmt(err >= 0, ("ALSA: Failed to get period size: %s\n", snd_strerror(err)), obt_period_size = period_size_f);
    pAlsaCfgObt->period_size = obt_period_size;

//    pAlsaCfgObt->access  = pAlsaCfgReq->access;  - unused and uninitialized.
    pAlsaCfgObt->fmt     = pAlsaCfgReq->fmt;

    LogRel2(("ALSA: HW params: %u Hz, %lu frames period, %lu frames buffer, %u channel(s), fmt=%d, access=%d\n",
             pAlsaCfgObt->freq, pAlsaCfgObt->period_size, pAlsaCfgObt->buffer_size, pAlsaCfgObt->cChannels,
             pAlsaCfgObt->fmt, -1 /*pAlsaCfgObt->access*/));
    return 0;
}


/**
 * Opens (creates) an ALSA stream.
 *
 * @returns VBox status code.
 * @param   pszDev      The name of the device to open.
 * @param   fIn         Whether this is an input stream to create or not.
 * @param   pAlsaCfgReq Requested configuration to create stream with (ALSA).
 * @param   pCfgReq     Requested configuration to create stream with (PDM).
 * @param   pAlsaCfgObt Obtained configuration the stream got created on
 *                      success.
 * @param   phPCM       Where to store the ALSA stream handle on success.
 */
static int alsaStreamOpen(const char *pszDev, bool fIn, PALSAAUDIOSTREAMCFG pAlsaCfgReq, PPDMAUDIOSTREAMCFG pCfgReq,
                          PALSAAUDIOSTREAMCFG pAlsaCfgObt,  snd_pcm_t **phPCM)
{
    AssertLogRelMsgReturn(pszDev && *pszDev,
                          ("ALSA: Invalid or no %s device name set\n", fIn ? "input" : "output"),
                          VERR_INVALID_NAME);
    RT_NOREF(pCfgReq);

    /*
     * Open the stream.
     */
    int rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    snd_pcm_t *hPCM = NULL;
    LogRel(("ALSA: Using %s device \"%s\"\n", fIn ? "input" : "output", pszDev));
    int err = snd_pcm_open(&hPCM, pszDev,
                           fIn ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                           SND_PCM_NONBLOCK);
    if (err >= 0)
    {
        err = snd_pcm_nonblock(hPCM, 1);
        if (err >= 0)
        {
            /*
             * Configure hardware stream parameters.
             */
            err = alsaStreamSetHwParams(hPCM, pAlsaCfgReq, pAlsaCfgObt);
            if (err >= 0)
            {
                /*
                 * Prepare it.
                 */
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                err = snd_pcm_prepare(hPCM);
                if (err >= 0)
                {
                    /*
                     * Configure software stream parameters and we're done.
                     */
                    rc = alsaStreamSetSWParams(hPCM, fIn, pAlsaCfgReq, pAlsaCfgObt);
                    if (RT_SUCCESS(rc))
                    {
                        *phPCM = hPCM;
                        return VINF_SUCCESS;
                    }
                }
                else
                    LogRel(("ALSA: snd_pcm_prepare failed: %s\n", snd_strerror(err)));
            }
        }
        else
            LogRel(("ALSA: Error setting output non-blocking mode: %s\n", snd_strerror(err)));
        alsaStreamClose(&hPCM);
    }
    else
        LogRel(("ALSA: Failed to open \"%s\" as %s device: %s\n", pszDev, fIn ? "input" : "output", snd_strerror(err)));
    *phPCM = NULL;
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
    PDMAudioStrmCfgCopy(&pStreamALSA->Cfg, pCfgReq);

    ALSAAUDIOSTREAMCFG Req;
    Req.fmt         = alsaAudioPropsToALSA(&pCfgReq->Props);
    Req.freq        = PDMAudioPropsHz(&pCfgReq->Props);
    Req.cChannels   = PDMAudioPropsChannels(&pCfgReq->Props);
    Req.period_size = pCfgReq->Backend.cFramesPeriod;
    Req.buffer_size = pCfgReq->Backend.cFramesBufferSize;
    Req.threshold   = PDMAudioPropsMilliToFrames(&pCfgReq->Props, 50);
    int rc = alsaStreamOpen(pCfgReq->enmDir == PDMAUDIODIR_IN ? pThis->szDefaultIn : pThis->szDefaultOut,
                            pCfgReq->enmDir == PDMAUDIODIR_IN,
                            &Req, pCfgReq, &pStreamALSA->AlsaCfg, &pStreamALSA->hPCM);
    if (RT_SUCCESS(rc))
    {
        rc = alsaALSAToAudioProps(&pCfgAcq->Props, pStreamALSA->AlsaCfg.fmt,
                                  pStreamALSA->AlsaCfg.cChannels, pStreamALSA->AlsaCfg.freq);
        if (RT_SUCCESS(rc))
        {
            pCfgAcq->Backend.cFramesPeriod          = pStreamALSA->AlsaCfg.period_size;
            pCfgAcq->Backend.cFramesBufferSize      = pStreamALSA->AlsaCfg.buffer_size;

            /* We have no objections to the pre-buffering that DrvAudio applies,
               only we need to adjust it relative to the actual buffer size. */
            pCfgAcq->Backend.cFramesPreBuffering    = (uint64_t)pCfgReq->Backend.cFramesPreBuffering
                                                    * pCfgAcq->Backend.cFramesBufferSize
                                                    / RT_MAX(pCfgReq->Backend.cFramesBufferSize, 1);

            PDMAudioStrmCfgCopy(&pStreamALSA->Cfg, pCfgAcq);
            LogFlowFunc(("returns success - hPCM=%p\n", pStreamALSA->hPCM));
            return rc;
        }
        alsaStreamClose(&pStreamALSA->hPCM);
    }
    LogFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          bool fImmediate)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamALSA, VERR_INVALID_POINTER);
    RT_NOREF(fImmediate);

    /** @todo r=bird: It's not like we can do much with a bad status... Check
     *        what the caller does... */
    return alsaStreamClose(&pStreamALSA->hPCM);
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    /*
     * Prepare the stream.
     */
    int rc = snd_pcm_prepare(pStreamALSA->hPCM);
    if (rc >= 0)
    {
        Assert(snd_pcm_state(pStreamALSA->hPCM) == SND_PCM_STATE_PREPARED);

        /*
         * Input streams should be started now, whereas output streams must
         * pre-buffer sufficent data before starting.
         */
        if (pStreamALSA->Cfg.enmDir == PDMAUDIODIR_IN)
        {
            rc = snd_pcm_start(pStreamALSA->hPCM);
            if (rc >= 0)
                rc = VINF_SUCCESS;
            else
            {
                LogRel(("ALSA: Error starting input stream '%s': %s (%d)\n", pStreamALSA->Cfg.szName, snd_strerror(rc), rc));
                rc = RTErrConvertFromErrno(-rc);
            }
        }
        else
            rc = VINF_SUCCESS;
    }
    else
    {
        LogRel(("ALSA: Error preparing stream '%s': %s (%d)\n", pStreamALSA->Cfg.szName, snd_strerror(rc), rc));
        rc = RTErrConvertFromErrno(-rc);
    }
    LogFlowFunc(("returns %Rrc (state %s)\n", rc, snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM))));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    int rc = snd_pcm_drop(pStreamALSA->hPCM);
    if (rc >= 0)
        rc = VINF_SUCCESS;
    else
    {
        LogRel(("ALSA: Error stopping stream '%s': %s (%d)\n", pStreamALSA->Cfg.szName, snd_strerror(rc), rc));
        rc = RTErrConvertFromErrno(-rc);
    }
    LogFlowFunc(("returns %Rrc (state %s)\n", rc, snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM))));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    /* Same as disable. */
    /** @todo r=bird: Try use pause and fallback on disable/enable if it isn't
     *        supported or doesn't work. */
    return drvHostAlsaAudioHA_StreamDisable(pInterface, pStream);
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    /* Same as enable. */
    return drvHostAlsaAudioHA_StreamEnable(pInterface, pStream);
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDrain}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    snd_pcm_state_t const enmState = snd_pcm_state(pStreamALSA->hPCM);
    LogFlowFunc(("Stream '%s' input state: %s (%d)\n", pStreamALSA->Cfg.szName, snd_pcm_state_name(enmState), enmState));

    /* Only for output streams. */
    AssertReturn(pStreamALSA->Cfg.enmDir == PDMAUDIODIR_OUT, VERR_WRONG_ORDER);

    int rc;
    switch (enmState)
    {
        case SND_PCM_STATE_RUNNING:
        case SND_PCM_STATE_PREPARED: /* not yet started */
        {
#if 0       /** @todo r=bird: You want EMT to block here for potentially 200-300ms worth
             *        of buffer to be drained?  That's a certifiably bad idea.  */
            int rc2 = snd_pcm_nonblock(pStreamALSA->hPCM, 0);
            AssertMsg(rc2 >= 0, ("snd_pcm_nonblock(, 0) -> %d\n", rc2));
#endif
            rc = snd_pcm_drain(pStreamALSA->hPCM);
            if (rc >= 0 || rc == -EAGAIN)
                rc = VINF_SUCCESS;
            else
            {
                LogRel(("ALSA: Error draining output of '%s': %s (%d)\n", pStreamALSA->Cfg.szName, snd_strerror(rc), rc));
                rc = RTErrConvertFromErrno(-rc);
            }
#if 0
            rc2 = snd_pcm_nonblock(pStreamALSA->hPCM, 1);
            AssertMsg(rc2 >= 0, ("snd_pcm_nonblock(, 1) -> %d\n", rc2));
#endif
            break;
        }

        default:
            rc = VINF_SUCCESS;
            break;
    }
    LogFlowFunc(("returns %Rrc (state %s)\n", rc, snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM))));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostAlsaAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                          PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostAlsaAudioHA_StreamEnable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostAlsaAudioHA_StreamDisable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostAlsaAudioHA_StreamPause(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostAlsaAudioHA_StreamResume(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostAlsaAudioHA_StreamDrain(pInterface, pStream);

        case PDMAUDIOSTREAMCMD_END:
        case PDMAUDIOSTREAMCMD_32BIT_HACK:
        case PDMAUDIOSTREAMCMD_INVALID:
            /* no default*/
            break;
    }
    return VERR_NOT_SUPPORTED;
}


/**
 * Returns the available audio frames queued.
 *
 * @returns VBox status code.
 * @param   hPCM           ALSA stream handle.
 * @param   pcFramesAvail   Where to store the available frames.
 */
static int alsaStreamGetAvail(snd_pcm_t *hPCM, snd_pcm_sframes_t *pcFramesAvail)
{
    AssertPtr(hPCM);
    AssertPtr(pcFramesAvail);

    int rc;
    snd_pcm_sframes_t cFramesAvail = snd_pcm_avail_update(hPCM);
    if (cFramesAvail > 0)
    {
        LogFunc(("cFramesAvail=%ld\n", cFramesAvail));
        *pcFramesAvail = cFramesAvail;
        return VINF_SUCCESS;
    }

    /*
     * We can maybe recover from an EPIPE...
     */
    if (cFramesAvail == -EPIPE)
    {
        rc = alsaStreamRecover(hPCM);
        if (RT_SUCCESS(rc))
        {
            cFramesAvail = snd_pcm_avail_update(hPCM);
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

    rc = RTErrConvertFromErrno(-(int)cFramesAvail);
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

    uint32_t          cbAvail      = 0;
    snd_pcm_sframes_t cFramesAvail = 0;
    int rc = alsaStreamGetAvail(pStreamALSA->hPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
        cbAvail = PDMAudioPropsFramesToBytes(&pStreamALSA->Cfg.Props, cFramesAvail);

    return cbAvail;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostAlsaAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;

    uint32_t          cbAvail      = 0;
    snd_pcm_sframes_t cFramesAvail = 0;
    int rc = alsaStreamGetAvail(pStreamALSA->hPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
        cbAvail = PDMAudioPropsFramesToBytes(&pStreamALSA->Cfg.Props, cFramesAvail);

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

    /*
     * This is only relevant to output streams (input streams can't have
     * any pending, unplayed data).
     */
    uint32_t cbPending = 0;
    if (pStreamALSA->Cfg.enmDir == PDMAUDIODIR_OUT)
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
        int rc = snd_pcm_avail_delay(pStreamALSA->hPCM, &cFramesAvail, &cFramesDelay);

        /*
         * We now also get the state as the pending value should be zero when
         * we're not in a playing state.
         */
        snd_pcm_state_t enmState = snd_pcm_state(pStreamALSA->hPCM);
        switch (enmState)
        {
            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_DRAINING:
                if (rc >= 0)
                {
                    if ((uint32_t)cFramesAvail >= pStreamALSA->Cfg.Backend.cFramesBufferSize)
                        cbPending = 0;
                    else
                        cbPending = PDMAudioPropsFramesToBytes(&pStreamALSA->Cfg.Props, cFramesDelay);
                }
                break;

            default:
                break;
        }
        Log2Func(("returns %u (%#x) - cFramesBufferSize=%RU32 cFramesAvail=%ld cFramesDelay=%ld rc=%d; enmState=%s (%d) \n",
                  cbPending, cbPending, pStreamALSA->Cfg.Backend.cFramesBufferSize, cFramesAvail, cFramesDelay, rc,
                  snd_pcm_state_name(enmState), enmState));
    }
    return cbPending;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostAlsaAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                               PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PALSAAUDIOSTREAM pStreamALSA = (PALSAAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamALSA, PDMHOSTAUDIOSTREAMSTATE_INVALID);

    PDMHOSTAUDIOSTREAMSTATE enmStreamState = PDMHOSTAUDIOSTREAMSTATE_OKAY;
    snd_pcm_state_t         enmAlsaState   = snd_pcm_state(pStreamALSA->hPCM);
    if (enmAlsaState == SND_PCM_STATE_DRAINING)
    {
        /* We're operating in non-blocking mode, so we must (at least for a demux
           config) call snd_pcm_drain again to drive it forward.  Otherwise we
           might be stuck in the drain state forever. */
        Log5Func(("Calling snd_pcm_drain again...\n"));
        snd_pcm_drain(pStreamALSA->hPCM);
        enmAlsaState = snd_pcm_state(pStreamALSA->hPCM);
    }

    if (enmAlsaState == SND_PCM_STATE_DRAINING)
        enmStreamState = PDMHOSTAUDIOSTREAMSTATE_DRAINING;
#if (((SND_LIB_MAJOR) << 16) | ((SND_LIB_MAJOR) << 8) | (SND_LIB_SUBMINOR)) >= 0x10002 /* was added in 1.0.2 */
    else if (enmAlsaState == SND_PCM_STATE_DISCONNECTED)
        enmStreamState = PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING;
#endif

    Log5Func(("Stream '%s': ALSA state=%s -> %s\n",
              pStreamALSA->Cfg.szName, snd_pcm_state_name(enmAlsaState), PDMHostAudioStreamStateGetName(enmStreamState) ));
    return enmStreamState;
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
    Log4Func(("@%#RX64: pvBuf=%p cbBuf=%#x (%u) state=%s - %s\n", pStreamALSA->offInternal, pvBuf, cbBuf, cbBuf,
              snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM)), pStreamALSA->Cfg.szName));

    /*
     * Figure out how much we can read without trouble (we're doing
     * non-blocking reads, but whatever).
     */
    snd_pcm_sframes_t cAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->hPCM, &cAvail);
    if (RT_SUCCESS(rc))
    {
        if (!cAvail) /* No data yet? */
        {
            snd_pcm_state_t enmState = snd_pcm_state(pStreamALSA->hPCM);
            switch (enmState)
            {
                case SND_PCM_STATE_PREPARED:
                    /** @todo r=bird: explain the logic here...    */
                    cAvail = PDMAudioPropsBytesToFrames(&pStreamALSA->Cfg.Props, cbBuf);
                    break;

                case SND_PCM_STATE_SUSPENDED:
                    rc = alsaStreamResume(pStreamALSA->hPCM);
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

    size_t cbToRead = PDMAudioPropsFramesToBytes(&pStreamALSA->Cfg.Props, cAvail);
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
        snd_pcm_uframes_t const cFramesToRead = PDMAudioPropsBytesToFrames(&pStreamALSA->Cfg.Props, cbToRead);
        AssertBreakStmt(cFramesToRead > 0, rc = VERR_NO_DATA);

        snd_pcm_sframes_t cFramesRead = snd_pcm_readi(pStreamALSA->hPCM, pvBuf, cFramesToRead);
        if (cFramesRead > 0)
        {
            /*
             * We should not run into a full mixer buffer or we lose samples and
             * run into an endless loop if ALSA keeps producing samples ("null"
             * capture device for example).
             */
            uint32_t const cbRead = PDMAudioPropsFramesToBytes(&pStreamALSA->Cfg.Props, cFramesRead);
            Assert(cbRead <= cbToRead);

            cbToRead    -= cbRead;
            cbReadTotal += cbRead;
            pvBuf        = (uint8_t *)pvBuf + cbRead;
            pStreamALSA->offInternal += cbRead;
        }
        else
        {
            /*
             * Try recover from overrun and re-try.
             * Other conditions/errors we cannot and will just quit the loop.
             */
            if (cFramesRead == -EPIPE)
            {
                rc = alsaStreamRecover(pStreamALSA->hPCM);
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

    LogFlowFunc(("returns %Rrc and %#x (%d) bytes (%u bytes left); state %s\n",
                 rc, cbReadTotal, cbReadTotal, cbToRead, snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM))));
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
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);
    Log4Func(("@%#RX64: pvBuf=%p cbBuf=%#x (%u) state=%s - %s\n", pStreamALSA->offInternal, pvBuf, cbBuf, cbBuf,
              snd_pcm_state_name(snd_pcm_state(pStreamALSA->hPCM)), pStreamALSA->Cfg.szName));
    if (cbBuf)
        AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    else
    {
        /* Fend off draining calls. */
        *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    /*
     * Determine how much we can write (caller actually did this
     * already, but we repeat it just to be sure or something).
     */
    snd_pcm_sframes_t cFramesAvail;
    int rc = alsaStreamGetAvail(pStreamALSA->hPCM, &cFramesAvail);
    if (RT_SUCCESS(rc))
    {
        Assert(cFramesAvail);
        if (cFramesAvail)
        {
            PCPDMAUDIOPCMPROPS pProps    = &pStreamALSA->Cfg.Props;
            uint32_t           cbToWrite = PDMAudioPropsFramesToBytes(pProps, (uint32_t)cFramesAvail);
            if (cbToWrite)
            {
                if (cbToWrite > cbBuf)
                    cbToWrite = cbBuf;

                /*
                 * Try write the data.
                 */
                uint32_t cFramesToWrite = PDMAudioPropsBytesToFrames(pProps, cbToWrite);
                snd_pcm_sframes_t cFramesWritten = snd_pcm_writei(pStreamALSA->hPCM, pvBuf, cFramesToWrite);
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
                        rc = alsaStreamRecover(pStreamALSA->hPCM);
                        if (RT_FAILURE(rc))
                            break;
                        LogFlowFunc(("Recovered from playback (iTry=%u)\n", iTry));
                    }
                    else
                    {
                        /* An suspended event occurred, needs resuming. */
                        rc = alsaStreamResume(pStreamALSA->hPCM);
                        if (RT_FAILURE(rc))
                        {
                            LogRel(("ALSA: Failed to resume output stream (iTry=%u, rc=%Rrc)\n", iTry, rc));
                            break;
                        }
                        LogFlowFunc(("Resumed suspended output stream (iTry=%u)\n", iTry));
                    }

                    cFramesWritten = snd_pcm_writei(pStreamALSA->hPCM, pvBuf, cFramesToWrite);
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
                        rc = RTErrConvertFromErrno(-(int)cFramesWritten);
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
    pThis->IHostAudio.pfnGetConfig                  = drvHostAlsaAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = drvHostAlsaAudioHA_GetDevices;
    pThis->IHostAudio.pfnSetDevice                  = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvHostAlsaAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostAlsaAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostAlsaAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostAlsaAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostAlsaAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostAlsaAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = drvHostAlsaAudioHA_StreamGetPending;
    pThis->IHostAudio.pfnStreamGetState             = drvHostAlsaAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostAlsaAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostAlsaAudioHA_StreamCapture;

    /*
     * Read configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "DefaultOutput|DefaultInput", "");

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
 * ALSA audio driver registration record.
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

