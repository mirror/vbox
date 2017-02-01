/* $Id$ */
/** @file
 * Video recording audio backend for Main.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/**
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

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>
#include "DrvAudioVideoRec.h"
#include "ConsoleImpl.h"

#include "Logging.h"

#include "../../Devices/Audio/DrvAudio.h"
#include "EbmlWriter.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>

#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>

#ifdef VBOX_WITH_LIBOPUS
# include <opus.h>
#endif

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
    /** Recorded data will be written a .webm file. */
    AVRECCONTAINERTYPE_WEBM         = 2
} AVRECCONTAINERTYPE;

/**
 * Structure for keeping generic container parameters.
 */
typedef struct AVRECCONTAINERPARMS
{
    /** The container's type. */
    AVRECCONTAINERTYPE      enmType;

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
 * Structure for keeping generic codec parameters.
 */
typedef struct AVRECCODECPARMS
{
    /** The encoding rate to use. */
    uint32_t                uHz;
    /** Duration of the frame in samples (per channel).
     *
     *  For Opus, valid frame size are:
     *  ms           Frame size
     *  2.5          120
     *  5            240
     *  10           480
     *  20 (Default) 960
     *  40           1920
     *  60           2880
     */
    /** Number of audio channels to encode.
     *  Currently we only supported stereo (2) channels. */
    uint8_t                 cChannels;
    /** The codec's bitrate. 0 if not used / cannot be specified. */
    uint32_t                uBitrate;

} AVRECCODECPARMS, *PAVRECCODECPARMS;

/**
 * Structure for keeping codec-specific data.
 */
typedef struct AVRECCODEC
{
    /** Generic codec parameters. */
    AVRECCODECPARMS         Parms;
    union
    {
#ifdef VBOX_WITH_LIBOPUS
        struct
        {
            /** Encoder we're going to use. */
            OpusEncoder    *pEnc;
            /** Number of samples per frame. */
            uint32_t        csFrame;
            /** The maximum frame size (in samples) we can handle. */
            uint32_t        csFrameMax;
        } Opus;
#endif /* VBOX_WITH_LIBOPUS */
    };

#ifdef VBOX_WITH_STATISTICS /** @todo Make these real STAM values. */
    struct
    {
        /** Number of frames encoded. */
        uint64_t        cEncFrames;
        /** Total time (in ms) of already encoded audio data. */
        uint64_t        msEncTotal;
    } STAM;
#endif /* VBOX_WITH_STATISTICS */

} AVRECCODEC, *PAVRECCODEC;

typedef struct AVRECSINK
{
    /** @todo Add types for container / codec as soon as we implement more stuff. */

    /** Container data to use for data processing. */
    AVRECCONTAINER       Con;
    /** Codec data this stream uses for encoding. */
    AVRECCODEC           Codec;
} AVRECSINK, *PAVRECSINK;

/**
 * Audio video recording output stream.
 */
typedef struct AVRECSTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    /** (Audio) frame buffer. */
    PRTCIRCBUF           pCircBuf;
    /** Pointer to sink to use for writing. */
    PAVRECSINK           pSink;
} AVRECSTREAMOUT, *PAVRECSTREAMOUT;

/**
 * Video recording audio driver instance data.
 */
typedef struct DRVAUDIOVIDEOREC
{
    /** Pointer to audio video recording object. */
    AudioVideoRec       *pAudioVideoRec;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS           pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO        IHostAudio;
    /** Pointer to the console object. */
    ComObjPtr<Console>   pConsole;
    /** Pointer to the DrvAudio port interface that is above us. */
    PPDMIAUDIOCONNECTOR  pDrvAudio;
    /** The driver's sink for writing output to. */
    AVRECSINK            Sink;
} DRVAUDIOVIDEOREC, *PDRVAUDIOVIDEOREC;

/** Makes DRVAUDIOVIDEOREC out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface) \
    ( (PDRVAUDIOVIDEOREC)((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIOVIDEOREC, IHostAudio)) )

/**
 * Initializes a recording sink.
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pSink               Sink to initialize.
 * @param   pConParms           Container parameters to set.
 * @param   pCodecParms         Codec parameters to set.
 */
static int avRecSinkInit(PDRVAUDIOVIDEOREC pThis, PAVRECSINK pSink, PAVRECCONTAINERPARMS pConParms, PAVRECCODECPARMS pCodecParms)
{
    uint32_t uHz = pCodecParms->uHz;

    /* Opus only supports certain input sample rates in an efficient manner.
     * So make sure that we use those by resampling the data to the requested rate. */
    if      (uHz > 24000) uHz = 48000;
    else if (uHz > 16000) uHz = 24000;
    else if (uHz > 12000) uHz = 16000;
    else if (uHz > 8000 ) uHz = 12000;
    else     uHz = 8000;

    OpusEncoder *pEnc = NULL;

    int orc;
    pEnc = opus_encoder_create(pCodecParms->uHz, pCodecParms->cChannels, OPUS_APPLICATION_AUDIO, &orc);
    if (orc != OPUS_OK)
    {
        LogRel(("VideoRec: Audio codec failed to initialize: %s\n", opus_strerror(orc)));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    AssertPtr(pEnc);

    opus_encoder_ctl(pEnc, OPUS_SET_BITRATE(pCodecParms->uBitrate));
    if (orc != OPUS_OK)
    {
        LogRel(("VideoRec: Audio codec failed to set bitrate (%RU32): %s\n", pCodecParms->uBitrate, opus_strerror(orc)));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    LogRel(("VideoRec: Recording audio in %RU16Hz, %RU8 channels, %RU32 bpS\n",
            pCodecParms->uHz, pCodecParms->cChannels, pCodecParms->uBitrate / 1000));

    int rc;

    try
    {
        switch (pConParms->enmType)
        {
            case AVRECCONTAINERTYPE_MAIN_CONSOLE:
            {
                pSink->Con.Main.pConsole = pThis->pConsole;

                rc = VINF_SUCCESS;
                break;
            }

            case AVRECCONTAINERTYPE_WEBM:
            {
                rc = VINF_SUCCESS;
                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        pSink->Con.Parms.enmType     = pConParms->enmType;

        pSink->Codec.Parms.uHz       = uHz;
        pSink->Codec.Parms.cChannels = pCodecParms->cChannels;
        pSink->Codec.Parms.uBitrate  = pCodecParms->uBitrate;

        pSink->Codec.Opus.pEnc       = pEnc;
        pSink->Codec.Opus.csFrame    = uHz / 50;

#ifdef VBOX_WITH_STATISTICS
        pSink->Codec.STAM.cEncFrames = 0;
        pSink->Codec.STAM.msEncTotal = 0;
#endif

        /* Calculate the maximum frame size. */
        pSink->Codec.Opus.csFrameMax = 48000                   /* Maximum sample rate Opus can handle */
                                     * pCodecParms->cChannels; /* Number of channels */
    }

    return rc;
}


/**
 * Shuts down (closes) a recording sink,
 *
 * @returns IPRT status code.
 * @param   pSink               Recording sink to shut down.
 */
static void avRecSinkShutdown(PAVRECSINK pSink)
{
    AssertPtrReturnVoid(pSink);

#ifdef VBOX_WITH_LIBOPUS
    if (pSink->Codec.Opus.pEnc)
    {
        opus_encoder_destroy(pSink->Codec.Opus.pEnc);
        pSink->Codec.Opus.pEnc = NULL;
    }
#endif
    switch (pSink->Con.Parms.enmType)
    {
        case AVRECCONTAINERTYPE_WEBM:
        {
            if (pSink->Con.WebM.pWebM)
            {
                int rc2 = pSink->Con.WebM.pWebM->Close();
                AssertRC(rc2);

                delete pSink->Con.WebM.pWebM;
                pSink->Con.WebM.pWebM = NULL;
            }
            break;
        }

        case AVRECCONTAINERTYPE_MAIN_CONSOLE:
        default:
            break;
    }
}


/**
 * Creates an audio output stream and associates it with the specified recording sink.
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pStream             Audio output stream to create.
 * @param   pSink               Recording sink to associate audio output stream to.
 * @param   pCfgReq             Requested configuration by the audio backend.
 * @param   pCfgAcq             Acquired configuration by the audio output stream.
 */
static int avRecCreateStreamOut(PDRVAUDIOVIDEOREC pThis, PPDMAUDIOSTREAM pStream,
                                PAVRECSINK pSink, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pSink,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    PAVRECSTREAMOUT pStreamOut = (PAVRECSTREAMOUT)pStream;

    if (pCfgReq->DestSource.Dest != PDMAUDIOPLAYBACKDEST_FRONT)
    {
        AssertFailed();

        if (pCfgAcq)
            pCfgAcq->cSampleBufferSize = 0;

        LogRel2(("VideoRec: Support for surround audio not implemented yet\n"));
        return VERR_NOT_SUPPORTED;
    }

    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStreamOut->Props);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_LIBOPUS
    /* If we only record audio, create our own WebM writer instance here. */
    if (pSink->Con.Parms.enmType == AVRECCONTAINERTYPE_WEBM)
    {
        pSink->Con.WebM.pWebM = new WebMWriter();
        rc = pSink->Con.WebM.pWebM->Create("/tmp/acap.webm", RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE, /** @todo Fix path! */
                                           WebMWriter::AudioCodec_Opus, WebMWriter::VideoCodec_None);
        if (RT_SUCCESS(rc))
            rc = pSink->Con.WebM.pWebM->AddAudioTrack(pSink->Codec.Parms.uHz, pStreamOut->Props.cChannels, pStreamOut->Props.cBits,
                                                      &pSink->Con.WebM.uTrack);
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = RTCircBufCreate(&pStreamOut->pCircBuf, (pSink->Codec.Opus.csFrame * pSink->Codec.Parms.cChannels) * sizeof(uint16_t));
    if (RT_SUCCESS(rc))
    {
        pStreamOut->pSink = pSink; /* Assign sink to stream. */

        if (pCfgAcq)
        {
            /* Make sure to let the driver backend know that we need the audio data in
             * a specific sampling rate Opus is optimized for. */
            pCfgAcq->uHz               = pSink->Codec.Parms.uHz;
            pCfgAcq->cSampleBufferSize = _4K; /** @todo Make this configurable. */
        }
    }
#else
    RT_NOREF(pThis, pSink, pStream, pCfgReq, pCfgAcq);
    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_LIBOPUS */

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Destroys (closes) an audio output stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pStream             Audio output stream to destroy.
 */
static int avRecDestroyStreamOut(PDRVAUDIOVIDEOREC pThis, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pThis);
    PAVRECSTREAMOUT pStreamOut = (PAVRECSTREAMOUT)pStream;

    if (pStreamOut->pCircBuf)
    {
        RTCircBufDestroy(pStreamOut->pCircBuf);
        pStreamOut->pCircBuf = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * Controls an audio output stream
 *
 * @returns IPRT status code.
 * @param   pThis               Driver instance.
 * @param   pStream             Audio output stream to control.
 * @param   enmStreamCmd        Stream command to issue.
 */
static int avRecControlStreamOut(PDRVAUDIOVIDEOREC pThis,
                                 PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    RT_NOREF(enmStreamCmd);

    RT_NOREF(pThis);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        case PDMAUDIOSTREAMCMD_PAUSE:
            break;

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            break;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvAudioVideoRecInit(PPDMIHOSTAUDIO pInterface)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    AVRECCONTAINERPARMS ContainerParms;
    ContainerParms.enmType = AVRECCONTAINERTYPE_MAIN_CONSOLE; /** @todo Make this configurable. */

    AVRECCODECPARMS CodecParms;
    CodecParms.uHz       = 48000;  /** @todo Make this configurable. */
    CodecParms.cChannels = 2;      /** @todo Make this configurable. */
    CodecParms.uBitrate  = 196000; /** @todo Make this configurable. */

    int rc = avRecSinkInit(pThis, &pThis->Sink, &ContainerParms, &CodecParms);
    if (RT_FAILURE(rc))
    {
        LogRel(("VideoRec: Audio recording driver failed to initialize, rc=%Rrc\n", rc));
    }
    else
        LogRel2(("VideoRec: Audio recording driver initialized\n"));

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamCapture(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface, pStream, pvBuf, cbBuf);

    if (pcbRead)
        *pcbRead = 0;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                    PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf,
                                                    uint32_t *pcbWritten)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PDRVAUDIOVIDEOREC pThis      = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);
    RT_NOREF(pThis);
    PAVRECSTREAMOUT   pStreamOut = (PAVRECSTREAMOUT)pStream;

    int rc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;

    /*
     * Call the encoder with the data.
     */
#ifdef VBOX_WITH_LIBOPUS
    PAVRECSINK pSink    = pStreamOut->pSink;
    AssertPtr(pSink);
    PRTCIRCBUF pCircBuf = pStreamOut->pCircBuf;
    AssertPtr(pCircBuf);

    void  *pvCircBuf;
    size_t cbCircBuf;

    uint32_t cbToWrite = cbBuf;

    /*
     * Fetch as much as we can into our internal ring buffer.
     */
    while (   cbToWrite
           && RTCircBufFree(pCircBuf))
    {
        RTCircBufAcquireWriteBlock(pCircBuf, cbToWrite, &pvCircBuf, &cbCircBuf);

        if (cbCircBuf)
        {
            memcpy(pvCircBuf, (uint8_t *)pvBuf + cbWrittenTotal, cbCircBuf),
            cbWrittenTotal += cbCircBuf;
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbCircBuf);

        if (   RT_FAILURE(rc)
            || !cbCircBuf)
        {
            break;
        }
    }

    /*
     * Process our internal ring buffer and encode the data.
     */

    uint8_t abSrc[_64K]; /** @todo Fix! */
    size_t  cbSrc;

    const uint32_t csFrame = pSink->Codec.Opus.csFrame;
    const uint32_t cbFrame = PDMAUDIOPCMPROPS_S2B(&pStreamOut->Props, csFrame);

    while (RTCircBufUsed(pCircBuf) >= cbFrame)
    {
        cbSrc = 0;

        while (cbSrc < cbFrame)
        {
            RTCircBufAcquireReadBlock(pCircBuf, cbFrame - cbSrc, &pvCircBuf, &cbCircBuf);

            if (cbCircBuf)
            {
                memcpy(&abSrc[cbSrc], pvCircBuf, cbCircBuf);

                cbSrc += cbCircBuf;
                Assert(cbSrc <= sizeof(abSrc));
            }

            RTCircBufReleaseReadBlock(pCircBuf, cbCircBuf);

            if (!cbCircBuf)
                break;
        }

# ifdef DEBUG_andy
        RTFILE fh;
        RTFileOpen(&fh, "/tmp/acap.pcm", RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, abSrc, cbSrc, NULL);
        RTFileClose(fh);
# endif

        Assert(cbSrc == cbFrame);

        /*
         * Opus always encodes PER FRAME, that is, exactly 2.5, 5, 10, 20, 40 or 60 ms of audio data.
         *
         * A packet can have up to 120ms worth of audio data.
         * Anything > 120ms of data will result in a "corrupted package" error message by
         * by decoding application.
         */
        uint8_t abDst[_64K]; /** @todo Fix! */
        size_t  cbDst = sizeof(abDst);

        /* Call the encoder to encode one frame per iteration. */
        opus_int32 cbWritten = opus_encode(pSink->Codec.Opus.pEnc,
                                           (opus_int16 *)abSrc, csFrame, abDst, cbDst);
        if (cbWritten > 0)
        {
# ifdef VBOX_WITH_STATISTICS
            /* Get overall frames encoded. */
            uint32_t cEncFrames          = opus_packet_get_nb_frames(abDst, cbDst);
            uint32_t cEncSamplesPerFrame = opus_packet_get_samples_per_frame(abDst, pSink->Codec.Parms.uHz);
            uint32_t csEnc               = cEncFrames * cEncSamplesPerFrame;

            pSink->Codec.STAM.cEncFrames += cEncFrames;
            pSink->Codec.STAM.msEncTotal += 20 /* Default 20 ms */ * cEncFrames;

            LogFunc(("%RU64ms [%RU64 frames]: cbSrc=%zu, cbDst=%zu, cEncFrames=%RU32, cEncSamplesPerFrame=%RU32, csEnc=%RU32\n",
                     pSink->Codec.STAM.msEncTotal, pSink->Codec.STAM.cEncFrames,
                     cbSrc, cbDst, cEncFrames, cEncSamplesPerFrame, csEnc));
# endif
            Assert((uint32_t)cbWritten <= cbDst);
            cbDst = RT_MIN((uint32_t)cbWritten, cbDst); /* Update cbDst to actual bytes encoded (written). */

            switch (pSink->Con.Parms.enmType)
            {
                case AVRECCONTAINERTYPE_MAIN_CONSOLE:
                {
                    HRESULT hr = pSink->Con.Main.pConsole->i_audioVideoRecSendAudio(abDst, cbDst, RTTimeMilliTS() /* Now */);
                    Assert(hr == S_OK);

                    break;
                }

                case AVRECCONTAINERTYPE_WEBM:
                {
                    WebMWriter::BlockData_Opus blockData = { abDst, cbDst, RTTimeMilliTS() };
                    rc = pSink->Con.WebM.pWebM->WriteBlock(pSink->Con.WebM.uTrack, &blockData, sizeof(blockData));
                    AssertRC(rc);

                    break;
                }

                default:
                    AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
                    break;
            }
        }
        else if (cbWritten < 0)
        {
            AssertMsgFailed(("Encoding failed: %s\n", opus_strerror(cbWritten)));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_FAILURE(rc))
            break;
    }
#else
    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_LIBOPUS */

    /*
     * Always report back all samples acquired, regardless of whether the
     * encoder actually did process those.
     */
    if (pcbWritten)
        *pcbWritten = cbWrittenTotal;

    LogFlowFunc(("csReadTotal=%RU32, rc=%Rrc\n", cbWrittenTotal, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioVideoRecGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    pBackendCfg->cbStreamOut    = sizeof(AVRECSTREAMOUT);
    pBackendCfg->cbStreamIn     = 0;
    pBackendCfg->cMaxStreamsIn  = 0;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvAudioVideoRecShutdown(PPDMIHOSTAUDIO pInterface)
{
    LogFlowFuncEnter();

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    avRecSinkShutdown(&pThis->Sink);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioVideoRecGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                      PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    /* For now we only have one sink, namely the driver's one.
     * Later each stream could have its own one, to e.g. router different stream to different sinks .*/
    PAVRECSINK pSink = &pThis->Sink;

    if (pCfgReq->enmDir == PDMAUDIODIR_OUT)
        return avRecCreateStreamOut(pThis, pStream, pSink, pCfgReq, pCfgAcq);

    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    if (pStream->enmDir == PDMAUDIODIR_OUT)
        return avRecDestroyStreamOut(pThis, pStream);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamControl(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    if (pStream->enmDir == PDMAUDIODIR_OUT)
        return avRecControlStreamOut(pThis,  pStream, enmStreamCmd);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvAudioVideoRecStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    return (  PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED
            | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE | PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for video recording. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVideoRecQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVIDEOREC pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVIDEOREC);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


AudioVideoRec::AudioVideoRec(Console *pConsole)
    : mpDrv(NULL)
    , mpConsole(pConsole)
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
 * Construct a audio video recording driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/* static */
DECLCALLBACK(int) AudioVideoRec::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIOVIDEOREC pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVIDEOREC);

    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

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
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvAudioVideoRec);

    /*
     * Get the Console object pointer.
     */
    void *pvUser;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectConsole", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"ObjectConsole\" value, rc=%Rrc\n", rc), rc);

    /* CFGM tree saves the pointer to Console in the Object node of AudioVideoRec. */
    pThis->pConsole = (Console *)pvUser;

    /*
     * Get the pointer to the audio driver instance.
     */
    rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"Object\" value, rc=%Rrc\n", rc), rc);

    pThis->pAudioVideoRec = (AudioVideoRec *)pvUser;
    pThis->pAudioVideoRec->mpDrv = pThis;

    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls.
     * Described in CFGM tree.
     */
    pThis->pDrvAudio = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    AssertMsgReturn(pThis->pDrvAudio, ("Configuration error: No upper interface specified!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
/* static */
DECLCALLBACK(void) AudioVideoRec::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIOVIDEOREC pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVIDEOREC);
    LogFlowFuncEnter();

    /*
     * If the AudioVideoRec object is still alive, we must clear it's reference to
     * us since we'll be invalid when we return from this method.
     */
    if (pThis->pAudioVideoRec)
    {
        pThis->pAudioVideoRec->mpDrv = NULL;
        pThis->pAudioVideoRec = NULL;
    }
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

