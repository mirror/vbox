/* $Id$ */
/** @file
 * DevSB16 - VBox SB16 Audio Controller.
 */

/*
 * Copyright (C) 2015-2021 Oracle Corporation
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
 * This code is based on: sb16.c from QEMU AUDIO subsystem (r3917).
 * QEMU Soundblaster 16 emulation
 *
 * Copyright (c) 2003-2005 Vassili Karpov (malc)
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
#define LOG_GROUP LOG_GROUP_DEV_SB16
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/AssertGuest.h>

#include "VBoxDD.h"

#include "AudioMixBuffer.h"
#include "AudioMixer.h"
#include "AudioHlp.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The maximum number of separate streams we currently implement.
 *  Currently we only support one stream only, namely the output stream. */
#define SB16_MAX_STREAMS                1
/** The (zero-based) index of the output stream in \a aStreams. */
#define SB16_IDX_OUT                    0

/** Current saved state version. */
#define SB16_SAVE_STATE_VERSION         2
/** The version used in VirtualBox version 3.0 and earlier. This didn't include the config dump. */
#define SB16_SAVE_STATE_VERSION_VBOX_30 1


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char e3[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the SB16 state. */
typedef struct SB16STATE *PSB16STATE;

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
/**
 * Asynchronous I/O state for an SB16 stream.
 */
typedef struct SB16STREAMSTATEAIO
{
    PPDMTHREAD              pThread;
    /** Event for letting the thread know there is some data to process. */
    SUPSEMEVENT             hEvtProcess;
    /** Critical section for synchronizing access. */
    RTCRITSECT              CritSect;
    /** Started indicator. */
    volatile bool           fStarted;
    /** Shutdown indicator. */
    volatile bool           fShutdown;
    /** Whether the thread should do any data processing or not. */
    volatile bool           fEnabled;
    bool                    afPadding[5];
} SB16STREAMSTATEAIO;
/** Pointer to the async I/O state for a SB16 stream. */
typedef SB16STREAMSTATEAIO *PSB16STREAMSTATEAIO;
#endif /* VBOX_WITH_AUDIO_SB16_ASYNC_IO */

/**
 * The internal state of a SB16 stream.
 */
typedef struct SB16STREAMSTATE
{
    /** Flag indicating whether this stream is in enabled state or not. */
    bool                    fEnabled;
#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
    /** Asynchronous I/O state members. */
    SB16STREAMSTATEAIO      AIO;
#endif
    /** DMA cache to read data from / write data to. */
    PRTCIRCBUF              pCircBuf;
} SB16STREAMSTATE;
/** Pointer to internal state of an SB16 stream. */
typedef SB16STREAMSTATE *PSB16STREAMSTATE;

/**
 * Structure defining a (host backend) driver stream.
 * Each driver has its own instances of audio mixer streams, which then
 * can go into the same (or even different) audio mixer sinks.
 */
typedef struct SB16DRIVERSTREAM
{
    /** Associated mixer stream handle. */
    R3PTRTYPE(PAUDMIXSTREAM)        pMixStrm;
    /** The stream's current configuration. */
} SB16DRIVERSTREAM, *PSB16DRIVERSTREAM;

/**
 * Struct for tracking a host backend driver, i.e. our per-LUN data.
 */
typedef struct SB16DRIVER
{
    /** Node for storing this driver in our device driver list of SB16STATE. */
    RTLISTNODER3                    Node;
    /** Pointer to SB16 controller (state). */
    R3PTRTYPE(PSB16STATE)           pSB16State;
    /** Pointer to attached driver base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Audio connector interface to the underlying host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)  pConnector;
    /** Stream for output. */
    SB16DRIVERSTREAM                Out;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                fFlags;
    /** LUN # to which this driver has been assigned. */
    uint8_t                         uLUN;
    /** Whether this driver is in an attached state or not. */
    bool                            fAttached;
    /** The LUN description. */
    char                            szDesc[2+48];
} SB16DRIVER;
/** Pointer to the per-LUN data. */
typedef SB16DRIVER *PSB16DRIVER;

/**
 * Runtime configurable debug stuff for a SB16 stream.
 */
typedef struct SB16STREAMDEBUGRT
{
    /** Whether debugging is enabled or not. */
    bool                            fEnabled;
    uint8_t                         Padding[7];
    /** File for dumping DMA reads / writes.
     *  For input streams, this dumps data being written to the device DMA,
     *  whereas for output streams this dumps data being read from the device DMA. */
    R3PTRTYPE(PAUDIOHLPFILE)        pFileDMA;
} SB16STREAMDEBUGRT;

/**
 * Debug stuff for a SB16 stream.
 */
typedef struct SB16STREAMDEBUG
{
    /** Runtime debug stuff. */
    SB16STREAMDEBUGRT               Runtime;
} SB16STREAMDEBUG;

/**
 * Structure for a SB16 stream.
 */
typedef struct SB16STREAM
{
    /** The stream's own index in \a aStreams of SB16STATE.
     *  Set to UINT8_MAX if not set (yet). */
    uint8_t                         uIdx;
    /** The timer for pumping data thru the attached LUN drivers. */
    TMTIMERHANDLE                   hTimerIO;
    /** The timer interval for pumping data thru the LUN drivers in timer ticks. */
    uint64_t                        cTicksTimerIOInterval;
    /** Timestamp of the last timer callback (sb16TimerIO).
     * Used to calculate the time actually elapsed between two timer callbacks.
     * This currently ASSMUMES that we only have one single (output) stream. */
    uint64_t                        tsTimerIO; /** @todo Make this a per-stream value. */
    /** The stream's current configuration. */
    PDMAUDIOSTREAMCFG               Cfg;
    /** Internal state of this stream. */
    SB16STREAMSTATE                 State;
    /** Debug stuff. */
    SB16STREAMDEBUG                 Dbg;
} SB16STREAM;
/** Pointer to a SB16 stream */
typedef SB16STREAM *PSB16STREAM;

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
/**
 * Asynchronous I/O thread context (arguments).
 */
typedef struct SB16STREAMTHREADCTX
{
    /** The SB16 device state. */
    PSB16STATE              pThis;
    /** The SB16 stream state. */
    PSB16STREAM             pStream;
} SB16STREAMTHREADCTX;
/** Pointer to the context for an async I/O thread. */
typedef SB16STREAMTHREADCTX *PSB16STREAMTHREADCTX;
#endif /* VBOX_WITH_AUDIO_SB16_ASYNC_IO */

/**
 * SB16 debug settings.
 */
typedef struct SB16STATEDEBUG
{
    /** Whether debugging is enabled or not. */
    bool                    fEnabled;
    bool                    afAlignment[7];
    /** Path where to dump the debug output to.
     *  Can be NULL, in which the system's temporary directory will be used then. */
    R3PTRTYPE(char *)       pszOutPath;
} SB16STATEDEBUG;

/**
 * The SB16 state.
 */
typedef struct SB16STATE
{
#ifdef VBOX
    /** Pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** Pointer to the connector of the attached audio driver. */
    PPDMIAUDIOCONNECTOR pDrv;
    int irqCfg;
    int dmaCfg;
    int hdmaCfg;
    int portCfg;
    int verCfg;
#endif
    int irq;
    int dma;
    int hdma;
    int port;
    int ver;

    int dsp_in_idx;
    int dsp_out_data_len;
    int fmt_stereo;
    int fmt_signed;
    int fmt_bits;
    PDMAUDIOFMT fmt;
    int dma_auto;
    int block_size;
    int fifo;
    int freq;
    int time_const;
    int speaker;
    int dsp_in_needed_bytes;
    int cmd;
    int use_hdma;
    int highspeed;
    int can_write; /** @todo r=andy BUGBUG Value never gets set to 0! */

    int v2x6;

    uint8_t csp_param;
    uint8_t csp_value;
    uint8_t csp_mode;
    uint8_t csp_index;
    uint8_t csp_regs[256];
    uint8_t csp_reg83[4];
    int csp_reg83r;
    int csp_reg83w;

    uint8_t dsp_in_data[10];
    uint8_t dsp_out_data[50];
    uint8_t test_reg;
    uint8_t last_read_byte;
    int nzero;

    int left_till_irq; /** Note: Can be < 0. */

    int dma_running;
    int bytes_per_second;
    int align;

    RTLISTANCHOR        lstDrv;
    /** IRQ timer   */
    TMTIMERHANDLE       hTimerIRQ;
    /** The base interface for LUN\#0. */
    PDMIBASE            IBase;

    /** Array of all SB16 hardware audio stream. */
    SB16STREAM             aStreams[SB16_MAX_STREAMS];
    /** The device's software mixer. */
    R3PTRTYPE(PAUDIOMIXER) pMixer;
    /** Audio sink for PCM output. */
    R3PTRTYPE(PAUDMIXSINK) pSinkOut;

    /** The two mixer I/O ports (port + 4). */
    IOMIOPORTHANDLE     hIoPortsMixer;
    /** The 10 DSP I/O ports (port + 6). */
    IOMIOPORTHANDLE     hIoPortsDsp;

    /** Debug settings. */
    SB16STATEDEBUG                  Dbg;

    /* mixer state */
    uint8_t mixer_nreg;
    uint8_t mixer_regs[256];

#ifdef VBOX_WITH_STATISTICS
    STAMPROFILE             StatTimerIO;
    STAMCOUNTER             StatBytesRead;
#endif
} SB16STATE;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
DECLINLINE(PDMAUDIODIR) sb16GetDirFromIndex(uint8_t uIdx);

static int  sb16StreamEnable(PSB16STATE pThis, PSB16STREAM pStream, bool fEnable, bool fForce);
static void sb16StreamReset(PSB16STATE pThis, PSB16STREAM pStream);
static int  sb16StreamOpen(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream);
static void sb16StreamClose(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream);
DECLINLINE(PAUDMIXSINK) sb16StreamIndexToSink(PSB16STATE pThis, uint8_t uIdx);
static void sb16StreamTransferScheduleNext(PSB16STATE pThis, PSB16STREAM pStream, uint32_t cSamples);
static void sb16StreamUpdate(PSB16STREAM pStream, PAUDMIXSINK pSink);
static int  sb16StreamDoDmaOutput(PSB16STATE pThis, PSB16STREAM pStream, int uDmaChan, uint32_t offDma, uint32_t cbDma, uint32_t cbToRead, uint32_t *pcbRead);

static DECLCALLBACK(void) sb16TimerIO(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, void *pvUser);
static DECLCALLBACK(void) sb16TimerIRQ(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, void *pvUser);
DECLINLINE(void) sb16TimerSet(PPDMDEVINS pDevIns, PSB16STREAM pStream, uint64_t cTicksToDeadline);

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
static int  sb16StreamAsyncIOCreate(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream);
static int  sb16StreamAsyncIODestroy(PPDMDEVINS pDevIns, PSB16STREAM pStream);
static int  sb16StreamAsyncIONotify(PPDMDEVINS pDevIns, PSB16STREAM pStream);
#endif /* VBOX_WITH_AUDIO_SB16_ASYNC_IO */

static void sb16SpeakerControl(PSB16STATE pThis, int on);
static void sb16UpdateVolume(PSB16STATE pThis);

#if 0 // unused // def DEBUG
DECLINLINE(void) log_dsp(PSB16STATE pThis)
{
    LogFlowFunc(("%s:%s:%d:%s:dmasize=%d:freq=%d:const=%d:speaker=%d\n",
                 pThis->fmt_stereo ? "Stereo" : "Mono",
                 pThis->fmt_signed ? "Signed" : "Unsigned",
                 pThis->fmt_bits,
                 pThis->dma_auto ? "Auto" : "Single",
                 pThis->block_size,
                 pThis->freq,
                 pThis->time_const,
                 pThis->speaker));
}
#endif

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
/**
 * @callback_method_impl{FNPDMTHREADDEV}
 */
static DECLCALLBACK(int) sb16StreamAsyncIOThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    PSB16STREAMTHREADCTX pCtx = (PSB16STREAMTHREADCTX)pThread->pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    PSB16STATE pThis = pCtx->pThis;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    PSB16STREAM pStream = pCtx->pStream;
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    AssertReturn(pThread == pStream->State.AIO.pThread, VERR_INVALID_PARAMETER);

    PAUDMIXSINK pSink = sb16StreamIndexToSink(pThis, pStream->uIdx);
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);

    while (   pThread->enmState != PDMTHREADSTATE_TERMINATING
           && pThread->enmState != PDMTHREADSTATE_TERMINATED)
    {
        int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pStream->State.AIO.hEvtProcess, RT_INDEFINITE_WAIT);
        if (pStream->State.AIO.fShutdown)
            break;
        AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
        if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
            return VINF_SUCCESS;
        if (rc == VERR_INTERRUPTED)
            continue;

        sb16StreamUpdate(pStream, pSink);
    }

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDEV}
 */
static DECLCALLBACK(int) sb16StreamAsyncIOWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PSB16STREAMTHREADCTX pCtx = (PSB16STREAMTHREADCTX)pThread->pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    PSB16STREAM pStream = pCtx->pStream;
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    sb16StreamAsyncIONotify(pDevIns, pStream);

    return VINF_SUCCESS;
}

/**
 * Creates the async I/O thread for a specific SB16 audio stream.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared SB16 state.
 * @param   pStream             SB16 audio stream to create the async I/O thread for.
 */
static int sb16StreamAsyncIOCreate(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream)
{
    PSB16STREAMSTATEAIO pAIO = &pStream->State.AIO;

    int rc;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
    {
        pAIO->fShutdown = false;
        pAIO->fEnabled  = true; /* Enabled by default. */

        rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pAIO->hEvtProcess);
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pAIO->CritSect);
            if (RT_SUCCESS(rc))
            {
                /** @todo Make the device thread naming more specific once we have more streams here. */

                char szDevTag[20];
                RTStrPrintf(szDevTag, sizeof(szDevTag), "SB16-%u", pDevIns->iInstance);

                PSB16STREAMTHREADCTX pCtx = (PSB16STREAMTHREADCTX)RTMemAllocZ(sizeof(SB16STREAMTHREADCTX));
                if (pCtx)
                {
                    pCtx->pStream = pStream;
                    pCtx->pThis   = pThis;

                    rc = PDMDevHlpThreadCreate(pDevIns, &pAIO->pThread, pCtx, sb16StreamAsyncIOThread,
                                               sb16StreamAsyncIOWakeUp, 0, RTTHREADTYPE_IO, szDevTag);
                    if (RT_FAILURE(rc))
                        RTMemFree(pCtx);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Lets the stream's async I/O thread know that there is some data to process.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pStream             The SB16 stream to notify async I/O thread.
 */
static int sb16StreamAsyncIONotify(PPDMDEVINS pDevIns, PSB16STREAM pStream)
{
    return PDMDevHlpSUPSemEventSignal(pDevIns, pStream->State.AIO.hEvtProcess);
}

/**
 * Destroys the async I/O thread of a specific SB16 audio stream.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pStream             SB16 audio stream to destroy the async I/O thread for.
 */
static int sb16StreamAsyncIODestroy(PPDMDEVINS pDevIns, PSB16STREAM pStream)
{
    PSB16STREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return VINF_SUCCESS;

    ASMAtomicWriteBool(&pAIO->fShutdown, true);

    int rc = sb16StreamAsyncIONotify(pDevIns, pStream);
    AssertRC(rc);

    rc = PDMDevHlpThreadDestroy(pDevIns, pAIO->pThread, NULL);
    AssertRC(rc);

    rc = RTCritSectDelete(&pAIO->CritSect);
    AssertRC(rc);

    if (pStream->State.AIO.hEvtProcess != NIL_SUPSEMEVENT)
    {
        PDMDevHlpSUPSemEventSignal(pDevIns, pStream->State.AIO.hEvtProcess);
        PDMDevHlpSUPSemEventClose(pDevIns, pStream->State.AIO.hEvtProcess);
        pStream->State.AIO.hEvtProcess = NIL_SUPSEMEVENT;
    }

    pAIO->fShutdown = false;
    return rc;
}
#endif /* VBOX_WITH_AUDIO_SB16_ASYNC_IO */

static void sb16SpeakerControl(PSB16STATE pThis, int on)
{
    pThis->speaker = on;
}

static void sb16Control(PPDMDEVINS pDevIns, PSB16STATE pThis, int hold)
{
    int dma = pThis->use_hdma ? pThis->hdma : pThis->dma;
    pThis->dma_running = hold;

    LogFlowFunc(("hold %d high %d dma %d\n", hold, pThis->use_hdma, dma));

    PDMDevHlpDMASetDREQ(pThis->pDevInsR3, dma, hold);

    /* We only support one output stream at the moment, so keep things simple here for now. */
    PSB16STREAM pStream = &pThis->aStreams[SB16_IDX_OUT];

    if (hold)
    {
        int rc = VINF_SUCCESS;

        if (pThis->freq > 0)
        {
            rc = sb16StreamOpen(pDevIns, pThis, pStream);
            if (RT_SUCCESS(rc))
                sb16UpdateVolume(pThis);
        }

        if (RT_SUCCESS(rc))
        {
            rc = sb16StreamEnable(pThis, pStream, true /* fEnable */, false /* fForce */);
            if (RT_SUCCESS(rc))
            {
                sb16TimerSet(pDevIns, pStream, pStream->cTicksTimerIOInterval);

                PDMDevHlpDMASchedule(pThis->pDevInsR3);
            }
        }
    }
    else
    {
        sb16StreamEnable(pThis, pStream, false /* fEnable */, false /* fForce */);
    }
}

#define DMA8_AUTO 1
#define DMA8_HIGH 2

static void continue_dma8(PPDMDEVINS pDevIns, PSB16STATE pThis)
{
    sb16Control(pDevIns, pThis, 1);
}

static void dma_cmd8(PPDMDEVINS pDevIns, PSB16STATE pThis, int mask, int dma_len)
{
    pThis->fmt        = PDMAUDIOFMT_U8;
    pThis->use_hdma   = 0;
    pThis->fmt_bits   = 8;
    pThis->fmt_signed = 0;
    pThis->fmt_stereo = (pThis->mixer_regs[0x0e] & 2) != 0;

    if (-1 == pThis->time_const)
    {
        if (pThis->freq <= 0)
            pThis->freq = 11025;
    }
    else
    {
        int tmp = (256 - pThis->time_const);
        pThis->freq = (1000000 + (tmp / 2)) / tmp;
    }

    if (dma_len != -1)
    {
        pThis->block_size = dma_len << pThis->fmt_stereo;
    }
    else
    {
        /* This is apparently the only way to make both Act1/PL
           and SecondReality/FC work

           r=andy Wow, actually someone who remembers Future Crew :-)

           Act1 sets block size via command 0x48 and it's an odd number
           SR does the same with even number
           Both use stereo, and Creatives own documentation states that
           0x48 sets block size in bytes less one.. go figure */
        pThis->block_size &= ~pThis->fmt_stereo;
    }

    pThis->freq >>= pThis->fmt_stereo;
    pThis->left_till_irq = pThis->block_size;
    pThis->bytes_per_second = (pThis->freq << pThis->fmt_stereo);
    /* pThis->highspeed = (mask & DMA8_HIGH) != 0; */
    pThis->dma_auto = (mask & DMA8_AUTO) != 0;
    pThis->align = (1 << pThis->fmt_stereo) - 1;

    if (pThis->block_size & pThis->align)
        LogFlowFunc(("warning: misaligned block size %d, alignment %d\n", pThis->block_size, pThis->align + 1));

    LogFlowFunc(("freq %d, stereo %d, sign %d, bits %d, dma %d, auto %d, fifo %d, high %d\n",
                 pThis->freq, pThis->fmt_stereo, pThis->fmt_signed, pThis->fmt_bits,
                 pThis->block_size, pThis->dma_auto, pThis->fifo, pThis->highspeed));

    continue_dma8(pDevIns, pThis);
    sb16SpeakerControl(pThis, 1);
}

static void dma_cmd(PPDMDEVINS pDevIns, PSB16STATE pThis, uint8_t cmd, uint8_t d0, int dma_len)
{
    pThis->use_hdma   = cmd < 0xc0;
    pThis->fifo       = (cmd >> 1) & 1;
    pThis->dma_auto   = (cmd >> 2) & 1;
    pThis->fmt_signed = (d0 >> 4) & 1;
    pThis->fmt_stereo = (d0 >> 5) & 1;

    switch (cmd >> 4)
    {
        case 11:
            pThis->fmt_bits = 16;
            break;

        case 12:
            pThis->fmt_bits = 8;
            break;
    }

    if (-1 != pThis->time_const)
    {
#if 1
        int tmp = 256 - pThis->time_const;
        pThis->freq = (1000000 + (tmp / 2)) / tmp;
#else
        /* pThis->freq = 1000000 / ((255 - pThis->time_const) << pThis->fmt_stereo); */
        pThis->freq = 1000000 / ((255 - pThis->time_const));
#endif
        pThis->time_const = -1;
    }

    pThis->block_size = dma_len + 1;
    pThis->block_size <<= ((pThis->fmt_bits == 16) ? 1 : 0);
    if (!pThis->dma_auto)
    {
        /*
         * It is clear that for DOOM and auto-init this value
         * shouldn't take stereo into account, while Miles Sound Systems
         * setsound.exe with single transfer mode wouldn't work without it
         * wonders of SB16 yet again.
         */
        pThis->block_size <<= pThis->fmt_stereo;
    }

    LogFlowFunc(("freq %d, stereo %d, sign %d, bits %d, dma %d, auto %d, fifo %d, high %d\n",
                 pThis->freq, pThis->fmt_stereo, pThis->fmt_signed, pThis->fmt_bits,
                 pThis->block_size, pThis->dma_auto, pThis->fifo, pThis->highspeed));

    if (16 == pThis->fmt_bits)
        pThis->fmt = pThis->fmt_signed ? PDMAUDIOFMT_S16 : PDMAUDIOFMT_U16;
    else
        pThis->fmt = pThis->fmt_signed ? PDMAUDIOFMT_S8 : PDMAUDIOFMT_U8;

    pThis->left_till_irq = pThis->block_size;

    pThis->bytes_per_second = (pThis->freq << pThis->fmt_stereo) << ((pThis->fmt_bits == 16) ? 1 : 0);
    pThis->highspeed = 0;
    pThis->align = (1 << (pThis->fmt_stereo + (pThis->fmt_bits == 16))) - 1;
    if (pThis->block_size & pThis->align)
    {
        LogFlowFunc(("warning: misaligned block size %d, alignment %d\n",
                     pThis->block_size, pThis->align + 1));
    }

    sb16Control(pDevIns, pThis, 1);
    sb16SpeakerControl(pThis, 1);
}

static inline void dsp_set_data(PSB16STATE pThis, uint8_t val)
{
    LogFlowFunc(("%#x\n", val));
    if ((size_t) pThis->dsp_out_data_len < sizeof (pThis->dsp_out_data))
        pThis->dsp_out_data[pThis->dsp_out_data_len++] = val;
}

static inline uint8_t dsp_get_data(PSB16STATE pThis)
{
    if (pThis->dsp_in_idx)
        return pThis->dsp_in_data[--pThis->dsp_in_idx];
    AssertMsgFailed(("DSP input buffer underflow\n"));
    return 0;
}

static void sb16HandleCommand(PPDMDEVINS pDevIns, PSB16STATE pThis, uint8_t cmd)
{
    LogFlowFunc(("command %#x\n", cmd));

    if (cmd > 0xaf && cmd < 0xd0)
    {
        if (cmd & 8) /** @todo Handle recording. */
            LogFlowFunc(("ADC not yet supported (command %#x)\n", cmd));

        switch (cmd >> 4)
        {
            case 11:
            case 12:
                break;
            default:
                LogFlowFunc(("%#x wrong bits\n", cmd));
        }

        pThis->dsp_in_needed_bytes = 3;
    }
    else
    {
        pThis->dsp_in_needed_bytes = 0;

        switch (cmd)
        {
            case 0x03:
                dsp_set_data(pThis, 0x10); /* pThis->csp_param); */
                goto warn;

            case 0x04:
                pThis->dsp_in_needed_bytes = 1;
                goto warn;

            case 0x05:
                pThis->dsp_in_needed_bytes = 2;
                goto warn;

            case 0x08:
                /* __asm__ ("int3"); */
                goto warn;

            case 0x0e:
                pThis->dsp_in_needed_bytes = 2;
                goto warn;

            case 0x09:
                dsp_set_data(pThis, 0xf8);
                goto warn;

            case 0x0f:
                pThis->dsp_in_needed_bytes = 1;
                goto warn;

            case 0x10:
                pThis->dsp_in_needed_bytes = 1;
                goto warn;

            case 0x14:
                pThis->dsp_in_needed_bytes = 2;
                pThis->block_size = 0;
                break;

            case 0x1c:              /* Auto-Initialize DMA DAC, 8-bit */
                dma_cmd8(pDevIns, pThis, DMA8_AUTO, -1);
                break;

            case 0x20:              /* Direct ADC, Juice/PL */
                dsp_set_data(pThis, 0xff);
                goto warn;

            case 0x35:
                LogFlowFunc(("0x35 - MIDI command not implemented\n"));
                break;

            case 0x40:
                pThis->freq = -1;
                pThis->time_const = -1;
                pThis->dsp_in_needed_bytes = 1;
                break;

            case 0x41:
                pThis->freq = -1;
                pThis->time_const = -1;
                pThis->dsp_in_needed_bytes = 2;
                break;

            case 0x42:
                pThis->freq = -1;
                pThis->time_const = -1;
                pThis->dsp_in_needed_bytes = 2;
                goto warn;

            case 0x45:
                dsp_set_data(pThis, 0xaa);
                goto warn;

            case 0x47:                /* Continue Auto-Initialize DMA 16bit */
                break;

            case 0x48:
                pThis->dsp_in_needed_bytes = 2;
                break;

            case 0x74:
                pThis->dsp_in_needed_bytes = 2; /* DMA DAC, 4-bit ADPCM */
                LogFlowFunc(("0x75 - DMA DAC, 4-bit ADPCM not implemented\n"));
                break;

            case 0x75:              /* DMA DAC, 4-bit ADPCM Reference */
                pThis->dsp_in_needed_bytes = 2;
                LogFlowFunc(("0x74 - DMA DAC, 4-bit ADPCM Reference not implemented\n"));
                break;

            case 0x76:              /* DMA DAC, 2.6-bit ADPCM */
                pThis->dsp_in_needed_bytes = 2;
                LogFlowFunc(("0x74 - DMA DAC, 2.6-bit ADPCM not implemented\n"));
                break;

            case 0x77:              /* DMA DAC, 2.6-bit ADPCM Reference */
                pThis->dsp_in_needed_bytes = 2;
                LogFlowFunc(("0x74 - DMA DAC, 2.6-bit ADPCM Reference not implemented\n"));
                break;

            case 0x7d:
                LogFlowFunc(("0x7d - Autio-Initialize DMA DAC, 4-bit ADPCM Reference\n"));
                LogFlowFunc(("not implemented\n"));
                break;

            case 0x7f:
                LogFlowFunc(("0x7d - Autio-Initialize DMA DAC, 2.6-bit ADPCM Reference\n"));
                LogFlowFunc(("not implemented\n"));
                break;

            case 0x80:
                pThis->dsp_in_needed_bytes = 2;
                break;

            case 0x90:
            case 0x91:
                dma_cmd8(pDevIns, pThis, (((cmd & 1) == 0) ? 1 : 0) | DMA8_HIGH, -1);
                break;

            case 0xd0:              /* halt DMA operation. 8bit */
                sb16Control(pDevIns, pThis, 0);
                break;

            case 0xd1:              /* speaker on */
                sb16SpeakerControl(pThis, 1);
                break;

            case 0xd3:              /* speaker off */
                sb16SpeakerControl(pThis, 0);
                break;

            case 0xd4:              /* continue DMA operation. 8bit */
                /* KQ6 (or maybe Sierras audblst.drv in general) resets
                   the frequency between halt/continue */
                continue_dma8(pDevIns, pThis);
                break;

            case 0xd5:              /* halt DMA operation. 16bit */
                sb16Control(pDevIns, pThis, 0);
                break;

            case 0xd6:              /* continue DMA operation. 16bit */
                sb16Control(pDevIns, pThis, 1);
                break;

            case 0xd9:              /* exit auto-init DMA after this block. 16bit */
                pThis->dma_auto = 0;
                break;

            case 0xda:              /* exit auto-init DMA after this block. 8bit */
                pThis->dma_auto = 0;
                break;

            case 0xe0:              /* DSP identification */
                pThis->dsp_in_needed_bytes = 1;
                break;

            case 0xe1:
                dsp_set_data(pThis, pThis->ver & 0xff);
                dsp_set_data(pThis, pThis->ver >> 8);
                break;

            case 0xe2:
                pThis->dsp_in_needed_bytes = 1;
                goto warn;

            case 0xe3:
            {
                for (int i = sizeof (e3) - 1; i >= 0; --i)
                    dsp_set_data(pThis, e3[i]);

                break;
            }

            case 0xe4:              /* write test reg */
                pThis->dsp_in_needed_bytes = 1;
                break;

            case 0xe7:
                LogFlowFunc(("Attempt to probe for ESS (0xe7)?\n"));
                break;

            case 0xe8:              /* read test reg */
                dsp_set_data(pThis, pThis->test_reg);
                break;

            case 0xf2:
            case 0xf3:
                dsp_set_data(pThis, 0xaa);
                pThis->mixer_regs[0x82] |= (cmd == 0xf2) ? 1 : 2;
                PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 1);
                break;

            case 0xf8:
                /* Undocumented, used by old Creative diagnostic programs. */
                dsp_set_data(pThis, 0);
                goto warn;

            case 0xf9:
                pThis->dsp_in_needed_bytes = 1;
                goto warn;

            case 0xfa:
                dsp_set_data(pThis, 0);
                goto warn;

            case 0xfc:              /* FIXME */
                dsp_set_data(pThis, 0);
                goto warn;

            default:
                LogFlowFunc(("Unrecognized command %#x\n", cmd));
                break;
        }
    }

    if (!pThis->dsp_in_needed_bytes)
        LogFlow(("\n"));

exit:

     if (!pThis->dsp_in_needed_bytes)
        pThis->cmd = -1;
     else
        pThis->cmd = cmd;

    return;

warn:
    LogFlowFunc(("warning: command %#x,%d is not truly understood yet\n", cmd, pThis->dsp_in_needed_bytes));
    goto exit;
}

DECLINLINE(uint16_t) dsp_get_lohi(PSB16STATE pThis)
{
    const uint8_t hi = dsp_get_data(pThis);
    const uint8_t lo = dsp_get_data(pThis);
    return RT_MAKE_U16(lo, hi);
}

DECLINLINE(uint16_t) dsp_get_hilo(PSB16STATE pThis)
{
    const uint8_t lo = dsp_get_data(pThis);
    const uint8_t hi = dsp_get_data(pThis);
    return RT_MAKE_U16(lo, hi);
}

static void dsp_cmd_complete(PPDMDEVINS pDevIns, PSB16STATE pThis)
{
    LogFlowFunc(("Command %#x, in_index %d, needed_bytes %d\n", pThis->cmd, pThis->dsp_in_idx, pThis->dsp_in_needed_bytes));

    int v0, v1, v2;

    PSB16STREAM pStream = &pThis->aStreams[SB16_IDX_OUT]; /** @ŧodo Improve this. */

    if (pThis->cmd > 0xaf && pThis->cmd < 0xd0)
    {
        v2 = dsp_get_data(pThis);
        v1 = dsp_get_data(pThis);
        v0 = dsp_get_data(pThis);

        if (pThis->cmd & 8)
            LogFlowFunc(("ADC params cmd = %#x d0 = %d, d1 = %d, d2 = %d\n", pThis->cmd, v0, v1, v2));
        else
        {
            LogFlowFunc(("cmd = %#x d0 = %d, d1 = %d, d2 = %d\n", pThis->cmd, v0, v1, v2));
            dma_cmd(pDevIns, pThis, pThis->cmd, v0, v1 + (v2 << 8));
        }
    }
    else
    {
        switch (pThis->cmd)
        {
            case 0x04:
            {
                pThis->csp_mode = dsp_get_data(pThis);
                pThis->csp_reg83r = 0;
                pThis->csp_reg83w = 0;
                LogFlowFunc(("CSP command 0x04: mode=%#x\n", pThis->csp_mode));
                break;
            }

            case 0x05:
            {
                pThis->csp_param = dsp_get_data(pThis);
                pThis->csp_value = dsp_get_data(pThis);
                LogFlowFunc(("CSP command 0x05: param=%#x value=%#x\n", pThis->csp_param, pThis->csp_value));
                break;
            }

            case 0x0e:
            {
                v0 = dsp_get_data(pThis);
                v1 = dsp_get_data(pThis);
                LogFlowFunc(("write CSP register %d <- %#x\n", v1, v0));
                if (v1 == 0x83)
                {
                    LogFlowFunc(("0x83[%d] <- %#x\n", pThis->csp_reg83r, v0));
                    pThis->csp_reg83[pThis->csp_reg83r % 4] = v0;
                    pThis->csp_reg83r += 1;
                }
                else
                    pThis->csp_regs[v1] = v0;
                break;
            }

            case 0x0f:
            {
                v0 = dsp_get_data(pThis);
                LogFlowFunc(("read CSP register %#x -> %#x, mode=%#x\n", v0, pThis->csp_regs[v0], pThis->csp_mode));
                if (v0 == 0x83)
                {
                    LogFlowFunc(("0x83[%d] -> %#x\n", pThis->csp_reg83w, pThis->csp_reg83[pThis->csp_reg83w % 4]));
                    dsp_set_data(pThis, pThis->csp_reg83[pThis->csp_reg83w % 4]);
                    pThis->csp_reg83w += 1;
                }
                else
                    dsp_set_data(pThis, pThis->csp_regs[v0]);
                break;
            }

            case 0x10:
                v0 = dsp_get_data(pThis);
                LogFlowFunc(("cmd 0x10 d0=%#x\n", v0));
                break;

            case 0x14:
                dma_cmd8(pDevIns, pThis, 0, dsp_get_lohi(pThis) + 1);
                break;

            case 0x22: /* Sets the master volume. */
                /** @todo Setting the master volume is not implemented yet. */
                break;

            case 0x40: /* Sets the timer constant; SB16 is able to use sample rates via 0x41 instead. */
                pThis->time_const = dsp_get_data(pThis);
                LogFlowFunc(("set time const %d\n", pThis->time_const));
                break;

            case 0x42: /* Sets the input rate (in Hz). */
#if 0
                LogFlowFunc(("cmd 0x42 might not do what it think it should\n"));
#endif
                RT_FALL_THROUGH(); /** @todo BUGBUG FT2 sets output freq with this, go figure. */

            case 0x41: /* Sets the output rate (in Hz). */
            {
                pThis->freq = dsp_get_hilo(pThis);
                LogFlowFunc(("set freq %d\n", pThis->freq));
                break;
            }

            case 0x48:
            {
                pThis->block_size = dsp_get_lohi(pThis) + 1;
                LogFlowFunc(("set dma block len %d\n", pThis->block_size));
                break;
            }

            case 0x74:
                RT_FALL_THROUGH();
            case 0x75:
                RT_FALL_THROUGH();
            case 0x76:
                RT_FALL_THROUGH();
            case 0x77:
                /* ADPCM stuff, ignore. */
                break;

            case 0x80: /* Sets the IRQ. */
            {
                sb16StreamTransferScheduleNext(pThis, pStream, dsp_get_lohi(pThis) + 1);
                break;
            }

            case 0xe0:
            {
                v0 = dsp_get_data(pThis);
                pThis->dsp_out_data_len = 0;
                LogFlowFunc(("E0=%#x\n", v0));
                dsp_set_data(pThis, ~v0);
                break;
            }

            case 0xe2:
            {
                v0 = dsp_get_data(pThis);
                LogFlowFunc(("E2=%#x\n", v0));
                break;
            }

            case 0xe4:
                pThis->test_reg = dsp_get_data(pThis);
                break;

            case 0xf9:
                v0 = dsp_get_data(pThis);
                switch (v0)
                {
                    case 0x0e:
                        dsp_set_data(pThis, 0xff);
                        break;

                    case 0x0f:
                        dsp_set_data(pThis, 0x07);
                        break;

                    case 0x37:
                        dsp_set_data(pThis, 0x38);
                        break;

                    default:
                        dsp_set_data(pThis, 0x00);
                        break;
                }
                break;

            default:
                LogRel2(("SB16: Unrecognized command %#x, skipping\n", pThis->cmd));
                return;
        }
    }

    pThis->cmd = -1;
    return;
}

static void sb16CmdResetLegacy(PSB16STATE pThis)
{
    LogFlowFuncEnter();

    pThis->freq       = 11025;
    pThis->fmt_signed = 0;
    pThis->fmt_bits   = 8;
    pThis->fmt_stereo = 0;

    /*
     * Reset all streams.
     */
    for (unsigned i = 0; i < SB16_MAX_STREAMS; i++)
    {
        sb16StreamEnable(pThis, &pThis->aStreams[i], false /* fEnable */, false /* fForce */);
        sb16StreamReset(pThis, &pThis->aStreams[i]);
    }
}

static void sb16CmdReset(PPDMDEVINS pDevIns, PSB16STATE pThis)
{
    PDMDevHlpISASetIrq(pDevIns, pThis->irq, 0);
    if (pThis->dma_auto)
    {
        PDMDevHlpISASetIrq(pDevIns, pThis->irq, 1);
        PDMDevHlpISASetIrq(pDevIns, pThis->irq, 0);
    }

    pThis->mixer_regs[0x82] = 0;
    pThis->dma_auto = 0;
    pThis->dsp_in_idx = 0;
    pThis->dsp_out_data_len = 0;
    pThis->left_till_irq = 0;
    pThis->dsp_in_needed_bytes = 0;
    pThis->block_size = -1;
    pThis->nzero = 0;
    pThis->highspeed = 0;
    pThis->v2x6 = 0;
    pThis->cmd = -1;

    dsp_set_data(pThis, 0xaa);
    sb16SpeakerControl(pThis, 0);

    sb16Control(pDevIns, pThis, 0);
    sb16CmdResetLegacy(pThis);
}

/**
 * @callback_method_impl{PFNIOMIOPORTNEWOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC) sb16IoPortDspWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    RT_NOREF(pvUser, cb);

    LogFlowFunc(("write %#x <- %#x\n", offPort, u32));
    switch (offPort)
    {
        case 0:
            switch (u32)
            {
                case 0x00:
                {
                    if (pThis->v2x6 == 1)
                    {
                        if (0 && pThis->highspeed)
                        {
                            pThis->highspeed = 0;
                            PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 0);
                            sb16Control(pDevIns, pThis, 0);
                        }
                        else
                            sb16CmdReset(pDevIns, pThis);
                    }
                    pThis->v2x6 = 0;
                    break;
                }

                case 0x01:
                case 0x03:              /* FreeBSD kludge */
                    pThis->v2x6 = 1;
                    break;

                case 0xc6:
                    pThis->v2x6 = 0;    /* Prince of Persia, csp.sys, diagnose.exe */
                    break;

                case 0xb8:              /* Panic */
                    sb16CmdReset(pDevIns, pThis);
                    break;

                case 0x39:
                    dsp_set_data(pThis, 0x38);
                    sb16CmdReset(pDevIns, pThis);
                    pThis->v2x6 = 0x39;
                    break;

                default:
                    pThis->v2x6 = u32;
                    break;
            }
            break;

        case 6:                        /* Write data or command | write status */
#if 0
            if (pThis->highspeed)
                break;
#endif
            if (0 == pThis->dsp_in_needed_bytes)
            {
                sb16HandleCommand(pDevIns, pThis, u32);
#if 0
                if (0 == pThis->needed_bytes) {
                    log_dsp (pThis);
                }
#endif
            }
            else
            {
                if (pThis->dsp_in_idx == sizeof (pThis->dsp_in_data))
                {
                    AssertMsgFailed(("DSP input data overrun\n"));
                }
                else
                {
                    pThis->dsp_in_data[pThis->dsp_in_idx++] = u32;
                    if (pThis->dsp_in_idx == pThis->dsp_in_needed_bytes)
                    {
                        pThis->dsp_in_needed_bytes = 0;
                        dsp_cmd_complete(pDevIns, pThis);
#if 0
                        log_dsp (pThis);
#endif
                    }
                }
            }
            break;

        default:
            LogFlowFunc(("offPort=%#x, u32=%#x)\n", offPort, u32));
            break;
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{PFNIOMIOPORTNEWIN}
 */
static DECLCALLBACK(VBOXSTRICTRC) sb16IoPortDspRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    uint32_t retval;
    int ack = 0;
    RT_NOREF(pvUser, cb);


    /** @todo reject non-byte access?
     *  The spec does not mention a non-byte access so we should check how real hardware behaves. */

    switch (offPort)
    {
        case 0:                     /* reset */
            retval = 0xff;
            break;

        case 4:                     /* read data */
            if (pThis->dsp_out_data_len)
            {
                retval = pThis->dsp_out_data[--pThis->dsp_out_data_len];
                pThis->last_read_byte = retval;
            }
            else
            {
                if (pThis->cmd != -1)
                    LogFlowFunc(("empty output buffer for command %#x\n", pThis->cmd));
                retval = pThis->last_read_byte;
                /* goto error; */
            }
            break;

        case 6:                     /* 0 can write */
            retval = pThis->can_write ? 0 : 0x80;
            break;

        case 7:                     /* timer interrupt clear */
            /* LogFlowFunc(("timer interrupt clear\n")); */
            retval = 0;
            break;

        case 8:                     /* data available status | irq 8 ack */
            retval = (!pThis->dsp_out_data_len || pThis->highspeed) ? 0 : 0x80;
            if (pThis->mixer_regs[0x82] & 1)
            {
                ack = 1;
                pThis->mixer_regs[0x82] &= ~1;
                PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 0);
            }
            break;

        case 9:                     /* irq 16 ack */
            retval = 0xff;
            if (pThis->mixer_regs[0x82] & 2)
            {
                ack = 1;
                pThis->mixer_regs[0x82] &= ~2;
               PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 0);
            }
            break;

        default:
            LogFlowFunc(("warning: sb16IoPortDspRead %#x error\n", offPort));
            return VERR_IOM_IOPORT_UNUSED;
    }

    if (!ack)
        LogFlowFunc(("read %#x -> %#x\n", offPort, retval));

    *pu32 = retval;
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Mixer functions                                                                                                              *
*********************************************************************************************************************************/

static uint8_t sb16MixRegToVol(PSB16STATE pThis, int reg)
{
    /* The SB16 mixer has a 0 to -62dB range in 32 levels (2dB each step).
     * We use a 0 to -96dB range in 256 levels (0.375dB each step).
     * Only the top 5 bits of a mixer register are used.
     */
    uint8_t steps = 31 - (pThis->mixer_regs[reg] >> 3);
    uint8_t vol   = 255 - steps * 16 / 3;   /* (2dB*8) / (0.375dB*8) */
    return vol;
}

/**
 * Returns the device's current master volume.
 *
 * @param   pThis               SB16 state.
 * @param   pVol                Where to store the master volume information.
 */
static void sb16GetMasterVolume(PSB16STATE pThis, PPDMAUDIOVOLUME pVol)
{
    /* There's no mute switch, only volume controls. */
    uint8_t lvol = sb16MixRegToVol(pThis, 0x30);
    uint8_t rvol = sb16MixRegToVol(pThis, 0x31);

    pVol->fMuted = false;
    pVol->uLeft  = lvol;
    pVol->uRight = rvol;
}

/**
 * Returns the device's current output stream volume.
 *
 * @param   pThis               SB16 state.
 * @param   pVol                Where to store the output stream volume information.
 */
static void sb16GetPcmOutVolume(PSB16STATE pThis, PPDMAUDIOVOLUME pVol)
{
    /* There's no mute switch, only volume controls. */
    uint8_t lvol = sb16MixRegToVol(pThis, 0x32);
    uint8_t rvol = sb16MixRegToVol(pThis, 0x33);

    pVol->fMuted = false;
    pVol->uLeft  = lvol;
    pVol->uRight = rvol;
}

static void sb16UpdateVolume(PSB16STATE pThis)
{
    PDMAUDIOVOLUME VolMaster;
    sb16GetMasterVolume(pThis, &VolMaster);

    PDMAUDIOVOLUME VolOut;
    sb16GetPcmOutVolume(pThis, &VolOut);

    /* Combine the master + output stream volume. */
    PDMAUDIOVOLUME VolCombined;
    RT_ZERO(VolCombined);

    VolCombined.fMuted = VolMaster.fMuted || VolOut.fMuted;
    if (!VolCombined.fMuted)
    {
        VolCombined.uLeft  = (   (VolOut.uLeft    ? VolOut.uLeft     : 1)
                               * (VolMaster.uLeft ? VolMaster.uLeft  : 1)) / PDMAUDIO_VOLUME_MAX;

        VolCombined.uRight = (  (VolOut.uRight    ? VolOut.uRight    : 1)
                              * (VolMaster.uRight ? VolMaster.uRight : 1)) / PDMAUDIO_VOLUME_MAX;
    }

    int rc2 = AudioMixerSinkSetVolume(pThis->pSinkOut, &VolCombined);
    AssertRC(rc2);
}

static void sb16MixerReset(PSB16STATE pThis)
{
    memset(pThis->mixer_regs, 0xff, 0x7f);
    memset(pThis->mixer_regs + 0x83, 0xff, sizeof (pThis->mixer_regs) - 0x83);

    pThis->mixer_regs[0x02] = 4;    /* master volume 3bits */
    pThis->mixer_regs[0x06] = 4;    /* MIDI volume 3bits */
    pThis->mixer_regs[0x08] = 0;    /* CD volume 3bits */
    pThis->mixer_regs[0x0a] = 0;    /* voice volume 2bits */

    /* d5=input filt, d3=lowpass filt, d1,d2=input source */
    pThis->mixer_regs[0x0c] = 0;

    /* d5=output filt, d1=stereo switch */
    pThis->mixer_regs[0x0e] = 0;

    /* voice volume L d5,d7, R d1,d3 */
    pThis->mixer_regs[0x04] = (12 << 4) | 12;
    /* master ... */
    pThis->mixer_regs[0x22] = (12 << 4) | 12;
    /* MIDI ... */
    pThis->mixer_regs[0x26] = (12 << 4) | 12;

    /* master/voice/MIDI L/R volume */
    for (int i = 0x30; i < 0x36; i++)
        pThis->mixer_regs[i] = 24 << 3; /* -14 dB */

    /* treble/bass */
    for (int i = 0x44; i < 0x48; i++)
        pThis->mixer_regs[i] = 0x80;

    /* Update the master (mixer) and PCM out volumes. */
    sb16UpdateVolume(pThis);

    /*
     * Reset mixer sinks.
     *
     * Do the reset here instead of in sb16StreamReset();
     * the mixer sink(s) might still have data to be processed when an audio stream gets reset.
     */
    if (pThis->pSinkOut)
        AudioMixerSinkReset(pThis->pSinkOut);
}

static int magic_of_irq(int irq)
{
    switch (irq)
    {
        case 5:
            return 2;
        case 7:
            return 4;
        case 9:
            return 1;
        case 10:
            return 8;
        default:
            break;
    }

    LogFlowFunc(("bad irq %d\n", irq));
    return 2;
}

static int irq_of_magic(int magic)
{
    switch (magic)
    {
        case 1:
            return 9;
        case 2:
            return 5;
        case 4:
            return 7;
        case 8:
            return 10;
        default:
            break;
    }

    LogFlowFunc(("bad irq magic %d\n", magic));
    return -1;
}

static int mixer_write_indexb(PSB16STATE pThis, uint8_t val)
{
    pThis->mixer_nreg = val;
    return VINF_SUCCESS;
}

#ifndef VBOX
static uint32_t popcount(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}
#endif

static uint32_t lsbindex(uint32_t u)
{
#ifdef VBOX
    return u ? ASMBitFirstSetU32(u) - 1 : 32;
#else
    return popcount((u & -(int32_t)u) - 1);
#endif
}

/* Convert SB16 to SB Pro mixer volume (left). */
static inline void sb16ConvVolumeL(PSB16STATE pThis, unsigned reg, uint8_t val)
{
    /* High nibble in SBP mixer. */
    pThis->mixer_regs[reg] = (pThis->mixer_regs[reg] & 0x0f) | (val & 0xf0);
}

/* Convert SB16 to SB Pro mixer volume (right). */
static inline void sb16ConvVolumeR(PSB16STATE pThis, unsigned reg, uint8_t val)
{
    /* Low nibble in SBP mixer. */
    pThis->mixer_regs[reg] = (pThis->mixer_regs[reg] & 0xf0) | (val >> 4);
}

/* Convert SB Pro to SB16 mixer volume (left + right). */
static inline void sb16ConvVolumeOldToNew(PSB16STATE pThis, unsigned reg, uint8_t val)
{
    /* Left channel. */
    pThis->mixer_regs[reg + 0] = (val & 0xf0) | RT_BIT(3);
    /* Right channel (the register immediately following). */
    pThis->mixer_regs[reg + 1] = (val << 4)   | RT_BIT(3);
}


static int mixer_write_datab(PSB16STATE pThis, uint8_t val)
{
    bool        fUpdateMaster = false;
    bool        fUpdateStream = false;

    LogFlowFunc(("sb16IoPortMixerWrite [%#x] <- %#x\n", pThis->mixer_nreg, val));

    switch (pThis->mixer_nreg)
    {
        case 0x00:
            sb16MixerReset(pThis);
            /* And update the actual volume, too. */
            fUpdateMaster = true;
            fUpdateStream = true;
            break;

        case 0x04:  /* Translate from old style voice volume (L/R). */
            sb16ConvVolumeOldToNew(pThis, 0x32, val);
            fUpdateStream = true;
            break;

        case 0x22:  /* Translate from old style master volume (L/R). */
            sb16ConvVolumeOldToNew(pThis, 0x30, val);
            fUpdateMaster = true;
            break;

        case 0x26:  /* Translate from old style MIDI volume (L/R). */
            sb16ConvVolumeOldToNew(pThis, 0x34, val);
            break;

        case 0x28:  /* Translate from old style CD volume (L/R). */
            sb16ConvVolumeOldToNew(pThis, 0x36, val);
            break;

        case 0x2E:  /* Translate from old style line volume (L/R). */
            sb16ConvVolumeOldToNew(pThis, 0x38, val);
            break;

        case 0x30:  /* Translate to old style master volume (L). */
            sb16ConvVolumeL(pThis, 0x22, val);
            fUpdateMaster = true;
            break;

        case 0x31:  /* Translate to old style master volume (R). */
            sb16ConvVolumeR(pThis, 0x22, val);
            fUpdateMaster = true;
            break;

        case 0x32:  /* Translate to old style voice volume (L). */
            sb16ConvVolumeL(pThis, 0x04, val);
            fUpdateStream = true;
            break;

        case 0x33:  /* Translate to old style voice volume (R). */
            sb16ConvVolumeR(pThis, 0x04, val);
            fUpdateStream = true;
            break;

        case 0x34:  /* Translate to old style MIDI volume (L). */
            sb16ConvVolumeL(pThis, 0x26, val);
            break;

        case 0x35:  /* Translate to old style MIDI volume (R). */
            sb16ConvVolumeR(pThis, 0x26, val);
            break;

        case 0x36:  /* Translate to old style CD volume (L). */
            sb16ConvVolumeL(pThis, 0x28, val);
            break;

        case 0x37:  /* Translate to old style CD volume (R). */
            sb16ConvVolumeR(pThis, 0x28, val);
            break;

        case 0x38:  /* Translate to old style line volume (L). */
            sb16ConvVolumeL(pThis, 0x2E, val);
            break;

        case 0x39:  /* Translate to old style line volume (R). */
            sb16ConvVolumeR(pThis, 0x2E, val);
            break;

        case 0x80:
        {
            int irq = irq_of_magic(val);
            LogFlowFunc(("setting irq to %d (val=%#x)\n", irq, val));
            if (irq > 0)
                pThis->irq = irq;
            break;
        }

        case 0x81:
        {
            int dma, hdma;

            dma = lsbindex (val & 0xf);
            hdma = lsbindex (val & 0xf0);
            if (dma != pThis->dma || hdma != pThis->hdma)
                LogFlow(("SB16: attempt to change DMA 8bit %d(%d), 16bit %d(%d) (val=%#x)\n",
                         dma, pThis->dma, hdma, pThis->hdma, val));
#if 0
            pThis->dma = dma;
            pThis->hdma = hdma;
#endif
            break;
        }

        case 0x82:
            LogFlowFunc(("attempt to write into IRQ status register (val=%#x)\n", val));
            return VINF_SUCCESS;

        default:
            if (pThis->mixer_nreg >= 0x80)
                LogFlowFunc(("attempt to write mixer[%#x] <- %#x\n", pThis->mixer_nreg, val));
            break;
    }

    pThis->mixer_regs[pThis->mixer_nreg] = val;

    /* Update the master (mixer) volume. */
    if (   fUpdateMaster
        || fUpdateStream)
    {
        sb16UpdateVolume(pThis);
    }

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{PFNIOMIOPORTNEWOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC) sb16IoPortMixerWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    RT_NOREF(pvUser);

    switch (cb)
    {
        case 1:
            switch (offPort)
            {
                case 0:
                    mixer_write_indexb(pThis, u32);
                    break;
                case 1:
                    mixer_write_datab(pThis, u32);
                    break;
                default:
                    AssertFailed();
            }
            break;
        case 2:
            mixer_write_indexb(pThis, u32 & 0xff);
            mixer_write_datab(pThis, (u32 >> 8) & 0xff);
            break;
        default:
            ASSERT_GUEST_MSG_FAILED(("offPort=%#x cb=%d u32=%#x\n", offPort, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{PFNIOMIOPORTNEWIN}
 */
static DECLCALLBACK(VBOXSTRICTRC) sb16IoPortMixerRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    RT_NOREF(pvUser, cb, offPort);

#ifndef DEBUG_SB16_MOST
    if (pThis->mixer_nreg != 0x82)
        LogFlowFunc(("sb16IoPortMixerRead[%#x] -> %#x\n", pThis->mixer_nreg, pThis->mixer_regs[pThis->mixer_nreg]));
#else
    LogFlowFunc(("sb16IoPortMixerRead[%#x] -> %#x\n", pThis->mixer_nreg, pThis->mixer_regs[pThis->mixer_nreg]));
#endif
    *pu32 = pThis->mixer_regs[pThis->mixer_nreg];
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   DMA handling                                                                                                                 *
*********************************************************************************************************************************/

/**
 * Worker for sb16DMARead.
 */

/**
 * @callback_method_impl{FNDMATRANSFERHANDLER,
 *      Worker callback for both DMA channels.}
 */
static DECLCALLBACK(uint32_t) sb16DMARead(PPDMDEVINS pDevIns, void *pvUser, unsigned uChannel, uint32_t off, uint32_t cb)

{
    PSB16STATE  pThis   = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    AssertPtr(pThis);
    PSB16STREAM pStream = (PSB16STREAM)pvUser;
    AssertPtr(pStream);

    int till, copy, free;

    if (pThis->block_size <= 0)
    {
        LogFlowFunc(("invalid block size=%d uChannel=%d off=%d cb=%d\n", pThis->block_size, uChannel, off, cb));
        return off;
    }

    if (pThis->left_till_irq < 0)
        pThis->left_till_irq = pThis->block_size;

    free = cb;

    copy = free;
    till = pThis->left_till_irq;

    Log4Func(("pos=%d %d, till=%d, len=%d\n", off, free, till, cb));

    if (copy >= till)
    {
        if (0 == pThis->dma_auto)
        {
            copy = till;
        }
        else
        {
            if (copy >= till + pThis->block_size)
                copy = till; /* Make sure we won't skip IRQs. */
        }
    }

    STAM_COUNTER_ADD(&pThis->StatBytesRead, copy);

    uint32_t written = 0; /* Shut up GCC. */
    int rc = sb16StreamDoDmaOutput(pThis, pStream, uChannel, off, cb, copy, &written);
    AssertRC(rc);

    /** @todo Convert the rest to uin32_t / size_t. */
    off = (off + (int)written) % cb;
    pThis->left_till_irq -= (int)written; /** @todo r=andy left_till_irq can be < 0. Correct? Revisit this. */

    Log3Func(("pos %d/%d, free=%d, till=%d, copy=%d, written=%RU32, block_size=%d\n",
              off, cb, free, pThis->left_till_irq, copy, copy, pThis->block_size));

    if (pThis->left_till_irq <= 0)
    {
        pThis->mixer_regs[0x82] |= (uChannel & 4) ? 2 : 1;

        PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 1);

        if (0 == pThis->dma_auto) /** @todo r=andy BUGBUG Why do we first assert the IRQ if dma_auto is 0? Revisit this. */
        {
            sb16Control(pDevIns, pThis, 0);
            sb16SpeakerControl(pThis, 0);
        }
    }

    while (pThis->left_till_irq <= 0)
        pThis->left_till_irq += pThis->block_size;

    return off;
}


/*********************************************************************************************************************************
*   Timer-related code                                                                                                           *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{PFNTMTIMERDEV}
 */
static DECLCALLBACK(void) sb16TimerIRQ(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, void *pvUser)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    RT_NOREF(pvUser, hTimer);

    LogFlowFuncEnter();

    pThis->can_write = 1;
    PDMDevHlpISASetIrq(pDevIns, pThis->irq, 1);
}

/**
 * Sets the stream's I/O timer to a new expiration time.
 *
 * @param   pDevIns             The device instance.
 * @param   pStream             SB16 stream to set timer for.
 * @param   cTicksToDeadline    The number of ticks to the new deadline.
 */
DECLINLINE(void) sb16TimerSet(PPDMDEVINS pDevIns, PSB16STREAM pStream, uint64_t cTicksToDeadline)
{
    int rc = PDMDevHlpTimerSetRelative(pDevIns, pStream->hTimerIO, cTicksToDeadline, NULL /*pu64Now*/);
    AssertRC(rc);
}

/**
 * @callback_method_impl{FNTMTIMERDEV}
 */
static DECLCALLBACK(void) sb16TimerIO(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, void *pvUser)
{
    PSB16STREAM pStream = (PSB16STREAM)pvUser;
    AssertPtrReturnVoid(pStream);

    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    STAM_PROFILE_START(&pThis->StatTimerIO, a);
    AssertReturnVoid(hTimer == pStream->hTimerIO);

    const uint64_t cTicksNow = PDMDevHlpTimerGet(pDevIns, pStream->hTimerIO);

    pStream->tsTimerIO = cTicksNow;

    PAUDMIXSINK pSink = sb16StreamIndexToSink(pThis, pStream->uIdx);
    AssertPtrReturnVoid(pSink);

    const bool fSinkActive = AudioMixerSinkIsActive(pSink);

    LogFlowFunc(("fSinkActive=%RTbool\n", fSinkActive));

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
    sb16StreamAsyncIONotify(pDevIns, pStream);
#else
    sb16StreamUpdate(pStream, pSink);
#endif

    /* Schedule the next transfer. */
    PDMDevHlpDMASchedule(pDevIns);

    if (fSinkActive)
    {
        /** @todo adjust cTicks down by now much cbOutMin represents. */
        sb16TimerSet(pDevIns, pStream, pStream->cTicksTimerIOInterval);
    }

    STAM_PROFILE_STOP(&pThis->StatTimerIO, a);
}


/*********************************************************************************************************************************
*   Driver handling                                                                                                              *
*********************************************************************************************************************************/

/**
 * Retrieves a specific driver stream of a SB16 driver.
 *
 * @returns Pointer to driver stream if found, or NULL if not found.
 * @param   pDrv                Driver to retrieve driver stream for.
 * @param   enmDir              Stream direction to retrieve.
 * @param   dstSrc              Stream destination / source to retrieve.
 */
static PSB16DRIVERSTREAM sb16GetDrvStream(PSB16DRIVER pDrv, PDMAUDIODIR enmDir, PDMAUDIODSTSRCUNION dstSrc)
{
    PSB16DRIVERSTREAM pDrvStream = NULL;

    if (enmDir == PDMAUDIODIR_IN)
    {
        return NULL; /** @todo Recording not implemented yet. */
    }
    else if (enmDir == PDMAUDIODIR_OUT)
    {
        LogFunc(("enmDst=%RU32\n", dstSrc.enmDst));

        switch (dstSrc.enmDst)
        {
            case PDMAUDIOPLAYBACKDST_FRONT:
                pDrvStream = &pDrv->Out;
                break;
            default:
                AssertFailed();
                break;
        }
    }
    else
        AssertFailed();

    return pDrvStream;
}

/**
 * Adds a driver stream to a specific mixer sink.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pMixSink    Mixer sink to add driver stream to.
 * @param   pCfg        Stream configuration to use.
 * @param   pDrv        Driver stream to add.
 */
static int sb16AddDrvStream(PPDMDEVINS pDevIns, PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg, PSB16DRIVER pDrv)
{
    AssertReturn(pCfg->enmDir == PDMAUDIODIR_OUT, VERR_NOT_IMPLEMENTED); /* We don't support recording for SB16 so far. */

    PPDMAUDIOSTREAMCFG pStreamCfg = PDMAudioStrmCfgDup(pCfg);
    if (!pStreamCfg)
        return VERR_NO_MEMORY;

    if (!RTStrPrintf(pStreamCfg->szName, sizeof(pStreamCfg->szName), "%s", pCfg->szName))
    {
        PDMAudioStrmCfgFree(pStreamCfg);
        return VERR_BUFFER_OVERFLOW;
    }

    LogFunc(("[LUN#%RU8] %s\n", pDrv->uLUN, pStreamCfg->szName));

    int rc;

    PSB16DRIVERSTREAM pDrvStream = sb16GetDrvStream(pDrv, pStreamCfg->enmDir, pStreamCfg->u);
    if (pDrvStream)
    {
        AssertMsg(pDrvStream->pMixStrm == NULL, ("[LUN#%RU8] Driver stream already present when it must not\n", pDrv->uLUN));

        PAUDMIXSTREAM pMixStrm;
        rc = AudioMixerSinkCreateStream(pMixSink, pDrv->pConnector, pStreamCfg, 0 /* fFlags */, pDevIns, &pMixStrm);
        LogFlowFunc(("LUN#%RU8: Created stream \"%s\" for sink, rc=%Rrc\n", pDrv->uLUN, pStreamCfg->szName, rc));
        if (RT_SUCCESS(rc))
        {
            rc = AudioMixerSinkAddStream(pMixSink, pMixStrm);
            LogFlowFunc(("LUN#%RU8: Added stream \"%s\" to sink, rc=%Rrc\n", pDrv->uLUN, pStreamCfg->szName, rc));

            if (RT_SUCCESS(rc))
            {
                pDrvStream->pMixStrm = pMixStrm;
            }
            else
                AudioMixerStreamDestroy(pMixStrm, pDevIns);
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    PDMAudioStrmCfgFree(pStreamCfg);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds all current driver streams to a specific mixer sink.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The SB16 state.
 * @param   pMixSink    Mixer sink to add stream to.
 * @param   pCfg        Stream configuration to use.
 */
static int sb16AddDrvStreams(PPDMDEVINS pDevIns, PSB16STATE pThis, PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixSink, VERR_INVALID_POINTER);

    if (!AudioHlpStreamCfgIsValid(pCfg))
        return VERR_INVALID_PARAMETER;

    int rc = AudioMixerSinkSetFormat(pMixSink, &pCfg->Props);
    if (RT_FAILURE(rc))
        return rc;

    PSB16DRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        int rc2 = sb16AddDrvStream(pDevIns, pMixSink, pCfg, pDrv);
        if (RT_FAILURE(rc2))
            LogFunc(("Attaching stream failed with %Rrc\n", rc2));

        /* Do not pass failure to rc here, as there might be drivers which aren't
         * configured / ready yet. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Removes a driver stream from a specific mixer sink.
 *
 * @param   pDevIns     The device instance.
 * @param   pMixSink    Mixer sink to remove audio streams from.
 * @param   enmDir      Stream direction to remove.
 * @param   dstSrc      Stream destination / source to remove.
 * @param   pDrv        Driver stream to remove.
 */
static void sb16RemoveDrvStream(PPDMDEVINS pDevIns, PAUDMIXSINK pMixSink, PDMAUDIODIR enmDir,
                                PDMAUDIODSTSRCUNION dstSrc, PSB16DRIVER pDrv)
{
    PSB16DRIVERSTREAM pDrvStream = sb16GetDrvStream(pDrv, enmDir, dstSrc);
    if (pDrvStream)
    {
        if (pDrvStream->pMixStrm)
        {
            LogFlowFunc(("[LUN#%RU8]\n", pDrv->uLUN));

            AudioMixerSinkRemoveStream(pMixSink, pDrvStream->pMixStrm);

            AudioMixerStreamDestroy(pDrvStream->pMixStrm, pDevIns);
            pDrvStream->pMixStrm = NULL;
        }
    }
}

/**
 * Removes all driver streams from a specific mixer sink.
 *
 * @param   pDevIns     The device instance.
 * @param   pThisCC     The SB16 state.
 * @param   pMixSink    Mixer sink to remove audio streams from.
 * @param   enmDir      Stream direction to remove.
 * @param   dstSrc      Stream destination / source to remove.
 */
static void sb16RemoveDrvStreams(PPDMDEVINS pDevIns, PSB16STATE pThis, PAUDMIXSINK pMixSink,
                                 PDMAUDIODIR enmDir, PDMAUDIODSTSRCUNION dstSrc)
{
    AssertPtrReturnVoid(pMixSink);

    PSB16DRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
        sb16RemoveDrvStream(pDevIns, pMixSink, enmDir, dstSrc, pDrv);
}

/**
 * Adds a specific SB16 driver to the driver chain.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The SB16 device state.
 * @param   pDrv        The SB16 driver to add.
 */
static int sb16AddDrv(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16DRIVER pDrv)
{
    int rc = VINF_SUCCESS;

    for (unsigned i = 0; i < SB16_MAX_STREAMS; i++)
    {
        if (AudioHlpStreamCfgIsValid(&pThis->aStreams[i].Cfg))
        {
            int rc2 = sb16AddDrvStream(pDevIns,
                                       sb16StreamIndexToSink(pThis, pThis->aStreams[i].uIdx), &pThis->aStreams[i].Cfg, pDrv);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}

/**
 * Removes a specific SB16 driver from the driver chain and destroys its
 * associated streams.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The SB16 device state.
 * @param   pDrv        SB16 driver to remove.
 */
static void sb16RemoveDrv(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16DRIVER pDrv)
{
    RT_NOREF(pDevIns);

    /** @todo We only implement one single output (playback) stream at the moment. */

    if (pDrv->Out.pMixStrm)
    {
        AudioMixerSinkRemoveStream(pThis->pSinkOut, pDrv->Out.pMixStrm);
        AudioMixerStreamDestroy(pDrv->Out.pMixStrm, pDevIns);
        pDrv->Out.pMixStrm = NULL;
    }

    RTListNodeRemove(&pDrv->Node);
}


/*********************************************************************************************************************************
*   Stream handling                                                                                                              *
*********************************************************************************************************************************/

static int sb16StreamDoDmaOutput(PSB16STATE pThis, PSB16STREAM pStream,
                                 int uDmaChan, uint32_t offDma, uint32_t cbDma, uint32_t cbToRead, uint32_t *pcbRead)
{
    uint32_t cbFree      = (uint32_t)RTCircBufFree(pStream->State.pCircBuf);
    uint32_t cbReadTotal = 0;

    //Assert(cbToRead <= cbFree); /** @todo Add STAM value for overflows. */

    cbToRead = RT_MIN(cbToRead, cbFree);

    int rc = VINF_SUCCESS;

    void  *pv;
    size_t cb;

    while (cbToRead)
    {
        uint32_t cbChunk = RT_MIN(cbDma - offDma, cbToRead);

        RTCircBufAcquireWriteBlock(pStream->State.pCircBuf, cbChunk, &pv, &cb);

        uint32_t cbRead;
        rc = PDMDevHlpDMAReadMemory(pThis->pDevInsR3, uDmaChan, pv, offDma, cb, &cbRead);
        AssertMsgRCReturn(rc, ("Reading from DMA failed, rc=%Rrc\n", rc), rc);
        Assert(cbRead == cb);

        if (RT_LIKELY(!pStream->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
            AudioHlpFileWrite(pStream->Dbg.Runtime.pFileDMA, pv, cbRead, 0 /* fFlags */);

        RTCircBufReleaseWriteBlock(pStream->State.pCircBuf, cbRead);

        Assert(cbToRead >= cbRead);
        cbToRead    -= cbRead;
        offDma       = (offDma + cbRead) % cbDma;
        cbReadTotal += cbRead;
    }

    if (pcbRead)
        *pcbRead = cbReadTotal;

    return rc;
}

/**
 * Enables or disables a SB16 audio stream.
 *
 * @returns VBox status code.
 * @param   pThis       The SB16 state.
 * @param   pStream     The SB16 stream to enable or disable.
 * @param   fEnable     Whether to enable or disable the stream.
 * @param   fForce      Whether to force re-opening the stream or not.
 *                      Otherwise re-opening only will happen if the PCM properties have changed.
 */
static int sb16StreamEnable(PSB16STATE pThis, PSB16STREAM pStream, bool fEnable, bool fForce)
{
    if (   !fForce
        && fEnable == pStream->State.fEnabled)
        return VINF_SUCCESS;

    LogFlowFunc(("fEnable=%RTbool, fForce=%RTbool, fStreamEnabled=%RTbool\n", fEnable, fForce, pStream->State.fEnabled));

    /* First, enable or disable the stream and the stream's sink. */
    int rc = AudioMixerSinkCtl(sb16StreamIndexToSink(pThis, pStream->uIdx),
                               fEnable ? AUDMIXSINKCMD_ENABLE : AUDMIXSINKCMD_DISABLE);
    AssertRCReturn(rc, rc);

    pStream->State.fEnabled = fEnable;

    return rc;
}

/**
 * Retrieves the audio mixer sink of a corresponding SB16 stream.
 *
 * @returns Pointer to audio mixer sink if found, or NULL if not found / invalid.
 * @param   pThis               The SB16 state.
 * @param   uIdx                Stream index to get audio mixer sink for.
 */
DECLINLINE(PAUDMIXSINK) sb16StreamIndexToSink(PSB16STATE pThis, uint8_t uIdx)
{
    AssertReturn(uIdx <= SB16_MAX_STREAMS, NULL);

    /* Dead simple for now; make this more sophisticated if we have more stuff to cover. */
    if (uIdx == SB16_IDX_OUT)
        return pThis->pSinkOut; /* Can be NULL if not configured / set up yet. */

    AssertMsgFailed(("No sink attached (yet) for index %RU8\n", uIdx));
    return NULL;
}

/**
 * Returns the audio direction of a specified stream descriptor.
 *
 * @returns Audio direction.
 * @param   uIdx                Stream index to get audio direction for.
 */
DECLINLINE(PDMAUDIODIR) sb16GetDirFromIndex(uint8_t uIdx)
{
    AssertReturn(uIdx <= SB16_MAX_STREAMS, PDMAUDIODIR_INVALID);

    /* Dead simple for now; make this more sophisticated if we have more stuff to cover. */
    if (uIdx == SB16_IDX_OUT)
        return PDMAUDIODIR_OUT;

    return PDMAUDIODIR_INVALID;
}

/**
 * Creates a SB16 audio stream.
 *
 * @returns VBox status code.
 * @param   pThisCC             The SB16 state.
 * @param   pStream             The SB16 stream to create.
 * @param   uIdx                Stream index to assign.
 */
static int sb16StreamCreate(PSB16STATE pThis, PSB16STREAM pStream, uint8_t uIdx)
{
    LogFlowFuncEnter();

    pStream->Dbg.Runtime.fEnabled = pThis->Dbg.fEnabled;

    int rc2;

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
    rc2 = sb16StreamAsyncIOCreate(pThis->pDevInsR3, pThis, pStream);
    AssertRCReturn(rc2, rc2);
#endif

    if (RT_LIKELY(!pStream->Dbg.Runtime.fEnabled))
    { /* likely */ }
    else
    {
        char szFile[64];

        if (sb16GetDirFromIndex(pStream->uIdx) == PDMAUDIODIR_IN)
            RTStrPrintf(szFile, sizeof(szFile), "sb16StreamWriteSD%RU8", pStream->uIdx);
        else
            RTStrPrintf(szFile, sizeof(szFile), "sb16StreamReadSD%RU8", pStream->uIdx);

        char szPath[RTPATH_MAX];
        rc2 = AudioHlpFileNameGet(szPath, sizeof(szPath), pThis->Dbg.pszOutPath, szFile,
                                  0 /* uInst */, AUDIOHLPFILETYPE_WAV, AUDIOHLPFILENAME_FLAGS_NONE);
        AssertRC(rc2);
        rc2 = AudioHlpFileCreate(AUDIOHLPFILETYPE_WAV, szPath, AUDIOHLPFILE_FLAGS_NONE, &pStream->Dbg.Runtime.pFileDMA);
        AssertRC(rc2);

        /* Delete stale debugging files from a former run. */
        AudioHlpFileDelete(pStream->Dbg.Runtime.pFileDMA);
    }

    pStream->uIdx = uIdx;

    return VINF_SUCCESS;
}

/**
 * Destroys a SB16 audio stream.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The SB16 state.
 * @param   pStream             The SB16 stream to destroy.
 */
static int sb16StreamDestroy(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream)
{
    LogFlowFuncEnter();

    sb16StreamClose(pDevIns, pThis, pStream);

#ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
    int rc = sb16StreamAsyncIODestroy(pDevIns, pStream);
    AssertRCReturn(rc, rc);
#else
    RT_NOREF(pDevIns);
#endif

    if (pStream->State.pCircBuf)
    {
        RTCircBufDestroy(pStream->State.pCircBuf);
        pStream->State.pCircBuf = NULL;
    }

    if (RT_LIKELY(!pStream->Dbg.Runtime.fEnabled))
    { /* likely */ }
    else
    {
        AudioHlpFileDestroy(pStream->Dbg.Runtime.pFileDMA);
        pStream->Dbg.Runtime.pFileDMA = NULL;
    }

    pStream->uIdx = UINT8_MAX;

    return VINF_SUCCESS;
}

/**
 * Resets a SB16 stream.
 *
 * @param   pThis               The SB16 state.
 * @param   pStream             The SB16 stream to reset.
 */
static void sb16StreamReset(PSB16STATE pThis, PSB16STREAM pStream)
{
    LogFlowFuncEnter();

    switch (pStream->uIdx)
    {
        case SB16_IDX_OUT:
        {
            pStream->Cfg.enmDir    = PDMAUDIODIR_OUT;
            pStream->Cfg.u.enmDst  = PDMAUDIOPLAYBACKDST_FRONT;
            pStream->Cfg.enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

            PDMAudioPropsInit(&pStream->Cfg.Props, 1 /* 8-bit */, false /* fSigned */, 1 /* Mono */, pThis->freq);
            RTStrCopy(pStream->Cfg.szName, sizeof(pStream->Cfg.szName), "Output");

            break;
        }

        default:
            AssertFailed();
            break;
    }

    /** @todo Also reset corresponding DSP values here? */
}

/**
 * Opens a SB16 stream with its current mixer settings.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The SB16 device state.
 * @param   pStream     The SB16 stream to open.
 *
 * @note    This currently only supports the one and only output stream.
 */
static int sb16StreamOpen(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream)
{
    LogFlowFuncEnter();

    PDMAUDIOSTREAMCFG Cfg;
    RT_ZERO(Cfg);

    /** @todo Implement mixer sink selection (and it's in/out + destination mapping) here once we add more streams. */
    PDMAudioPropsInit(&Cfg.Props, pThis->fmt_bits / 8, pThis->fmt_signed != 0, 1 << pThis->fmt_stereo, pThis->freq);

    /* Bail out early if the PCM properties did not change. */
    if (PDMAudioPropsAreEqual(&Cfg.Props, &pStream->Cfg.Props))
        return VINF_SUCCESS;

    PDMAUDIODSTSRCUNION dstSrc;
    PDMAUDIODIR         enmDir;

    switch (pStream->uIdx)
    {
        case SB16_IDX_OUT:
        {
            Cfg.enmDir      = PDMAUDIODIR_OUT;
            Cfg.u.enmDst    = PDMAUDIOPLAYBACKDST_FRONT;
            Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

            RTStrCopy(Cfg.szName, sizeof(Cfg.szName), "Output");

            dstSrc.enmDst = PDMAUDIOPLAYBACKDST_FRONT;
            enmDir        = PDMAUDIODIR_OUT;
            break;
        }

        default:
            AssertFailed();
            break;
    }

    LogRel2(("SB16: (Re-)Opening stream '%s' (%RU32Hz, %RU8 channels, %s%RU8)\n", Cfg.szName, Cfg.Props.uHz,
             PDMAudioPropsChannels(&Cfg.Props), Cfg.Props.fSigned ? "S" : "U",  PDMAudioPropsSampleBits(&Cfg.Props)));

    int rc = PDMAudioStrmCfgCopy(&pStream->Cfg, &Cfg);
    if (RT_FAILURE(rc))
        return rc;

    /* (Re-)create the stream's internal ring buffer. */
    if (pStream->State.pCircBuf)
    {
        RTCircBufDestroy(pStream->State.pCircBuf);
        pStream->State.pCircBuf = NULL;
    }

    const uint32_t cbCircBuf = PDMAudioPropsMilliToBytes(&pStream->Cfg.Props, 2 * 10 /* ms */);

    rc = RTCircBufCreate(&pStream->State.pCircBuf, cbCircBuf);
    AssertRCReturn(rc, rc);

    /* Set scheduling hint (if available). */
    if (pStream->cTicksTimerIOInterval)
        pStream->Cfg.Device.cMsSchedulingHint = 1000 /* ms */
                                              / (  PDMDevHlpTimerGetFreq(pDevIns, pStream->hTimerIO)
                                              / RT_MIN(pStream->cTicksTimerIOInterval, 1));

    PAUDMIXSINK pMixerSink = sb16StreamIndexToSink(pThis, pStream->uIdx);
    AssertPtrReturn(pMixerSink, VERR_INVALID_POINTER);

    sb16RemoveDrvStreams(pDevIns, pThis,
                         sb16StreamIndexToSink(pThis, pStream->uIdx), pStream->Cfg.enmDir, pStream->Cfg.u);

    rc = sb16AddDrvStreams(pDevIns, pThis, pMixerSink, &pStream->Cfg);

    if (RT_SUCCESS(rc))
    {
        if (RT_LIKELY(!pStream->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
        {
            /* Make sure to close + delete a former debug file, as the PCM format has changed (e.g. U8 -> S16). */
            if (AudioHlpFileIsOpen(pStream->Dbg.Runtime.pFileDMA))
            {
                AudioHlpFileClose(pStream->Dbg.Runtime.pFileDMA);
                AudioHlpFileDelete(pStream->Dbg.Runtime.pFileDMA);
            }

            int rc2 = AudioHlpFileOpen(pStream->Dbg.Runtime.pFileDMA, AUDIOHLPFILE_DEFAULT_OPEN_FLAGS,
                                       &pStream->Cfg.Props);
            AssertRC(rc2);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Closes a SB16 stream.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           SB16 state.
 * @param   pStream         The SB16 stream to close.
 */
static void sb16StreamClose(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16STREAM pStream)
{
    RT_NOREF(pDevIns, pThis, pStream);

    LogFlowFuncEnter();

    /* Nothing to do in here right now. */
}

static void sb16StreamTransferScheduleNext(PSB16STATE pThis, PSB16STREAM pStream, uint32_t cbBytes)
{
    RT_NOREF(pStream);

#if 0
    uint32_t const freq     = pThis->freq > 0 ? pThis->freq : 11025;
    uint32_t const bytes    = cSamples << pThis->fmt_stereo << (pThis->fmt_bits == 16 ? 1 : 0);
    uint64_t const uTimerHz = PDMDevHlpTimerGetFreq(pThis->pDevInsR3, pThis->hTimerIRQ);
    uint64_t const cTicks   = (bytes * uTimerHz) / freq;
#endif

    uint64_t const uTimerHz = PDMDevHlpTimerGetFreq(pThis->pDevInsR3, pThis->hTimerIRQ);

    const uint64_t usBytes        = PDMAudioPropsBytesToMicro(&pStream->Cfg.Props, cbBytes);
    const uint64_t cTransferTicks = PDMDevHlpTimerFromMicro(pThis->pDevInsR3, pThis->hTimerIRQ, usBytes);

    LogFlowFunc(("%RU32 bytes -> %RU64 ticks\n", cbBytes, cTransferTicks));

    if (cTransferTicks < uTimerHz / 1024) /** @todo Explain this. */
    {
        LogFlowFunc(("IRQ\n"));
        PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 1);
    }
    else
    {
        LogFlowFunc(("Scheduled\n"));
        PDMDevHlpTimerSetRelative(pThis->pDevInsR3, pThis->hTimerIRQ, cTransferTicks, NULL);
    }
}

/**
 * Main function for off-DMA data processing by the audio backend(s).
 *
 * Might be called by a timer callback or by an async I/O worker thread, depending
 * on the configuration.
 *
 * @param   pStream     The SB16 stream to open.
 * @param   pSink       Mixer sink to use for updating.
 */
static void sb16StreamUpdate(PSB16STREAM pStream, PAUDMIXSINK pSink)
{
    if (sb16GetDirFromIndex(pStream->uIdx) == PDMAUDIODIR_OUT)
    {
        uint32_t const cbSinkWritable     = AudioMixerSinkGetWritable(pSink);
        uint32_t const cbStreamReadable   = (uint32_t)RTCircBufUsed(pStream->State.pCircBuf);

        uint32_t       cbToReadFromStream = RT_MIN(cbStreamReadable, cbSinkWritable);
        /* Make sure that we always align the number of bytes when reading to the stream's PCM properties. */
        cbToReadFromStream = PDMAudioPropsFloorBytesToFrame(&pStream->Cfg.Props, cbToReadFromStream);

        Log3Func(("[SD%RU8] cbSinkWritable=%RU32, cbStreamReadable=%RU32\n", pStream->uIdx, cbSinkWritable, cbStreamReadable));

        void /*const*/ *pvSrcBuf;
        size_t          cbSrcBuf;

        while (cbToReadFromStream > 0)
        {
            uint32_t cbWritten = 0;

            RTCircBufAcquireReadBlock(pStream->State.pCircBuf, cbToReadFromStream, &pvSrcBuf, &cbSrcBuf);

            int rc2 = AudioMixerSinkWrite(pSink, AUDMIXOP_COPY, pvSrcBuf, cbSrcBuf, &cbWritten);
            AssertRC(rc2);
            Assert(cbWritten <= cbSrcBuf);

            RTCircBufReleaseReadBlock(pStream->State.pCircBuf, cbWritten);

            Assert(cbToReadFromStream >= cbWritten);
            cbToReadFromStream -= cbWritten;
        }
    }
    else
        AssertFailed(); /** @todo Recording not implemented yet. */

    int rc2 = AudioMixerSinkUpdate(pSink);
    AssertRC(rc2);
}


/*********************************************************************************************************************************
*   Saved state handling                                                                                                         *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) sb16LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PSB16STATE    pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;
    RT_NOREF(uPass);

    pHlp->pfnSSMPutS32(pSSM, pThis->irqCfg);
    pHlp->pfnSSMPutS32(pSSM, pThis->dmaCfg);
    pHlp->pfnSSMPutS32(pSSM, pThis->hdmaCfg);
    pHlp->pfnSSMPutS32(pSSM, pThis->portCfg);
    pHlp->pfnSSMPutS32(pSSM, pThis->verCfg);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Worker for sb16SaveExec.
 */
static int sb16Save(PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM, PSB16STATE pThis)
{
    pHlp->pfnSSMPutS32(pSSM, pThis->irq);
    pHlp->pfnSSMPutS32(pSSM, pThis->dma);
    pHlp->pfnSSMPutS32(pSSM, pThis->hdma);
    pHlp->pfnSSMPutS32(pSSM, pThis->port);
    pHlp->pfnSSMPutS32(pSSM, pThis->ver);
    pHlp->pfnSSMPutS32(pSSM, pThis->dsp_in_idx);
    pHlp->pfnSSMPutS32(pSSM, pThis->dsp_out_data_len);
    pHlp->pfnSSMPutS32(pSSM, pThis->fmt_stereo);
    pHlp->pfnSSMPutS32(pSSM, pThis->fmt_signed);
    pHlp->pfnSSMPutS32(pSSM, pThis->fmt_bits);

    pHlp->pfnSSMPutU32(pSSM, pThis->fmt);

    pHlp->pfnSSMPutS32(pSSM, pThis->dma_auto);
    pHlp->pfnSSMPutS32(pSSM, pThis->block_size);
    pHlp->pfnSSMPutS32(pSSM, pThis->fifo);
    pHlp->pfnSSMPutS32(pSSM, pThis->freq);
    pHlp->pfnSSMPutS32(pSSM, pThis->time_const);
    pHlp->pfnSSMPutS32(pSSM, pThis->speaker);
    pHlp->pfnSSMPutS32(pSSM, pThis->dsp_in_needed_bytes);
    pHlp->pfnSSMPutS32(pSSM, pThis->cmd);
    pHlp->pfnSSMPutS32(pSSM, pThis->use_hdma);
    pHlp->pfnSSMPutS32(pSSM, pThis->highspeed);
    pHlp->pfnSSMPutS32(pSSM, pThis->can_write);
    pHlp->pfnSSMPutS32(pSSM, pThis->v2x6);

    pHlp->pfnSSMPutU8 (pSSM, pThis->csp_param);
    pHlp->pfnSSMPutU8 (pSSM, pThis->csp_value);
    pHlp->pfnSSMPutU8 (pSSM, pThis->csp_mode);
    pHlp->pfnSSMPutU8 (pSSM, pThis->csp_param); /* Bug compatible! */
    pHlp->pfnSSMPutMem(pSSM, pThis->csp_regs, 256);
    pHlp->pfnSSMPutU8 (pSSM, pThis->csp_index);
    pHlp->pfnSSMPutMem(pSSM, pThis->csp_reg83, 4);
    pHlp->pfnSSMPutS32(pSSM, pThis->csp_reg83r);
    pHlp->pfnSSMPutS32(pSSM, pThis->csp_reg83w);

    pHlp->pfnSSMPutMem(pSSM, pThis->dsp_in_data, sizeof(pThis->dsp_in_data));
    pHlp->pfnSSMPutMem(pSSM, pThis->dsp_out_data, sizeof(pThis->dsp_out_data));
    pHlp->pfnSSMPutU8 (pSSM, pThis->test_reg);
    pHlp->pfnSSMPutU8 (pSSM, pThis->last_read_byte);

    pHlp->pfnSSMPutS32(pSSM, pThis->nzero);
    pHlp->pfnSSMPutS32(pSSM, pThis->left_till_irq);
    pHlp->pfnSSMPutS32(pSSM, pThis->dma_running);
    pHlp->pfnSSMPutS32(pSSM, pThis->bytes_per_second);
    pHlp->pfnSSMPutS32(pSSM, pThis->align);

    pHlp->pfnSSMPutS32(pSSM, pThis->mixer_nreg);
    return pHlp->pfnSSMPutMem(pSSM, pThis->mixer_regs, 256);
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) sb16SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    sb16LiveExec(pDevIns, pSSM, 0);
    return sb16Save(pHlp, pSSM, pThis);
}

/**
 * Worker for sb16LoadExec.
 */
static int sb16Load(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, PSB16STATE pThis)
{
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMGetS32(pSSM, &pThis->irq);
    pHlp->pfnSSMGetS32(pSSM, &pThis->dma);
    pHlp->pfnSSMGetS32(pSSM, &pThis->hdma);
    pHlp->pfnSSMGetS32(pSSM, &pThis->port);
    pHlp->pfnSSMGetS32(pSSM, &pThis->ver);
    pHlp->pfnSSMGetS32(pSSM, &pThis->dsp_in_idx);
    pHlp->pfnSSMGetS32(pSSM, &pThis->dsp_out_data_len);
    pHlp->pfnSSMGetS32(pSSM, &pThis->fmt_stereo);
    pHlp->pfnSSMGetS32(pSSM, &pThis->fmt_signed);
    pHlp->pfnSSMGetS32(pSSM, &pThis->fmt_bits);

    PDMDEVHLP_SSM_GET_ENUM32_RET(pHlp, pSSM, pThis->fmt, PDMAUDIOFMT);

    pHlp->pfnSSMGetS32(pSSM, &pThis->dma_auto);
    pHlp->pfnSSMGetS32(pSSM, &pThis->block_size);
    pHlp->pfnSSMGetS32(pSSM, &pThis->fifo);
    pHlp->pfnSSMGetS32(pSSM, &pThis->freq);
    pHlp->pfnSSMGetS32(pSSM, &pThis->time_const);
    pHlp->pfnSSMGetS32(pSSM, &pThis->speaker);
    pHlp->pfnSSMGetS32(pSSM, &pThis->dsp_in_needed_bytes);
    pHlp->pfnSSMGetS32(pSSM, &pThis->cmd);
    pHlp->pfnSSMGetS32(pSSM, &pThis->use_hdma);
    pHlp->pfnSSMGetS32(pSSM, &pThis->highspeed);
    pHlp->pfnSSMGetS32(pSSM, &pThis->can_write);
    pHlp->pfnSSMGetS32(pSSM, &pThis->v2x6);

    pHlp->pfnSSMGetU8 (pSSM, &pThis->csp_param);
    pHlp->pfnSSMGetU8 (pSSM, &pThis->csp_value);
    pHlp->pfnSSMGetU8 (pSSM, &pThis->csp_mode);
    pHlp->pfnSSMGetU8 (pSSM, &pThis->csp_param);   /* Bug compatible! */
    pHlp->pfnSSMGetMem(pSSM, pThis->csp_regs, 256);
    pHlp->pfnSSMGetU8 (pSSM, &pThis->csp_index);
    pHlp->pfnSSMGetMem(pSSM, pThis->csp_reg83, 4);
    pHlp->pfnSSMGetS32(pSSM, &pThis->csp_reg83r);
    pHlp->pfnSSMGetS32(pSSM, &pThis->csp_reg83w);

    pHlp->pfnSSMGetMem(pSSM, pThis->dsp_in_data, sizeof(pThis->dsp_in_data));
    pHlp->pfnSSMGetMem(pSSM, pThis->dsp_out_data, sizeof(pThis->dsp_out_data));
    pHlp->pfnSSMGetU8 (pSSM, &pThis->test_reg);
    pHlp->pfnSSMGetU8 (pSSM, &pThis->last_read_byte);

    pHlp->pfnSSMGetS32(pSSM, &pThis->nzero);
    pHlp->pfnSSMGetS32(pSSM, &pThis->left_till_irq);
    pHlp->pfnSSMGetS32(pSSM, &pThis->dma_running);
    pHlp->pfnSSMGetS32(pSSM, &pThis->bytes_per_second);
    pHlp->pfnSSMGetS32(pSSM, &pThis->align);

    int32_t mixer_nreg = 0;
    int rc = pHlp->pfnSSMGetS32(pSSM, &mixer_nreg);
    AssertRCReturn(rc, rc);
    pThis->mixer_nreg = (uint8_t)mixer_nreg;
    rc = pHlp->pfnSSMGetMem(pSSM, pThis->mixer_regs, 256);
    AssertRCReturn(rc, rc);

    if (pThis->dma_running)
    {
        sb16Control(pDevIns, pThis, 1);
        sb16SpeakerControl(pThis, pThis->speaker);
    }

    /* Update the master (mixer) and PCM out volumes. */
    sb16UpdateVolume(pThis);

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) sb16LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PSB16STATE    pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;

    AssertMsgReturn(    uVersion == SB16_SAVE_STATE_VERSION
                    ||  uVersion == SB16_SAVE_STATE_VERSION_VBOX_30,
                    ("%u\n", uVersion),
                    VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    if (uVersion > SB16_SAVE_STATE_VERSION_VBOX_30)
    {
        int32_t irq;
        pHlp->pfnSSMGetS32(pSSM, &irq);
        int32_t dma;
        pHlp->pfnSSMGetS32(pSSM, &dma);
        int32_t hdma;
        pHlp->pfnSSMGetS32(pSSM, &hdma);
        int32_t port;
        pHlp->pfnSSMGetS32(pSSM, &port);
        int32_t ver;
        int rc = pHlp->pfnSSMGetS32(pSSM, &ver);
        AssertRCReturn (rc, rc);

        if (   irq  != pThis->irqCfg
            || dma  != pThis->dmaCfg
            || hdma != pThis->hdmaCfg
            || port != pThis->portCfg
            || ver  != pThis->verCfg)
        {
            return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS,
                                           N_("config changed: irq=%x/%x dma=%x/%x hdma=%x/%x port=%x/%x ver=%x/%x (saved/config)"),
                                           irq,  pThis->irqCfg,
                                           dma,  pThis->dmaCfg,
                                           hdma, pThis->hdmaCfg,
                                           port, pThis->portCfg,
                                           ver,  pThis->verCfg);
        }
    }

    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    return sb16Load(pDevIns, pSSM, pThis);
}


/*********************************************************************************************************************************
*   IBase implementation                                                                                                         *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) sb16QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PSB16STATE pThis = RT_FROM_MEMBER(pInterface, SB16STATE, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}


/*********************************************************************************************************************************
*   Device (PDM) handling                                                                                                        *
*********************************************************************************************************************************/

/**
 * Attach command, internal version.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor has to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pThis       SB16 state.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 * @param   ppDrv       Attached driver instance on success. Optional.
 */
static int sb16AttachInternal(PSB16STATE pThis, unsigned uLUN, uint32_t fFlags, PSB16DRIVER *ppDrv)
{
    RT_NOREF(fFlags);

    /*
     * Attach driver.
     */
    PSB16DRIVER pDrv = (PSB16DRIVER)RTMemAllocZ(sizeof(SB16DRIVER));
    AssertReturn(pDrv, VERR_NO_MEMORY);
    RTStrPrintf(pDrv->szDesc, sizeof(pDrv->szDesc), "Audio driver port (SB16) for LUN #%u", uLUN);

    PPDMIBASE pDrvBase;
    int rc = PDMDevHlpDriverAttach(pThis->pDevInsR3, uLUN, &pThis->IBase, &pDrvBase, pDrv->szDesc);
    if (RT_SUCCESS(rc))
    {
        pDrv->pDrvBase   = pDrvBase;
        pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pDrvBase, PDMIAUDIOCONNECTOR);
        AssertMsg(pDrv->pConnector != NULL, ("Configuration error: LUN #%u has no host audio interface, rc=%Rrc\n", uLUN, rc));
        pDrv->pSB16State = pThis;
        pDrv->uLUN       = uLUN;

        /*
         * For now we always set the driver at LUN 0 as our primary
         * host backend. This might change in the future.
         */
        if (pDrv->uLUN == 0)
            pDrv->fFlags |= PDMAUDIODRVFLAGS_PRIMARY;

        LogFunc(("LUN#%u: pCon=%p, drvFlags=0x%x\n", uLUN, pDrv->pConnector, pDrv->fFlags));

        /* Attach to driver list if not attached yet. */
        if (!pDrv->fAttached)
        {
            RTListAppend(&pThis->lstDrv, &pDrv->Node);
            pDrv->fAttached = true;
        }

        if (ppDrv)
            *ppDrv = pDrv;
    }
    else
    {
        if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            LogFunc(("No attached driver for LUN #%u\n", uLUN));
        RTMemFree(pDrv);
    }

    LogFunc(("iLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}

/**
 * Detach command, internal version.
 *
 * This is called to let the device detach from a driver for a specified LUN
 * during runtime.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The SB16 device state.
 * @param   pDrv        Driver to detach from device.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static int sb16DetachInternal(PPDMDEVINS pDevIns, PSB16STATE pThis, PSB16DRIVER pDrv, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    /** @todo r=andy Any locking required here? */

    sb16RemoveDrv(pDevIns, pThis, pDrv);

    LogFunc(("uLUN=%u, fFlags=0x%x\n", pDrv->uLUN, fFlags));
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int) sb16Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);

    LogFunc(("iLUN=%u, fFlags=0x%x\n", iLUN, fFlags));

    /** @todo r=andy Any locking required here? */

    PSB16DRIVER pDrv;
    int rc2 = sb16AttachInternal(pThis, iLUN, fFlags, &pDrv);
    if (RT_SUCCESS(rc2))
        rc2 = sb16AddDrv(pDevIns, pThis, pDrv);

    if (RT_FAILURE(rc2))
        LogFunc(("Failed with %Rrc\n", rc2));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) sb16Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    RT_NOREF(fFlags);

    LogFunc(("iLUN=%u, fFlags=0x%x\n", iLUN, fFlags));

    PSB16DRIVER pDrv, pDrvNext;
    RTListForEachSafe(&pThis->lstDrv, pDrv, pDrvNext, SB16DRIVER, Node)
    {
        if (pDrv->uLUN == iLUN)
        {
            int rc2 = sb16DetachInternal(pDevIns, pThis, pDrv, fFlags);
            if (RT_SUCCESS(rc2))
            {
                RTMemFree(pDrv);
                pDrv = NULL;
            }
            break;
        }
    }
}


#ifdef VBOX_WITH_AUDIO_SB16_ONETIME_INIT
/**
 * Replaces a driver with a the NullAudio drivers.
 *
 * @returns VBox status code.
 * @param   pThis       Device instance.
 * @param   iLun        The logical unit which is being replaced.
 */
static int sb16ReconfigLunWithNullAudio(PSB16STATE pThis, unsigned iLun)
{
    int rc = PDMDevHlpDriverReconfigure2(pThis->pDevInsR3, iLun, "AUDIO", "NullAudio");
    if (RT_SUCCESS(rc))
        rc = sb16AttachInternal(pThis, iLun, 0 /* fFlags */, NULL /* ppDrv */);
    LogFunc(("pThis=%p, iLun=%u, rc=%Rrc\n", pThis, iLun, rc));
    return rc;
}
#endif


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) sb16DevReset(PPDMDEVINS pDevIns)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);

    LogRel2(("SB16: Reset\n"));

    /* Bring back the device to initial state, and especially make
     * sure there's no interrupt or DMA activity.
     */
    PDMDevHlpISASetIrq(pThis->pDevInsR3, pThis->irq, 0);

    pThis->mixer_regs[0x82] = 0;
    pThis->csp_regs[5]      = 1;
    pThis->csp_regs[9]      = 0xf8;

    pThis->dma_auto = 0;
    pThis->dsp_in_idx = 0;
    pThis->dsp_out_data_len = 0;
    pThis->left_till_irq = 0;
    pThis->dsp_in_needed_bytes = 0;
    pThis->block_size = -1;
    pThis->nzero = 0;
    pThis->highspeed = 0;
    pThis->v2x6 = 0;
    pThis->cmd = -1;

    sb16MixerReset(pThis);
    sb16SpeakerControl(pThis, 0);
    sb16Control(pDevIns, pThis, 0);
    sb16CmdResetLegacy(pThis);
}

/**
 * Powers off the device.
 *
 * @param   pDevIns             Device instance to power off.
 */
static DECLCALLBACK(void) sb16PowerOff(PPDMDEVINS pDevIns)
{
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);

    LogRel2(("SB16: Powering off ...\n"));

    /*
     * Destroy all streams.
     */
    for (unsigned i = 0; i < SB16_MAX_STREAMS; i++)
        sb16StreamDestroy(pDevIns, pThis, &pThis->aStreams[i]);

    /*
     * Destroy all sinks.
     */
    if (pThis->pSinkOut)
    {
        AudioMixerSinkDestroy(pThis->pSinkOut, pDevIns);
        pThis->pSinkOut = NULL;
    }
    /** @todo Ditto for sinks. */

    /*
     * Note: Destroy the mixer while powering off and *not* in sb16Destruct,
     *       giving the mixer the chance to release any references held to
     *       PDM audio streams it maintains.
     */
    if (pThis->pMixer)
    {
        AudioMixerDestroy(pThis->pMixer, pDevIns);
        pThis->pMixer = NULL;
    }
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) sb16Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns); /* this shall come first */
    PSB16STATE pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);

    LogFlowFuncEnter();

    PSB16DRIVER pDrv;
    while (!RTListIsEmpty(&pThis->lstDrv))
    {
        pDrv = RTListGetFirst(&pThis->lstDrv, SB16DRIVER, Node);

        RTListNodeRemove(&pDrv->Node);
        RTMemFree(pDrv);
    }

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnConstruct}
 */
static DECLCALLBACK(int) sb16Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns); /* this shall come first */
    PSB16STATE      pThis = PDMDEVINS_2_DATA(pDevIns, PSB16STATE);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    RT_NOREF(iInstance);

    Assert(iInstance == 0);

    /*
     * Initialize the data so sb16Destruct runs without a hitch if we return early.
     */
    pThis->pDevInsR3               = pDevIns;
    pThis->IBase.pfnQueryInterface = sb16QueryInterface;
    pThis->cmd                     = -1;

    pThis->csp_regs[5]             = 1;
    pThis->csp_regs[9]             = 0xf8;

    RTListInit(&pThis->lstDrv);

    /*
     * Validate and read config data.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "IRQ|DMA|DMA16|Port|Version|TimerHz|DebugEnabled|DebugPathOut", "");
    int rc = pHlp->pfnCFGMQuerySIntDef(pCfg, "IRQ", &pThis->irq, 5);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Failed to get the \"IRQ\" value"));
    pThis->irqCfg  = pThis->irq;

    rc = pHlp->pfnCFGMQuerySIntDef(pCfg, "DMA", &pThis->dma, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Failed to get the \"DMA\" value"));
    pThis->dmaCfg  = pThis->dma;

    rc = pHlp->pfnCFGMQuerySIntDef(pCfg, "DMA16", &pThis->hdma, 5);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Failed to get the \"DMA16\" value"));
    pThis->hdmaCfg = pThis->hdma;

    RTIOPORT Port;
    rc = pHlp->pfnCFGMQueryPortDef(pCfg, "Port", &Port, 0x220);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Failed to get the \"Port\" value"));
    pThis->port    = Port;
    pThis->portCfg = Port;

    uint16_t u16Version;
    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "Version", &u16Version, 0x0405);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Failed to get the \"Version\" value"));
    pThis->ver     = u16Version;
    pThis->verCfg  = u16Version;

    uint16_t uTimerHz;
    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "TimerHz", &uTimerHz, 100 /* Hz */);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: failed to read Hertz (Hz) rate as unsigned integer"));
    if (uTimerHz == 0)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: TimerHz is zero"));
    if (uTimerHz > 2048)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("SB16 configuration error: Max TimerHz value is 2048."));

    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "DebugEnabled", &pThis->Dbg.fEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("SB16 configuration error: failed to read debugging enabled flag as boolean"));

    rc = pHlp->pfnCFGMQueryStringAllocDef(pCfg, "DebugPathOut", &pThis->Dbg.pszOutPath, NULL);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("SB16 configuration error: failed to read debugging output path flag as string"));

    if (pThis->Dbg.fEnabled)
        LogRel2(("SB16: Debug output will be saved to '%s'\n", pThis->Dbg.pszOutPath));

    /*
     * Create internal software mixer.
     * Must come before we do the device's mixer reset.
     */
    rc = AudioMixerCreate("SB16 Mixer", 0 /* uFlags */, &pThis->pMixer);
    AssertRCReturn(rc, rc);

    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThis->pMixer, "PCM Output",
                              AUDMIXSINKDIR_OUTPUT, pDevIns, &pThis->pSinkOut);
    AssertRCReturn(rc, rc);

    /*
     * Create all hardware streams.
     * For now we have one stream only, namely the output (playback) stream.
     */
    AssertCompile(RT_ELEMENTS(pThis->aStreams) == SB16_MAX_STREAMS);
    for (unsigned i = 0; i < SB16_MAX_STREAMS; i++)
    {
        rc = sb16StreamCreate(pThis, &pThis->aStreams[i], i /* uIdx */);
        AssertRCReturn(rc, rc);
    }

    /*
     * Setup the mixer now that we've got the irq and dma channel numbers.
     */
    pThis->mixer_regs[0x80] = magic_of_irq(pThis->irq);
    pThis->mixer_regs[0x81] = (1 << pThis->dma) | (1 << pThis->hdma);
    pThis->mixer_regs[0x82] = 2 << 5;

    sb16MixerReset(pThis);

    /*
     * Create timers.
     */
    rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL, sb16TimerIRQ, pThis,
                              TMTIMER_FLAGS_DEFAULT_CRIT_SECT | TMTIMER_FLAGS_NO_RING0, "SB16 IRQ", &pThis->hTimerIRQ);
    AssertRCReturn(rc, rc);

    static const char * const s_apszNames[] = { "SB16 OUT" };
    AssertCompile(RT_ELEMENTS(s_apszNames) == SB16_MAX_STREAMS);
    for (unsigned i = 0; i < SB16_MAX_STREAMS; i++)
    {
        rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL, sb16TimerIO, &pThis->aStreams[i],
                                  TMTIMER_FLAGS_DEFAULT_CRIT_SECT | TMTIMER_FLAGS_NO_RING0, s_apszNames[i], &pThis->aStreams[i].hTimerIO);
        AssertRCReturn(rc, rc);

        pThis->aStreams[i].cTicksTimerIOInterval = PDMDevHlpTimerGetFreq(pDevIns, pThis->aStreams[i].hTimerIO) / uTimerHz;
        pThis->aStreams[i].tsTimerIO             = PDMDevHlpTimerGet(pDevIns, pThis->aStreams[i].hTimerIO);
    }

    /*
     * Register I/O and DMA.
     */
    static const IOMIOPORTDESC s_aAllDescs[] =
    {
        { "FM Music Status Port",           "FM Music Register Address Port",           NULL, NULL },   // 00h
        { NULL,                             "FM Music Data Port",                       NULL, NULL },   // 01h
        { "Advanced FM Music Status Port",  "Advanced FM Music Register Address Port",  NULL, NULL },   // 02h
        { NULL,                             "Advanced FM Music Data Port",              NULL, NULL },   // 03h
        { NULL,                             "Mixer chip Register Address Port",         NULL, NULL },   // 04h
        { "Mixer chip Data Port",           NULL,                                       NULL, NULL },   // 05h
        { NULL,                             "DSP Reset",                                NULL, NULL },   // 06h
        { "Unused7",                        "Unused7",                                  NULL, NULL },   // 07h
        { "FM Music Status Port",           "FM Music Register Port",                   NULL, NULL },   // 08h
        { NULL,                             "FM Music Data Port",                       NULL, NULL },   // 09h
        { "DSP Read Data Port",             NULL,                                       NULL, NULL },   // 0Ah
        { "UnusedB",                        "UnusedB",                                  NULL, NULL },   // 0Bh
        { "DSP Write-Buffer Status",        "DSP Write Command/Data",                   NULL, NULL },   // 0Ch
        { "UnusedD",                        "UnusedD",                                  NULL, NULL },   // 0Dh
        { "DSP Read-Buffer Status",         NULL,                                       NULL, NULL },   // 0Eh
        { "IRQ16ACK",                       NULL,                                       NULL, NULL },   // 0Fh
        { "CD-ROM Data Register",           "CD-ROM Command Register",                  NULL, NULL },   // 10h
        { "CD-ROM Status Register",         NULL,                                       NULL, NULL },   // 11h
        { NULL,                             "CD-ROM Reset Register",                    NULL, NULL },   // 12h
        { NULL,                             "CD-ROM Enable Register",                   NULL, NULL },   // 13h
        { NULL,                             NULL,                                       NULL, NULL },
    };

    rc = PDMDevHlpIoPortCreateAndMap(pDevIns, pThis->port + 0x04 /*uPort*/, 2 /*cPorts*/,
                                     sb16IoPortMixerWrite, sb16IoPortMixerRead,
                                     "SB16 - Mixer", &s_aAllDescs[4], &pThis->hIoPortsMixer);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIoPortCreateAndMap(pDevIns, pThis->port + 0x06 /*uPort*/, 10 /*cPorts*/,
                                     sb16IoPortDspWrite, sb16IoPortDspRead,
                                     "SB16 - DSP", &s_aAllDescs[6], &pThis->hIoPortsDsp);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpDMARegister(pDevIns, pThis->hdma, sb16DMARead, &pThis->aStreams[SB16_IDX_OUT] /* pvUser */);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpDMARegister(pDevIns, pThis->dma,  sb16DMARead, &pThis->aStreams[SB16_IDX_OUT] /* pvUser */);
    AssertRCReturn(rc, rc);

    pThis->can_write = 1;

    /*
     * Register Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, SB16_SAVE_STATE_VERSION, sizeof(SB16STATE), sb16LiveExec, sb16SaveExec, sb16LoadExec);
    AssertRCReturn(rc, rc);

# ifdef VBOX_WITH_AUDIO_SB16_ASYNC_IO
    LogRel(("SB16: Asynchronous I/O enabled\n"));
# endif

    /*
     * Attach drivers.  We ASSUME they are configured consecutively without any
     * gaps, so we stop when we hit the first LUN w/o a driver configured.
     */
    for (unsigned iLun = 0; ; iLun++)
    {
        AssertBreak(iLun < UINT8_MAX);
        LogFunc(("Trying to attach driver for LUN#%u ...\n", iLun));
        rc = sb16AttachInternal(pThis, iLun, 0 /* fFlags */, NULL /* ppDrv */);
        if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            LogFunc(("cLUNs=%u\n", iLun));
            break;
        }
        AssertLogRelMsgReturn(RT_SUCCESS(rc),  ("LUN#%u: rc=%Rrc\n", iLun, rc), rc);
    }

    sb16CmdResetLegacy(pThis);

#ifdef VBOX_WITH_AUDIO_SB16_ONETIME_INIT
    PSB16DRIVER pDrv, pNext;
    RTListForEachSafe(&pThis->lstDrv, pDrv, pNext, SB16DRIVER, Node)
    {
        /*
         * Only primary drivers are critical for the VM to run. Everything else
         * might not worth showing an own error message box in the GUI.
         */
        if (!(pDrv->fFlags & PDMAUDIODRVFLAGS_PRIMARY))
            continue;

        PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
        AssertPtr(pCon);

        /** @todo No input streams available for SB16 yet. */
        if (!AudioMixerStreamIsValid(pDrv->Out.pMixStrm))
        {
            LogRel(("SB16: Falling back to NULL backend (no sound audible)\n"));

            sb16CmdResetLegacy(pThis);
            sb16ReconfigLunWithNullAudio(pThis, pDrv->uLUN);
            pDrv = NULL; /* no longer valid */

            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                       N_("No audio devices could be opened. "
                                          "Selecting the NULL audio backend with the consequence that no sound is audible"));
        }
    }
#endif /* VBOX_WITH_AUDIO_SB16_ONETIME_INIT */

    /*
     * Register statistics.
     */
# ifdef VBOX_WITH_STATISTICS
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTimerIO,   STAMTYPE_PROFILE, "Timer",     STAMUNIT_TICKS_PER_CALL, "Profiling sb16TimerIO.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead, STAMTYPE_COUNTER, "BytesRead", STAMUNIT_BYTES,          "Bytes read from SB16 emulation.");
# endif

    return VINF_SUCCESS;
}

const PDMDEVREG g_DeviceSB16 =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "sb16",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_AUDIO,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(SB16STATE),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Sound Blaster 16 Controller",
#if defined(IN_RING3)
    /* .pszRCMod = */               "",
    /* .pszR0Mod = */               "",
    /* .pfnConstruct = */           sb16Construct,
    /* .pfnDestruct = */            sb16Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               sb16DevReset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              sb16Attach,
    /* .pfnDetach = */              sb16Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            sb16PowerOff,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           NULL,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

