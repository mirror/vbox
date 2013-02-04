/* $Id$ */
/** @file
 * VBox audio devices: filter driver, which sits between the host audio driver
 * and the virtual audio device and intercept all host driver operations.
 *
 * The filter is used mostly for remote audio input.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/cdefs.h>

#define AUDIO_CAP "filteraudio"
#include "vl_vbox.h"
#include "audio.h"
#include "audio_int.h"

#define FILTER_EXTENSIVE_LOGGING

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


static void IORingBufferCreate(PIORINGBUFFER *ppBuffer, uint32_t cbSize)
{
    PIORINGBUFFER pTmpBuffer;

    AssertPtr(ppBuffer);

    *ppBuffer = NULL;
    pTmpBuffer = RTMemAllocZ(sizeof(IORINGBUFFER));
    if (pTmpBuffer)
    {
        pTmpBuffer->pBuffer = RTMemAlloc(cbSize);
        if(pTmpBuffer->pBuffer)
        {
            pTmpBuffer->cBufSize = cbSize;
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

/*******************************************************************************
 *
 * Global structures section
 *
 ******************************************************************************/

/* Initialization status indicator used for the recreation of the AudioUnits. */
#define CA_STATUS_UNINIT    UINT32_C(0) /* The device is uninitialized */
#define CA_STATUS_IN_INIT   UINT32_C(1) /* The device is currently initializing */
#define CA_STATUS_INIT      UINT32_C(2) /* The device is initialized */
#define CA_STATUS_IN_UNINIT UINT32_C(3) /* The device is currently uninitializing */

struct
{
    struct audio_driver *pDrv;
    void *pDrvOpaque;
} filter_conf =
{
    INIT_FIELD(.pDrv =) NULL,
    INIT_FIELD(.pDrvOpaque =) NULL
};

/*
 * filterVoiceOut and filterVoiceIn are allocated at the end of the original driver HWVoice structure:
 * {
 *    HWVoiceOut;
 *    OriginalDriverHWVoiceData;
 *    filterVoiceOut;
 * }
 */
typedef struct filterVoiceOut
{
    /* HW voice input structure, which prepends the filterVoiceOut. */
    HWVoiceOut *phw;

    /* A ring buffer for transferring data to the playback thread */
    PIORINGBUFFER pBuf;

    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;

    /* Whether the output stream is used by the filter. */
    bool fIntercepted;

    /* Whether this stream is active. */
    bool fIsRunning;

    /* Sniffer level context for this audio output stream. */
    void *pvOutputCtx;
} filterVoiceOut;

typedef struct filterVoiceIn
{
    /* HW voice input structure, which prepends the filterVoiceIn. */
    HWVoiceIn *phw;

    /* A temporary position value. */
    uint32_t rpos;

    /* A ring buffer for transferring data from the recording thread */
    PIORINGBUFFER pBuf;

    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;

    /* the stream has been successfully initialized by host. */
    bool fHostOK;

    /* Whether the input stream is used by the filter. */
    bool fIntercepted;

    /* Whether this stream is active. */
    bool fIsRunning;

    /* Sniffer level context for this audio input stream. */
    void *pvInputCtx;
} filterVoiceIn;

#ifdef FILTER_EXTENSIVE_LOGGING
# define CA_EXT_DEBUG_LOG(a) Log(a)
#else
# define CA_EXT_DEBUG_LOG(a) do {} while(0)
#endif

/*******************************************************************************
 *
 * CoreAudio output section
 *
 ******************************************************************************/

/* We need some forward declarations */
static int filteraudio_run_out(HWVoiceOut *hw);
static int filteraudio_write(SWVoiceOut *sw, void *buf, int len);
static int filteraudio_ctl_out(HWVoiceOut *hw, int cmd, ...);
static void filteraudio_fini_out(HWVoiceOut *hw);
static int filteraudio_init_out(HWVoiceOut *hw, audsettings_t *as);
static int caInitOutput(HWVoiceOut *hw);
static void caReinitOutput(HWVoiceOut *hw);

static int fltInitOutput(filterVoiceOut *pVoice)
{
    uint32_t cFrames; /* default frame count */
    uint32_t cSamples; /* samples count */

    ASMAtomicXchgU32(&pVoice->status, CA_STATUS_IN_INIT);

    cFrames = 2048;

    /* Create the internal ring buffer. */
    cSamples = cFrames * pVoice->phw->info.nchannels;
    IORingBufferCreate(&pVoice->pBuf, cSamples << pVoice->phw->info.shift);
    if (!RT_VALID_PTR(pVoice->pBuf))
    {
        LogRel(("FilterAudio: [Output] Failed to create internal ring buffer\n"));
        return -1;
    }

    if (   pVoice->phw->samples != 0
        && pVoice->phw->samples != (int32_t)cSamples)
        LogRel(("FilterAudio: [Output] Warning! After recreation, the CoreAudio ring buffer doesn't has the same size as the device buffer (%RU32 vs. %RU32).\n", cSamples, (uint32_t)pVoice->phw->samples));
    ASMAtomicXchgU32(&pVoice->status, CA_STATUS_INIT);

    Log(("FilterAudio: [Output] Frame count: %RU32\n", cFrames));

    return 0;
}

static int filteraudio_run_out(HWVoiceOut *phw)
{
    uint32_t csAvail = 0;
    uint32_t cbToWrite = 0;
    uint32_t csToWrite = 0;
    uint32_t csWritten = 0;
    char *pcDst = NULL;
    st_sample_t *psSrc = NULL;

    filterVoiceOut *pVoice = (filterVoiceOut *)((uint8_t *)phw + filter_conf.pDrv->voice_size_out);

    if (!pVoice->fIntercepted)
    {
        return filter_conf.pDrv->pcm_ops->run_out(phw);
    }

    /* We return the live count in the case we are not initialized. This should
     * prevent any under runs. */
    if (ASMAtomicReadU32(&pVoice->status) != CA_STATUS_INIT)
        return audio_pcm_hw_get_live_out(pVoice->phw);

    /* Make sure the device is running */
    filteraudio_ctl_out(pVoice->phw, VOICE_ENABLE);

    /* How much space is available in the ring buffer */
    csAvail = IORingBufferFree(pVoice->pBuf) >> pVoice->phw->info.shift; /* bytes -> samples */

    /* How much data is available. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)audio_pcm_hw_get_live_out(pVoice->phw));

    CA_EXT_DEBUG_LOG(("FilterAudio: [Output] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << pVoice->phw->info.shift));

    /* Iterate as long as data is available */
    while (csWritten < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToWrite = RT_MIN(csAvail - csWritten, (uint32_t)(pVoice->phw->samples - pVoice->phw->rpos));
        cbToWrite = csToWrite << pVoice->phw->info.shift; /* samples -> bytes */
        CA_EXT_DEBUG_LOG(("FilterAudio: [Output] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));

        /* Try to acquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(pVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);

        /* How much to we get? */
        csToWrite = cbToWrite >> pVoice->phw->info.shift;
        CA_EXT_DEBUG_LOG(("FilterAudio: [Output] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));

        /* Break if nothing is free anymore. */
        if (RT_UNLIKELY(cbToWrite == 0))
            break;

        /* Copy the data from our mix buffer to the ring buffer. */
        psSrc = pVoice->phw->mix_buf + pVoice->phw->rpos;
        pVoice->phw->clip((uint8_t*)pcDst, psSrc, csToWrite);

        /* Release the ring buffer, so the read thread could start reading this data. */
        IORingBufferReleaseWriteBlock(pVoice->pBuf, cbToWrite);

        pVoice->phw->rpos = (pVoice->phw->rpos + csToWrite) % pVoice->phw->samples;

        /* How much have we written so far. */
        csWritten += csToWrite;
    }

    CA_EXT_DEBUG_LOG(("FilterAudio: [Output] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << pVoice->phw->info.shift));

    /* Return the count of samples we have processed. */
    return csWritten;
}

static int filteraudio_write(SWVoiceOut *sw, void *buf, int len)
{
    /* Every host backend just calls the generic function, so no need to forward. */
    return audio_pcm_sw_write (sw, buf, len);
}

static int filteraudio_ctl_out(HWVoiceOut *phw, int cmd, ...)
{
    uint32_t status;

    filterVoiceOut *pVoice = (filterVoiceOut *)((uint8_t *)phw + filter_conf.pDrv->voice_size_out);

    if (!pVoice->fIntercepted)
    {
        /* Note: audio.c does not use variable parameters '...', so ok to forward only 'phw' and 'cmd'. */
        return filter_conf.pDrv->pcm_ops->ctl_out(phw, cmd);
    }

    status = ASMAtomicReadU32(&pVoice->status);
    if (!(status == CA_STATUS_INIT))
        return 0;

    switch (cmd)
    {
        case VOICE_ENABLE:
            {
                /* Only start the device if it is actually stopped */
                if (!pVoice->fIsRunning)
                {
                    IORingBufferReset(pVoice->pBuf);
                    filter_output_begin(&pVoice->pvOutputCtx, &pVoice->phw->info, pVoice->phw->samples);
                }
                break;
            }
        case VOICE_DISABLE:
            {
                /* Only stop the device if it is actually running */
                if (pVoice->fIsRunning)
                {
                    filter_output_end(pVoice->pvOutputCtx);
                }
                break;
            }
    }
    return 0;
}

static void filteraudio_fini_out(HWVoiceOut *phw)
{
    int rc = 0;
    uint32_t status;

    filterVoiceOut *pVoice = (filterVoiceOut *)((uint8_t *)phw + filter_conf.pDrv->voice_size_out);

    if (!pVoice->fIntercepted)
    {
        filter_conf.pDrv->pcm_ops->fini_out(phw);
        return;
    }

    status = ASMAtomicReadU32(&pVoice->status);
    if (!(status == CA_STATUS_INIT))
        return;

    rc = filteraudio_ctl_out(phw, VOICE_DISABLE);
    if (RT_LIKELY(rc == 0))
    {
        ASMAtomicXchgU32(&pVoice->status, CA_STATUS_IN_UNINIT);
        IORingBufferDestroy(pVoice->pBuf);
        pVoice->pBuf = NULL;
        ASMAtomicXchgU32(&pVoice->status, CA_STATUS_UNINIT);
    }
    else
        LogRel(("FilterAudio: [Output] Failed to stop playback (%RI32)\n", rc));
}

static int filteraudio_init_out(HWVoiceOut *phw, audsettings_t *as)
{
    int rc = 0;

    filterVoiceOut *pVoice = (filterVoiceOut *)((uint8_t *)phw + filter_conf.pDrv->voice_size_out);

    if (!filter_output_intercepted())
    {
        pVoice->fIntercepted = false;
        return filter_conf.pDrv->pcm_ops->init_out(phw, as);
    }

    /* Output is not tested and is not used currently */
    AssertFailed();
    return -1;

    ASMAtomicXchgU32(&pVoice->status, CA_STATUS_UNINIT);

    pVoice->fIntercepted = true;
    pVoice->phw = phw;
    pVoice->phw->samples = 0;

    /* Initialize the hardware info section with the audio settings */
    audio_pcm_init_info(&pVoice->phw->info, as);

    rc = fltInitOutput(pVoice);
    if (RT_UNLIKELY(rc != 0))
        return rc;

    /* The samples have to correspond to the internal ring buffer size. */
    pVoice->phw->samples = (IORingBufferSize(pVoice->pBuf) >> pVoice->phw->info.shift) / pVoice->phw->info.nchannels;

    Log(("FilterAudio: [Output] HW samples: %d\n", pVoice->phw->samples));

    return 0;
}

/*******************************************************************************
 *
 * FilterAudio input section
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
static DECLCALLBACK(int) fltRecordingCallback(void* pvCallback,
                                              uint32_t cbSamples,
                                              const void *pvSamples)
{
    int rc = VINF_SUCCESS;
    uint32_t csAvail = 0;
    uint32_t csToWrite = 0;
    uint32_t cbToWrite = 0;
    uint32_t csWritten = 0;
    char *pcDst = NULL;

    filterVoiceIn *pVoice = (filterVoiceIn *)pvCallback;

    Assert((cbSamples % sizeof(st_sample_t)) == 0);

    if (!pVoice->fIsRunning)
        return VINF_SUCCESS;

    /* If nothing is pending return immediately. */
    if (cbSamples == 0)
        return VINF_SUCCESS;

    /* How much space is free in the ring buffer? */
    csAvail = IORingBufferFree(pVoice->pBuf) / sizeof(st_sample_t); /* bytes -> samples */

    /* How much space is used in the audio buffer. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, cbSamples / sizeof(st_sample_t));

    CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail * sizeof(st_sample_t)));

    /* Iterate as long as data is available */
    while(csWritten < csAvail)
    {
        /* How much is left? */
        csToWrite = csAvail - csWritten;
        cbToWrite = csToWrite * sizeof(st_sample_t);
        CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));

        /* Try to acquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(pVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);

        /* How much do we get? */
        csToWrite = cbToWrite / sizeof(st_sample_t);
        CA_EXT_DEBUG_LOG(("FilterAudio: [Input] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));

        /* Break if nothing is free anymore. */
        if (RT_UNLIKELY(csToWrite == 0))
            break;

        /* Copy the data from the audio buffer to the ring buffer. */
        memcpy(pcDst, (uint8_t *)pvSamples + (csWritten * sizeof(st_sample_t)), cbToWrite);

        /* Release the ring buffer, so the main thread could start reading this data. */
        IORingBufferReleaseWriteBlock(pVoice->pBuf, cbToWrite);

        csWritten += csToWrite;
    }

    CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten * sizeof(st_sample_t)));

    return rc;
}

static int filteraudio_run_in(HWVoiceIn *phw)
{
    uint32_t csAvail = 0;
    uint32_t cbToRead = 0;
    uint32_t csToRead = 0;
    uint32_t csReads = 0;
    char *pcSrc;
    st_sample_t *psDst;
    filterVoiceIn *pVoice;

    if (!filter_conf.pDrv)
    {
        AssertFailed();
        return -1;
    }

    pVoice = (filterVoiceIn *)((uint8_t *)phw + filter_conf.pDrv->voice_size_in);

    if (!pVoice->fIntercepted)
    {
        if (!pVoice->fHostOK)
        {
            /* Host did not initialize the voice. */
            Log(("FilterAudio: [Input]: run_in voice %p (hw %p) not available on host\n", pVoice, pVoice->phw));
            return -1;
        }

        Log(("FilterAudio: [Input]: forwarding run_in for voice %p (hw %p)\n", pVoice, pVoice->phw));
        return filter_conf.pDrv->pcm_ops->run_in(phw);
    }

    Log(("FilterAudio: [Input]: run_in for voice %p (hw %p)\n", pVoice, pVoice->phw));

    if (!pVoice->fIsRunning)
        return 0;

    /* How much space is used in the ring buffer? */
    csAvail = IORingBufferUsed(pVoice->pBuf) / sizeof(st_sample_t); /* bytes -> samples */

    /* How much space is available in the mix buffer. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)(pVoice->phw->samples - audio_pcm_hw_get_live_in (pVoice->phw)));
    CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Start reading buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail * sizeof(st_sample_t)));

    /* Iterate as long as data is available */
    while (csReads < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToRead = RT_MIN(csAvail - csReads, (uint32_t)(pVoice->phw->samples - pVoice->phw->wpos));
        cbToRead = csToRead * sizeof(st_sample_t);
        CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Try reading %RU32 samples (%RU32 bytes)\n", csToRead, cbToRead));

        /* Try to acquire the necessary block from the ring buffer. */
        IORingBufferAquireReadBlock(pVoice->pBuf, cbToRead, &pcSrc, &cbToRead);

        /* How much to we get? */
        csToRead = cbToRead / sizeof(st_sample_t);
        CA_EXT_DEBUG_LOG(("FilterAudio: [Input] There are %RU32 samples (%RU32 bytes) available\n", csToRead, cbToRead));

        /* Break if nothing is used anymore. */
        if (csToRead == 0)
            break;

        /* Copy the data from our ring buffer to the mix buffer. */
        psDst = pVoice->phw->conv_buf + pVoice->phw->wpos;
        memcpy(psDst, pcSrc, cbToRead);

        /* Release the read buffer, so it could be used for new data. */
        IORingBufferReleaseReadBlock(pVoice->pBuf, cbToRead);

        pVoice->phw->wpos = (pVoice->phw->wpos + csToRead) % pVoice->phw->samples;

        /* How much have we reads so far. */
        csReads += csToRead;
    }

    CA_EXT_DEBUG_LOG(("FilterAudio: [Input] Finished reading buffer with %RU32 samples (%RU32 bytes)\n", csReads, csReads * sizeof(st_sample_t)));

    return csReads;
}

static int filteraudio_read(SWVoiceIn *sw, void *buf, int size)
{
    /* Every host backend just calls the generic function, so no need to forward. */
    return audio_pcm_sw_read (sw, buf, size);
}

static int filteraudio_ctl_in(HWVoiceIn *phw, int cmd, ...)
{
    int rc = VINF_SUCCESS;
    filterVoiceIn *pVoice;

    if (!filter_conf.pDrv)
    {
        AssertFailed();
        return -1;
    }

    pVoice = (filterVoiceIn *)((uint8_t *)phw + filter_conf.pDrv->voice_size_in);

    if (cmd == VOICE_ENABLE)
    {
        /* Decide who will provide input audio: filter or host driver. */
        if (!filter_input_intercepted())
        {
            if (!pVoice->fHostOK)
            {
                /* Host did not initialize the voice. */
                Log(("FilterAudio: [Input]: ctl_in ENABLE voice %p (hw %p) not available on host\n", pVoice, pVoice->phw));
                return -1;
            }

            /* Note: audio.c does not use variable parameters '...', so ok to forward only 'phw' and 'cmd'. */
            Log(("FilterAudio: [Input]: forwarding ctl_in ENABLE for voice %p (hw %p)\n", pVoice, pVoice->phw));
            return filter_conf.pDrv->pcm_ops->ctl_in(phw, cmd);
        }

        /* The filter will use this voice. */
        Log(("FilterAudio: [Input]: ctl_in ENABLE for voice %p (hw %p), cmd %d\n", pVoice, pVoice->phw, cmd));

        if (ASMAtomicReadU32(&pVoice->status) != CA_STATUS_INIT)
            return -1;

        /* Only start the device if it is actually stopped */
        if (!pVoice->fIsRunning)
        {
            IORingBufferReset(pVoice->pBuf);

            /* Sniffer will inform us on a second thread for new incoming audio data.
             * Therefore register an callback function, which will process the new data.
             * */
            rc = filter_input_begin(&pVoice->pvInputCtx, fltRecordingCallback, pVoice, pVoice->phw, pVoice->phw->samples);
            if (RT_SUCCESS(rc))
            {
                pVoice->fIsRunning = true;

                /* Remember that this voice is used by the filter. */
                pVoice->fIntercepted = true;
            }
        }
        if (RT_FAILURE(rc))
        {
            LogRel(("FilterAudio: [Input] Failed to start recording (%Rrc)\n", rc));
            return -1;
        }
    }
    else if (cmd == VOICE_DISABLE)
    {
        if (ASMAtomicReadU32(&pVoice->status) != CA_STATUS_INIT)
            return -1;

        /* Check if the voice has been intercepted. */
        if (!pVoice->fIntercepted)
        {
            if (!pVoice->fHostOK)
            {
                /* Host did not initialize the voice. Theoretically should not happen, because
                 * audio.c should not disable a voice which has not been enabled at all.
                 */
                Log(("FilterAudio: [Input]: ctl_in DISABLE voice %p (hw %p) not available on host\n", pVoice, pVoice->phw));
                return -1;
            }

            /* Note: audio.c does not use variable parameters '...', so ok to forward only 'phw' and 'cmd'. */
            Log(("FilterAudio: [Input]: forwarding ctl_in DISABLE for voice %p (hw %p)\n", pVoice, pVoice->phw));
            return filter_conf.pDrv->pcm_ops->ctl_in(phw, cmd);
        }

        /* The filter used this voice. */
        Log(("FilterAudio: [Input]: ctl_in DISABLE for voice %p (hw %p), cmd %d\n", pVoice, pVoice->phw, cmd));

        /* Only stop the device if it is actually running */
        if (pVoice->fIsRunning)
        {
            pVoice->fIsRunning = false;
            /* Tell the sniffer to not to use this context anymore. */
            filter_input_end(pVoice->pvInputCtx);
        }

        /* This voice is no longer used by the filter. */
        pVoice->fIntercepted = false;
    }
    else
    {
        return -1; /* Unknown command. */
    }

    return 0;
}

static void filteraudio_fini_in(HWVoiceIn *phw)
{
    int ret = -1;
    filterVoiceIn *pVoice;

    if (!filter_conf.pDrv)
    {
        AssertFailed();
        return;
    }

    pVoice = (filterVoiceIn *)((uint8_t *)phw + filter_conf.pDrv->voice_size_in);

    /* Uninitialize both host and filter parts of the voice. */
    if (pVoice->fHostOK)
    {
        /* Uninit host part only if it was initialized by host. */
        Log(("FilterAudio: [Input]: forwarding fini_in for voice %p (hw %p)\n", pVoice, pVoice->phw));
        filter_conf.pDrv->pcm_ops->fini_in(phw);
    }

    Log(("FilterAudio: [Input]: fini_in for voice %p (hw %p)\n", pVoice, pVoice->phw));

    if (ASMAtomicReadU32(&pVoice->status) != CA_STATUS_INIT)
        return;

    /* If this voice is intercepted by filter, try to stop it. */
    if (pVoice->fIntercepted)
    {
        ret = filteraudio_ctl_in(phw, VOICE_DISABLE);
    }
    else
    {
        ret = 0;
    }

    if (RT_LIKELY(ret == 0))
    {
        ASMAtomicWriteU32(&pVoice->status, CA_STATUS_IN_UNINIT);
        IORingBufferDestroy(pVoice->pBuf);
        pVoice->pBuf = NULL;
        pVoice->rpos = 0;
        ASMAtomicWriteU32(&pVoice->status, CA_STATUS_UNINIT);
    }
    else
        LogRel(("FilterAudio: [Input] Failed to stop recording (%RI32)\n", ret));
}

static int filteraudio_init_in(HWVoiceIn *phw, audsettings_t *as)
{
    int hostret = -1;
    filterVoiceIn *pVoice;

    if (!filter_conf.pDrv)
    {
        AssertFailed();
        return -1;
    }

    pVoice = (filterVoiceIn *)((uint8_t *)phw + filter_conf.pDrv->voice_size_in);

    /* Initialize both host and filter parts of the voice. */
    Log(("FilterAudio: [Input]: forwarding init_in for voice %p (hw %p)\n", pVoice, pVoice->phw));
    hostret = filter_conf.pDrv->pcm_ops->init_in(phw, as);

    Log(("FilterAudio: [Input]: init_in for voice %p (hw %p), hostret = %d\n", pVoice, pVoice->phw, hostret));

    ASMAtomicWriteU32(&pVoice->status, CA_STATUS_UNINIT);

    pVoice->phw = phw;
    pVoice->rpos = 0;
    pVoice->pBuf = NULL;
    pVoice->fHostOK = (hostret == 0);
    pVoice->fIntercepted = false;
    pVoice->fIsRunning = false;
    pVoice->pvInputCtx = NULL;

    if (!pVoice->fHostOK)
    {
        /* Initialize required fields of the common part of the voice. */
        pVoice->phw->samples = 2048;

        /* Initialize the hardware info section with the audio settings */
        audio_pcm_init_info(&pVoice->phw->info, as);
    }

    ASMAtomicWriteU32(&pVoice->status, CA_STATUS_IN_INIT);

    /* Create the internal ring buffer. */
    IORingBufferCreate(&pVoice->pBuf, pVoice->phw->samples * sizeof(st_sample_t));

    if (!RT_VALID_PTR(pVoice->pBuf))
    {
        LogRel(("FilterAudio: [Input] Failed to create internal ring buffer\n"));
        return -1;
    }

    ASMAtomicWriteU32(&pVoice->status, CA_STATUS_INIT);

    Log(("FilterAudio: [Input] HW samples: %d\n", pVoice->phw->samples));
    return 0;
}

/*******************************************************************************
 *
 * FilterAudio global section
 *
 ******************************************************************************/

static void *filteraudio_audio_init(void)
{
    /* This is not supposed to be called. */
    Log(("FilterAudio: Init\n"));
    AssertFailed();
    return NULL;
}

static void filteraudio_audio_fini(void *opaque)
{
    Log(("FilterAudio: Init fini %p\n", opaque));
    /* Forward to the host driver. */
    Assert(opaque == filter_conf.pDrvOpaque);
    if (filter_conf.pDrv)
    {
        filter_conf.pDrv->fini(opaque);
        filter_conf.pDrv = NULL;
        filter_conf.pDrvOpaque = NULL;
    }
}

static struct audio_pcm_ops filteraudio_pcm_ops =
{
    filteraudio_init_out,
    filteraudio_fini_out,
    filteraudio_run_out,
    filteraudio_write,
    filteraudio_ctl_out,

    filteraudio_init_in,
    filteraudio_fini_in,
    filteraudio_run_in,
    filteraudio_read,
    filteraudio_ctl_in
};

static struct audio_driver filteraudio_audio_driver =
{
    INIT_FIELD(name           =) "filteraudio",
    INIT_FIELD(descr          =)
    "FilterAudio: filter driver between host audio and virtual device",
    INIT_FIELD(options        =) NULL,
    INIT_FIELD(init           =) filteraudio_audio_init,
    INIT_FIELD(fini           =) filteraudio_audio_fini,
    INIT_FIELD(pcm_ops        =) &filteraudio_pcm_ops,
    INIT_FIELD(can_be_default =) 1,
    INIT_FIELD(max_voices_out =) 1,
    INIT_FIELD(max_voices_in  =) 1,
    INIT_FIELD(voice_size_out =) sizeof(filterVoiceOut),
    INIT_FIELD(voice_size_in  =) sizeof(filterVoiceIn)
};

struct audio_driver *filteraudio_install(struct audio_driver *pDrv, void *pDrvOpaque)
{
    Log(("FilterAudio: [Install]: intercepting driver [%s]\n", pDrv->name));

    /* Modify the audio driver structure to be like the original driver. */
    filteraudio_audio_driver.name           = pDrv->name;
    filteraudio_audio_driver.descr          = pDrv->descr;
    filteraudio_audio_driver.options        = pDrv->options;
    filteraudio_audio_driver.can_be_default = pDrv->can_be_default;
    filteraudio_audio_driver.max_voices_out = pDrv->max_voices_out;
    filteraudio_audio_driver.max_voices_in  = pDrv->max_voices_in;
    filteraudio_audio_driver.voice_size_out = pDrv->voice_size_out + sizeof(filterVoiceOut);
    filteraudio_audio_driver.voice_size_in  = pDrv->voice_size_in + sizeof(filterVoiceIn);

    filter_conf.pDrv = pDrv;
    filter_conf.pDrvOpaque = pDrvOpaque;

    return &filteraudio_audio_driver;
}

int filteraudio_is_host_voice_in_ok(struct audio_driver *pDrv, HWVoiceIn *phw)
{
    filterVoiceIn *pVoice;

    if (pDrv != &filteraudio_audio_driver)
    {
        /* This is not the driver for which the filter was installed.
         * The filter has no idea and assumes that if the voice
         * is not NULL then it is a valid host voice.
         */
        return (phw != NULL);
    }

    if (!filter_conf.pDrv)
    {
        AssertFailed();
        return (phw != NULL);
    }

    pVoice = (filterVoiceIn *)((uint8_t *)phw + filter_conf.pDrv->voice_size_in);

    return pVoice->fHostOK;
}

int filteraudio_is_host_voice_out_ok(struct audio_driver *pDrv, HWVoiceOut *phw)
{
    /* Output is not yet implemented and there are no filter voices.
     * The filter has no idea and assumes that if the voice
     * is not NULL then it is a valid host voice.
     *
     * @todo: similar to filteraudio_is_host_voice_in_ok
     */
    NOREF(pDrv);
    return (phw != NULL);
}
