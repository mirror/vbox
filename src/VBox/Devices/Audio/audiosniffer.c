/* $Id$ */
/** @file
 * VBox audio device: Audio sniffer device
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/file.h>

#include "Builtins.h"
#include "../../vl_vbox.h"

#include "audio.h"
#include "audio_int.h"

typedef struct _AUDIOSNIFFERSTATE
{
    /** If the device is enabled. */
    bool fEnabled;

    /** Whether audio should reach the host driver too. */
    bool fKeepHostAudio;

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

    void                         *pCapFileCtx;
} AUDIOSNIFFERSTATE;

static AUDIOSNIFFERSTATE *g_pData = NULL;

typedef struct {
    RTFILE      capFile;
    int         curSampPerSec;
    int         curBitsPerSmp;
    int         curChannels;
    uint64_t    lastChunk;
} AUDCAPSTATE;

typedef struct {
    uint16_t    wFormatTag;
    uint16_t    nChannels;
    uint32_t    nSamplesPerSec;
    uint32_t    nAvgBytesPerSec;
    uint16_t    nBlockAlign;
    uint16_t    wBitsPerSample;
} WAVEFMTHDR;

static update_prev_chunk(AUDCAPSTATE *pState)
{
    size_t          written;
    uint64_t        cur_ofs;
    uint64_t        new_ofs;
    uint32_t        chunk_len;

    Assert(pState);
    /* Write the size of the previous data chunk, if there was one. */
    if (pState->lastChunk)
    {
        cur_ofs = RTFileTell(pState->capFile);
        chunk_len = cur_ofs - pState->lastChunk - sizeof(chunk_len);
        RTFileWriteAt(pState->capFile, pState->lastChunk, &chunk_len, sizeof(chunk_len), &written);
        RTFileSeek(pState->capFile, 0, RTFILE_SEEK_END, &new_ofs);
    }
}

static int create_capture_file(AUDCAPSTATE *pState, const char *fname)
{
    int             rc;
    size_t          written;

    Assert(pState);
    memset(pState, 0, sizeof(*pState));
    /* Create the file and write the RIFF header. */
    rc = RTFileOpen(&pState->capFile, fname, 
                    RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE);
    rc = RTFileWrite(pState->capFile, "RIFFxxxx", 8, &written);
    return rc;
}

static int close_capture_file(AUDCAPSTATE *pState)
{
    int             rc;
    size_t          written;
    uint64_t        cur_ofs;
    uint32_t        riff_len;

    Assert(pState);
    update_prev_chunk(pState);

    /* Update the global RIFF header. */
    cur_ofs = RTFileTell(pState->capFile);
    riff_len = cur_ofs - 8;
    RTFileWriteAt(pState->capFile, 4, &riff_len, sizeof(riff_len), &written);
    
    rc = RTFileClose(pState->capFile);
    return rc;
}

static inline int16_t clip_natural_int16_t(int64_t v)
{
    if (v >= 0x7f000000) {
        return 0x7fff;
    }
    else if (v < -2147483648LL) {
        return (-32767-1);
    }
    return  ((int16_t) (v >> (32 - 16)));
}

static int update_capture_file(AUDCAPSTATE *pState, HWVoiceOut *hw, st_sample_t *pSamples, unsigned cSamples)
{
    size_t          written;
    uint16_t        buff[16384];
    unsigned        i;
    uint16_t        *dst_smp;

    Assert(pState);
    /* If the audio format changed, start a new WAVE chunk. */
    if (   hw->info.freq != pState->curSampPerSec
        || hw->info.bits != pState->curBitsPerSmp
        || hw->info.nchannels != pState->curChannels)
    {
        WAVEFMTHDR      wave_hdr;
        uint32_t        chunk_len;

        update_prev_chunk(pState);

        /* Build a new format ('fmt ') chunk. */
        wave_hdr.wFormatTag      = 1;    /* Linear PCM */
        wave_hdr.nChannels       = hw->info.nchannels;
        wave_hdr.nSamplesPerSec  = hw->info.freq;
        wave_hdr.nAvgBytesPerSec = hw->info.bytes_per_second;
        wave_hdr.nBlockAlign     = 4;
        wave_hdr.wBitsPerSample  = hw->info.bits;

        pState->curSampPerSec = hw->info.freq;
        pState->curBitsPerSmp = hw->info.bits;
        pState->curChannels   = hw->info.nchannels;

        /* Write the header to file. */
        RTFileWrite(pState->capFile, "WAVEfmt ", 8, &written);
        chunk_len = sizeof(wave_hdr);
        RTFileWrite(pState->capFile, &chunk_len, sizeof(chunk_len), &written);
        RTFileWrite(pState->capFile, &wave_hdr, sizeof(wave_hdr), &written);
        /* Write data chunk marker with dummy length. */
        RTFileWrite(pState->capFile, "dataxxxx", 8, &written);
        pState->lastChunk = RTFileTell(pState->capFile) - 4;
    }

    /* Convert the samples from internal format. */
    //@todo: use mixer engine helpers instead?
    for (i = 0, dst_smp = buff; i < cSamples; ++i)
    {
        *dst_smp++ = clip_natural_int16_t(pSamples->l); 
        *dst_smp++ = clip_natural_int16_t(pSamples->r);
        ++pSamples;
    }

//    LogRel(("Audio: captured %d samples\n", cSamples));
    /* Write the audio data. */
    RTFileWrite(pState->capFile, buff, cSamples * (hw->info.bits / 8) * hw->info.nchannels, &written);
    return VINF_SUCCESS;
}

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

    if (g_pData)
        update_capture_file(g_pData->pCapFileCtx, hw, pvSamples, cSamples);

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
 * Audio Sniffer PDM device.
 */

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
    AUDIOSNIFFERSTATE *pThis = PDMINS_2_DATA(pDevIns, AUDIOSNIFFERSTATE *);
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    close_capture_file(pThis->pCapFileCtx);
    RTMemFree(pThis->pCapFileCtx);

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
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0"))
    {
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    /*
     * Initialize data.
     */
    pThis->fEnabled = false;
    pThis->fKeepHostAudio = true;
    pThis->pDrv = NULL;

    /*
     * Interfaces
     */
    /* Base */
    pThis->IBase.pfnQueryInterface = iface_QueryInterface;

    /* Audio Sniffer port */
    pThis->IPort.pfnSetup = iface_Setup;

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

    pThis->pCapFileCtx = RTMemAlloc(sizeof(AUDCAPSTATE));
    create_capture_file(pThis->pCapFileCtx, "c:\\vbox.wav");

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
