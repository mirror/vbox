/* $Id$ */
/** @file
 * Video recording audio backend for Main.
 *
 * This driver is part of Main and is responsible for providing audio
 * data to Main's video capturing feature.
 *
 * The driver itself implements a PDM host audio backend, which in turn
 * provides the driver with the required audio data and audio events.
 *
 * For now there is support for the following destinations (called "sinks"):
 *
 * - Direct writing of .webm files to the host.
 * - Communicating with Main via the Console object to send the encoded audio data to.
 *   The Console object in turn then will route the data to the Display / video capturing interface then.
 */

/*
 * Copyright (C) 2016-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* This code makes use of the Vorbis (libvorbis) and Opus codec (libopus):
 *
 * Copyright 2001-2011 Xiph.Org, Skype Limited, Octasic,
 *                     Jean-Marc Valin, Timothy B. Terriberry,
 *                     CSIRO, Gregory Maxwell, Mark Borgerding,
 *                     Erik de Castro Lopo
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of Internet Society, IETF or IETF Trust, nor the
 * names of specific contributors, may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Opus is subject to the royalty-free patent licenses which are
 * specified at:
 *
 * Xiph.Org Foundation:
 * https://datatracker.ietf.org/ipr/1524/
 *
 * Microsoft Corporation:
 * https://datatracker.ietf.org/ipr/1914/
 *
 * Broadcom Corporation:
 * https://datatracker.ietf.org/ipr/1526/
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_RECORDING
#include "LoggingNew.h"

#include "DrvAudioRec.h"
#include "ConsoleImpl.h"

#include "WebMWriter.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>

#include "VBox/com/VirtualBox.h"
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/vmmr3vtable.h>
#include <VBox/err.h>
#include "VBox/settings.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Enumeration for specifying the recording container type.
 */
typedef enum AVRECCONTAINERTYPE
{
    /** Unknown / invalid container type. */
    AVRECCONTAINERTYPE_UNKNOWN      = 0,
    /** Recorded data goes to Main / Console. */
    AVRECCONTAINERTYPE_MAIN_CONSOLE = 1,
    /** Recorded data will be written to a .webm file. */
    AVRECCONTAINERTYPE_WEBM         = 2
} AVRECCONTAINERTYPE;

/**
 * Structure for keeping generic container parameters.
 */
typedef struct AVRECCONTAINERPARMS
{
    /** Stream index (hint). */
    uint32_t                idxStream;
    /** The container's type. */
    AVRECCONTAINERTYPE      enmType;
    union
    {
        /** WebM file specifics. */
        struct
        {
            /** Allocated file name to write .webm file to. Must be free'd. */
            char *pszFile;
        } WebM;
    };

} AVRECCONTAINERPARMS, *PAVRECCONTAINERPARMS;

/**
 * Structure for keeping container-specific data.
 */
typedef struct AVRECCONTAINER
{
    /** Generic container parameters. */
    AVRECCONTAINERPARMS     Parms;

    union
    {
        struct
        {
            /** Pointer to Console. */
            Console        *pConsole;
        } Main;

        struct
        {
            /** Pointer to WebM container to write recorded audio data to.
             *  See the AVRECMODE enumeration for more information. */
            WebMWriter     *pWebM;
            /** Assigned track number from WebM container. */
            uint8_t         uTrack;
        } WebM;
    };
} AVRECCONTAINER, *PAVRECCONTAINER;

/**
 * Audio video recording sink.
 */
typedef struct AVRECSINK
{
    /** Pointer to audio codec to use. */
    PRECORDINGCODEC      pCodec;
    /** Container data to use for data processing. */
    AVRECCONTAINER       Con;
    /** Timestamp (in ms) of when the sink was created. */
    uint64_t             tsStartMs;
} AVRECSINK, *PAVRECSINK;

/**
 * Audio video recording (output) stream.
 */
typedef struct AVRECSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** (Audio) frame buffer. */
    PRTCIRCBUF              pCircBuf;
    /** Pointer to sink to use for writing. */
    PAVRECSINK              pSink;
    /** Last encoded PTS (in ms). */
    uint64_t                uLastPTSMs;
    /** Temporary buffer for the input (source) data to encode. */
    void                   *pvSrcBuf;
    /** Size (in bytes) of the temporary buffer holding the input (source) data to encode. */
    size_t                  cbSrcBuf;
    /** Temporary buffer for the encoded output (destination) data. */
    void                   *pvDstBuf;
    /** Size (in bytes) of the temporary buffer holding the encoded output (destination) data. */
    size_t                  cbDstBuf;
} AVRECSTREAM, *PAVRECSTREAM;

/**
 * Video recording audio driver instance data.
 */
typedef struct DRVAUDIORECORDING
{
    /** Pointer to audio video recording object. */
    AudioVideoRec       *pAudioVideoRec;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS           pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO        IHostAudio;
    /** Pointer to the console object. */
    ComPtr<Console>      pConsole;
    /** Pointer to the DrvAudio port interface that is above us. */
    AVRECCONTAINERPARMS  ContainerParms;
    /** Weak pointer to recording context to use. */
    RecordingContext    *pRecCtx;
    /** The driver's sink for writing output to. */
    AVRECSINK            Sink;
} DRVAUDIORECORDING, *PDRVAUDIORECORDING;


AudioVideoRec::AudioVideoRec(Console *pConsole)
    : AudioDriver(pConsole)
    , mpDrv(NULL)
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


/**
 * Applies recording settings to this driver instance.
 *
 * @returns VBox status code.
 * @param   Settings        Recording settings to apply.
 */
int AudioVideoRec::applyConfiguration(const settings::RecordingSettings &Settings)
{
    /** @todo Do some validation here. */
    mSettings = Settings; /* Note: Does have an own copy operator. */
    return VINF_SUCCESS;
}


int AudioVideoRec::configureDriver(PCFGMNODE pLunCfg, PCVMMR3VTABLE pVMM)
{
    /** @todo For now we're using the configuration of the first screen (screen 0) here audio-wise. */
    unsigned const idxScreen = 0;

    AssertReturn(mSettings.mapScreens.size() >= 1, VERR_INVALID_PARAMETER);
    const settings::RecordingScreenSettings &screenSettings = mSettings.mapScreens[idxScreen];

    int vrc = pVMM->pfnCFGMR3InsertInteger(pLunCfg, "ContainerType", (uint64_t)screenSettings.enmDest);
    AssertRCReturn(vrc, vrc);
    if (screenSettings.enmDest == RecordingDestination_File)
    {
        vrc = pVMM->pfnCFGMR3InsertString(pLunCfg, "ContainerFileName", Utf8Str(screenSettings.File.strName).c_str());
        AssertRCReturn(vrc, vrc);
    }

    vrc = pVMM->pfnCFGMR3InsertInteger(pLunCfg, "StreamIndex", (uint32_t)idxScreen);
    AssertRCReturn(vrc, vrc);

    return AudioDriver::configureDriver(pLunCfg, pVMM);
}


/*********************************************************************************************************************************
*   PDMIHOSTAUDIO                                                                                                                *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "VideoRec");
    pBackendCfg->cbStream       = sizeof(AVRECSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsIn  = 0;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioVideoRecHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(pInterface, enmDir);
    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * Creates an audio output stream and associates it with the specified recording sink.
 *
 * @returns VBox status code.
 * @param   pThis               Driver instance.
 * @param   pStreamAV           Audio output stream to create.
 * @param   pSink               Recording sink to associate audio output stream to.
 * @param   pCodec              The audio codec, for stream parameters.
 * @param   pCfgReq             Requested configuration by the audio backend.
 * @param   pCfgAcq             Acquired configuration by the audio output stream.
 */
static int avRecCreateStreamOut(PDRVAUDIORECORDING pThis, PAVRECSTREAM pStreamAV,
                                PAVRECSINK pSink, PRECORDINGCODEC pCodec, PCPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pStreamAV, VERR_INVALID_POINTER);
    AssertPtrReturn(pSink,     VERR_INVALID_POINTER);
    AssertPtrReturn(pCodec,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,   VERR_INVALID_POINTER);

    if (pCfgReq->enmPath != PDMAUDIOPATH_OUT_FRONT)
    {
        LogRel(("Recording: Support for surround audio not implemented yet\n"));
        AssertFailed();
        return VERR_NOT_SUPPORTED;
    }

    /* Stuff which has to be set by now. */
    Assert(pCodec->Parms.cbFrame);
    Assert(pCodec->Parms.msFrame);

    int vrc = RTCircBufCreate(&pStreamAV->pCircBuf, pCodec->Parms.cbFrame * 2 /* Use "double buffering" */);
    if (RT_SUCCESS(vrc))
    {
        size_t cbScratchBuf = pCodec->Parms.cbFrame;
        pStreamAV->pvSrcBuf = RTMemAlloc(cbScratchBuf);
        if (pStreamAV->pvSrcBuf)
        {
            pStreamAV->cbSrcBuf = cbScratchBuf;
            pStreamAV->pvDstBuf = RTMemAlloc(cbScratchBuf);
            if (pStreamAV->pvDstBuf)
            {
                pStreamAV->cbDstBuf = cbScratchBuf;

                pStreamAV->pSink      = pSink; /* Assign sink to stream. */
                pStreamAV->uLastPTSMs = 0;

                /* Make sure to let the driver backend know that we need the audio data in
                 * a specific sampling rate the codec is optimized for. */
/** @todo r=bird: pCfgAcq->Props isn't initialized at all, except for uHz... */
                pCfgAcq->Props.uHz         = pCodec->Parms.Audio.PCMProps.uHz;
//                pCfgAcq->Props.cShift      = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pCfgAcq->Props.cbSample, pCfgAcq->Props.cChannels);

                /* Every Opus frame marks a period for now. Optimize this later. */
                pCfgAcq->Backend.cFramesPeriod       = PDMAudioPropsMilliToFrames(&pCfgAcq->Props, pCodec->Parms.msFrame);
                pCfgAcq->Backend.cFramesBufferSize   = PDMAudioPropsMilliToFrames(&pCfgAcq->Props, 100 /*ms*/); /** @todo Make this configurable. */
                pCfgAcq->Backend.cFramesPreBuffering = pCfgAcq->Backend.cFramesPeriod * 2;
            }
            else
                vrc = VERR_NO_MEMORY;
        }
        else
            vrc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PCPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVAUDIORECORDING pThis     = RT_FROM_CPP_MEMBER(pInterface, DRVAUDIORECORDING, IHostAudio);
    PAVRECSTREAM       pStreamAV = (PAVRECSTREAM)pStream;
    AssertPtrReturn(pStreamAV, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        return VERR_NOT_SUPPORTED;

    /* For now we only have one sink, namely the driver's one.
     * Later each stream could have its own one, to e.g. router different stream to different sinks .*/
    PAVRECSINK pSink = &pThis->Sink;

    int vrc = avRecCreateStreamOut(pThis, pStreamAV, pSink, pSink->pCodec, pCfgReq, pCfgAcq);
    PDMAudioStrmCfgCopy(&pStreamAV->Cfg, pCfgAcq);

    return vrc;
}


/**
 * Destroys (closes) an audio output stream.
 *
 * @returns VBox status code.
 * @param   pThis               Driver instance.
 * @param   pStreamAV           Audio output stream to destroy.
 */
static int avRecDestroyStreamOut(PDRVAUDIORECORDING pThis, PAVRECSTREAM pStreamAV)
{
    RT_NOREF(pThis);

    if (pStreamAV->pCircBuf)
    {
        RTCircBufDestroy(pStreamAV->pCircBuf);
        pStreamAV->pCircBuf = NULL;
    }

    if (pStreamAV->pvSrcBuf)
    {
        Assert(pStreamAV->cbSrcBuf);
        RTMemFree(pStreamAV->pvSrcBuf);
        pStreamAV->pvSrcBuf = NULL;
        pStreamAV->cbSrcBuf = 0;
    }

    if (pStreamAV->pvDstBuf)
    {
        Assert(pStreamAV->cbDstBuf);
        RTMemFree(pStreamAV->pvDstBuf);
        pStreamAV->pvDstBuf = NULL;
        pStreamAV->cbDstBuf = 0;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          bool fImmediate)
{
    PDRVAUDIORECORDING pThis     = RT_FROM_CPP_MEMBER(pInterface, DRVAUDIORECORDING, IHostAudio);
    PAVRECSTREAM       pStreamAV = (PAVRECSTREAM)pStream;
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    RT_NOREF(fImmediate);

    int vrc = VINF_SUCCESS;
    if (pStreamAV->Cfg.enmDir == PDMAUDIODIR_OUT)
        vrc = avRecDestroyStreamOut(pThis, pStreamAV);

    return vrc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDrain}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvAudioVideoRecHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                               PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMHOSTAUDIOSTREAMSTATE_INVALID);
    return PDMHOSTAUDIOSTREAMSTATE_OKAY;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvAudioVideoRecHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pInterface);
    PAVRECSTREAM pStreamAV = (PAVRECSTREAM)pStream;
    AssertPtrReturn(pStreamAV, VERR_INVALID_POINTER);
    if (cbBuf)
        AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(pcbWritten, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;

    PAVRECSINK pSink       = pStreamAV->pSink;
    AssertPtr(pSink);
    PRECORDINGCODEC pCodec = pSink->pCodec;
    AssertPtr(pCodec);
    PRTCIRCBUF pCircBuf    = pStreamAV->pCircBuf;
    AssertPtr(pCircBuf);

    uint32_t cbToWrite = cbBuf;

    /*
     * Write as much as we can into our internal ring buffer.
     */
    while (   cbToWrite > 0
           && RTCircBufFree(pCircBuf))
    {
        void  *pvCircBuf = NULL;
        size_t cbCircBuf = 0;
        RTCircBufAcquireWriteBlock(pCircBuf, cbToWrite, &pvCircBuf, &cbCircBuf);

        if (cbCircBuf)
        {
            memcpy(pvCircBuf, (uint8_t *)pvBuf + cbWrittenTotal, cbCircBuf),
            cbWrittenTotal += (uint32_t)cbCircBuf;
            Assert(cbToWrite >= cbCircBuf);
            cbToWrite      -= (uint32_t)cbCircBuf;
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbCircBuf);
        AssertBreak(cbCircBuf);
    }

    /*
     * Process our internal ring buffer and encode the data.
     */

    /* Only encode data if we have data for at least one full frame. */
    while (RTCircBufUsed(pCircBuf) >= pCodec->Parms.cbFrame)
    {
        LogFunc(("cbAvail=%zu, csFrame=%RU32, cbFrame=%RU32\n",
                 RTCircBufUsed(pCircBuf), pCodec->Parms.csFrame, pCodec->Parms.cbFrame));

        /** @todo Can we encode more than a frame at a time? Optimize this! */
        uint32_t const cbFramesToEncode = pCodec->Parms.cbFrame; /* 1 frame. */

        uint32_t cbSrc = 0;
        while (cbSrc < cbFramesToEncode)
        {
            void  *pvCircBuf = NULL;
            size_t cbCircBuf = 0;
            RTCircBufAcquireReadBlock(pCircBuf, cbFramesToEncode - cbSrc, &pvCircBuf, &cbCircBuf);

            if (cbCircBuf)
            {
                memcpy((uint8_t *)pStreamAV->pvSrcBuf + cbSrc, pvCircBuf, cbCircBuf);

                cbSrc += (uint32_t)cbCircBuf;
                Assert(cbSrc <= pStreamAV->cbSrcBuf);
            }

            RTCircBufReleaseReadBlock(pCircBuf, cbCircBuf);
            AssertBreak(cbCircBuf);
        }

        Assert(cbSrc == cbFramesToEncode);

        RECORDINGFRAME Frame;
        Frame.Audio.cbBuf = cbFramesToEncode;
        Frame.Audio.pvBuf = (uint8_t *)pStreamAV->pvSrcBuf;

        size_t cEncoded /* Blocks encoded */, cbEncoded /* Bytes encoded */;
        vrc = recordingCodecEncode(pSink->pCodec,
                                   /* Source */
                                   &Frame,
                                   /* Dest */
                                   pStreamAV->pvDstBuf, pStreamAV->cbDstBuf, &cEncoded, &cbEncoded);
        if (   RT_SUCCESS(vrc)
            && cEncoded)
        {
            Assert(cbEncoded);

            if (pStreamAV->uLastPTSMs == 0)
                pStreamAV->uLastPTSMs = RTTimeProgramMilliTS(); /* We want the absolute time (in ms) since program start. */

            const uint64_t uDurationMs = pSink->pCodec->Parms.msFrame * cEncoded;
            const uint64_t uPTSMs      = pStreamAV->uLastPTSMs;

            pStreamAV->uLastPTSMs += uDurationMs;

            switch (pSink->Con.Parms.enmType)
            {
                case AVRECCONTAINERTYPE_MAIN_CONSOLE:
                {
                    HRESULT hrc = pSink->Con.Main.pConsole->i_recordingSendAudio(pStreamAV->pvDstBuf, cbEncoded, uPTSMs);
                    Assert(hrc == S_OK);
                    RT_NOREF(hrc);
                    break;
                }

                case AVRECCONTAINERTYPE_WEBM:
                {
                    WebMWriter::BlockData_Audio blockData = { pStreamAV->pvDstBuf, cbEncoded, uPTSMs };
                    vrc = pSink->Con.WebM.pWebM->WriteBlock(pSink->Con.WebM.uTrack, &blockData, sizeof(blockData));
                    AssertRC(vrc);
                    break;
                }

                default:
                    AssertFailedStmt(vrc = VERR_NOT_IMPLEMENTED);
                    break;
            }
        }
        else if (RT_FAILURE(vrc)) /* Something went wrong -- report all bytes as being processed, to not hold up others. */
            cbWrittenTotal = cbBuf;

        if (RT_FAILURE(vrc))
            break;

    } /* while */

    *pcbWritten = cbWrittenTotal;

    LogFlowFunc(("csReadTotal=%RU32, vrc=%Rrc\n", cbWrittenTotal, vrc));
    return VINF_SUCCESS; /* Don't propagate encoding errors to the caller. */
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvAudioVideoRecHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return 0; /* Video capturing does not provide any input. */
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioVideoRecHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface, pStream, pvBuf, cbBuf);
    *pcbRead = 0;
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   PDMIBASE                                                                                                                     *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVideoRecQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIORECORDING pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIORECORDING);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/*********************************************************************************************************************************
*   PDMDRVREG                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Shuts down (closes) a recording sink,
 *
 * @returns VBox status code.
 * @param   pSink               Recording sink to shut down.
 */
static void avRecSinkShutdown(PAVRECSINK pSink)
{
    AssertPtrReturnVoid(pSink);

    recordingCodecFinalize(pSink->pCodec);
    recordingCodecDestroy(pSink->pCodec);
    pSink->pCodec = NULL;

    switch (pSink->Con.Parms.enmType)
    {
        case AVRECCONTAINERTYPE_WEBM:
        {
            if (pSink->Con.WebM.pWebM)
            {
                LogRel2(("Recording: Finished recording audio to file '%s' (%zu bytes)\n",
                         pSink->Con.WebM.pWebM->GetFileName().c_str(), pSink->Con.WebM.pWebM->GetFileSize()));

                int vrc2 = pSink->Con.WebM.pWebM->Close();
                AssertRC(vrc2);

                delete pSink->Con.WebM.pWebM;
                pSink->Con.WebM.pWebM = NULL;
            }
            break;
        }

        case AVRECCONTAINERTYPE_MAIN_CONSOLE:
            RT_FALL_THROUGH();
        default:
            break;
    }
}


/**
 * @interface_method_impl{PDMDRVREG,pfnPowerOff}
 */
/*static*/ DECLCALLBACK(void) AudioVideoRec::drvPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVAUDIORECORDING pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIORECORDING);
    LogFlowFuncEnter();
    avRecSinkShutdown(&pThis->Sink);
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
/*static*/ DECLCALLBACK(void) AudioVideoRec::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIORECORDING pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIORECORDING);

    LogFlowFuncEnter();

    switch (pThis->ContainerParms.enmType)
    {
        case AVRECCONTAINERTYPE_WEBM:
        {
            avRecSinkShutdown(&pThis->Sink);
            RTStrFree(pThis->ContainerParms.WebM.pszFile);
            break;
        }

        default:
            break;
    }

    /*
     * If the AudioVideoRec object is still alive, we must clear it's reference to
     * us since we'll be invalid when we return from this method.
     */
    if (pThis->pAudioVideoRec)
    {
        pThis->pAudioVideoRec->mpDrv = NULL;
        pThis->pAudioVideoRec = NULL;
    }

    LogFlowFuncLeave();
}


/**
 * Initializes a recording sink.
 *
 * @returns VBox status code.
 * @param   pThis               Driver instance.
 * @param   pSink               Sink to initialize.
 * @param   pConParms           Container parameters to set.
 * @param   pCodec              Codec to use.
 */
static int avRecSinkInit(PDRVAUDIORECORDING pThis, PAVRECSINK pSink, PAVRECCONTAINERPARMS pConParms, PRECORDINGCODEC pCodec)
{
    AssertReturn(pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    pSink->pCodec = pCodec;

    /*
     * Container setup.
     */
    try
    {
        switch (pConParms->enmType)
        {
            case AVRECCONTAINERTYPE_MAIN_CONSOLE:
            {
                if (pThis->pConsole)
                {
                    pSink->Con.Main.pConsole = pThis->pConsole;
                }
                else
                    vrc = VERR_NOT_SUPPORTED;
                break;
            }

            case AVRECCONTAINERTYPE_WEBM:
            {
                /* If we only record audio, create our own WebM writer instance here. */
                if (!pSink->Con.WebM.pWebM) /* Do we already have our WebM writer instance? */
                {
                    /** @todo Add sink name / number to file name. */
                    const char *pszFile = pSink->Con.Parms.WebM.pszFile;
                    AssertPtr(pszFile);

                    pSink->Con.WebM.pWebM = new WebMWriter();
                    vrc = pSink->Con.WebM.pWebM->Open(pszFile,
                                                      /** @todo Add option to add some suffix if file exists instead of overwriting? */
                                                      RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE,
                                                      pSink->pCodec->Parms.enmAudioCodec, RecordingVideoCodec_None);
                    if (RT_SUCCESS(vrc))
                    {
                        const PPDMAUDIOPCMPROPS pPCMProps = &pCodec->Parms.Audio.PCMProps;

                        vrc = pSink->Con.WebM.pWebM->AddAudioTrack(pSink->pCodec,
                                                                   PDMAudioPropsHz(pPCMProps), PDMAudioPropsChannels(pPCMProps),
                                                                   PDMAudioPropsSampleBits(pPCMProps), &pSink->Con.WebM.uTrack);
                        if (RT_SUCCESS(vrc))
                        {
                            LogRel(("Recording: Recording audio to audio file '%s'\n", pszFile));
                        }
                        else
                            LogRel(("Recording: Error creating audio track for audio file '%s' (%Rrc)\n", pszFile, vrc));
                    }
                    else
                        LogRel(("Recording: Error creating audio file '%s' (%Rrc)\n", pszFile, vrc));
                }
                break;
            }

            default:
                vrc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    catch (std::bad_alloc &)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(vrc))
    {
        pSink->Con.Parms.enmType = pConParms->enmType;
        pSink->tsStartMs         = RTTimeMilliTS();

        return VINF_SUCCESS;
    }

    recordingCodecDestroy(pSink->pCodec);
    pSink->pCodec = NULL;

    LogRel(("Recording: Error creating sink (%Rrc)\n", vrc));
    return vrc;
}


/**
 * Construct a audio video recording driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/*static*/ DECLCALLBACK(int) AudioVideoRec::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIORECORDING pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIORECORDING);
    RT_NOREF(fFlags);

    LogRel(("Audio: Initializing video recording audio driver\n"));
    LogFlowFunc(("fFlags=0x%x\n", fFlags));

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
    pThis->IHostAudio.pfnGetConfig                  = drvAudioVideoRecHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = NULL;
    pThis->IHostAudio.pfnSetDevice                  = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvAudioVideoRecHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvAudioVideoRecHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvAudioVideoRecHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamEnable               = drvAudioVideoRecHA_StreamEnable;
    pThis->IHostAudio.pfnStreamDisable              = drvAudioVideoRecHA_StreamDisable;
    pThis->IHostAudio.pfnStreamPause                = drvAudioVideoRecHA_StreamPause;
    pThis->IHostAudio.pfnStreamResume               = drvAudioVideoRecHA_StreamResume;
    pThis->IHostAudio.pfnStreamDrain                = drvAudioVideoRecHA_StreamDrain;
    pThis->IHostAudio.pfnStreamGetState             = drvAudioVideoRecHA_StreamGetState;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetWritable          = drvAudioVideoRecHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamPlay                 = drvAudioVideoRecHA_StreamPlay;
    pThis->IHostAudio.pfnStreamGetReadable          = drvAudioVideoRecHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamCapture              = drvAudioVideoRecHA_StreamCapture;

    /*
     * Read configuration.
     */
    PCPDMDRVHLPR3 const pHlp = pDrvIns->pHlpR3;
    /** @todo validate it.    */

    /*
     * Get the Console object pointer.
     */
    com::Guid ConsoleUuid(COM_IIDOF(IConsole));
    IConsole *pIConsole = (IConsole *)PDMDrvHlpQueryGenericUserObject(pDrvIns, ConsoleUuid.raw());
    AssertLogRelReturn(pIConsole, VERR_INTERNAL_ERROR_3);
    Console *pConsole = static_cast<Console *>(pIConsole);
    AssertLogRelReturn(pConsole, VERR_INTERNAL_ERROR_3);

    pThis->pConsole = pConsole;
    AssertReturn(!pThis->pConsole.isNull(), VERR_INVALID_POINTER);
    pThis->pAudioVideoRec = pConsole->i_recordingGetAudioDrv();
    AssertPtrReturn(pThis->pAudioVideoRec, VERR_INVALID_POINTER);

    pThis->pAudioVideoRec->mpDrv = pThis;

    /*
     * Get the recording container parameters from the audio driver instance.
     */
    RT_ZERO(pThis->ContainerParms);
    PAVRECCONTAINERPARMS pConParams = &pThis->ContainerParms;

    int vrc = pHlp->pfnCFGMQueryU32(pCfg, "StreamIndex", (uint32_t *)&pConParams->idxStream);
    AssertRCReturn(vrc, vrc);

    vrc = pHlp->pfnCFGMQueryU32(pCfg, "ContainerType", (uint32_t *)&pConParams->enmType);
    AssertRCReturn(vrc, vrc);

    switch (pConParams->enmType)
    {
        case AVRECCONTAINERTYPE_WEBM:
            vrc = pHlp->pfnCFGMQueryStringAlloc(pCfg, "ContainerFileName", &pConParams->WebM.pszFile);
            AssertRCReturn(vrc, vrc);
            break;

        default:
            break;
    }

    /*
     * Obtain the recording context.
     */
    pThis->pRecCtx =  pConsole->i_recordingGetContext();
    AssertPtrReturn(pThis->pRecCtx, VERR_INVALID_POINTER);

    /*
     * Get the codec configuration.
     */
    RecordingStream *pStream = pThis->pRecCtx->GetStream(pConParams->idxStream);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    const PRECORDINGCODEC pCodec = pStream->GetAudioCodec();
    AssertPtrReturn(pCodec, VERR_INVALID_POINTER);

    /*
     * Init the recording sink.
     */
    vrc = avRecSinkInit(pThis, &pThis->Sink, &pThis->ContainerParms, pCodec);
    if (RT_SUCCESS(vrc))
        LogRel2(("Recording: Audio recording driver initialized\n"));
    else
        LogRel(("Recording: Audio recording driver initialization failed: %Rrc\n", vrc));

    return vrc;
}


/**
 * Video recording audio driver registration record.
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
    sizeof(DRVAUDIORECORDING),
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
    AudioVideoRec::drvPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

