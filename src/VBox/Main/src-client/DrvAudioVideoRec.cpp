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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>
#include "DrvAudioVideoRec.h"
#include "ConsoleImpl.h"

#include "Logging.h"

#include "../../Devices/Audio/DrvAudio.h"
#include "../../Devices/Audio/AudioMixBuffer.h"
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
 * Enumeration for audio/video recording driver recording mode.
 */
typedef enum AVRECMODE
{
    /** Unknown / invalid recording mode. */
    AVRECMODE_UNKNOWN     = 0,
    /** Only record audio.
     *  This mode does not need to talk to the video recording driver,
     *  as this driver then simply creates an own WebM container. */
    AVRECMODE_AUDIO       = 1,
    /** Records audio and video.
     *  Needs to work together with the video recording driver in
     *  order to get a full-featured WebM container. */
    AVRECMODE_AUDIO_VIDEO = 2
} AVRECMODE;

/**
 * Structure for keeping codec specific data.
 */
typedef struct AVRECCODEC
{
    union
    {
#ifdef VBOX_WITH_LIBOPUS
        struct
        {
            /** Encoder we're going to use. */
            OpusEncoder *pEnc;
        } Opus;
#endif /* VBOX_WITH_LIBOPUS */
    };
} AVRECCODEC, *PAVRECCODEC;

/**
 * Audio video recording output stream.
 */
typedef struct AVRECSTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    uint64_t             old_ticks;
    uint64_t             cSamplesSentPerSec;
    /** Codec-specific data.
     *  As every stream can be different, one codec per stream is needed. */
    AVRECCODEC           Codec;
    /** (Audio) frame buffer. */
    PRTCIRCBUF           pCircBuf;
    /** Pointer to WebM container to write recorded audio data to.
     *  See the AVRECMODE enumeration for more information. */
    WebMWriter          *pWebM;
    /** Assigned track number from WebM container. */
    uint8_t              uTrack;
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
    /** Pointer to the DrvAudio port interface that is above us. */
    PPDMIAUDIOCONNECTOR  pDrvAudio;
    /** Recording mode. */
    AVRECMODE            enmMode;
} DRVAUDIOVIDEOREC, *PDRVAUDIOVIDEOREC;

/** Makes DRVAUDIOVIDEOREC out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface) \
    ( (PDRVAUDIOVIDEOREC)((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIOVIDEOREC, IHostAudio)) )


static int avRecCreateStreamOut(PPDMIHOSTAUDIO pInterface,
                                PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PAVRECSTREAMOUT pStreamOut = (PAVRECSTREAMOUT)pStream;

    int rc;

#ifdef VBOX_WITH_LIBOPUS
    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);

    if (pThis->enmMode == AVRECMODE_AUDIO)
    {
        pStreamOut->pWebM = new WebMWriter();
        rc = pStreamOut->pWebM->Create("/tmp/acap.webm", RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE, /** @todo Fix path! */
                                       WebMWriter::AudioCodec_Opus, WebMWriter::VideoCodec_None);
        if (RT_SUCCESS(rc))
            rc = pStreamOut->pWebM->AddAudioTrack(44100, 2, 16, &pStreamOut->uTrack);
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = RTCircBufCreate(&pStreamOut->pCircBuf, _4K);
    if (RT_FAILURE(rc))
        return rc;

    rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStreamOut->Props);
    if (RT_SUCCESS(rc))
    {
        OpusEncoder *pEnc = NULL;

        int orc;
        pEnc = opus_encoder_create(48000 /* Hz */, 2 /* Stereo */, OPUS_APPLICATION_AUDIO, &orc);
        if (orc != OPUS_OK)
        {
            LogRel(("VideoRec: Audio codec failed to initialize: %s\n", opus_strerror(orc)));
            return VERR_AUDIO_BACKEND_INIT_FAILED;
        }

        AssertPtr(pEnc);

#if 0
        orc = opus_encoder_ctl(pEnc, OPUS_SET_BITRATE(DrvAudioHlpCalcBitrate()));
        if (orc != OPUS_OK)
        {
            LogRel(("VideoRec: Audio codec failed to set bitrate: %s\n", opus_strerror(orc)));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
        }
        else
        {
#endif
        pStreamOut->Codec.Opus.pEnc = pEnc;

        if (pCfgAcq)
            pCfgAcq->cSampleBufferSize = _4K; /** @todo Make this configurable. */
    }
#else
    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_LIBOPUS */

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int avRecControlStreamOut(PPDMIHOSTAUDIO pInterface,
                                 PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    RT_NOREF(enmStreamCmd);

    PDRVAUDIOVIDEOREC pThis = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);
    RT_NOREF(pThis);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
            break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            AudioMixBufReset(&pStream->MixBuf);
            break;
        }

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

    pThis->enmMode = AVRECMODE_AUDIO; /** @todo Fix mode! */

    int rc;

    try
    {
        switch (pThis->enmMode)
        {
            /* In audio-only mode we're creating our own WebM writer instance,
             * as we don't have to synchronize with any external source, such as video recording data.*/
            case AVRECMODE_AUDIO:
            {
                rc = VINF_SUCCESS;
                break;
            }

            case AVRECMODE_AUDIO_VIDEO:
            {
                rc = VERR_NOT_SUPPORTED;
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
                                                    PPDMAUDIOSTREAM pStream, const void *pvBuf2, uint32_t cbBuf2,
                                                    uint32_t *pcbWritten)
{
    RT_NOREF2(pvBuf2, cbBuf2);

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    PDRVAUDIOVIDEOREC pThis      = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);
    PAVRECSTREAMOUT   pStreamOut = (PAVRECSTREAMOUT)pStream;

    uint32_t cLive = AudioMixBufLive(&pStream->MixBuf);

    uint64_t now = PDMDrvHlpTMGetVirtualTime(pThis->pDrvIns);
    uint64_t ticks = now  - pStreamOut->old_ticks;
    uint64_t ticks_per_second = PDMDrvHlpTMGetVirtualFreq(pThis->pDrvIns);

    /* Minimize the rounding error: samples = int((ticks * freq) / ticks_per_second + 0.5). */
    uint32_t cSamplesPlayed = (int)((2 * ticks * pStreamOut->Props.uHz + ticks_per_second) / ticks_per_second / 2);

    /* Don't play more than available. */
    if (cSamplesPlayed > cLive)
        cSamplesPlayed = cLive;

    /* Remember when samples were consumed. */
    pStreamOut->old_ticks = now;

    int cSamplesToSend = cSamplesPlayed;

    LogFlowFunc(("uFreq=%RU32, cChan=%RU8, cBits=%RU8, fSigned=%RTbool, cSamplesToSend=%RU32\n",
                 pStreamOut->Props.uHz,   pStreamOut->Props.cChannels,
                 pStreamOut->Props.cBits, pStreamOut->Props.fSigned, cSamplesToSend));

    uint32_t csReadTotal = 0;

    int rc;

    /*
     * Call the encoder with the data.
     */
#ifdef VBOX_WITH_LIBOPUS

    uint16_t cbFrameSize = 960; /** @todo 20ms worth of audio data. */

    PRTCIRCBUF pCircBuf = pStreamOut->pCircBuf;

    while (RTCircBufUsed(pCircBuf) < cbFrameSize)
    {
        void  *pvBuf;
        size_t cbBuf;
        RTCircBufAcquireWriteBlock(pCircBuf, RTCircBufFree(pCircBuf), &pvBuf, &cbBuf);

        uint32_t cbRead = 0;

        if (cbBuf)
        {
            uint32_t csRead = 0;
            rc = AudioMixBufReadCirc(&pStream->MixBuf, pvBuf, cbBuf, &csRead);
            if (RT_SUCCESS(rc))
            {
                AudioMixBufFinish(&pStream->MixBuf, csRead);

                cbRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, csRead);
                csReadTotal += csRead;
            }
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbRead);

        if (   RT_FAILURE(rc)
            || !cbRead)
        {
            break;
        }

        if (RTCircBufUsed(pCircBuf) >= cbFrameSize)
        {
            uint8_t abSrc[_4K];
            size_t  cbSrc = 0;

            while (cbSrc < cbFrameSize)
            {
                RTCircBufAcquireReadBlock(pCircBuf, cbFrameSize - cbSrc, &pvBuf, &cbBuf);

                if (cbBuf)
                {
                    memcpy(&abSrc[cbSrc], pvBuf, cbBuf);

                    cbSrc += cbBuf;
                    Assert(cbSrc <= sizeof(abSrc));
                }

                RTCircBufReleaseReadBlock(pCircBuf, cbBuf);

                if (!cbBuf)
                    break;
            }

            /*
             * Opus always encodes PER FRAME, that is, 2.5, 5, 10, 20, 40 or 60 ms of audio data.
             *
             * A packet can have up to 120ms worth of audio data.
             * Anything > 120ms of data will result in a "corrupted package" error message by
             * by decoding application.
             */
            uint8_t abDst[_4K];
            size_t  cbDst = sizeof(abDst);

            /* Call the encoder to encode one frame per iteration. */
            opus_encoder_ctl(pStreamOut->Codec.Opus.pEnc, OPUS_SET_BITRATE(196000)); /** @todo Only needed for VBR encoding. */
            opus_int32 cbWritten = opus_encode(pStreamOut->Codec.Opus.pEnc,
                                               (opus_int16 *)abSrc, cbSrc, abDst, cbDst);
            if (cbWritten > 0)
            {
                cbDst = cbWritten;
                Assert(cbDst <= sizeof(abDst));

                /* Call the WebM writer to actually write the encoded audio data. */
                WebMWriter::BlockData_Opus blockData = { abDst, cbDst };
                rc = pStreamOut->pWebM->WriteBlock(pStreamOut->uTrack, &blockData, sizeof(blockData));
                AssertRC(rc);
            }
            else if (cbWritten < 0)
            {
                AssertMsgFailed(("Encoding failed: %s\n", opus_strerror(cbWritten)));
                rc = VERR_INVALID_PARAMETER;
            }

            if (RT_FAILURE(rc))
                break;
        }
    }
#else
    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_LIBOPUS */

    /*
     * Always report back all samples acquired, regardless of whether the
     * encoder actually did process those.
     */
    if (pcbWritten)
        *pcbWritten = csReadTotal;

    LogFlowFunc(("csReadTotal=%RU32, rc=%Rrc\n", csReadTotal, rc));
    return rc;
}


static int avRecDestroyStreamOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIOVIDEOREC pThis      = PDMIHOSTAUDIO_2_DRVAUDIOVIDEOREC(pInterface);
    RT_NOREF(pThis);
    PAVRECSTREAMOUT   pStreamOut = (PAVRECSTREAMOUT)pStream;

    if (pStreamOut->pCircBuf)
    {
        RTCircBufDestroy(pStreamOut->pCircBuf);
        pStreamOut->pCircBuf = NULL;
    }

#ifdef VBOX_WITH_LIBOPUS
    if (pStreamOut->Codec.Opus.pEnc)
    {
        opus_encoder_destroy(pStreamOut->Codec.Opus.pEnc);
        pStreamOut->Codec.Opus.pEnc = NULL;
    }
#endif

    switch (pThis->enmMode)
    {
        case AVRECMODE_AUDIO:
        {
            if (pStreamOut->pWebM)
                pStreamOut->pWebM->Close();
            break;
        }

        case AVRECMODE_AUDIO_VIDEO:
        default:
            break;
    }

    return VINF_SUCCESS;
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

    switch (pThis->enmMode)
    {
        case AVRECMODE_AUDIO:
        case AVRECMODE_AUDIO_VIDEO:
        default:
            break;
    }
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

    if (pCfgReq->enmDir == PDMAUDIODIR_OUT)
        return avRecCreateStreamOut(pInterface, pStream, pCfgReq, pCfgAcq);

    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVideoRecStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    if (pStream->enmDir == PDMAUDIODIR_OUT)
        return avRecDestroyStreamOut(pInterface, pStream);

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

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    if (pStream->enmDir == PDMAUDIODIR_OUT)
        return avRecControlStreamOut(pInterface,  pStream, enmStreamCmd);

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
     * Get the AudioVideoRec object pointer.
     */
    void *pvUser = NULL;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
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

