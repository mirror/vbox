/* $Id$ */
/** @file
 * Video recording stream code.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_MAIN_DISPLAY
#include "LoggingNew.h"

#include "VideoRec.h"
#include "VideoRecStream.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/err.h>
#include <VBox/com/VirtualBox.h>


/**
 * Retrieves a specific recording stream of a recording context.
 *
 * @returns Pointer to recording stream if found, or NULL if not found.
 * @param   pCtx                Recording context to look up stream for.
 * @param   uScreen             Screen number of recording stream to look up.
 */
PVIDEORECSTREAM videoRecStreamGet(PVIDEORECCONTEXT pCtx, uint32_t uScreen)
{
    AssertPtrReturn(pCtx, NULL);

    PVIDEORECSTREAM pStream;

    try
    {
        pStream = pCtx->vecStreams.at(uScreen);
    }
    catch (std::out_of_range &)
    {
        pStream = NULL;
    }

    return pStream;
}

/**
 * Locks a recording stream.
 *
 * @param   pStream             Recording stream to lock.
 */
void videoRecStreamLock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectEnter(&pStream->CritSect);
    AssertRC(rc);
}

/**
 * Unlocks a locked recording stream.
 *
 * @param   pStream             Recording stream to unlock.
 */
void videoRecStreamUnlock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectLeave(&pStream->CritSect);
    AssertRC(rc);
}

/**
 * Opens a recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to open.
 * @param   pCfg                Recording configuration to use.
 */
int videoRecStreamOpen(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,    VERR_INVALID_POINTER);

    Assert(pStream->enmDst == VIDEORECDEST_INVALID);

    int rc;

    switch (pCfg->enmDst)
    {
        case VIDEORECDEST_FILE:
        {
            Assert(pCfg->File.strName.isNotEmpty());

            char *pszAbsPath = RTPathAbsDup(com::Utf8Str(pCfg->File.strName).c_str());
            AssertPtrReturn(pszAbsPath, VERR_NO_MEMORY);

            RTPathStripSuffix(pszAbsPath);

            char *pszSuff = RTStrDup(".webm");
            if (!pszSuff)
            {
                RTStrFree(pszAbsPath);
                rc = VERR_NO_MEMORY;
                break;
            }

            char *pszFile = NULL;

            if (pCfg->aScreens.size() > 1)
                rc = RTStrAPrintf(&pszFile, "%s-%u%s", pszAbsPath, pStream->uScreenID + 1, pszSuff);
            else
                rc = RTStrAPrintf(&pszFile, "%s%s", pszAbsPath, pszSuff);

            if (RT_SUCCESS(rc))
            {
                uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_WRITE;

                /* Play safe: the file must not exist, overwriting is potentially
                 * hazardous as nothing prevents the user from picking a file name of some
                 * other important file, causing unintentional data loss. */
                fOpen |= RTFILE_O_CREATE;

                RTFILE hFile;
                rc = RTFileOpen(&hFile, pszFile, fOpen);
                if (rc == VERR_ALREADY_EXISTS)
                {
                    RTStrFree(pszFile);
                    pszFile = NULL;

                    RTTIMESPEC ts;
                    RTTimeNow(&ts);
                    RTTIME time;
                    RTTimeExplode(&time, &ts);

                    if (pCfg->aScreens.size() > 1)
                        rc = RTStrAPrintf(&pszFile, "%s-%04d-%02u-%02uT%02u-%02u-%02u-%09uZ-%u%s",
                                          pszAbsPath, time.i32Year, time.u8Month, time.u8MonthDay,
                                          time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond,
                                          pStream->uScreenID + 1, pszSuff);
                    else
                        rc = RTStrAPrintf(&pszFile, "%s-%04d-%02u-%02uT%02u-%02u-%02u-%09uZ%s",
                                          pszAbsPath, time.i32Year, time.u8Month, time.u8MonthDay,
                                          time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond,
                                          pszSuff);

                    if (RT_SUCCESS(rc))
                        rc = RTFileOpen(&hFile, pszFile, fOpen);
                }

                if (RT_SUCCESS(rc))
                {
                    pStream->enmDst       = VIDEORECDEST_FILE;
                    pStream->File.hFile   = hFile;
                    pStream->File.pszFile = pszFile; /* Assign allocated string to our stream's config. */
                }
            }

            RTStrFree(pszSuff);
            RTStrFree(pszAbsPath);

            if (RT_FAILURE(rc))
            {
                LogRel(("VideoRec: Failed to open file '%s' for screen %RU32, rc=%Rrc\n",
                        pszFile ? pszFile : "<Unnamed>", pStream->uScreenID, rc));
                RTStrFree(pszFile);
            }

            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * VideoRec utility function to initialize video recording context.
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to video recording context.
 * @param   uScreen             Screen number to record.
 */
int VideoRecStreamInit(PVIDEORECCONTEXT pCtx, uint32_t uScreen)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    PVIDEORECCFG pCfg = &pCtx->Cfg;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
    if (pCfg->Audio.fEnabled)
    {
        /* Sanity. */
        AssertReturn(pCfg->Audio.uHz,       VERR_INVALID_PARAMETER);
        AssertReturn(pCfg->Audio.cBits,     VERR_INVALID_PARAMETER);
        AssertReturn(pCfg->Audio.cChannels, VERR_INVALID_PARAMETER);
    }
#endif

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (!pStream)
        return VERR_NOT_FOUND;

    int rc = videoRecStreamOpen(pStream, pCfg);
    if (RT_FAILURE(rc))
        return rc;

    pStream->pCtx = pCtx;

    if (pCfg->Video.fEnabled)
        rc = videoRecStreamInitVideo(pStream, pCfg);

    switch (pStream->enmDst)
    {
        case VIDEORECDEST_FILE:
        {
            rc = pStream->File.pWEBM->OpenEx(pStream->File.pszFile, &pStream->File.hFile,
#ifdef VBOX_WITH_AUDIO_VIDEOREC
                                             pCfg->Audio.fEnabled ? WebMWriter::AudioCodec_Opus : WebMWriter::AudioCodec_None,
#else
                                             WebMWriter::AudioCodec_None,
#endif
                                             pCfg->Video.fEnabled ? WebMWriter::VideoCodec_VP8 : WebMWriter::VideoCodec_None);
            if (RT_FAILURE(rc))
            {
                LogRel(("VideoRec: Failed to create the capture output file '%s' (%Rrc)\n", pStream->File.pszFile, rc));
                break;
            }

            const char *pszFile = pStream->File.pszFile;

            if (pCfg->Video.fEnabled)
            {
                rc = pStream->File.pWEBM->AddVideoTrack(pCfg->Video.uWidth, pCfg->Video.uHeight, pCfg->Video.uFPS,
                                                        &pStream->uTrackVideo);
                if (RT_FAILURE(rc))
                {
                    LogRel(("VideoRec: Failed to add video track to output file '%s' (%Rrc)\n", pszFile, rc));
                    break;
                }

                LogRel(("VideoRec: Recording video of screen #%u with %RU32x%RU32 @ %RU32 kbps, %RU32 FPS (track #%RU8)\n",
                        uScreen, pCfg->Video.uWidth, pCfg->Video.uHeight, pCfg->Video.uRate, pCfg->Video.uFPS,
                        pStream->uTrackVideo));
            }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (pCfg->Audio.fEnabled)
            {
                rc = pStream->File.pWEBM->AddAudioTrack(pCfg->Audio.uHz, pCfg->Audio.cChannels, pCfg->Audio.cBits,
                                                        &pStream->uTrackAudio);
                if (RT_FAILURE(rc))
                {
                    LogRel(("VideoRec: Failed to add audio track to output file '%s' (%Rrc)\n", pszFile, rc));
                    break;
                }

                LogRel(("VideoRec: Recording audio in %RU16Hz, %RU8 bit, %RU8 %s (track #%RU8)\n",
                        pCfg->Audio.uHz, pCfg->Audio.cBits, pCfg->Audio.cChannels, pCfg->Audio.cChannels ? "channels" : "channel",
                        pStream->uTrackAudio));
            }
#endif

            if (   pCfg->Video.fEnabled
#ifdef VBOX_WITH_AUDIO_VIDEOREC
                || pCfg->Audio.fEnabled
#endif
               )
            {
                char szWhat[32] = { 0 };
                if (pCfg->Video.fEnabled)
                    RTStrCat(szWhat, sizeof(szWhat), "video");
#ifdef VBOX_WITH_AUDIO_VIDEOREC
                if (pCfg->Audio.fEnabled)
                {
                    if (pCfg->Video.fEnabled)
                        RTStrCat(szWhat, sizeof(szWhat), " + ");
                    RTStrCat(szWhat, sizeof(szWhat), "audio");
                }
#endif
                LogRel(("VideoRec: Recording %s to '%s'\n", szWhat, pszFile));
            }

            break;
        }

        default:
            AssertFailed(); /* Should never happen. */
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = videoRecStreamClose(pStream);
        AssertRC(rc2);
        return rc;
    }

    pStream->fEnabled = true;

    return VINF_SUCCESS;
}

/**
 * Closes a recording stream.
 * Depending on the stream's recording destination, this function closes all associated handles
 * and finalizes recording.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to close.
 *
 */
int videoRecStreamClose(PVIDEORECSTREAM pStream)
{
    int rc = VINF_SUCCESS;

    if (pStream->fEnabled)
    {
        switch (pStream->enmDst)
        {
            case VIDEORECDEST_FILE:
            {
                if (pStream->File.pWEBM)
                    rc = pStream->File.pWEBM->Close();
                break;
            }

            default:
                AssertFailed(); /* Should never happen. */
                break;
        }

        pStream->Blocks.Clear();

        LogRel(("VideoRec: Recording screen #%u stopped\n", pStream->uScreenID));
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("VideoRec: Error stopping recording screen #%u, rc=%Rrc\n", pStream->uScreenID, rc));
        return rc;
    }

    switch (pStream->enmDst)
    {
        case VIDEORECDEST_FILE:
        {
            AssertPtr(pStream->File.pszFile);
            if (RTFileIsValid(pStream->File.hFile))
            {
                rc = RTFileClose(pStream->File.hFile);
                if (RT_SUCCESS(rc))
                {
                    LogRel(("VideoRec: Closed file '%s'\n", pStream->File.pszFile));
                }
                else
                {
                    LogRel(("VideoRec: Error closing file '%s', rc=%Rrc\n", pStream->File.pszFile, rc));
                    break;
                }
            }

            RTStrFree(pStream->File.pszFile);
            pStream->File.pszFile = NULL;

            if (pStream->File.pWEBM)
            {
                delete pStream->File.pWEBM;
                pStream->File.pWEBM = NULL;
            }
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        pStream->enmDst = VIDEORECDEST_INVALID;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Uninitializes a recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to uninitialize.
 */
int videoRecStreamUninit(PVIDEORECSTREAM pStream)
{
    int rc = VINF_SUCCESS;

    if (pStream->pCtx->Cfg.Video.fEnabled)
    {
        int rc2 = videoRecStreamUnitVideo(pStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

/**
 * Uninitializes video recording for a certain recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to uninitialize video recording for.
 */
int videoRecStreamUnitVideo(PVIDEORECSTREAM pStream)
{
#ifdef VBOX_WITH_LIBVPX
    /* At the moment we only have VPX. */
    return videoRecStreamUninitVideoVPX(pStream);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

#ifdef VBOX_WITH_LIBVPX
/**
 * Uninitializes the VPX codec for a certain recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to uninitialize VPX codec for.
 */
int videoRecStreamUninitVideoVPX(PVIDEORECSTREAM pStream)
{
    vpx_img_free(&pStream->Video.Codec.VPX.RawImage);
    vpx_codec_err_t rcv = vpx_codec_destroy(&pStream->Video.Codec.VPX.Ctx);
    Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

    return VINF_SUCCESS;
}
#endif

/**
 * Initializes the video recording for a certain recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to initialize video recording for.
 * @param   pCfg                Video recording configuration to use for initialization.
 */
int videoRecStreamInitVideo(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg)
{
#ifdef VBOX_WITH_LIBVPX
    /* At the moment we only have VPX. */
    return videoRecStreamInitVideoVPX(pStream, pCfg);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

#ifdef VBOX_WITH_LIBVPX
/**
 * Initializes the VPX codec for a certain recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to initialize VPX codec for.
 * @param   pCfg                Video recording configuration to use for initialization.
 */
int videoRecStreamInitVideoVPX(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg)
{
    pStream->Video.uWidth                = pCfg->Video.uWidth;
    pStream->Video.uHeight               = pCfg->Video.uHeight;
    pStream->Video.cFailedEncodingFrames = 0;

    PVIDEORECVIDEOCODEC pVC = &pStream->Video.Codec;

    pStream->Video.uDelayMs = RT_MS_1SEC / pCfg->Video.uFPS;

# ifdef VBOX_WITH_LIBVPX_VP9
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp9_cx();
# else /* Default is using VP8. */
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp8_cx();
# endif

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(pCodecIface, &pVC->VPX.Cfg, 0 /* Reserved */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("VideoRec: Failed to get default config for VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_AVREC_CODEC_INIT_FAILED;
    }

    /* Target bitrate in kilobits per second. */
    pVC->VPX.Cfg.rc_target_bitrate = pCfg->Video.uRate;
    /* Frame width. */
    pVC->VPX.Cfg.g_w = pCfg->Video.uWidth;
    /* Frame height. */
    pVC->VPX.Cfg.g_h = pCfg->Video.uHeight;
    /* 1ms per frame. */
    pVC->VPX.Cfg.g_timebase.num = 1;
    pVC->VPX.Cfg.g_timebase.den = 1000;
    /* Disable multithreading. */
    pVC->VPX.Cfg.g_threads = 0;

    /* Initialize codec. */
    rcv = vpx_codec_enc_init(&pVC->VPX.Ctx, pCodecIface, &pVC->VPX.Cfg, 0 /* Flags */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("VideoRec: Failed to initialize VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_AVREC_CODEC_INIT_FAILED;
    }

    if (!vpx_img_alloc(&pVC->VPX.RawImage, VPX_IMG_FMT_I420, pCfg->Video.uWidth, pCfg->Video.uHeight, 1))
    {
        LogRel(("VideoRec: Failed to allocate image %RU32x%RU32\n", pCfg->Video.uWidth, pCfg->Video.uHeight));
        return VERR_NO_MEMORY;
    }

    /* Save a pointer to the first raw YUV plane. */
    pStream->Video.Codec.VPX.pu8YuvBuf = pVC->VPX.RawImage.planes[0];

    return VINF_SUCCESS;
}
#endif

#ifdef VBOX_WITH_LIBVPX
/**
 * Encodes the source image and write the encoded image to the stream's destination.
 *
 * @returns IPRT status code.
 * @param   pStream             Stream to encode and submit to.
 * @param   uTimeStampMs        Absolute timestamp (PTS) of frame (in ms) to encode.
 * @param   pFrame              Frame to encode and submit.
 */
int videoRecStreamWriteVideoVPX(PVIDEORECSTREAM pStream, uint64_t uTimeStampMs, PVIDEORECVIDEOFRAME pFrame)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pFrame,  VERR_INVALID_POINTER);

    int rc;

    AssertPtr(pStream->pCtx);
    PVIDEORECCFG        pCfg = &pStream->pCtx->Cfg;
    PVIDEORECVIDEOCODEC pVC  = &pStream->Video.Codec;

    /* Presentation Time Stamp (PTS). */
    vpx_codec_pts_t pts = uTimeStampMs;
    vpx_codec_err_t rcv = vpx_codec_encode(&pVC->VPX.Ctx,
                                           &pVC->VPX.RawImage,
                                           pts                                    /* Time stamp */,
                                           pStream->Video.uDelayMs                /* How long to show this frame */,
                                           0                                      /* Flags */,
                                           pCfg->Video.Codec.VPX.uEncoderDeadline /* Quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        if (pStream->Video.cFailedEncodingFrames++ < 64)
        {
            LogRel(("VideoRec: Failed to encode video frame: %s\n", vpx_codec_err_to_string(rcv)));
            return VERR_GENERAL_FAILURE;
        }
    }

    pStream->Video.cFailedEncodingFrames = 0;

    vpx_codec_iter_t iter = NULL;
    rc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pPacket = vpx_codec_get_cx_data(&pVC->VPX.Ctx, &iter);
        if (!pPacket)
            break;

        switch (pPacket->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
            {
                WebMWriter::BlockData_VP8 blockData = { &pVC->VPX.Cfg, pPacket };
                rc = pStream->File.pWEBM->WriteBlock(pStream->uTrackVideo, &blockData, sizeof(blockData));
                break;
            }

            default:
                AssertFailed();
                LogFunc(("Unexpected video packet type %ld\n", pPacket->kind));
                break;
        }
    }

    return rc;
}
#endif /* VBOX_WITH_LIBVPX */

