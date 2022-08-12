/* $Id$ */
/** @file
 * Recording codec wrapper.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_RECORDING
#include "LoggingNew.h"

#include <VBox/err.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "RecordingInternals.h"
#include "RecordingUtils.h"
#include "WebMWriter.h"

#include <math.h>


/*********************************************************************************************************************************
*   VPX (VP8 / VP9) codec                                                                                                        *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_LIBVPX
/** @copydoc RECORDINGCODECOPS::pfnInit */
static DECLCALLBACK(int) recordingCodecVPXInit(PRECORDINGCODEC pCodec)
{
# ifdef VBOX_WITH_LIBVPX_VP9
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp9_cx();
# else /* Default is using VP8. */
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp8_cx();
# endif
    PRECORDINGCODECVPX pVPX = &pCodec->Video.VPX;

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(pCodecIface, &pVPX->Cfg, 0 /* Reserved */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("Recording: Failed to get default config for VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    /* Target bitrate in kilobits per second. */
    pVPX->Cfg.rc_target_bitrate = pCodec->Parms.uBitrate;
    /* Frame width. */
    pVPX->Cfg.g_w = pCodec->Parms.Video.uWidth;
    /* Frame height. */
    pVPX->Cfg.g_h = pCodec->Parms.Video.uHeight;
    /* 1ms per frame. */
    pVPX->Cfg.g_timebase.num = 1;
    pVPX->Cfg.g_timebase.den = 1000;
    /* Disable multithreading. */
    pVPX->Cfg.g_threads      = 0;

    /* Initialize codec. */
    rcv = vpx_codec_enc_init(&pVPX->Ctx, pCodecIface, &pVPX->Cfg, 0 /* Flags */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("Recording: Failed to initialize VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    if (!vpx_img_alloc(&pVPX->RawImage, VPX_IMG_FMT_I420,
                       pCodec->Parms.Video.uWidth, pCodec->Parms.Video.uHeight, 1))
    {
        LogRel(("Recording: Failed to allocate image %RU32x%RU32\n", pCodec->Parms.Video.uWidth, pCodec->Parms.Video.uHeight));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    /* Save a pointer to the first raw YUV plane. */
    pVPX->pu8YuvBuf = pVPX->RawImage.planes[0];

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnDestroy */
static DECLCALLBACK(int) recordingCodecVPXDestroy(PRECORDINGCODEC pCodec)
{
    PRECORDINGCODECVPX pVPX = &pCodec->Video.VPX;

    vpx_img_free(&pVPX->RawImage);
    pVPX->pu8YuvBuf = NULL; /* Was pointing to VPX.RawImage. */

    vpx_codec_err_t rcv = vpx_codec_destroy(&pVPX->Ctx);
    Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnParseOptions */
static DECLCALLBACK(int) recordingCodecVPXParseOptions(PRECORDINGCODEC pCodec, const Utf8Str &strOptions)
{
    size_t pos = 0;
    com::Utf8Str key, value;
    while ((pos = strOptions.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("vc_quality", com::Utf8Str::CaseInsensitive) == 0)
        {
            const PRECORDINGCODECVPX pVPX = &pCodec->Video.VPX;

            if (value.compare("realtime", com::Utf8Str::CaseInsensitive) == 0)
                pVPX->uEncoderDeadline = VPX_DL_REALTIME;
            else if (value.compare("good", com::Utf8Str::CaseInsensitive) == 0)
            {
                AssertStmt(pCodec->Parms.Video.uFPS, pCodec->Parms.Video.uFPS = 25);
                pVPX->uEncoderDeadline = 1000000 / pCodec->Parms.Video.uFPS;
            }
            else if (value.compare("best", com::Utf8Str::CaseInsensitive) == 0)
                pVPX->uEncoderDeadline = VPX_DL_BEST_QUALITY;
            else
                pVPX->uEncoderDeadline = value.toUInt32();
        }
        else
            LogRel2(("Recording: Unknown option '%s' (value '%s'), skipping\n", key.c_str(), value.c_str()));
    } /* while */

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnEncode */
static DECLCALLBACK(int) recordingCodecVPXEncode(PRECORDINGCODEC pCodec, PRECORDINGFRAME pFrame,
                                                 void *pvDst, size_t cbDst, size_t *pcEncoded, size_t *pcbEncoded)
{
    RT_NOREF(pvDst, cbDst, pcEncoded, pcbEncoded);

    AssertPtrReturn(pFrame, VERR_INVALID_POINTER);

    PRECORDINGVIDEOFRAME pVideoFrame = pFrame->VideoPtr;

    int vrc = RecordingUtilsRGBToYUV(pVideoFrame->uPixelFormat,
                                     /* Destination */
                                     pCodec->Video.VPX.pu8YuvBuf, pVideoFrame->uWidth, pVideoFrame->uHeight,
                                     /* Source */
                                     pVideoFrame->pu8RGBBuf, pCodec->Parms.Video.uWidth, pCodec->Parms.Video.uHeight);

    PRECORDINGCODECVPX pVPX = &pCodec->Video.VPX;

    /* Presentation TimeStamp (PTS). */
    vpx_codec_pts_t pts = pFrame->msTimestamp;
    vpx_codec_err_t rcv = vpx_codec_encode(&pVPX->Ctx,
                                           &pVPX->RawImage,
                                           pts                          /* Timestamp */,
                                           pCodec->Parms.Video.uDelayMs /* How long to show this frame */,
                                           0                            /* Flags */,
                                           pVPX->uEncoderDeadline       /* Quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        if (pCodec->Stats.cEncErrors++ < 64) /** @todo Make this configurable. */
            LogRel(("Recording: Failed to encode video frame: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_RECORDING_ENCODING_FAILED;
    }

    pCodec->Stats.cEncErrors = 0;

    vpx_codec_iter_t iter = NULL;
    vrc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pPacket = vpx_codec_get_cx_data(&pVPX->Ctx, &iter);
        if (!pPacket)
            break;

        switch (pPacket->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
            {
                WebMWriter::BlockData_VP8 blockData = { &pCodec->Video.VPX.Cfg, pPacket };
                AssertPtr(pCodec->Callbacks.pfnWriteData);

                vrc = pCodec->Callbacks.pfnWriteData(pCodec, &blockData, sizeof(blockData), pCodec->Callbacks.pvUser);
                break;
            }

            default:
                AssertFailed();
                LogFunc(("Unexpected video packet type %ld\n", pPacket->kind));
                break;
        }
    }

    return vrc;
}
#endif /* VBOX_WITH_LIBVPX */


/*********************************************************************************************************************************
*   Opus codec                                                                                                                   *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_LIBOPUS
/** @copydoc RECORDINGCODECOPS::pfnInit */
static DECLCALLBACK(int) recordingCodecOpusInit(PRECORDINGCODEC pCodec)
{
    const PPDMAUDIOPCMPROPS pProps = &pCodec->Parms.Audio.PCMProps;

    uint32_t       uHz       = PDMAudioPropsHz(pProps);
    uint8_t  const cChannels = PDMAudioPropsChannels(pProps);

    /* Opus only supports certain input sample rates in an efficient manner.
     * So make sure that we use those by resampling the data to the requested rate. */
    if      (uHz > 24000) uHz = VBOX_RECORDING_OPUS_HZ_MAX;
    else if (uHz > 16000) uHz = 24000;
    else if (uHz > 12000) uHz = 16000;
    else if (uHz > 8000 ) uHz = 12000;
    else     uHz = 8000;

    int opus_rc;
    OpusEncoder *pEnc = opus_encoder_create(uHz, cChannels, OPUS_APPLICATION_AUDIO, &opus_rc);
    if (opus_rc != OPUS_OK)
    {
        LogRel(("Recording: Audio codec failed to initialize: %s\n", opus_strerror(opus_rc)));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    AssertPtr(pEnc);

    if (pCodec->Parms.uBitrate) /* Only explicitly set the bitrate management if we specified one. Otherwise let Opus decide. */
    {
        opus_encoder_ctl(pEnc, OPUS_SET_BITRATE(pCodec->Parms.uBitrate));
        if (opus_rc != OPUS_OK)
        {
            opus_encoder_destroy(pEnc);
            pEnc = NULL;

            LogRel(("Recording: Audio codec failed to set bitrate (%RU32): %s\n", pCodec->Parms.uBitrate, opus_strerror(opus_rc)));
            return VERR_RECORDING_CODEC_INIT_FAILED;
        }
    }

    opus_rc = opus_encoder_ctl(pEnc, OPUS_SET_VBR(pCodec->Parms.uBitrate == 0 ? 1 : 0));
    if (opus_rc != OPUS_OK)
    {
        LogRel(("Recording: Audio codec failed to %s VBR mode: %s\n",
                pCodec->Parms.uBitrate == 0 ? "disable" : "enable", opus_strerror(opus_rc)));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    pCodec->Audio.Opus.pEnc = pEnc;

    PDMAudioPropsInit(pProps,
                      PDMAudioPropsSampleSize(pProps), PDMAudioPropsIsSigned(pProps), PDMAudioPropsChannels(pProps), uHz);

    if (!pCodec->Parms.msFrame) /* No ms per frame defined? Use default. */
        pCodec->Parms.msFrame = VBOX_RECORDING_OPUS_FRAME_MS_DEFAULT;

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnDestroy */
static DECLCALLBACK(int) recordingCodecOpusDestroy(PRECORDINGCODEC pCodec)
{
    PRECORDINGCODECOPUS pOpus = &pCodec->Audio.Opus;

    if (pOpus->pEnc)
    {
        opus_encoder_destroy(pOpus->pEnc);
        pOpus->pEnc = NULL;
    }

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnEncode */
static DECLCALLBACK(int) recordingCodecOpusEncode(PRECORDINGCODEC pCodec,
                                                  const PRECORDINGFRAME pFrame, void *pvDst, size_t cbDst,
                                                  size_t *pcEncoded, size_t *pcbEncoded)
{
    const PPDMAUDIOPCMPROPS pPCMProps = &pCodec->Parms.Audio.PCMProps;

    Assert         (pCodec->Parms.cbFrame);
    AssertReturn   (pFrame->Audio.cbBuf % pCodec->Parms.cbFrame == 0, VERR_INVALID_PARAMETER);
    Assert         (pFrame->Audio.cbBuf);
    AssertReturn   (pFrame->Audio.cbBuf % pPCMProps->cbFrame == 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcEncoded,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcbEncoded, VERR_INVALID_POINTER);

    int vrc = VINF_SUCCESS;

    /*
     * Opus always encodes PER "OPUS FRAME", that is, exactly 2.5, 5, 10, 20, 40 or 60 ms of audio data.
     *
     * A packet can have up to 120ms worth of audio data.
     * Anything > 120ms of data will result in a "corrupted package" error message by
     * by decoding application.
     */
    opus_int32 cbWritten = opus_encode(pCodec->Audio.Opus.pEnc,
                                       (opus_int16 *)pFrame->Audio.pvBuf, (int)(pFrame->Audio.cbBuf / pPCMProps->cbFrame /* Number of audio frames */),
                                       (uint8_t *)pvDst, (opus_int32)cbDst);
    if (cbWritten < 0)
    {
        LogRel(("Recording: opus_encode() failed (%s)\n", opus_strerror(cbWritten)));
        return VERR_RECORDING_ENCODING_FAILED;
    }

    /* Get overall frames encoded. */
    *pcEncoded  = opus_packet_get_nb_frames((uint8_t *)pvDst, cbWritten);
    *pcbEncoded = cbWritten;

    if (RT_FAILURE(vrc))
        LogRel(("Recording: Encoding Opus data failed, rc=%Rrc\n", vrc));

    Log3Func(("cbSrc=%zu, cbDst=%zu, cEncoded=%zu, cbEncoded=%zu, vrc=%Rrc\n", pFrame->Audio.cbBuf, cbDst,
              RT_SUCCESS(vrc) ? *pcEncoded : 0, RT_SUCCESS(vrc) ? *pcbEncoded : 0, vrc));

    return vrc;
}
#endif /* VBOX_WITH_LIBOPUS */


/*********************************************************************************************************************************
*   Ogg Vorbis codec                                                                                                             *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_LIBVORBIS
/** @copydoc RECORDINGCODECOPS::pfnInit */
static DECLCALLBACK(int) recordingCodecVorbisInit(PRECORDINGCODEC pCodec)
{
    const PPDMAUDIOPCMPROPS pPCMProps = &pCodec->Parms.Audio.PCMProps;

    /** @todo BUGBUG When left out this call, vorbis_block_init() does not find oggpack_writeinit and all goes belly up ... */
    oggpack_buffer b;
    oggpack_writeinit(&b);

    vorbis_info_init(&pCodec->Audio.Vorbis.info);

    int vorbis_rc;
    if (pCodec->Parms.uBitrate == 0) /* No bitrate management? Then go for ABR (Average Bit Rate) only. */
        vorbis_rc = vorbis_encode_init_vbr(&pCodec->Audio.Vorbis.info,
                                           PDMAudioPropsChannels(pPCMProps), PDMAudioPropsHz(pPCMProps),
                                           (float).4 /* Quality, from -.1 (lowest) to 1 (highest) */);
    else
        vorbis_rc = vorbis_encode_setup_managed(&pCodec->Audio.Vorbis.info, PDMAudioPropsChannels(pPCMProps), PDMAudioPropsHz(pPCMProps),
                                                -1 /* max bitrate (unset) */, pCodec->Parms.uBitrate /* kbps, nominal */, -1 /* min bitrate (unset) */);
    if (vorbis_rc)
    {
        LogRel(("Recording: Audio codec failed to setup %s mode (bitrate %RU32): %d\n",
                pCodec->Parms.uBitrate == 0 ? "VBR" : "bitrate management", pCodec->Parms.uBitrate, vorbis_rc));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    vorbis_rc = vorbis_encode_setup_init(&pCodec->Audio.Vorbis.info);
    if (vorbis_rc)
    {
        LogRel(("Recording: vorbis_encode_setup_init() failed (%d)\n", vorbis_rc));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    /* Initialize the analysis state and encoding storage. */
    vorbis_rc = vorbis_analysis_init(&pCodec->Audio.Vorbis.dsp_state, &pCodec->Audio.Vorbis.info);
    if (vorbis_rc)
    {
        vorbis_info_clear(&pCodec->Audio.Vorbis.info);
        LogRel(("Recording: vorbis_analysis_init() failed (%d)\n", vorbis_rc));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    vorbis_rc = vorbis_block_init(&pCodec->Audio.Vorbis.dsp_state, &pCodec->Audio.Vorbis.block_cur);
    if (vorbis_rc)
    {
        vorbis_info_clear(&pCodec->Audio.Vorbis.info);
        LogRel(("Recording: vorbis_block_init() failed (%d)\n", vorbis_rc));
        return VERR_RECORDING_CODEC_INIT_FAILED;
    }

    if (!pCodec->Parms.msFrame) /* No ms per frame defined? Use default. */
        pCodec->Parms.msFrame = VBOX_RECORDING_VORBIS_FRAME_MS_DEFAULT;

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnDestroy */
static DECLCALLBACK(int) recordingCodecVorbisDestroy(PRECORDINGCODEC pCodec)
{
    PRECORDINGCODECVORBIS pVorbis = &pCodec->Audio.Vorbis;

    vorbis_block_clear(&pVorbis->block_cur);
    vorbis_dsp_clear  (&pVorbis->dsp_state);
    vorbis_info_clear (&pVorbis->info);

    return VINF_SUCCESS;
}

/** @copydoc RECORDINGCODECOPS::pfnEncode */
static DECLCALLBACK(int) recordingCodecVorbisEncode(PRECORDINGCODEC pCodec,
                                                    const PRECORDINGFRAME pFrame, void *pvDst, size_t cbDst,
                                                    size_t *pcEncoded, size_t *pcbEncoded)
{
    const PPDMAUDIOPCMPROPS pPCMProps = &pCodec->Parms.Audio.PCMProps;

    Assert         (pCodec->Parms.cbFrame);
    AssertReturn   (pFrame->Audio.cbBuf % pCodec->Parms.cbFrame == 0, VERR_INVALID_PARAMETER);
    Assert         (pFrame->Audio.cbBuf);
    AssertReturn   (pFrame->Audio.cbBuf % PDMAudioPropsFrameSize(pPCMProps) == 0, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    int const cbFrame = PDMAudioPropsFrameSize(pPCMProps);
    int const cFrames = (int)(pFrame->Audio.cbBuf / cbFrame);

    /* Write non-interleaved frames. */
    float  **buffer = vorbis_analysis_buffer(&pCodec->Audio.Vorbis.dsp_state, cFrames);
    int16_t *puSrc  = (int16_t *)pFrame->Audio.pvBuf; RT_NOREF(puSrc);

    /* Convert samples into floating point. */
    /** @todo This is sloooooooooooow! Optimize this! */
    uint8_t const cChannels = PDMAudioPropsChannels(pPCMProps);
    AssertReturn(cChannels == 2, VERR_NOT_SUPPORTED);

    float const div = 1.0f / 32768.0f;

    for(int f = 0; f < cFrames; f++)
    {
        buffer[0][f] = (float)puSrc[0] * div;
        buffer[1][f] = (float)puSrc[1] * div;
        puSrc += cChannels;
    }

    int vorbis_rc = vorbis_analysis_wrote(&pCodec->Audio.Vorbis.dsp_state, cFrames);
    if (vorbis_rc)
    {
        LogRel(("Recording: vorbis_analysis_wrote() failed (%d)\n", vorbis_rc));
        return VERR_RECORDING_ENCODING_FAILED;
    }

    if (pcEncoded)
        *pcEncoded = 0;
    if (pcbEncoded)
        *pcbEncoded = 0;

    size_t cBlocksEncoded = 0;
    size_t cBytesEncoded  = 0;

    uint8_t *puDst = (uint8_t *)pvDst;

    while (vorbis_analysis_blockout(&pCodec->Audio.Vorbis.dsp_state, &pCodec->Audio.Vorbis.block_cur) == 1 /* More available? */)
    {
        vorbis_rc = vorbis_analysis(&pCodec->Audio.Vorbis.block_cur, NULL);
        if (vorbis_rc < 0)
        {
            LogRel(("Recording: vorbis_analysis() failed (%d)\n", vorbis_rc));
            vorbis_rc = 0; /* Reset */
            vrc = VERR_RECORDING_ENCODING_FAILED;
            break;
        }

        vorbis_rc = vorbis_bitrate_addblock(&pCodec->Audio.Vorbis.block_cur);
        if (vorbis_rc < 0)
        {
            LogRel(("Recording: vorbis_bitrate_addblock() failed (%d)\n", vorbis_rc));
            vorbis_rc = 0; /* Reset */
            vrc = VERR_RECORDING_ENCODING_FAILED;
            break;
        }

        uint64_t const uDurationMs = pCodec->Parms.msFrame;

        /* Vorbis expects us to flush packets one at a time directly to the container.
         *
         * If we flush more than one packet in a row, players can't decode this then. */
        ogg_packet op;
        while ((vorbis_rc = vorbis_bitrate_flushpacket(&pCodec->Audio.Vorbis.dsp_state, &op)) > 0)
        {
            cBytesEncoded += op.bytes;
            AssertBreakStmt(cBytesEncoded <= cbDst, vrc = VERR_BUFFER_OVERFLOW);
            cBlocksEncoded++;

            if (pCodec->Callbacks.pfnWriteData)
            {
                WebMWriter::BlockData_Audio blockData = { op.packet, (size_t)op.bytes, pCodec->uLastTimeStampMs };
                pCodec->Callbacks.pfnWriteData(pCodec, &blockData, sizeof(blockData), pCodec->Callbacks.pvUser);
            }

            pCodec->uLastTimeStampMs += uDurationMs;
        }

        RT_NOREF(puDst);

        /* Note: When vorbis_rc is 0, this marks the last packet, a negative values means error. */
        if (vorbis_rc < 0)
        {
            LogRel(("Recording: vorbis_bitrate_flushpacket() failed (%d)\n", vorbis_rc));
            vorbis_rc = 0; /* Reset */
            vrc = VERR_RECORDING_ENCODING_FAILED;
            break;
        }
    }

    if (vorbis_rc < 0)
    {
        LogRel(("Recording: vorbis_analysis_blockout() failed (%d)\n", vorbis_rc));
        return VERR_RECORDING_ENCODING_FAILED;
    }

    if (pcbEncoded)
        *pcbEncoded = 0;
    if (pcEncoded)
        *pcEncoded  = 0;

    if (RT_FAILURE(vrc))
        LogRel(("Recording: Encoding Vorbis audio data failed, rc=%Rrc\n", vrc));

    Log3Func(("cbSrc=%zu, cbDst=%zu, cEncoded=%zu, cbEncoded=%zu, vrc=%Rrc\n",
              pFrame->Audio.cbBuf, cbDst, cBlocksEncoded, cBytesEncoded, vrc));

    return vrc;
}

static DECLCALLBACK(int) recordingCodecVorbisFinalize(PRECORDINGCODEC pCodec)
{
    int vorbis_rc = vorbis_analysis_wrote(&pCodec->Audio.Vorbis.dsp_state, 0 /* Means finalize */);
    if (vorbis_rc)
    {
        LogRel(("Recording: vorbis_analysis_wrote() failed for finalizing stream (%d)\n", vorbis_rc));
        return VERR_RECORDING_ENCODING_FAILED;
    }

    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_LIBVORBIS */


/*********************************************************************************************************************************
*   Codec API                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Initializes an audio codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec instance to initialize.
 * @param   pCallbacks          Codec callback table to use for the codec.
 * @param   Settings            Screen settings to use for initialization.
 */
static int recordingCodecInitAudio(const PRECORDINGCODEC pCodec,
                                   const PRECORDINGCODECCALLBACKS pCallbacks, const settings::RecordingScreenSettings &Settings)
{
    AssertReturn(pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO, VERR_INVALID_PARAMETER);

    Utf8Str strCodec;
    settings::RecordingScreenSettings::audioCodecToString(pCodec->Parms.enmAudioCodec, strCodec);
    LogRel(("Recording: Initializing audio codec '%s'\n", strCodec.c_str()));

    const PPDMAUDIOPCMPROPS pPCMProps = &pCodec->Parms.Audio.PCMProps;

    PDMAudioPropsInit(pPCMProps,
                      Settings.Audio.cBits / 8,
                      true /* fSigned */, Settings.Audio.cChannels, Settings.Audio.uHz);
    pCodec->Parms.uBitrate = 0; /** @todo No bitrate management for audio yet. */

    if (pCallbacks)
        memcpy(&pCodec->Callbacks, pCallbacks, sizeof(RECORDINGCODECCALLBACKS));

    int vrc = VINF_SUCCESS;

    if (pCodec->Ops.pfnParseOptions)
        vrc = pCodec->Ops.pfnParseOptions(pCodec, Settings.strOptions);

    if (RT_SUCCESS(vrc))
        vrc = pCodec->Ops.pfnInit(pCodec);

    if (RT_SUCCESS(vrc))
    {
        Assert(PDMAudioPropsAreValid(pPCMProps));

        uint32_t uBitrate = pCodec->Parms.uBitrate; /* Bitrate management could have been changed by pfnInit(). */

        LogRel2(("Recording: Audio codec is initialized with %RU32Hz, %RU8 channel(s), %RU8 bits per sample\n",
                 PDMAudioPropsHz(pPCMProps), PDMAudioPropsChannels(pPCMProps), PDMAudioPropsSampleBits(pPCMProps)));
        LogRel2(("Recording: Audio codec's bitrate management is %s (%RU32 kbps)\n", uBitrate ? "enabled" : "disabled", uBitrate));

        if (!pCodec->Parms.msFrame || pCodec->Parms.msFrame >= RT_MS_1SEC) /* Not set yet by codec stuff above? */
            pCodec->Parms.msFrame = 20; /* 20ms by default should be a sensible value; to prevent division by zero. */

        pCodec->Parms.csFrame  = PDMAudioPropsHz(pPCMProps) / (RT_MS_1SEC / pCodec->Parms.msFrame);
        pCodec->Parms.cbFrame  = PDMAudioPropsFramesToBytes(pPCMProps, pCodec->Parms.csFrame);

        LogFlowFunc(("cbSample=%RU32, msFrame=%RU32 -> csFrame=%RU32, cbFrame=%RU32, uBitrate=%RU32\n",
                     PDMAudioPropsSampleSize(pPCMProps), pCodec->Parms.msFrame, pCodec->Parms.csFrame, pCodec->Parms.cbFrame, pCodec->Parms.uBitrate));
    }
    else
        LogRel(("Recording: Error initializing audio codec (%Rrc)\n", vrc));

    return vrc;
}

/**
 * Initializes a video codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec instance to initialize.
 * @param   pCallbacks          Codec callback table to use for the codec.
 * @param   Settings            Screen settings to use for initialization.
 */
static int recordingCodecInitVideo(const PRECORDINGCODEC pCodec,
                                   const PRECORDINGCODECCALLBACKS pCallbacks, const settings::RecordingScreenSettings &Settings)
{
    Utf8Str strTemp;
    settings::RecordingScreenSettings::videoCodecToString(pCodec->Parms.enmVideoCodec, strTemp);
    LogRel(("Recording: Initializing video codec '%s'\n", strTemp.c_str()));

    pCodec->Parms.uBitrate       = Settings.Video.ulRate;
    pCodec->Parms.Video.uFPS     = Settings.Video.ulFPS;
    pCodec->Parms.Video.uWidth   = Settings.Video.ulWidth;
    pCodec->Parms.Video.uHeight  = Settings.Video.ulHeight;
    pCodec->Parms.Video.uDelayMs = RT_MS_1SEC / pCodec->Parms.Video.uFPS;

    if (pCallbacks)
        memcpy(&pCodec->Callbacks, pCallbacks, sizeof(RECORDINGCODECCALLBACKS));

    AssertReturn(pCodec->Parms.uBitrate, VERR_INVALID_PARAMETER);        /* Bitrate must be set. */
    AssertStmt(pCodec->Parms.Video.uFPS, pCodec->Parms.Video.uFPS = 25); /* Prevent division by zero. */

    AssertReturn(pCodec->Parms.Video.uHeight, VERR_INVALID_PARAMETER);
    AssertReturn(pCodec->Parms.Video.uWidth, VERR_INVALID_PARAMETER);
    AssertReturn(pCodec->Parms.Video.uDelayMs, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    if (pCodec->Ops.pfnParseOptions)
        vrc = pCodec->Ops.pfnParseOptions(pCodec, Settings.strOptions);

    if (   RT_SUCCESS(vrc)
        && pCodec->Ops.pfnInit)
        vrc = pCodec->Ops.pfnInit(pCodec);

    if (RT_SUCCESS(vrc))
    {
        pCodec->Parms.enmType       = RECORDINGCODECTYPE_VIDEO;
        pCodec->Parms.enmVideoCodec = RecordingVideoCodec_VP8; /** @todo No VP9 yet. */
    }
    else
        LogRel(("Recording: Error initializing video codec (%Rrc)\n", vrc));

    return vrc;
}

#ifdef VBOX_WITH_AUDIO_RECORDING
/**
 * Lets an audio codec parse advanced options given from a string.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec instance to parse options for.
 * @param   strOptions          Options string to parse.
 */
static DECLCALLBACK(int) recordingCodecAudioParseOptions(PRECORDINGCODEC pCodec, const Utf8Str &strOptions)
{
    AssertReturn(pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO, VERR_INVALID_PARAMETER);

    size_t pos = 0;
    com::Utf8Str key, value;
    while ((pos = strOptions.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("ac_profile", com::Utf8Str::CaseInsensitive) == 0)
        {
            if (value.compare("low", com::Utf8Str::CaseInsensitive) == 0)
            {
                PDMAudioPropsInit(&pCodec->Parms.Audio.PCMProps, 16, true /* fSigned */, 1 /* Channels */, 8000 /* Hz */);
            }
            else if (value.startsWith("med" /* "med[ium]" */, com::Utf8Str::CaseInsensitive) == 0)
            {
                /* Stay with the defaults. */
            }
            else if (value.compare("high", com::Utf8Str::CaseInsensitive) == 0)
            {
                PDMAudioPropsInit(&pCodec->Parms.Audio.PCMProps, 16, true /* fSigned */, 2 /* Channels */, 48000 /* Hz */);
            }
        }
        else
            LogRel(("Recording: Unknown option '%s' (value '%s'), skipping\n", key.c_str(), value.c_str()));

    } /* while */

    return VINF_SUCCESS;
}
#endif

/**
 * Creates an audio codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec instance to create.
 * @param   enmAudioCodec       Audio codec to create.
 */
int recordingCodecCreateAudio(PRECORDINGCODEC pCodec, RecordingAudioCodec_T enmAudioCodec)
{
    int vrc;

    switch (enmAudioCodec)
    {
# ifdef VBOX_WITH_LIBOPUS
        case RecordingAudioCodec_Opus:
        {
            pCodec->Ops.pfnInit         = recordingCodecOpusInit;
            pCodec->Ops.pfnDestroy      = recordingCodecOpusDestroy;
            pCodec->Ops.pfnParseOptions = recordingCodecAudioParseOptions;
            pCodec->Ops.pfnEncode       = recordingCodecOpusEncode;

            vrc = VINF_SUCCESS;
            break;
        }
# endif /* VBOX_WITH_LIBOPUS */

# ifdef VBOX_WITH_LIBVORBIS
        case RecordingAudioCodec_OggVorbis:
        {
            pCodec->Ops.pfnInit         = recordingCodecVorbisInit;
            pCodec->Ops.pfnDestroy      = recordingCodecVorbisDestroy;
            pCodec->Ops.pfnParseOptions = recordingCodecAudioParseOptions;
            pCodec->Ops.pfnEncode       = recordingCodecVorbisEncode;
            pCodec->Ops.pfnFinalize     = recordingCodecVorbisFinalize;

            vrc = VINF_SUCCESS;
            break;
        }
# endif /* VBOX_WITH_LIBVORBIS */

        default:
            vrc = VERR_RECORDING_CODEC_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(vrc))
    {
        pCodec->Parms.enmType       = RECORDINGCODECTYPE_AUDIO;
        pCodec->Parms.enmAudioCodec = enmAudioCodec;
    }

    return vrc;
}

/**
 * Creates a video codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec instance to create.
 * @param   enmVideoCodec       Video codec to create.
 */
int recordingCodecCreateVideo(PRECORDINGCODEC pCodec, RecordingVideoCodec_T enmVideoCodec)
{
    int vrc;

    switch (enmVideoCodec)
    {
# ifdef VBOX_WITH_LIBVPX
        case RecordingVideoCodec_VP8:
        {
            pCodec->Ops.pfnInit         = recordingCodecVPXInit;
            pCodec->Ops.pfnDestroy      = recordingCodecVPXDestroy;
            pCodec->Ops.pfnParseOptions = recordingCodecVPXParseOptions;
            pCodec->Ops.pfnEncode       = recordingCodecVPXEncode;

            vrc = VINF_SUCCESS;
            break;
        }
# endif /* VBOX_WITH_LIBVPX */

        default:
            vrc = VERR_RECORDING_CODEC_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(vrc))
    {
        pCodec->Parms.enmType       = RECORDINGCODECTYPE_VIDEO;
        pCodec->Parms.enmVideoCodec = enmVideoCodec;

#ifdef VBOX_WITH_STATISTICS
        pCodec->Stats.cEncBlocks = 0;
        pCodec->Stats.msEncTotal = 0;
#endif
    }

    return vrc;
}

/**
 * Initializes a codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to initialize.
 * @param   pCallbacks          Codec callback table to use. Optional and may be NULL.
 * @param   Settings            Settings to use for initializing the codec.
 */
int recordingCodecInit(const PRECORDINGCODEC pCodec, const PRECORDINGCODECCALLBACKS pCallbacks, const settings::RecordingScreenSettings &Settings)
{
    int vrc;
    if (pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO)
        vrc = recordingCodecInitAudio(pCodec, pCallbacks, Settings);
    else if (pCodec->Parms.enmType == RECORDINGCODECTYPE_VIDEO)
        vrc = recordingCodecInitVideo(pCodec, pCallbacks, Settings);
    else
        AssertFailedStmt(vrc = VERR_NOT_SUPPORTED);
    return vrc;
}

/**
 * Destroys an audio codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to destroy.
 */
static int recordingCodecDestroyAudio(PRECORDINGCODEC pCodec)
{
    AssertReturn(pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO, VERR_INVALID_PARAMETER);

    return pCodec->Ops.pfnDestroy(pCodec);
}

/**
 * Destroys a video codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to destroy.
 */
static int recordingCodecDestroyVideo(PRECORDINGCODEC pCodec)
{
    AssertReturn(pCodec->Parms.enmType == RECORDINGCODECTYPE_VIDEO, VERR_INVALID_PARAMETER);

    return pCodec->Ops.pfnDestroy(pCodec);
}

/**
 * Destroys the codec.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to destroy.
 */
int recordingCodecDestroy(PRECORDINGCODEC pCodec)
{
    if (pCodec->Parms.enmType == RECORDINGCODECTYPE_INVALID)
        return VINF_SUCCESS;

    int vrc;

    if (pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO)
        vrc = recordingCodecDestroyAudio(pCodec);
    else if (pCodec->Parms.enmType == RECORDINGCODECTYPE_VIDEO)
        vrc =recordingCodecDestroyVideo(pCodec);
    else
        AssertFailedReturn(VERR_NOT_SUPPORTED);

    if (RT_SUCCESS(vrc))
    {
        pCodec->Parms.enmType       = RECORDINGCODECTYPE_INVALID;
        pCodec->Parms.enmVideoCodec = RecordingVideoCodec_None;
    }

    return vrc;
}

/**
 * Feeds the codec encoder with data to encode.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to use.
 * @param   pFrame              Pointer to frame data to encode.
 * @param   pvDst               Where to store the encoded data on success.
 * @param   cbDst               Size (in bytes) of \a pvDst.
 * @param   pcEncoded           Where to return the number of encoded blocks in \a pvDst on success. Optional.
 * @param   pcbEncoded          Where to return the number of encoded bytes in \a pvDst on success. Optional.
 */
int recordingCodecEncode(PRECORDINGCODEC pCodec,
                         const PRECORDINGFRAME pFrame, void *pvDst, size_t cbDst,
                         size_t *pcEncoded, size_t *pcbEncoded)
{
    AssertPtrReturn(pCodec->Ops.pfnEncode, VERR_NOT_SUPPORTED);

    size_t cEncoded, cbEncoded;
    int vrc = pCodec->Ops.pfnEncode(pCodec, pFrame, pvDst, cbDst, &cEncoded, &cbEncoded);
    if (RT_SUCCESS(vrc))
    {
#ifdef VBOX_WITH_STATISTICS
        pCodec->Stats.cEncBlocks += cEncoded;
        pCodec->Stats.msEncTotal += pCodec->Parms.msFrame * cEncoded;
#endif
        if (pcEncoded)
            *pcEncoded = cEncoded;
        if (pcbEncoded)
            *pcbEncoded = cbEncoded;
    }

    return vrc;
}

/**
 * Tells the codec that has to finalize the stream.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec to finalize stream for.
 */
int recordingCodecFinalize(PRECORDINGCODEC pCodec)
{
    if (pCodec->Ops.pfnFinalize)
        return pCodec->Ops.pfnFinalize(pCodec);
    return VINF_SUCCESS;
}

