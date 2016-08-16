/* $Id$ */
/** @file
 * Video recording audio backend for Main.
 */

/*
 * Copyright (C) 2014-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "DrvAudioVideoRec.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>

#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

/* Initialization status indicator used for the recreation of the AudioUnits. */
#define CA_STATUS_UNINIT    UINT32_C(0) /* The device is uninitialized */
#define CA_STATUS_IN_INIT   UINT32_C(1) /* The device is currently initializing */
#define CA_STATUS_INIT      UINT32_C(2) /* The device is initialized */
#define CA_STATUS_IN_UNINIT UINT32_C(3) /* The device is currently uninitializing */

/// @todo move t_sample as a PDM interface
//typedef struct { int mute;  uint32_t r; uint32_t l; } volume_t;

#define INT_MAX         0x7fffffff
volume_t videorec_nominal_volume = {
    0,
    INT_MAX,
    INT_MAX
};

/* The desired buffer length in milliseconds. Will be the target total stream
 * latency on newer version of pulse. Apparent latency can be less (or more.)
 * In case its need to be used. Currently its not used.
 */
#if 0
static struct
{
    int         buffer_msecs_out;
    int         buffer_msecs_in;
} confAudioVideoRec
=
{
    INIT_FIELD (.buffer_msecs_out = ) 100,
    INIT_FIELD (.buffer_msecs_in  = ) 100,
};
#endif

/**
 * Audio video recording driver instance data.
 *
 * @extends PDMIAUDIOSNIFFERCONNECTOR
 */
typedef struct DRVAUDIOVIDEOREC
{
    /** Pointer to audio video recording object. */
    AudioVideoRec      *pAudioVideoRec;
    PPDMDRVINS          pDrvIns;
    /** Pointer to the driver instance structure. */
    PDMIHOSTAUDIO       IHostAudio;
    ConsoleVRDPServer *pConsoleVRDPServer;
    /** Pointer to the DrvAudio port interface that is above it. */
    PPDMIAUDIOCONNECTOR       pUpPort;
} DRVAUDIOVIDEOREC, *PDRVAUDIOVIDEOREC;
typedef struct PDMAUDIOHSTSTRMOUT PDMAUDIOHSTSTRMOUT;
typedef PDMAUDIOHSTSTRMOUT *PPDMAUDIOHSTSTRMOUT;

typedef struct VIDEORECAUDIOIN
{
    /* Audio and audio details for recording */
    PDMAUDIOHSTSTRMIN   pHostVoiceIn;
    void * pvUserCtx;
    /* Number of bytes per frame (bitsPerSample * channels) of the actual input format. */
    uint32_t cBytesPerFrame;
    /* Frequency of the actual audio format. */
    uint32_t uFrequency;
    /* If the actual format frequence differs from the requested format, this is not NULL. */
    void *rate;
    /* Temporary buffer for st_sample_t representation of the input audio data. */
    void *pvSamplesBuffer;
    /* buffer for bytes of samples (not rate converted) */
    uint32_t cbSamplesBufferAllocated;
    /* Temporary buffer for frequency conversion. */
    void *pvRateBuffer;
    /* buffer for bytes rate converted samples */
    uint32_t cbRateBufferAllocated;
    /* A ring buffer for transferring data to the playback thread */
    PRTCIRCBUF pRecordedVoiceBuf;
    t_sample * convAudioDevFmtToStSampl;
    uint32_t fIsInit;
    uint32_t status;
} VIDEORECAUDIOIN, *PVIDEORECAUDIOIN;

typedef struct VIDEORECAUDIOOUT
{
    PDMAUDIOHSTSTRMOUT pHostVoiceOut;
    uint64_t old_ticks;
    uint64_t cSamplesSentPerSec;
} VIDEORECAUDIOOUT, *PVIDEORECAUDIOOUT;

static DECLCALLBACK(int) drvAudioVideoRecInit(PPDMIHOSTAUDIO pInterface)
{
    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

/** @todo Replace this with drvAudioHlpPcmPropsFromCfg(). */
static int drvAudioVideoRecPcmInitInfo(PDMAUDIOPCMPROPS * pProps, PPDMAUDIOSTREAMCFG as)
{
    int rc = VINF_SUCCESS;

    uint8_t cBits = 8, cShift = 0;
    bool fSigned = false;

    switch (as->enmFormat)
    {
        case PDMAUDIOFMT_S8:
            fSigned = 1;
        case PDMAUDIOFMT_U8:
            break;

        case PDMAUDIOFMT_S16:
            fSigned = 1;
        case PDMAUDIOFMT_U16:
            cBits = 16;
            cShift = 1;
            break;

        case PDMAUDIOFMT_S32:
            fSigned = 1;
        case PDMAUDIOFMT_U32:
            cBits = 32;
            cShift = 2;
            break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    pProps->uHz = as->uHz;
    pProps->cBits = cBits;
    pProps->fSigned = fSigned;
    pProps->cChannels = as->cChannels;
    pProps->cShift = (as->cChannels == 2) + cShift;
    pProps->uAlign = (1 << pProps->cShift) - 1;
    pProps->cbBitrate = (pProps->cBits * pProps->uHz * pProps->cChannels) / 8;
    pProps->fSwapEndian = (as->enmEndianness != PDMAUDIOHOSTENDIANESS);

    return rc;
}

/*
 * Hard voice (playback)
 */
static int audio_pcm_hw_find_min_out (PPDMAUDIOHSTSTRMOUT hw, int *nb_livep)
{
    PPDMAUDIOGSTSTRMOUT sw;
    PPDMAUDIOGSTSTRMOUT pIter;
    int m = INT_MAX;
    int nb_live = 0;

    RTListForEach(&hw->lstGstStrmOut, pIter, PDMAUDIOGSTSTRMOUT, Node)
    {
        sw = pIter;
        if (sw->State.fActive || !sw->State.fEmpty)
        {
            m = RT_MIN (m, sw->cTotalSamplesWritten);
            nb_live += 1;
        }
    }

    *nb_livep = nb_live;
    return m;
}

static int audio_pcm_hw_get_live_out2 (PPDMAUDIOHSTSTRMOUT hw, int *nb_live)
{
    int smin;

    smin = audio_pcm_hw_find_min_out (hw, nb_live);

    if (!*nb_live) {
        return 0;
    }
    else
    {
        int live = smin;

        if (live < 0 || live > hw->cSampleBufferSize)
        {
            LogFlowFunc(("Error: live=%d hw->samples=%d\n", live, hw->cSampleBufferSize));
            return 0;
        }
        return live;
    }
}


static int audio_pcm_hw_get_live_out (PPDMAUDIOHSTSTRMOUT hw)
{
    int nb_live;
    int live;

    live = audio_pcm_hw_get_live_out2 (hw, &nb_live);
    if (live < 0 || live > hw->cSampleBufferSize)
    {
        LogFlowFunc(("Error: live=%d hw->samples=%d\n", live, hw->cSampleBufferSize));
        return 0;
    }
    return live;
}

/*
 * Hard voice (capture)
 */
static int audio_pcm_hw_find_min_in (PPDMAUDIOHSTSTRMIN hw)
{
    PPDMAUDIOGSTSTRMIN pIter;
    int m = hw->cTotalSamplesCaptured;

    RTListForEach(&hw->lstGstStreamsIn, pIter, PDMAUDIOGSTSTRMIN, Node)
    {
        if (pIter->State.fActive)
        {
            m = RT_MIN (m, pIter->cTotalHostSamplesRead);
        }
    }
    return m;
}

int audio_pcm_hw_get_live_in (PPDMAUDIOHSTSTRMIN hw)
{
    int live = hw->cTotalSamplesCaptured - audio_pcm_hw_find_min_in (hw);
    if (live < 0 || live > hw->cSampleBufferSize)
    {
        LogFlowFunc(("Error: live=%d hw->samples=%d\n", live, hw->cSampleBufferSize));
        return 0;
    }
    return live;
}

static inline void *advance (void *p, int incr)
{
    uint8_t *d = (uint8_t*)p;
    return (d + incr);
}

static int vrdeReallocSampleBuf(PVIDEORECAUDIOIN pVRDEVoice, uint32_t cSamples)
{
    uint32_t cbBuffer = cSamples * sizeof(PDMAUDIOSAMPLE);
    if (cbBuffer > pVRDEVoice->cbSamplesBufferAllocated)
    {
        /** @todo r=andy Why not using RTMemReAlloc? */
        if (pVRDEVoice->pvSamplesBuffer)
        {
            RTMemFree(pVRDEVoice->pvSamplesBuffer);
            pVRDEVoice->pvSamplesBuffer = NULL;
        }
        pVRDEVoice->pvSamplesBuffer = RTMemAlloc(cbBuffer);
        if (pVRDEVoice->pvSamplesBuffer)
            pVRDEVoice->cbSamplesBufferAllocated = cbBuffer;
        else
            pVRDEVoice->cbSamplesBufferAllocated = 0;
    }

    return VINF_SUCCESS;
}

static int vrdeReallocRateAdjSampleBuf(PVIDEORECAUDIOIN pVRDEVoice, uint32_t cSamples)
{
    uint32_t cbBuffer = cSamples * sizeof(PDMAUDIOSAMPLE);
    if (cbBuffer > pVRDEVoice->cbRateBufferAllocated)
    {
        RTMemFree(pVRDEVoice->pvRateBuffer);
        pVRDEVoice->pvRateBuffer = RTMemAlloc(cbBuffer);
        if (pVRDEVoice->pvRateBuffer)
            pVRDEVoice->cbRateBufferAllocated = cbBuffer;
        else
            pVRDEVoice->cbRateBufferAllocated = 0;
    }

    return VINF_SUCCESS;
}

/*******************************************************************************
 *
 * AudioVideoRec input section
 *
 ******************************************************************************/

/*
 * Callback to feed audio input buffer. Samples format is be the same as
 * in the voice. The caller prepares st_sample_t.
 *
 * @param cbSamples Size of pvSamples array in bytes.
 * @param pvSamples Points to an array of samples.
 *
 * @return IPRT status code.
 */
static int vrdeRecordingCallback(PVIDEORECAUDIOIN pVRDEVoice, uint32_t cbSamples, const void *pvSamples)
{
    int rc = VINF_SUCCESS;
    size_t csWritten = 0;

    Assert((cbSamples % sizeof(PDMAUDIOSAMPLE)) == 0);

    if (!pVRDEVoice->fIsInit)
        return VINF_SUCCESS;

    /* If nothing is pending return immediately. */
    if (cbSamples == 0)
        return VINF_SUCCESS;

    /* How much space is free in the ring buffer? */
    size_t csAvail = RTCircBufFree(pVRDEVoice->pRecordedVoiceBuf) / sizeof(PDMAUDIOSAMPLE); /* bytes -> samples */

    /* How much space is used in the audio buffer. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, cbSamples / sizeof(PDMAUDIOSAMPLE));

    /* Iterate as long as data is available. */
    while (csWritten < csAvail)
    {
        /* How much is left? */
        size_t csToWrite = csAvail - csWritten;
        size_t cbToWrite = csToWrite * sizeof(PDMAUDIOSAMPLE);

        /* Try to acquire the necessary space from the ring buffer. */
        void *pcDst;
        RTCircBufAcquireWriteBlock(pVRDEVoice->pRecordedVoiceBuf, cbToWrite, &pcDst, &cbToWrite);

        /* How much do we get? */
        csToWrite = cbToWrite / sizeof(PDMAUDIOSAMPLE);

        /* Copy the data from the audio buffer to the ring buffer in PVRDEVoice. */
        if (csToWrite)
        {
            memcpy(pcDst, (uint8_t *)pvSamples + (csWritten * sizeof(PDMAUDIOSAMPLE)), cbToWrite);
            csWritten += csToWrite;
        }

        /* Release the ring buffer, so the main thread could start reading this data. */
        RTCircBufReleaseWriteBlock(pVRDEVoice->pRecordedVoiceBuf, cbToWrite);

        if (RT_UNLIKELY(csToWrite == 0))
            break;
    }

    LogFlowFunc(("Finished writing buffer with %RU32 samples (%RU32 bytes)\n",
                 csWritten, csWritten * sizeof(PDMAUDIOSAMPLE)));

    return rc;
}

static DECLCALLBACK(int) drvAudioVideoRecInitOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHostVoiceOut, PPDMAUDIOSTREAMCFG pCfg)
{
    LogFlowFuncEnter();
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);

    PVIDEORECAUDIOOUT pVRDEVoiceOut = (PVIDEORECAUDIOOUT)pHostVoiceOut;
    pHostVoiceOut->cSampleBufferSize = _4K; /* 4096 samples * 4 = 16K bytes total. */

    return drvAudioVideoRecPcmInitInfo(&pVRDEVoiceOut->pHostVoiceOut.Props, pCfg);
}

static DECLCALLBACK(int) drvAudioVideoRecInitIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHostVoiceIn, PPDMAUDIOSTREAMCFG pCfg)
{
    LogFlowFuncEnter();
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);

    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pHostVoiceIn;
    pHostVoiceIn->cSampleBufferSize = _4K; /* 4096 samples * 4 = 16K bytes total. */

    return drvAudioVideoRecPcmInitInfo(&pVRDEVoice->pHostVoiceIn.Props, pCfg);
}

static DECLCALLBACK(int) drvAudioVideoRecCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHostVoiceIn,
                                                   uint32_t *pcSamplesCaptured)
{
    /** @todo Take care of the size of the buffer allocated to pHostVoiceIn. */
    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pHostVoiceIn;

    /* use this from DrvHostCoreAudio.c */
    if (ASMAtomicReadU32(&pVRDEVoice->status) != CA_STATUS_INIT)
    {
        LogFlowFunc(("VRDE voice not initialized\n"));

        *pcSamplesCaptured = 0;
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* how much space is used in the ring buffer in pRecordedVocieBuf with pAudioVideoRec . Bytes-> samples*/
    size_t cSamplesRingBuffer = RTCircBufUsed(pVRDEVoice->pRecordedVoiceBuf) / sizeof(PDMAUDIOSAMPLE);

    /* How much space is available in the mix buffer. Use the smaller size of the too. */
    cSamplesRingBuffer = RT_MIN(cSamplesRingBuffer, (uint32_t)(pVRDEVoice->pHostVoiceIn.cSampleBufferSize -
                                audio_pcm_hw_get_live_in (&pVRDEVoice->pHostVoiceIn)));

    LogFlowFunc(("Start reading buffer with %d samples (%d bytes)\n", cSamplesRingBuffer,
                 cSamplesRingBuffer * sizeof(PDMAUDIOSAMPLE)));

    /* Iterate as long as data is available */
    size_t cSamplesRead = 0;
    while (cSamplesRead < cSamplesRingBuffer)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        size_t cSamplesToRead = RT_MIN(cSamplesRingBuffer - cSamplesRead,
                                       (uint32_t)(pVRDEVoice->pHostVoiceIn.cSampleBufferSize - pVRDEVoice->pHostVoiceIn.offSamplesWritten));
        size_t cbToRead = cSamplesToRead * sizeof(PDMAUDIOSAMPLE);
        LogFlowFunc(("Try reading %zu samples (%zu bytes)\n", cSamplesToRead, cbToRead));

        /* Try to acquire the necessary block from the ring buffer. Remeber in fltRecrodCallback we
         * we are filling this buffer with the audio data available from VRDP. Here we are reading it
         */
        /** @todo do I need to introduce a thread to fill the buffer in fltRecordcallback. So that
         * filling is in separate thread and the reading of that buffer is in separate thread
         */
        void *pvSrc;
        RTCircBufAcquireReadBlock(pVRDEVoice->pRecordedVoiceBuf, cbToRead, &pvSrc, &cbToRead);

        /* How much to we get? */
        cSamplesToRead = cbToRead / sizeof(PDMAUDIOSAMPLE);
        LogFlowFunc(("AuderVRDE: There are %d samples (%d bytes) available\n", cSamplesToRead, cbToRead));

        /* Break if nothing is used anymore. */
        if (cSamplesToRead)
        {
            /* Copy the data from our ring buffer to the mix buffer. */
            PPDMAUDIOSAMPLE psDst = pVRDEVoice->pHostVoiceIn.paSamples + pVRDEVoice->pHostVoiceIn.offSamplesWritten;
            memcpy(psDst, pvSrc, cbToRead);
        }

        /* Release the read buffer, so it could be used for new data. */
        RTCircBufReleaseReadBlock(pVRDEVoice->pRecordedVoiceBuf, cbToRead);

        if (!cSamplesToRead)
            break;

        pVRDEVoice->pHostVoiceIn.offSamplesWritten = (pVRDEVoice->pHostVoiceIn.offSamplesWritten + cSamplesToRead)
                                              % pVRDEVoice->pHostVoiceIn.cSampleBufferSize;

        /* How much have we reads so far. */
        cSamplesRead += cSamplesToRead;
    }

    LogFlowFunc(("Finished reading buffer with %zu samples (%zu bytes)\n",
                 cSamplesRead, cSamplesRead * sizeof(PDMAUDIOSAMPLE)));

    *pcSamplesCaptured = cSamplesRead;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHostVoiceOut,
                                                 uint32_t *pcSamplesPlayed)
{
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);
    PVIDEORECAUDIOOUT pVRDEVoiceOut = (PVIDEORECAUDIOOUT)pHostVoiceOut;

    /*
     * Just call the VRDP server with the data.
     */
    int live = audio_pcm_hw_get_live_out(pHostVoiceOut);
    uint64_t now = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
    uint64_t ticks = now  - pVRDEVoiceOut->old_ticks;
    uint64_t ticks_per_second = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    int cSamplesPlayed = (int)((2 * ticks * pHostVoiceOut->Props.uHz + ticks_per_second) / ticks_per_second / 2);
    if (cSamplesPlayed < 0)
        cSamplesPlayed = live;

    pHostVoiceOut->Props.cBits = 128; /** @todo Make this configurable (or at least a define)? */
    VRDEAUDIOFORMAT format = VRDE_AUDIO_FMT_MAKE(pHostVoiceOut->Props.uHz,
                                                 pHostVoiceOut->Props.cChannels,
                                                 pHostVoiceOut->Props.cBits, /* bits per sample */
                                                 !pHostVoiceOut->Props.fSigned);

    LogFlowFunc(("freq=%d, chan=%d, cBits = %d, fsigned = %d, cSamples=%d format=%d\n",
                 pHostVoiceOut->Props.uHz, pHostVoiceOut->Props.cChannels,
                 pHostVoiceOut->Props.cBits, pHostVoiceOut->Props.fSigned,
                 pHostVoiceOut->cSampleBufferSize, format));

    pVRDEVoiceOut->old_ticks = now;
    int cSamplesToSend = RT_MIN(live, cSamplesPlayed);

    if (pHostVoiceOut->cOffSamplesRead + cSamplesToSend > pHostVoiceOut->cSampleBufferSize)
    {
        /* send the samples till the end of pHostStereoSampleBuf */
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->paSamples[pHostVoiceOut->cOffSamplesRead],
                                                   (pHostVoiceOut->cSampleBufferSize - pHostVoiceOut->cOffSamplesRead), format);
        /*pHostStereoSampleBuff already has the samples which exceeded its space. They have overwriten the old
         * played sampled starting from offset 0. So based on the number of samples that we had to play,
         * read the number of samples from offset 0 .
         */
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->paSamples[0],
                                                   (cSamplesToSend - (pHostVoiceOut->cSampleBufferSize -
                                                                      pHostVoiceOut->cOffSamplesRead)),
                                                   format);
    }
    else
    {
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->paSamples[pHostVoiceOut->cOffSamplesRead],
                                                   cSamplesToSend, format);
    }

    pHostVoiceOut->cOffSamplesRead = (pHostVoiceOut->cOffSamplesRead + cSamplesToSend) % pHostVoiceOut->cSampleBufferSize;

    *pcSamplesPlayed = cSamplesToSend;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN hw)
{
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);
    LogFlowFuncEnter();
    pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHostVoiceOut)
{
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);
    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT hw,
                                                    PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);
    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHostVoiceIn,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PDRVAUDIOVIDEOREC pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVIDEOREC, IHostAudio);

    /* Initialize  VRDEVoice and return to VRDP server which returns this struct back to us
     * in the form void * pvContext
     */
    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pHostVoiceIn;
    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    /* initialize only if not already done */
    if (enmStreamCmd == PDMAUDIOSTREAMCMD_ENABLE)
    {
        /// @todo if (!pVRDEVoice->fIsInit)
        //    RTCircBufReset(pVRDEVoice->pRecordedVoiceBuf);
        pVRDEVoice->fIsInit = 1;
        pVRDEVoice->pHostVoiceIn = *pHostVoiceIn;
        pVRDEVoice->cBytesPerFrame = 1;
        pVRDEVoice->uFrequency = 0;
        pVRDEVoice->rate = NULL;
        pVRDEVoice->cbSamplesBufferAllocated = 0;
        pVRDEVoice->pvRateBuffer = NULL;
        pVRDEVoice->cbRateBufferAllocated = 0;

        pVRDEVoice->pHostVoiceIn.cSampleBufferSize = 2048;
        /* Initialize the hardware info section with the audio settings */

        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_IN_INIT);

        /* Create the internal ring buffer. */
        RTCircBufCreate(&pVRDEVoice->pRecordedVoiceBuf,
                        pVRDEVoice->pHostVoiceIn.cSampleBufferSize * sizeof(PDMAUDIOSAMPLE));

        if (!RT_VALID_PTR(pVRDEVoice->pRecordedVoiceBuf))
        {
            LogRel(("Failed to create internal ring buffer\n"));
            return  VERR_NO_MEMORY;
        }

        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_INIT);
        return pDrv->pConsoleVRDPServer->SendAudioInputBegin(NULL, pVRDEVoice, pHostVoiceIn->cSampleBufferSize,
                                                             pHostVoiceIn->Props.uHz,
                                                             pHostVoiceIn->Props.cChannels, pHostVoiceIn->Props.cBits);
    }
    else if (enmStreamCmd == PDMAUDIOSTREAMCMD_DISABLE)
    {
        pVRDEVoice->fIsInit = 0;
        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_IN_UNINIT);
        RTCircBufDestroy(pVRDEVoice->pRecordedVoiceBuf);
        pVRDEVoice->pRecordedVoiceBuf = NULL;
        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_UNINIT);
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVideoRecGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    pCfg->cbStreamIn     = sizeof(VIDEORECAUDIOIN);
    pCfg->cbStreamOut    = sizeof(VIDEORECAUDIOOUT);
    pCfg->cMaxStreamsIn  = UINT32_MAx;
    pCfg->cMaxStreamsOut = UINT32_MAX;
    pCfg->cSources       = 1;
    pCfg->cSinks         = 1;

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvAudioVideoRecShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVideoRecQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVIDEOREC  pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVIDEOREC);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}

AudioVideoRec::AudioVideoRec(Console *pConsole)
    : mpDrv(NULL),
      mParent(pConsole)
{
}

AudioVideoRec::~AudioVideoRec(void)
{
    if (mpDrv)
    {
        mpDrv->pAudioVideoRec = NULL;
        mpDrv = NULL;
    }
}

int AudioVideoRec::handleVideoRecSvrCmdAudioInputIntercept(bool fIntercept)
{
    LogFlowThisFunc(("fIntercept=%RTbool\n", fIntercept));
    return VINF_SUCCESS;
}

int AudioVideoRec::handleVideoRecSvrCmdAudioInputEventBegin(void *pvContext, int iSampleHz, int cChannels,
                                                            int cBits, bool fUnsigned)
{
    int bitIdx;
    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pvContext;
    LogFlowFunc(("handleVRDPCmdInputEventBegin\n"));
    /* Prepare a format convertion for the actually used format. */
    pVRDEVoice->cBytesPerFrame = ((cBits + 7) / 8) * cChannels;
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

    //PPDMIAUDIOCONNECTOR pPort = server->mConsole->getAudioVideoRec()->getDrvAudioPort();
    /* Call DrvAudio interface to get the t_sample type conversion function */
    /*mpDrv->pUpPort->pfnConvDevFmtToStSample(mpDrv->pUpPort,
                                            (cChannels == 2) ? 1 : 0,
                                            !fUnsigned, 0, bitIdx,
                                            pVRDEVoice->convAudioDevFmtToStSampl);*/
    if (pVRDEVoice->convAudioDevFmtToStSampl)
    {
        LogFlowFunc(("Failed to get the conversion function \n"));
    }
    LogFlowFunc(("Required freq as requested by VRDP Server = %d\n", iSampleHz));
    //if (iSampleHz && iSampleHz != pVRDEVoice->pHostVoiceIn.Props.uFrequency)
    {
        /** @todo if the above condition is false then pVRDEVoice->uFrequency will remain 0 */
        /*mpDrv->pUpPort->pfnPrepareAudioConversion(mpDrv->pUpPort, iSampleHz,
                                                  pVRDEVoice->pHostVoiceIn.Props.uFrequency,
                                                  &pVRDEVoice->rate);*/
        pVRDEVoice->uFrequency = iSampleHz;
        LogFlowFunc(("pVRDEVoice assigned requested freq =%d\n", pVRDEVoice->uFrequency));
    }
    return VINF_SUCCESS;
}

/*
 * pvContext: pointer to VRDP voice returned by the VRDP server. The is same pointer that we initialized in
 *            drvAudioVideoRecDisableEnableIn VOICE_ENABLE case.
 */
int AudioVideoRec::handleVideoRecSvrCmdAudioInputEventData(void *pvContext, const void *pvData, uint32_t cbData)
{
    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pvContext;
    PPDMAUDIOSAMPLE pHostStereoSampleBuf; /* target sample buffer */
    PPDMAUDIOSAMPLE pConvertedSampleBuf; /* samples adjusted for rate */
    uint32_t cSamples = cbData / pVRDEVoice->cBytesPerFrame; /* Count of samples */
    void * pTmpSampleBuf = NULL;
    uint32_t cConvertedSamples; /* samples adjusted for rate */
    uint32_t cbSamples = 0; /* count of bytes occupied by samples */
    int rc = VINF_SUCCESS;

    LogFlowFunc(("handleVRDPCmdInputEventData cbData = %d, bytesperfram=%d\n",
              cbData, pVRDEVoice->cBytesPerFrame));

    vrdeReallocSampleBuf(pVRDEVoice, cSamples);
    pHostStereoSampleBuf = (PPDMAUDIOSAMPLE)pVRDEVoice->pvSamplesBuffer;
    pVRDEVoice->convAudioDevFmtToStSampl(pHostStereoSampleBuf, pvData, cSamples, &videorec_nominal_volume);

    /* count of rate adjusted samples */
    pVRDEVoice->uFrequency = 22100; /** @todo handle this. How pVRDEVoice will get proper value */
    cConvertedSamples = (cSamples * pVRDEVoice->pHostVoiceIn.Props.uHz) / pVRDEVoice->uFrequency;
    vrdeReallocRateAdjSampleBuf(pVRDEVoice, cConvertedSamples);

    pConvertedSampleBuf = (PPDMAUDIOSAMPLE)pVRDEVoice->pvRateBuffer;
    if (pConvertedSampleBuf)
    {
        uint32_t cSampleSrc = cSamples;
        uint32_t cSampleDst = cConvertedSamples;
        /*mpDrv->pUpPort->pfnDoRateConversion(mpDrv->pUpPort, pVRDEVoice->rate, pHostStereoSampleBuf,
                                            pConvertedSampleBuf, &cSampleSrc, &cConvertedSamples);*/
        pTmpSampleBuf = pConvertedSampleBuf;
        cbSamples =  cConvertedSamples * sizeof(PDMAUDIOSAMPLE);
    }

    if (cbSamples)
        rc = vrdeRecordingCallback(pVRDEVoice, cbSamples, pTmpSampleBuf);

    return rc;
}

/*
 * pvContext: pointer to VRDP voice returned by the VRDP server. The is same pointer that we initialized in
 *            drvAudioVideoRecDisableEnableIn VOICE_ENABLE case.
 */
int AudioVideoRec::handleVideoRecSvrCmdAudioInputEventEnd(void *pvContext)
{
    LogFlowFuncEnter();

    PVIDEORECAUDIOIN pVRDEVoice = (PVIDEORECAUDIOIN)pvContext;
    AssertPtrReturn(pVRDEVoice, VERR_INVALID_POINTER);

    /* The caller will not use this context anymore. */
    if (pVRDEVoice->rate)
    {
        //mpDrv->pUpPort->pfnEndAudioConversion(mpDrv->pUpPort, pVRDEVoice->rate);
    }

    if (pVRDEVoice->pvSamplesBuffer)
    {
        RTMemFree(pVRDEVoice->pvSamplesBuffer);
        pVRDEVoice->pvSamplesBuffer = NULL;
    }

    if (pVRDEVoice->pvRateBuffer)
    {
        RTMemFree(pVRDEVoice->pvRateBuffer);
        pVRDEVoice->pvRateBuffer = NULL;
    }

    return VINF_SUCCESS;
}

/**
 * Construct a VRDE audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/* static */
DECLCALLBACK(int) AudioVideoRec::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVAUDIOVIDEOREC pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVIDEOREC);
    LogRel(("Audio: Initializing VRDE driver\n"));
    LogFlowFunc(("fFlags=0x%x\n", fFlags));

    /* we save the address of AudioVideoRec in Object node in CFGM tree and address of VRDP server in
     * ObjectVRDPServer node. So presence of both is necessary.
     */
    //if (!CFGMR3AreValuesValid(pCfg, "Object\0") || !CFGMR3AreValuesValid(pCfg, "ObjectVRDPServer\0"))
    //   return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvAudioVideoRecQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvAudioVideoRec);

    /* Get VRDPServer pointer. */
    void *pvUser;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectVRDPServer", &pvUser);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Confguration error: No/bad \"ObjectVRDPServer\" value, rc=%Rrc\n", rc));
        return rc;
    }

    /* CFGM tree saves the pointer to ConsoleVRDPServer in the Object node of AudioVideoRec. */
    pThis->pConsoleVRDPServer = (ConsoleVRDPServer *)pvUser;

    pvUser = NULL;
    rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Confguration error: No/bad \"Object\" value, rc=%Rrc\n", rc));
        return rc;
    }

    pThis->pAudioVideoRec = (AudioVideoRec *)pvUser;
    pThis->pAudioVideoRec->mpDrv = pThis;

    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls.
     * Described in CFGM tree.
     */
    pThis->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    if (!pThis->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No upper interface specified!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    return VINF_SUCCESS;
}

/* static */
DECLCALLBACK(void) AudioVideoRec::drvDestruct(PPDMDRVINS pDrvIns)
{
    LogFlowFuncEnter();
}

/**
 * VRDE audio driver registration record.
 */
const PDMDRVREG AudioVideoRec::DrvReg =
{
    PDM_DRVREG_VERSION,
    /* szName */
    "AudioVideoRec",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio driver for video recording",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOVIDEOREC),
    /* pfnConstruct */
    AudioVideoRec::drvConstruct,
    /* pfnDestruct */
    AudioVideoRec::drvDestruct,
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

