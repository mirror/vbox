/* $Id$ */
/** @file
 * VBox audio device: Audio sniffer device
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#define AUDIO_CAP "sniffer"
#include <VBox/vmm/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include "VBoxDD.h"
#include "vl_vbox.h"

#include "audio.h"
#include "audio_int.h"

typedef struct _AUDIOSNIFFERSTATE
{
    /** If the device is enabled. */
    bool fEnabled;

    /** Whether audio should reach the host driver too. */
    bool fKeepHostAudio;

    /** Whether audio input operations should be forwarded to the connector. */
    bool fInterceptAudioInput;

    /** Pointer to device instance. */
    PPDMDEVINS                   pDevIns;

    /** Audio Sniffer port base interface. */
    PDMIBASE                     IBase;
    /** Audio Sniffer port interface. */
    PDMIAUDIOSNIFFERPORT         IPort;

    /** Pointer to base interface of the driver. */
    PPDMIBASE                    pDrvBase;
    /** Audio Sniffer connector interface */
    PPDMIAUDIOSNIFFERCONNECTOR   pDrv;

} AUDIOSNIFFERSTATE;

static AUDIOSNIFFERSTATE *g_pData = NULL;

/*
 * Public sniffer callbacks to be called from audio driver.
 */

/* *** Subject to change ***
 * Process audio output. The function is called when an audio output
 * driver is about to play audio samples.
 *
 * It is expected that there is only one audio data flow,
 * i.e. one voice.
 *
 * @param hw           Audio samples information.
 * @param pvSamples    Pointer to audio samples.
 * @param cSamples     Number of audio samples in the buffer.
 * @returns     'true' if audio also to be played back by the output driver.
 *              'false' if audio should not be played.
 */
DECLCALLBACK(bool) sniffer_run_out (HWVoiceOut *hw, void *pvSamples, unsigned cSamples)
{
    int  samplesPerSec;
    int  nChannels;
    int  bitsPerSample;
    bool fUnsigned;

    if (!g_pData || !g_pData->pDrv || !g_pData->fEnabled)
    {
        return true;
    }

    samplesPerSec = hw->info.freq;
    nChannels     = hw->info.nchannels;
    bitsPerSample = hw->info.bits;
    fUnsigned     = (hw->info.sign == 0);

    g_pData->pDrv->pfnAudioSamplesOut (g_pData->pDrv, pvSamples, cSamples,
                                       samplesPerSec, nChannels, bitsPerSample, fUnsigned);

    return g_pData->fKeepHostAudio;
}


/*
 * Filter interface.
 */

/* Internal audio input context, which makes sure that:
 *   - the filter audio input callback is not called after the filter has issued filter_input_end;
 *   - maintains internal information and state of the audio stream.
 */
typedef struct SnifferInputCtx
{
    /* Whether the context is still in use by the filter or I'll check. */
    int32_t volatile cRefs;

    /* The filter callback for incoming audio data. */
    PFNAUDIOINPUTCALLBACK pfnFilterCallback;
    void *pvFilterCallback;

    /* Whether the stream has been ended by the filter. */
    bool fEndedByFilter;

    /* Context pointer returned by pfnAudioInputBegin. */
    void *pvUserCtx;

    /* Audio format used for recording. */
    HWVoiceIn *phw;

    /* Number of bytes per frame (bitsPerSample * channels) of the actual input format. */
    uint32_t cBytesPerFrame;

    /* Frequency of the actual audio format. */
    int iFreq;

    /* Convertion from the actual input format to st_sample_t. */
    t_sample *conv;

    /* If the actual format frequence differs from the requested format, this is not NULL. */
    void *rate;

    /* Temporary buffer for st_sample_t representation of the input audio data. */
    void *pvSamplesBuffer;
    uint32_t cbSamplesBufferAllocated;

    /* Temporary buffer for frequency conversion. */
    void *pvRateBuffer;
    uint32_t cbRateBufferAllocated;

} SnifferInputCtx;

static void ictxDelete(SnifferInputCtx *pCtx)
{
    /* The caller will not use this context anymore. */
    if (pCtx->rate)
    {
        st_rate_stop (pCtx->rate);
    }

    RTMemFree(pCtx->pvSamplesBuffer);
    RTMemFree(pCtx->pvRateBuffer);

    memset(pCtx, 0, sizeof(*pCtx));
    RTMemFree(pCtx);
}

static void ictxReallocSamplesBuffer(SnifferInputCtx *pCtx, uint32_t cs)
{
    uint32_t cbBuffer = cs * sizeof(st_sample_t);

    if (cbBuffer > pCtx->cbSamplesBufferAllocated)
    {
        RTMemFree(pCtx->pvSamplesBuffer);

        pCtx->pvSamplesBuffer = RTMemAlloc(cbBuffer);
        if (pCtx->pvSamplesBuffer)
        {
            pCtx->cbSamplesBufferAllocated = cbBuffer;
        }
        else
        {
            pCtx->cbSamplesBufferAllocated = 0;
        }
    }
}

static void ictxReallocRateBuffer(SnifferInputCtx *pCtx, uint32_t cs)
{
    uint32_t cbBuffer = cs * sizeof(st_sample_t);

    if (cbBuffer > pCtx->cbRateBufferAllocated)
    {
        RTMemFree(pCtx->pvRateBuffer);

        pCtx->pvRateBuffer = RTMemAlloc(cbBuffer);
        if (pCtx->pvRateBuffer)
        {
            pCtx->cbRateBufferAllocated = cbBuffer;
        }
        else
        {
            pCtx->cbRateBufferAllocated = 0;
        }
    }
}


/*
 * Filter audio output.
 */

/* Whether the filter should intercept audio output. */
int filter_output_intercepted(void)
{
    return 0; /* @todo Not implemented yet.*/
}

/* Filter informs that an audio output is starting. */
int filter_output_begin(void **ppvOutputCtx, struct audio_pcm_info *pinfo, int samples)
{
    return VERR_NOT_SUPPORTED; /* @todo Not implemented yet.*/
}

/* Filter informs that the audio output has been stopped. */
void filter_output_end(void *pvOutputCtx)
{
    return; /* @todo Not implemented yet.*/
}

/*
 * Filter audio input.
 */

/* Whether the filter should intercept audio input. */
int filter_input_intercepted(void)
{
    if (!g_pData || !g_pData->pDrv)
    {
        return 0;
    }

    return g_pData->fInterceptAudioInput;
}

/* Filter informs that an audio input is starting. */
int filter_input_begin (void **ppvInputCtx, PFNAUDIOINPUTCALLBACK pfnCallback, void *pvCallback, HWVoiceIn *phw, int cSamples)
{
    int rc = VINF_SUCCESS;

    SnifferInputCtx *pCtx = NULL;

    if (!g_pData || !g_pData->pDrv)
    {
        return VERR_NOT_SUPPORTED;
    }

    pCtx = (SnifferInputCtx *)RTMemAlloc(sizeof(SnifferInputCtx));

    if (!pCtx)
    {
        return VERR_NO_MEMORY;
    }

    pCtx->cRefs = 2; /* Context is used by both the filter and the user. */
    pCtx->pfnFilterCallback = pfnCallback;
    pCtx->pvFilterCallback = pvCallback;
    pCtx->fEndedByFilter = false;
    pCtx->pvUserCtx = NULL;
    pCtx->phw = phw;
    pCtx->cBytesPerFrame = 1;
    pCtx->iFreq = 0;
    pCtx->conv = NULL;
    pCtx->rate = NULL;
    pCtx->pvSamplesBuffer = NULL;
    pCtx->cbSamplesBufferAllocated = 0;
    pCtx->pvRateBuffer = NULL;
    pCtx->cbRateBufferAllocated = 0;

    rc = g_pData->pDrv->pfnAudioInputBegin (g_pData->pDrv,
                                            &pCtx->pvUserCtx,      /* Returned by the pDrv. */
                                            pCtx,
                                            cSamples,              /* How many samples in one block is preferred. */
                                            phw->info.freq,        /* Required frequency. */
                                            phw->info.nchannels,   /* Number of audio channels. */
                                            phw->info.bits);       /* A sample size in one channel, samples are signed. */

    if (RT_SUCCESS(rc))
    {
        *ppvInputCtx = pCtx;
    }
    else
    {
        RTMemFree(pCtx);
    }

    Log(("input_begin rc = %Rrc\n", rc));

    return rc;
}

/* Filter informs that the audio input must be stopped. */
void filter_input_end(void *pvCtx)
{
    int32_t c;

    SnifferInputCtx *pCtx = (SnifferInputCtx *)pvCtx;

    void *pvUserCtx = pCtx->pvUserCtx;

    pCtx->fEndedByFilter = true;

    c = ASMAtomicDecS32(&pCtx->cRefs);

    if (c == 0)
    {
        ictxDelete(pCtx);
        pCtx = NULL;
    }

    if (!g_pData || !g_pData->pDrv)
    {
        AssertFailed();
        return;
    }

    g_pData->pDrv->pfnAudioInputEnd (g_pData->pDrv,
                                     pvUserCtx);

    Log(("input_end\n"));
}


/*
 * Audio PDM device.
 */
static DECLCALLBACK(int) iface_AudioInputIntercept (PPDMIAUDIOSNIFFERPORT pInterface, bool fIntercept)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    Assert(g_pData == pThis);

    pThis->fInterceptAudioInput = fIntercept;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) iface_AudioInputEventBegin (PPDMIAUDIOSNIFFERPORT pInterface,
                                                     void *pvContext,
                                                     int iSampleHz,
                                                     int cChannels,
                                                     int cBits,
                                                     bool fUnsigned)
{
    int bitIdx;

    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    int rc = VINF_SUCCESS;

    SnifferInputCtx *pCtx = (SnifferInputCtx *)pvContext;

    Log(("FilterAudio: AudioInputEventBegin: %dHz,%dch,%dbits,%d ended %d\n",
         iSampleHz, cChannels, cBits, fUnsigned, pCtx->fEndedByFilter));

    Assert(g_pData == pThis);

    /* Prepare a format convertion for the actually used format. */
    pCtx->cBytesPerFrame = ((cBits + 7) / 8) * cChannels;

    if (cBits == 16)
    {
        bitIdx = 1;
    }
    else if (cBits == 32)
    {
        bitIdx = 2;
    }
    else
    {
        bitIdx = 0;
    }

    pCtx->conv = mixeng_conv[(cChannels == 2)? 1: 0] /* stereo */
                            [!fUnsigned]             /* sign */
                            [0]                      /* big endian */
                            [bitIdx];                /* bits */

    if (iSampleHz && iSampleHz != pCtx->phw->info.freq)
    {
        pCtx->rate = st_rate_start (iSampleHz, pCtx->phw->info.freq);
        pCtx->iFreq = iSampleHz;
    }

    return rc;
}

static DECLCALLBACK(int) iface_AudioInputEventData (PPDMIAUDIOSNIFFERPORT pInterface,
                                                    void *pvContext,
                                                    const void *pvData,
                                                    uint32_t cbData)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    int rc = VINF_SUCCESS;

    SnifferInputCtx *pCtx = (SnifferInputCtx *)pvContext;

    Log(("FilterAudio: AudioInputEventData: pvData %p. cbData %d, ended %d\n", pvData, cbData, pCtx->fEndedByFilter));

    Assert(g_pData == pThis);

    if (   !pCtx->fEndedByFilter
        && pCtx->conv)
    {
        /* Convert PCM samples to st_sample_t.
         * And then apply rate conversion if necessary.
         */

        /* Optimization: allocate 'ps' buffer once per context and reallocate if cbData changes.
         * Usually size of packets is constant.
         */
        st_sample_t *ps;
        uint32_t cs = cbData / pCtx->cBytesPerFrame; /* How many samples. */

        ictxReallocSamplesBuffer(pCtx, cs);

        ps = (st_sample_t *)pCtx->pvSamplesBuffer;
        if (ps)
        {
            void *pvSamples = NULL;
            uint32_t cbSamples = 0;

            Assert(pCtx->cbSamplesBufferAllocated >= cs * sizeof(st_sample_t));

            pCtx->conv(ps, pvData, cs, &nominal_volume);

            if (pCtx->rate)
            {
                st_sample_t *psConverted;
                uint32_t csConverted = (cs * pCtx->phw->info.freq) / pCtx->iFreq;

                ictxReallocRateBuffer(pCtx, csConverted);

                psConverted = (st_sample_t *)pCtx->pvRateBuffer;
                if (psConverted)
                {
                    int csSrc = cs;
                    int csDst = csConverted;

                    Assert(pCtx->cbRateBufferAllocated >= csConverted * sizeof(st_sample_t));

                    st_rate_flow (pCtx->rate,
                                  ps, psConverted,
                                  &csSrc, &csDst);

                    pvSamples = psConverted;
                    cbSamples = csDst * sizeof(st_sample_t); /* Use csDst as it may be != csConverted */
                }
                else
                {
                    rc = VERR_NO_MEMORY;
                }
            }
            else
            {
                pvSamples = ps;
                cbSamples = cs * sizeof(st_sample_t);
            }

            if (cbSamples)
            {
                rc = pCtx->pfnFilterCallback(pCtx->pvFilterCallback, cbSamples, pvSamples);
            }
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

static DECLCALLBACK(void) iface_AudioInputEventEnd (PPDMIAUDIOSNIFFERPORT pInterface,
                                                    void *pvContext)
{
    int32_t c;

    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    SnifferInputCtx *pCtx = (SnifferInputCtx *)pvContext;

    Log(("FilterAudio: AudioInputEventEnd: ended %d\n", pCtx->fEndedByFilter));

    Assert(g_pData == pThis);

    c = ASMAtomicDecS32(&pCtx->cRefs);

    if (c == 0)
    {
        ictxDelete(pCtx);
        pCtx = NULL;
    }
}

static DECLCALLBACK(int) iface_Setup (PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    Assert(g_pData == pThis);

    pThis->fEnabled = fEnable;
    pThis->fKeepHostAudio = fKeepHostAudio;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) iface_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOSNIFFERPORT, &pThis->IPort);
    return NULL;
}

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) audioSnifferR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    /* Zero the global pointer. */
    g_pData = NULL;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) audioSnifferR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int                rc    = VINF_SUCCESS;
    AUDIOSNIFFERSTATE *pThis = PDMINS_2_DATA(pDevIns, AUDIOSNIFFERSTATE *);

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "InterceptAudioInput\0"))
    {
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    /*
     * Initialize data.
     */
    pThis->fEnabled = false;
    pThis->fKeepHostAudio = true;
    pThis->pDrv = NULL;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "InterceptAudioInput", &pThis->fInterceptAudioInput, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"YieldOnLSRRead\" value"));

    /*
     * Interfaces
     */
    /* Base */
    pThis->IBase.pfnQueryInterface = iface_QueryInterface;

    /* Audio Sniffer port */
    pThis->IPort.pfnSetup = iface_Setup;
    pThis->IPort.pfnAudioInputIntercept = iface_AudioInputIntercept;
    pThis->IPort.pfnAudioInputEventBegin = iface_AudioInputEventBegin;
    pThis->IPort.pfnAudioInputEventData = iface_AudioInputEventData;
    pThis->IPort.pfnAudioInputEventEnd = iface_AudioInputEventEnd;

    /*
     * Get the corresponding connector interface
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Audio Sniffer Port");

    if (RT_SUCCESS(rc))
    {
        pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOSNIFFERCONNECTOR);
        AssertMsgStmt(pThis->pDrv, ("LUN #0 doesn't have a Audio Sniffer connector interface rc=%Rrc\n", rc),
                      rc = VERR_PDM_MISSING_INTERFACE);
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s/%d: warning: no driver attached to LUN #0.\n", pDevIns->pReg->szName, pDevIns->iInstance));
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Failed to attach LUN #0. rc=%Rrc\n", rc));
    }

    if (RT_SUCCESS (rc))
    {
        /* Save PDM device instance data for future reference. */
        pThis->pDevIns = pDevIns;

        /* Save the pointer to created instance in the global variable, so other
         * functions could reach it.
         */
        g_pData = pThis;
    }

    return rc;
}

/**
 * The Audio Sniffer device registration structure.
 */
const PDMDEVREG g_DeviceAudioSniffer =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "AudioSniffer",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio Sniffer device. Redirects audio data to sniffer driver.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(AUDIOSNIFFERSTATE),
    /* pfnConstruct */
    audioSnifferR3Construct,
    /* pfnDestruct */
    audioSnifferR3Destruct,
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
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    PDM_DEVREG_VERSION
};
