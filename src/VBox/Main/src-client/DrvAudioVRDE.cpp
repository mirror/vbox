/* $Id$ */
/** @file
 *
 * VBox Audio VRDE backend
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "DrvAudioVRDE.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>
#include <iprt/mem.h>
#include <iprt/cdefs.h>


/*******************************************************************************
 *
 * IO Ring Buffer section
 *
 ******************************************************************************/

/* Implementation of a lock free ring buffer which could be used in a multi
 * threaded environment. Note that only the acquire, release and getter
 * functions are threading aware. So don't use reset if the ring buffer is
 * still in use. */
typedef struct IORINGBUFFER
{
    /* The current read position in the buffer */
    uint32_t uReadPos;
    /* The current write position in the buffer */
    uint32_t uWritePos;
    /* How much space of the buffer is currently in use */
    volatile uint32_t cBufferUsed;
    /* How big is the buffer */
    uint32_t cBufSize;
    /* The buffer itself */
    char *pBuffer;
} IORINGBUFFER;
/* Pointer to an ring buffer structure */
typedef IORINGBUFFER* PIORINGBUFFER;

PPDMDRVINS gpDrvIns; //@todo handle this bad programming;

static void IORingBufferCreate(PIORINGBUFFER *ppBuffer, uint32_t cSize)
{
    PIORINGBUFFER pTmpBuffer;

    AssertPtr(ppBuffer);

    *ppBuffer = NULL;
    pTmpBuffer = RTMemAllocZ(sizeof(IORINGBUFFER));
    if (pTmpBuffer)
    {
        pTmpBuffer->pBuffer = RTMemAlloc(cSize);
        if(pTmpBuffer->pBuffer)
        {
            pTmpBuffer->cBufSize = cSize;
            *ppBuffer = pTmpBuffer;
        }
        else
            RTMemFree(pTmpBuffer);
    }
}

static void IORingBufferDestroy(PIORINGBUFFER pBuffer)
{
    if (pBuffer)
    {
        if (pBuffer->pBuffer)
            RTMemFree(pBuffer->pBuffer);
        RTMemFree(pBuffer);
    }
}

DECL_FORCE_INLINE(void) IORingBufferReset(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);

    pBuffer->uReadPos = 0;
    pBuffer->uWritePos = 0;
    pBuffer->cBufferUsed = 0;
}

DECL_FORCE_INLINE(uint32_t) IORingBufferFree(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferUsed(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferSize(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize;
}

static void IORingBufferAquireReadBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uUsed = 0;
    uint32_t uSize = 0;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is in use? */
    uUsed = ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uUsed > 0)
    {
        /* Get the size out of the requested size, the read block till the end
         * of the buffer & the currently used size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uReadPos, uUsed));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current read
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uReadPos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseReadBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uReadPos = (pBuffer->uReadPos + cSize) % pBuffer->cBufSize;
    ASMAtomicSubU32(&pBuffer->cBufferUsed, cSize);
}

static void IORingBufferAquireWriteBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uFree;
    uint32_t uSize;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is free? */
    uFree = pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uFree > 0)
    {
        /* Get the size out of the requested size, the write block till the end
         * of the buffer & the currently free size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uWritePos, uFree));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current write
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uWritePos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseWriteBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uWritePos = (pBuffer->uWritePos + cSize) % pBuffer->cBufSize;

    ASMAtomicAddU32(&pBuffer->cBufferUsed, cSize);
}

/****************** Ring Buffer Function Ends *****************/

//@todo need to see if they need to move to pdmifs.h
#define AUDIO_HOST_ENDIANNESS 0
#define VOICE_ENABLE 1
#define VOICE_DISABLE 2


/* Initialization status indicator used for the recreation of the AudioUnits. */
#define CA_STATUS_UNINIT    UINT32_C(0) /* The device is uninitialized */
#define CA_STATUS_IN_INIT   UINT32_C(1) /* The device is currently initializing */
#define CA_STATUS_INIT      UINT32_C(2) /* The device is initialized */
#define CA_STATUS_IN_UNINIT UINT32_C(3) /* The device is currently uninitializing */

//@todo move t_sample as a PDM interface
//typedef struct { int mute;  uint32_t r; uint32_t l; } volume_t;

#define INT_MAX         0x7fffffff
volume_t nominal_volume = {
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
} confAudioVRDE
=
{
    INIT_FIELD (.buffer_msecs_out = ) 100,
    INIT_FIELD (.buffer_msecs_in  = ) 100,
};
#endif
/**
 * Audio VRDE driver instance data.
 *
 * @extends PDMIAUDIOSNIFFERCONNECTOR
 */
typedef struct DRVAUDIOVRDE
{
    /** Pointer to audio VRDE object */
    AudioVRDE           *pAudioVRDE;
    PPDMDRVINS          pDrvIns;
    /** Pointer to the driver instance structure. */
    PDMIHOSTAUDIO       IHostAudioR3;
    ConsoleVRDPServer *pConsoleVRDPServer;
    /** Pointer to the DrvAudio port interface that is above it. */
    PPDMIAUDIOCONNECTOR       pUpPort;
} DRVAUDIOVRDE, *PDRVAUDIOVRDE;
typedef struct PDMHOSTVOICEOUT PDMHOSTVOICEOUT;
typedef PDMHOSTVOICEOUT *PPDMHOSTVOICEOUT;

typedef struct VRDEVoice
{
    /* Audio and audio details for recording */
    PDMHOSTVOICEIN   pHostVoiceIn;
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
    PIORINGBUFFER pRecordedVoiceBuf;
    t_sample * convAudioDevFmtToStSampl;
    uint32_t fIsInit;
    uint32_t status;
};
typedef VRDEVoice *PVRDEVoice;

typedef struct VRDEVoiceOut
{
    PDMHOSTVOICEOUT pHostVoiceOut;
    uint64_t old_ticks;
    uint64_t cSamplesSentPerSec;
};
typedef VRDEVoiceOut * PVRDEVoiceOut;

AudioVRDE::AudioVRDE(Console *console)
    : mpDrv(NULL),
      mParent(console)
{
}

AudioVRDE::~AudioVRDE()
{
    if (mpDrv)
    {
        mpDrv->pAudioVRDE = NULL;
        mpDrv = NULL;
    }
}

PPDMIAUDIOCONNECTOR AudioVRDE::getDrvAudioPort()
{
    Assert(mpDrv);
    return mpDrv->pUpPort;
}

void AudioVRDE::handleVRDESvrCmdAudioInputIntercept(bool fIntercept)
{
    LogFlow(("AudioVRDE: handleVRDPCmdInputIntercept\n"));
}

static DECLCALLBACK(void *)  drvAudioVRDEInit(PPDMIHOSTAUDIO pInterface)
{
    LogFlow(("drvAudioVRDEInit \n"));
    return 1;
}

static void drvAudioVRDEPcmInitInfo(PDMPCMPROPERTIES * pProps, audsettings_t *as)
{
    int bits = 8, sign = 0, shift = 0;
    LogFlow(("AudioVRDE: PcmInitInfo \n"));

    switch (as->fmt) {
    case AUD_FMT_S8:
        sign = 1;
    case AUD_FMT_U8:
        break;

    case AUD_FMT_S16:
        sign = 1;
    case AUD_FMT_U16:
        bits = 16;
        shift = 1;
        break;

    case AUD_FMT_S32:
        sign = 1;
    case AUD_FMT_U32:
        bits = 32;
        shift = 2;
        break;
    }

    pProps->uFrequency = as->freq;
    pProps->cBits = bits;
    pProps->fSigned = sign;
    pProps->cChannels = as->nchannels;
    pProps->cShift = (as->nchannels == 2) + shift;
    pProps->fAlign = (1 << pProps->cShift) - 1;
    pProps->cbPerSec = pProps->uFrequency << pProps->cShift;
    pProps->fSwapEndian = (as->endianness != AUDIO_HOST_ENDIANNESS);
}

/*
 * Hard voice (playback)
 */
static int audio_pcm_hw_find_min_out (PPDMHOSTVOICEOUT hw, int *nb_livep)
{
    PPDMGSTVOICEOUT sw;
    PPDMGSTVOICEOUT pIter;
    int m = INT_MAX;
    int nb_live = 0;
    LogFlow(("Hard Voice Playback \n"));

    RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
    {
        sw = pIter;
        if (sw->State.fActive || !sw->State.fEmpty)
        {
            m = audio_MIN (m, sw->cSamplesMixed);
            nb_live += 1;
        }
    }

    *nb_livep = nb_live;
    return m;
}

int audio_pcm_hw_get_live_out2 (PPDMHOSTVOICEOUT hw, int *nb_live)
{
    int smin;

    smin = audio_pcm_hw_find_min_out (hw, nb_live);

    if (!*nb_live) {
        return 0;
    }
    else
    {
        int live = smin;

        if (live < 0 || live > hw->cSamples)
        {
            LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
            return 0;
        }
        return live;
    }
}


int audio_pcm_hw_get_live_out (PPDMHOSTVOICEOUT hw)
{
    int nb_live;
    int live;

    live = audio_pcm_hw_get_live_out2 (hw, &nb_live);
    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }
    return live;
}

/*
 * Hard voice (capture)
 */
static int audio_pcm_hw_find_min_in (PPDMHOSTVOICEIN hw)
{
    PPDMGSTVOICEIN pIter;
    int m = hw->cSamplesCaptured;

    RTListForEach(&hw->HeadGstVoiceIn, pIter, PDMGSTVOICEIN, ListGstVoiceIn)
    {
        if (pIter->State.fActive)
        {
            m = audio_MIN (m, pIter->cHostSamplesAcquired);
        }
    }
    return m;
}

int audio_pcm_hw_get_live_in (PPDMHOSTVOICEIN hw)
{
    int live = hw->cSamplesCaptured - audio_pcm_hw_find_min_in (hw);
    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }
    return live;
}

static inline void *advance (void *p, int incr)
{
    uint8_t *d = (uint8_t*)p;
    return (d + incr);
}

uint64_t audio_get_ticks_per_sec (void)
{
    return PDMDrvHlpTMGetVirtualFreq (gpDrvIns);
}

uint64_t audio_get_clock (void)
{
    return PDMDrvHlpTMGetVirtualTime (gpDrvIns);
}

void VRDEReallocSampleBuf(PVRDEVoice pVRDEVoice, uint32_t cSamples)
{
    uint32_t cbBuffer = cSamples * sizeof(PDMHOSTSTEREOSAMPLE);
    if (cbBuffer > pVRDEVoice->cbSamplesBufferAllocated)
    {
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

}

void VRDEReallocRateAdjSampleBuf(PVRDEVoice pVRDEVoice, uint32_t cSamples)
{
    uint32_t cbBuffer = cSamples * sizeof(PDMHOSTSTEREOSAMPLE);
    if (cbBuffer > pVRDEVoice->cbRateBufferAllocated)
    {
        RTMemFree(pVRDEVoice->pvRateBuffer);
        pVRDEVoice->pvRateBuffer = RTMemAlloc(cbBuffer);
        if (pVRDEVoice->pvRateBuffer)
            pVRDEVoice->cbRateBufferAllocated = cbBuffer;
        else
            pVRDEVoice->cbRateBufferAllocated = 0;
    }
}

/*******************************************************************************
 *
 * AudioVRDE input section
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
static int fltRecordingCallback(PVRDEVoice pVRDEVoice, uint32_t cbSamples, const void *pvSamples)
{
    int rc = VINF_SUCCESS;
    uint32_t csAvail = 0;
    uint32_t csToWrite = 0;
    uint32_t cbToWrite = 0;
    uint32_t csWritten = 0;
    char *pcDst = NULL;

    LogFlow(("audio-filter: fltRecordingCallback\n"));

    Assert((cbSamples % sizeof(PDMHOSTSTEREOSAMPLE)) == 0);

    if (!pVRDEVoice->fIsInit)
        return VINF_SUCCESS;

    /* If nothing is pending return immediately. */
    if (cbSamples == 0)
        return VINF_SUCCESS;

    /* How much space is free in the ring buffer? */
    PPDMHOSTSTEREOSAMPLE psSrc;
    csAvail = IORingBufferFree(pVRDEVoice->pRecordedVoiceBuf) / sizeof(PDMHOSTSTEREOSAMPLE); /* bytes -> samples */

    /* How much space is used in the audio buffer. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, cbSamples / sizeof(PDMHOSTSTEREOSAMPLE));

    /* Iterate as long as data is available */
    while(csWritten < csAvail)
    {
        /* How much is left? */
        csToWrite = csAvail - csWritten;
        cbToWrite = csToWrite * sizeof(PDMHOSTSTEREOSAMPLE);

        /* Try to acquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(pVRDEVoice->pRecordedVoiceBuf, cbToWrite, &pcDst, &cbToWrite);

        /* How much do we get? */
        csToWrite = cbToWrite / sizeof(PDMHOSTSTEREOSAMPLE);

            /* Break if nothing is free anymore. */
        if (RT_UNLIKELY(csToWrite == 0))
            break;

        /* Copy the data from the audio buffer to the ring buffer in PVRDEVoice. */
        memcpy(pcDst, (uint8_t *)pvSamples + (csWritten * sizeof(PDMHOSTSTEREOSAMPLE)), cbToWrite);

        /* Release the ring buffer, so the main thread could start reading this data. */
        IORingBufferReleaseWriteBlock(pVRDEVoice->pRecordedVoiceBuf, cbToWrite);

        csWritten += csToWrite;
    }

    LogFlow(("AudioVRDE: [Input] Finished writing buffer with %RU32 samples (%RU32 bytes)\n",
              csWritten, csWritten * sizeof(PDMHOSTSTEREOSAMPLE)));

    return rc;
}


STDMETHODIMP AudioVRDE::handleVRDESvrCmdAudioInputEventBegin(void *pvContext, int iSampleHz, int cChannels,
                                                             int cBits, bool fUnsigned)
{
    int bitIdx;
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pvContext;
    LogFlow(("AudioVRDE: handleVRDPCmdInputEventBegin\n"));
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
    //PPDMIAUDIOCONNECTOR pPort = server->mConsole->getAudioVRDE()->getDrvAudioPort();
    /* Call DrvAudio interface to get the t_sample type conversion function */
    pVRDEVoice->convAudioDevFmtToStSampl = mpDrv->pUpPort->pfnConvDevFmtToStSample(mpDrv->pUpPort,
                                                                                   (cChannels == 2) ? 1 : 0,
                                                                                   !fUnsigned, 0, bitIdx
                                                                                  );
    if (pVRDEVoice->convAudioDevFmtToStSampl)
    {
        LogFlow(("AudioVRDE: Failed to get the conversion function \n"));
    }
    LogFlow(("AudioVRDE: Required freq as requested by VRDP Server = %d\n", iSampleHz));
    //if (iSampleHz && iSampleHz != pVRDEVoice->pHostVoiceIn.Props.uFrequency)
    {
        /* @todo if the above condition is false then pVRDEVoice->uFrequency will remain 0 */
        pVRDEVoice->rate = mpDrv->pUpPort->pfnPrepareAudioConversion(mpDrv->pUpPort, iSampleHz,
                                                                     pVRDEVoice->pHostVoiceIn.Props.uFrequency);
        pVRDEVoice->uFrequency = iSampleHz;
        LogFlow(("AudioVRDE: pVRDEVoice assigned requested freq =%d\n", pVRDEVoice->uFrequency));
    }
    return VINF_SUCCESS;
}

/*
 * pvContext: pointer to VRDP voice returned by the VRDP server. The is same pointer that we initialized in
 *            drvAudioVRDEDisableEnableIn VOICE_ENABLE case.
 */
void AudioVRDE::handleVRDESvrCmdAudioInputEventData(void *pvContext, const void *pvData, uint32_t cbData)
{
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pvContext;
    PPDMHOSTSTEREOSAMPLE pHostStereoSampleBuf; /* target sample buffer */
    PPDMHOSTSTEREOSAMPLE pConvertedSampleBuf; /* samples adjusted for rate */
    uint32_t cSamples = cbData / pVRDEVoice->cBytesPerFrame; /* Count of samples */
    void * pTmpSampleBuf = NULL;
    uint32_t cConvertedSamples; /* samples adjusted for rate */
    uint32_t cbSamples; /* count of bytes occupied by samples */
    uint32_t rc;
    LogFlow(("AudioVRDE: handleVRDPCmdInputEventData cbData = %d, bytesperfram=%d\n",
              cbData, pVRDEVoice->cBytesPerFrame));

    VRDEReallocSampleBuf(pVRDEVoice, cSamples);
    pHostStereoSampleBuf = (PPDMHOSTSTEREOSAMPLE)pVRDEVoice->pvSamplesBuffer;
    pVRDEVoice->convAudioDevFmtToStSampl(pHostStereoSampleBuf, pvData, cSamples, &nominal_volume);

    /* count of rate adjusted samples */
    pVRDEVoice->uFrequency = 22100; /* @todo handle this. How pVRDEVoice will get proper value */
    cConvertedSamples = (cSamples * pVRDEVoice->pHostVoiceIn.Props.uFrequency) / pVRDEVoice->uFrequency;
    VRDEReallocRateAdjSampleBuf(pVRDEVoice, cConvertedSamples);

    pConvertedSampleBuf = (PPDMHOSTSTEREOSAMPLE)pVRDEVoice->pvRateBuffer;

    if (pConvertedSampleBuf)
    {
        uint32_t cSampleSrc = cSamples;
        uint32_t cSampleDst = cConvertedSamples;
        mpDrv->pUpPort->pfnDoRateConversion(mpDrv->pUpPort, pVRDEVoice->rate, pHostStereoSampleBuf,
                                            pConvertedSampleBuf, &cSampleSrc, &cConvertedSamples);
        pTmpSampleBuf = pConvertedSampleBuf;
        cbSamples =  cConvertedSamples * sizeof(PDMHOSTSTEREOSAMPLE);
    }

    if (cbSamples)
    {
        rc = fltRecordingCallback(pVRDEVoice, cbSamples, pTmpSampleBuf);
    }
}

/*
 * pvContext: pointer to VRDP voice returned by the VRDP server. The is same pointer that we initialized in
 *            drvAudioVRDEDisableEnableIn VOICE_ENABLE case.
 */
void AudioVRDE::handleVRDESvrCmdAudioInputEventEnd(void *pvContext)
{
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pvContext;
    LogFlow(("AudioVRDE: handleVRDPCmdInputEventEnd\n"));
     /* The caller will not use this context anymore. */
    if (pVRDEVoice->rate)
    {
        mpDrv->pUpPort->pfnEndAudioConversion(mpDrv->pUpPort, pVRDEVoice->rate);
    }

    if (pVRDEVoice->pvSamplesBuffer)
    {
        RTMemFree(pVRDEVoice->pvSamplesBuffer);
        pVRDEVoice->pvSamplesBuffer = NULL;
    }
    if(pVRDEVoice->pvRateBuffer)
    {
        RTMemFree(pVRDEVoice->pvRateBuffer);
        pVRDEVoice->pvRateBuffer = NULL;
    }
}

static DECLCALLBACK(int)  drvAudioVRDEInitOut(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEOUT pHostVoiceOut, audsettings_t *as)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);

    PVRDEVoiceOut pVRDEVoiceOut = (PVRDEVoiceOut)pHostVoiceOut;
    LogFlow(("DrvAudioVRDEInitOut: audio input begin cShift=%d\n", pHostVoiceOut->Props.cShift));
    pHostVoiceOut->cSamples =  6174;
    drvAudioVRDEPcmInitInfo(&pVRDEVoiceOut->pHostVoiceOut.Props, as);
    return VINF_SUCCESS;

}

static DECLCALLBACK(int) drvAudioVRDEInitIn (PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEIN pHostVoiceIn, audsettings_t *as)
{
    LogFlow(("DrvAudioVRDE: drvAudioVRDEInitIn \n"));
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pHostVoiceIn;
    pHostVoiceIn->cSamples =  6174;
    drvAudioVRDEPcmInitInfo(&pVRDEVoice->pHostVoiceIn.Props, as);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVRDEPlayIn(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEIN pHostVoiceIn)
{
    uint32_t cbAvlblRingBuffer = 0;
    uint32_t cSamplesRingBuffer = 0;
    uint32_t cSamplesToRead = 0;
    uint32_t cSamplesRead = 0;
    uint32_t cbToRead;
    char *pcSrc;
    PDMHOSTSTEREOSAMPLE * psDst;
    //@todo take care of the size of the buffer allocated to pHostVoiceIn
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pHostVoiceIn;
    LogFlow(("DrvAudioVRDE: drvAudioVRDEPlayIn \n"));

 /* use this from DrvHostCoreAudio.c */
    if (ASMAtomicReadU32(&pVRDEVoice->status) != CA_STATUS_INIT)
    {
        LogFlow(("AudioVRDE: VRDE voice not initialized \n"));
        return 0;
    }

    /* how much space is used in the ring buffer in pRecordedVocieBuf with pAudioVRDE . Bytes-> samples*/
    cSamplesRingBuffer = IORingBufferUsed(pVRDEVoice->pRecordedVoiceBuf) / sizeof(PDMHOSTSTEREOSAMPLE);

    /* How much space is available in the mix buffer. Use the smaller size of the too. */
    cSamplesRingBuffer = RT_MIN(cSamplesRingBuffer, (uint32_t)(pVRDEVoice->pHostVoiceIn.cSamples -
                               audio_pcm_hw_get_live_in (&pVRDEVoice->pHostVoiceIn)));
    LogFlow(("AudioVRDE: [Input] Start reading buffer with %d samples (%d bytes)\n", cSamplesRingBuffer,
              cSamplesRingBuffer * sizeof(PDMHOSTSTEREOSAMPLE)));

    /* Iterate as long as data is available */
    while (cSamplesRead < cSamplesRingBuffer)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        cSamplesToRead = RT_MIN(cSamplesRingBuffer - cSamplesRead,
                                (uint32_t)(pVRDEVoice->pHostVoiceIn.cSamples - pVRDEVoice->pHostVoiceIn.offWrite));
        cbToRead = cSamplesToRead * sizeof(PDMHOSTSTEREOSAMPLE);
        LogFlow(("AudioVRDE: [Input] Try reading %RU32 samples (%RU32 bytes)\n", cSamplesToRead, cbToRead));

        /* Try to acquire the necessary block from the ring buffer. Remeber in fltRecrodCallback we
         * we are filling this buffer with the audio data available from VRDP. Here we are reading it
         */
        /*todo do I need to introduce a thread to fill the buffer in fltRecordcallback. So that
         * filling is in separate thread and the reading of that buffer is in separate thread
         */
        IORingBufferAquireReadBlock(pVRDEVoice->pRecordedVoiceBuf, cbToRead, &pcSrc, &cbToRead);

        /* How much to we get? */
        cSamplesToRead = cbToRead / sizeof(PDMHOSTSTEREOSAMPLE);
        LogFlow(("AuderVRDE: [Input] There are %d samples (%d bytes) available\n", cSamplesToRead, cbToRead));

        /* Break if nothing is used anymore. */
        if (cSamplesToRead == 0)
        {
            LogFlow(("AudioVRDE: Nothing to read \n"));
            break;
        }

        /* Copy the data from our ring buffer to the mix buffer. */
        psDst = pVRDEVoice->pHostVoiceIn.pConversionBuf + pVRDEVoice->pHostVoiceIn.offWrite;
        memcpy(psDst, pcSrc, cbToRead);

        /* Release the read buffer, so it could be used for new data. */
        IORingBufferReleaseReadBlock(pVRDEVoice->pRecordedVoiceBuf, cbToRead);

        pVRDEVoice->pHostVoiceIn.offWrite = (pVRDEVoice->pHostVoiceIn.offWrite + cSamplesToRead)
                                              % pVRDEVoice->pHostVoiceIn.cSamples;

        /* How much have we reads so far. */
        cSamplesRead += cSamplesToRead;
    }
    LogFlow(("AudioVRDE: [Input] Finished reading buffer with %d samples (%d bytes)\n",
               cSamplesRead, cSamplesRead * sizeof(PDMHOSTSTEREOSAMPLE)));

    return cSamplesRead;
}

static DECLCALLBACK(int) drvAudioVRDEPlayOut(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEOUT pHostVoiceOut)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);
    PVRDEVoiceOut pVRDEVoiceOut = (PVRDEVoiceOut)pHostVoiceOut;
    int live;
    uint8_t     *pu8Dst;
    int cSamplesPlayed;
    int cSamplesToSend = 0;
    /*
     * Just call the VRDP server with the data.
     */
    live = audio_pcm_hw_get_live_out (pHostVoiceOut);
    uint64_t now = audio_get_clock();
    uint64_t ticks = now  - pVRDEVoiceOut->old_ticks;
    uint64_t ticks_per_second = audio_get_ticks_per_sec();
    cSamplesPlayed = (int)((2 * ticks * pHostVoiceOut->Props.uFrequency + ticks_per_second) / ticks_per_second / 2);
    if (cSamplesPlayed < 0)
        cSamplesPlayed = live;
    pHostVoiceOut->Props.cBits = 128;
    VRDEAUDIOFORMAT format = VRDE_AUDIO_FMT_MAKE(pHostVoiceOut->Props.uFrequency,
                                                 pHostVoiceOut->Props.cChannels,
                                                 pHostVoiceOut->Props.cBits, /* bits per sample */
                                                 !pHostVoiceOut->Props.fSigned);
    LogFlow(("DrvAudioVRDE: send audio sample freq=%d, chan=%d, cBits = %d, fsigned = %d, cSamples=%d format=%d \n",
             pHostVoiceOut->Props.uFrequency, pHostVoiceOut->Props.cChannels,
             pHostVoiceOut->Props.cBits, pHostVoiceOut->Props.fSigned,
             pHostVoiceOut->cSamples, format)
            );
    pVRDEVoiceOut->old_ticks = now;
    cSamplesToSend = RT_MIN(live, cSamplesPlayed);
    if (pHostVoiceOut->offRead + cSamplesToSend > pHostVoiceOut->cSamples)
    {
        /* send the samples till the end of pHostStereoSampleBuf */
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->pHostSterioSampleBuf[pHostVoiceOut->offRead],
                                                   (pHostVoiceOut->cSamples - pHostVoiceOut->offRead), format);
        /*pHostStereoSampleBuff already has the samples which exceeded its space. They have overwriten the old
         * played sampled starting from offset 0. So based on the number of samples that we had to play,
         * read the number of samples from offset 0 .
         */
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->pHostSterioSampleBuf[0],
                                                   (cSamplesToSend - (pHostVoiceOut->cSamples -
                                                                     pHostVoiceOut->offRead)),
                                                   format);
    }
    else
    {
        pDrv->pConsoleVRDPServer->SendAudioSamples(&pHostVoiceOut->pHostSterioSampleBuf[pHostVoiceOut->offRead],
                                                   cSamplesToSend, format);
    }
        pHostVoiceOut->offRead = (pHostVoiceOut->offRead + cSamplesToSend) % pHostVoiceOut->cSamples;
    return  cSamplesToSend;
}

static DECLCALLBACK(void) drvAudioVRDEFiniIn(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEIN hw)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);
    LogFlow(("DrvAudioVRDE: drvAudioVRDEFiniIn \n"));
    pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);
}

static DECLCALLBACK(void) drvAudioVRDEFiniOut(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEOUT pHostVoiceOut)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);
    LogFlow(("DrvAudioVRDE: audio input end\n"));
}

static DECLCALLBACK(int) drvAudioVRDEDisableEnableOut(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEOUT hw, int cmd)
{
    LogFlow(("DrvAudioVRDE: drvAudioVRDEDisableEnableOut \n"));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioVRDEDisableEnableIn(PPDMIHOSTAUDIO pInterface, PPDMHOSTVOICEIN pHostVoiceIn, int cmd)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudioR3);

    /* Initialize  VRDEVoice and return to VRDP server which returns this struct back to us
     * in the form void * pvContext
     */
    PVRDEVoice pVRDEVoice = (PVRDEVoice)pHostVoiceIn;
    LogFlow(("DrvAudioVRDE: drvAudioVRDEDisableEnableIn \n"));
    /* initialize only if not already done */
    if (cmd == VOICE_ENABLE)
    {
        //@todo if (!pVRDEVoice->fIsInit)
        //    IORingBufferReset(pVRDEVoice->pRecordedVoiceBuf);
        LogFlow(("DrvAudioVRDE: Intializing the VRDE params and buffer \n"));
        pVRDEVoice->fIsInit = 1;
        pVRDEVoice->pHostVoiceIn = *pHostVoiceIn;
        pVRDEVoice->cBytesPerFrame =1 ;
        pVRDEVoice->uFrequency = 0;
        pVRDEVoice->rate = NULL;
        pVRDEVoice->cbSamplesBufferAllocated = 0;
        pVRDEVoice->pvRateBuffer = NULL;
        pVRDEVoice->cbRateBufferAllocated = 0;

        pVRDEVoice->pHostVoiceIn.cSamples = 2048;
        /* Initialize the hardware info section with the audio settings */

        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_IN_INIT);

        /* Create the internal ring buffer. */
        IORingBufferCreate(&pVRDEVoice->pRecordedVoiceBuf,
                           pVRDEVoice->pHostVoiceIn.cSamples * sizeof(PDMHOSTSTEREOSAMPLE));

        if (!RT_VALID_PTR(pVRDEVoice->pRecordedVoiceBuf))
        {
            LogRel(("AudioVRDE: [Input] Failed to create internal ring buffer\n"));
            return  VERR_NO_MEMORY;
        }

        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_INIT);
        return pDrv->pConsoleVRDPServer->SendAudioInputBegin(NULL, pVRDEVoice, pHostVoiceIn->cSamples,
                                                             pHostVoiceIn->Props.uFrequency,
                                                             pHostVoiceIn->Props.cChannels, pHostVoiceIn->Props.cBits);
    }
    else if (cmd == VOICE_DISABLE)
    {
        LogFlow(("DrvAudioVRDE: Cmd to disable VRDE \n"));
        pVRDEVoice->fIsInit = 0;
        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_IN_UNINIT);
        IORingBufferDestroy(pVRDEVoice->pRecordedVoiceBuf);
        pVRDEVoice->pRecordedVoiceBuf = NULL;
        ASMAtomicWriteU32(&pVRDEVoice->status, CA_STATUS_UNINIT);
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvAudioVRDEGetConf(PPDMIBASE pInterface, PPDMAUDIOCONF pAudioConf)
{
    LogFlow(("drvAudioVRDE: drvAudioVRDEGetConf \n"));
    /* @todo check if szHostVoiceOut = sizeof VRDEVoice works. VRDEVoice doesn't contain HOSTVOICEOUT. */
    pAudioConf->szHostVoiceOut = sizeof(VRDEVoice);
    pAudioConf->szHostVoiceIn = sizeof(VRDEVoice);
    pAudioConf->MaxHostVoicesOut = 1;
    pAudioConf->MaxHostVoicesIn = 1;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVRDEQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVRDE  pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudioR3);
    return NULL;
}


static DECLCALLBACK(void) drvAudioVRDEDestruct(PPDMDRVINS pDrvIns)
{
}

/**
 * Construct a DirectSound Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) AudioVRDE::drvAudioVRDEConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    LogRel(("drvAudioVRDEConstruct\n"));

    /* we save the address of AudioVRDE in Object node in CFGM tree and address of VRDP server in
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
    pThis->pDrvIns                          = pDrvIns;
    gpDrvIns = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface        = drvAudioVRDEQueryInterface;
    pThis->IHostAudioR3.pfnInitIn           = drvAudioVRDEInitIn;
    pThis->IHostAudioR3.pfnInitOut          = drvAudioVRDEInitOut;
    pThis->IHostAudioR3.pfnDisableEnableOut = drvAudioVRDEDisableEnableOut;
    pThis->IHostAudioR3.pfnDisableEnableIn  = drvAudioVRDEDisableEnableIn;
    pThis->IHostAudioR3.pfnFiniIn           = drvAudioVRDEFiniIn;
    pThis->IHostAudioR3.pfnFiniOut          = drvAudioVRDEFiniOut;
    pThis->IHostAudioR3.pfnPlayIn           = drvAudioVRDEPlayIn;
    pThis->IHostAudioR3.pfnPlayOut          = drvAudioVRDEPlayOut;
    pThis->IHostAudioR3.pfnGetConf          = drvAudioVRDEGetConf;
    pThis->IHostAudioR3.pfnInit             = drvAudioVRDEInit;

    /* Get VRDPServer pointer */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectVRDPServer", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("DrvAudioVRDE Confguration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    /* CFGM tree saves the pointer to ConsoleVRDPServer in the Object node of AudioVRDE */
    pThis->pConsoleVRDPServer = (ConsoleVRDPServer *)pv;
    pv = NULL;

    rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("DrvAudioVRDE Confguration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    pThis->pAudioVRDE = (AudioVRDE *)pv;
    pThis->pAudioVRDE->mpDrv = pThis;
    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls .
     * Described in CFGM tree.
     */
    pThis->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    if (!pThis->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No Audio Sniffer port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    return VINF_SUCCESS;
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvAudioVRDE =
{
     PDM_DRVREG_VERSION,
   /* szName */
    "AudioVRDE",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio VRDE",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOVRDE),
    /* pfnConstruct */
    AudioVRDE::drvAudioVRDEConstruct,
    /* pfnDestruct */
    drvAudioVRDEDestruct,
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



