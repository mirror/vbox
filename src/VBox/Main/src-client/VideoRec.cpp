/* $Id$ */
/** @file
 * Video recording (with optional audio recording) code.
 *
 * This code employs a separate encoding thread per recording context
 * to keep time spent in EMT as short as possible. Each configured VM display
 * is represented by an own recording stream, which in turn has its own rendering
 * queue. Common recording data across all recording streams is kept in a
 * separate queue in the recording context to minimize data duplication and
 * multiplexing overhead in EMT.
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

#include <stdexcept>
#include <vector>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/err.h>
#include <VBox/com/VirtualBox.h>

#include "WebMWriter.h"
#include "VideoRec.h"
#include "VideoRecInternals.h"
#include "VideoRecStream.h"
#include "VideoRecUtils.h"

using namespace com;

#ifdef DEBUG_andy
/** Enables dumping audio / video data for debugging reasons. */
//# define VBOX_VIDEOREC_DUMP
#endif

/**
 * Enumeration for a video recording state.
 */
enum VIDEORECSTS
{
    /** Not initialized. */
    VIDEORECSTS_UNINITIALIZED = 0,
    /** Initialized. */
    VIDEORECSTS_INITIALIZED   = 1,
    /** The usual 32-bit hack. */
    VIDEORECSTS_32BIT_HACK    = 0x7fffffff
};


#ifdef VBOX_VIDEOREC_DUMP
#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint16_t u16Magic;
    uint32_t u32Size;
    uint16_t u16Reserved1;
    uint16_t u16Reserved2;
    uint32_t u32OffBits;
} VIDEORECBMPHDR, *PVIDEORECBMPHDR;
AssertCompileSize(VIDEORECBMPHDR, 14);

typedef struct
{
    uint32_t u32Size;
    uint32_t u32Width;
    uint32_t u32Height;
    uint16_t u16Planes;
    uint16_t u16BitCount;
    uint32_t u32Compression;
    uint32_t u32SizeImage;
    uint32_t u32XPelsPerMeter;
    uint32_t u32YPelsPerMeter;
    uint32_t u32ClrUsed;
    uint32_t u32ClrImportant;
} VIDEORECBMPDIBHDR, *PVIDEORECBMPDIBHDR;
AssertCompileSize(VIDEORECBMPDIBHDR, 40);

#pragma pack(pop)
#endif /* VBOX_VIDEOREC_DUMP */


/**
 * Worker thread for all streams of a video recording context.
 *
 * For video frames, this also does the RGB/YUV conversion and encoding.
 */
static DECLCALLBACK(int) videoRecThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)pvUser;

    /* Signal that we're up and rockin'. */
    RTThreadUserSignal(hThreadSelf);

    LogFunc(("Thread started\n"));

    for (;;)
    {
        int rc = RTSemEventWait(pCtx->WaitEvent, RT_INDEFINITE_WAIT);
        AssertRCBreak(rc);

        /** @todo r=andy This is inefficient -- as we already wake up this thread
         *               for every screen from Main, we here go again (on every wake up) through
         *               all screens.  */
        for (VideoRecStreams::iterator itStream = pCtx->vecStreams.begin(); itStream != pCtx->vecStreams.end(); itStream++)
        {
            PVIDEORECSTREAM pStream = (*itStream);

            videoRecStreamLock(pStream);

            if (!pStream->fEnabled)
            {
                videoRecStreamUnlock(pStream);
                continue;
            }

            VideoRecBlockMap::iterator itBlockStream = pStream->Blocks.Map.begin();
            while (itBlockStream != pStream->Blocks.Map.end())
            {
                const uint64_t        uTimeStampMs = itBlockStream->first;
                      VideoRecBlocks *pBlocks      = itBlockStream->second;

                AssertPtr(pBlocks);

                while (!pBlocks->List.empty())
                {
                    PVIDEORECBLOCK pBlock = pBlocks->List.front();
                    AssertPtr(pBlock);

#ifdef VBOX_WITH_LIBVPX
                    if (pBlock->enmType == VIDEORECBLOCKTYPE_VIDEO)
                    {
                        PVIDEORECVIDEOFRAME pVideoFrame  = (PVIDEORECVIDEOFRAME)pBlock->pvData;

                        rc = videoRecRGBToYUV(pVideoFrame->uPixelFormat,
                                              /* Destination */
                                              pStream->Video.Codec.VPX.pu8YuvBuf, pVideoFrame->uWidth, pVideoFrame->uHeight,
                                              /* Source */
                                              pVideoFrame->pu8RGBBuf, pStream->Video.uWidth, pStream->Video.uHeight);
                        if (RT_SUCCESS(rc))
                            rc = videoRecStreamWriteVideoVPX(pStream, uTimeStampMs, pVideoFrame);
                    }
#endif
                    VideoRecBlockFree(pBlock);
                    pBlock = NULL;

                    pBlocks->List.pop_front();
                }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
                /* As each (enabled) screen has to get the same audio data, look for common (audio) data which needs to be
                 * written to the screen's assigned recording stream. */
                VideoRecBlockMap::iterator itCommon = pCtx->mapBlocksCommon.begin();
                while (itCommon != pCtx->mapBlocksCommon.end())
                {
                    VideoRecBlockList::iterator itBlockCommon = itCommon->second->List.begin();
                    while (itBlockCommon != itCommon->second->List.end())
                    {
                        PVIDEORECBLOCK pBlockCommon = (PVIDEORECBLOCK)(*itBlockCommon);
                        switch (pBlockCommon->enmType)
                        {
                            case VIDEORECBLOCKTYPE_AUDIO:
                            {
                                PVIDEORECAUDIOFRAME pAudioFrame = (PVIDEORECAUDIOFRAME)pBlockCommon->pvData;
                                AssertPtr(pAudioFrame);
                                AssertPtr(pAudioFrame->pvBuf);
                                Assert(pAudioFrame->cbBuf);

                                WebMWriter::BlockData_Opus blockData = { pAudioFrame->pvBuf, pAudioFrame->cbBuf,
                                                                         pBlockCommon->uTimeStampMs };
                                rc = pStream->File.pWEBM->WriteBlock(pStream->uTrackAudio, &blockData, sizeof(blockData));
                                break;
                            }

                            default:
                                AssertFailed();
                                break;
                        }

                        Assert(pBlockCommon->cRefs);
                        if (--pBlockCommon->cRefs == 0)
                        {
                            VideoRecBlockFree(pBlockCommon);
                            itCommon->second->List.erase(itBlockCommon);
                            itBlockCommon = itCommon->second->List.begin();
                        }
                        else
                            ++itBlockCommon;
                    }

                    /* If no entries are left over in the block map, remove it altogether. */
                    if (itCommon->second->List.empty())
                    {
                        delete itCommon->second;
                        pCtx->mapBlocksCommon.erase(itCommon);
                    }

                    itCommon = pCtx->mapBlocksCommon.begin();

                    LogFunc(("Common blocks: %zu\n", pCtx->mapBlocksCommon.size()));
                }
#endif
                ++itBlockStream;
            }

            videoRecStreamUnlock(pStream);
        }

        /* Keep going in case of errors. */

        if (ASMAtomicReadBool(&pCtx->fShutdown))
        {
            LogFunc(("Thread is shutting down ...\n"));
            break;
        }

    } /* for */

    LogFunc(("Thread ended\n"));
    return VINF_SUCCESS;
}

/**
 * Notifies a recording context's encoding thread.
 *
 * @returns IPRT status code.
 * @param   pCtx                Video recording context to notify thread for.
 */
static int videoRecThreadNotify(PVIDEORECCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    return RTSemEventSignal(pCtx->WaitEvent);
}

/**
 * Creates a video recording context.
 *
 * @returns IPRT status code.
 * @param   cScreens            Number of screens to create context for.
 * @param   pVideoRecCfg        Pointer to video recording configuration to use.
 * @param   ppCtx               Pointer to created video recording context on success.
 */
int VideoRecContextCreate(uint32_t cScreens, PVIDEORECCFG pVideoRecCfg, PVIDEORECCONTEXT *ppCtx)
{
    AssertReturn(cScreens,        VERR_INVALID_PARAMETER);
    AssertPtrReturn(pVideoRecCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(ppCtx,        VERR_INVALID_POINTER);

    VIDEORECCONTEXT *pCtx = NULL;
    try
    {
        pCtx = new VIDEORECCONTEXT();
    }
    catch (std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }

    int rc = RTCritSectInit(&pCtx->CritSect);
    if (RT_FAILURE(rc))
    {
        delete pCtx;
        return rc;
    }

    for (uint32_t uScreen = 0; uScreen < cScreens; uScreen++)
    {
        VIDEORECSTREAM *pStream = NULL;
        try
        {
            pStream = new VIDEORECSTREAM();
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = RTCritSectInit(&pStream->CritSect);
        if (RT_FAILURE(rc))
            break;

        try
        {
            pStream->uScreenID = uScreen;

            pCtx->vecStreams.push_back(pStream);

            pStream->File.pWEBM = new WebMWriter();
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pCtx->tsStartMs = RTTimeMilliTS();
        pCtx->enmState  = VIDEORECSTS_UNINITIALIZED;
        pCtx->fStarted  = false;
        pCtx->fShutdown = false;

        /* Copy the configuration to our context. */
        pCtx->Cfg       = *pVideoRecCfg;

        rc = RTSemEventCreate(&pCtx->WaitEvent);
        AssertRCReturn(rc, rc);

        rc = RTThreadCreate(&pCtx->Thread, videoRecThread, (void *)pCtx, 0,
                            RTTHREADTYPE_MAIN_WORKER, RTTHREADFLAGS_WAITABLE, "VideoRec");

        if (RT_SUCCESS(rc)) /* Wait for the thread to start. */
            rc = RTThreadUserWait(pCtx->Thread, 30 * RT_MS_1SEC /* 30s timeout */);

        if (RT_SUCCESS(rc))
        {
            pCtx->enmState = VIDEORECSTS_INITIALIZED;
            pCtx->fStarted = true;

            if (ppCtx)
                *ppCtx = pCtx;
        }
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = VideoRecContextDestroy(pCtx);
        AssertRC(rc2);
    }

    return rc;
}

/**
 * Destroys a video recording context.
 *
 * @param pCtx                  Video recording context to destroy.
 */
int VideoRecContextDestroy(PVIDEORECCONTEXT pCtx)
{
    if (!pCtx)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (pCtx->enmState == VIDEORECSTS_INITIALIZED)
    {
        LogFunc(("Shutting down thread ...\n"));

        /* Set shutdown indicator. */
        ASMAtomicWriteBool(&pCtx->fShutdown, true);

        /* Signal the thread and wait for it to shut down. */
        rc = videoRecThreadNotify(pCtx);
        if (RT_SUCCESS(rc))
            rc = RTThreadWait(pCtx->Thread, 10 * 1000 /* 10s timeout */, NULL);

        if (RT_SUCCESS(rc))
        {
            /* Disable the context. */
            ASMAtomicWriteBool(&pCtx->fStarted, false);

            int rc2 = RTSemEventDestroy(pCtx->WaitEvent);
            AssertRC(rc2);

            pCtx->WaitEvent = NIL_RTSEMEVENT;
        }
    }

    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

    rc = RTCritSectEnter(&pCtx->CritSect);
    if (RT_SUCCESS(rc))
    {
        VideoRecStreams::iterator it = pCtx->vecStreams.begin();
        while (it != pCtx->vecStreams.end())
        {
            PVIDEORECSTREAM pStream = (*it);

            videoRecStreamLock(pStream);

            int rc2 = videoRecStreamClose(pStream);
            if (RT_SUCCESS(rc))
                rc = rc2;

            rc2 = videoRecStreamUninit(pStream);
            if (RT_SUCCESS(rc))
                rc = rc2;

            pCtx->vecStreams.erase(it);
            it = pCtx->vecStreams.begin();

            videoRecStreamUnlock(pStream);

            RTCritSectDelete(&pStream->CritSect);

            delete pStream;
            pStream = NULL;
        }

        /* Sanity. */
        Assert(pCtx->vecStreams.empty());
        Assert(pCtx->mapBlocksCommon.size() == 0);

        int rc2 = RTCritSectLeave(&pCtx->CritSect);
        AssertRC(rc2);

        RTCritSectDelete(&pCtx->CritSect);

        delete pCtx;
        pCtx = NULL;
    }

    return rc;
}

/**
 * Returns which recording features currently are enabled for a given configuration.
 *
 * @returns Enabled video recording features.
 * @param   pCfg                Pointer to recording configuration.
 */
VIDEORECFEATURES VideoRecGetFeatures(PVIDEORECCFG pCfg)
{
    if (!pCfg)
        return VIDEORECFEATURE_NONE;

    VIDEORECFEATURES fFeatures = VIDEORECFEATURE_NONE;

    if (pCfg->Video.fEnabled)
        fFeatures |= VIDEORECFEATURE_VIDEO;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
    if (pCfg->Audio.fEnabled)
        fFeatures |= VIDEORECFEATURE_AUDIO;
#endif

    return fFeatures;
}

/**
 * Checks if recording engine is ready to accept new recording data for a given screen.
 *
 * @returns true if recording engine is ready, false if not.
 * @param   pCtx                Pointer to video recording context.
 * @param   uScreen             Screen ID.
 * @param   uTimeStampMs        Current time stamp (in ms). Currently not being used.
 */
bool VideoRecIsReady(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t uTimeStampMs)
{
    AssertPtrReturn(pCtx, false);
    RT_NOREF(uTimeStampMs);

    if (ASMAtomicReadU32(&pCtx->enmState) != VIDEORECSTS_INITIALIZED)
        return false;

    bool fIsReady = false;

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (pStream)
    {
        videoRecStreamLock(pStream);
        fIsReady = pStream->fEnabled;
        videoRecStreamUnlock(pStream);
    }

    /* Note: Do not check for other constraints like the video FPS rate here,
     *       as this check then also would affect other (non-FPS related) stuff
     *       like audio data. */

    return fIsReady;
}

/**
 * Returns whether a given recording context has been started or not.
 *
 * @returns true if active, false if not.
 * @param   pCtx                 Pointer to video recording context.
 */
bool VideoRecIsStarted(PVIDEORECCONTEXT pCtx)
{
    if (!pCtx)
        return false;

    return ASMAtomicReadBool(&pCtx->fStarted);
}

/**
 * Checks if a specified limit for recording has been reached.
 *
 * @returns true if any limit has been reached.
 * @param   pCtx                Pointer to video recording context.
 * @param   uScreen             Screen ID.
 * @param   tsNowMs             Current time stamp (in ms).
 */
bool VideoRecIsLimitReached(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t tsNowMs)
{
    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (   !pStream
        || !pStream->fEnabled)
    {
        return false;
    }

    const PVIDEORECCFG pCfg = &pCtx->Cfg;

    if (   pCfg->uMaxTimeS
        && tsNowMs >= pCtx->tsStartMs + (pCfg->uMaxTimeS * RT_MS_1SEC))
    {
        return true;
    }

    if (pCfg->enmDst == VIDEORECDEST_FILE)
    {

        if (pCfg->File.uMaxSizeMB)
        {
            uint64_t sizeInMB = pStream->File.pWEBM->GetFileSize() / _1M;
            if(sizeInMB >= pCfg->File.uMaxSizeMB)
                return true;
        }

        /* Check for available free disk space */
        if (   pStream->File.pWEBM
            && pStream->File.pWEBM->GetAvailableSpace() < 0x100000) /** @todo r=andy WTF? Fix this. */
        {
            LogRel(("VideoRec: Not enough free storage space available, stopping video capture\n"));
            return true;
        }
    }

    return false;
}

/**
 * Sends an audio frame to the video encoding thread.
 *
 * @thread  EMT
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to the video recording context.
 * @param   pvData              Audio frame data to send.
 * @param   cbData              Size (in bytes) of (encoded) audio frame data.
 * @param   uTimeStampMs        Time stamp (in ms) of audio playback.
 */
int VideoRecSendAudioFrame(PVIDEORECCONTEXT pCtx, const void *pvData, size_t cbData, uint64_t uTimeStampMs)
{
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    /* To save time spent in EMT, do the required audio multiplexing in the encoding thread.
     *
     * The multiplexing is needed to supply all recorded (enabled) screens with the same
     * audio data at the same given point in time.
     */
    PVIDEORECBLOCK pBlock = (PVIDEORECBLOCK)RTMemAlloc(sizeof(VIDEORECBLOCK));
    AssertPtrReturn(pBlock, VERR_NO_MEMORY);
    pBlock->enmType = VIDEORECBLOCKTYPE_AUDIO;

    PVIDEORECAUDIOFRAME pFrame = (PVIDEORECAUDIOFRAME)RTMemAlloc(sizeof(VIDEORECAUDIOFRAME));
    AssertPtrReturn(pFrame, VERR_NO_MEMORY);

    pFrame->pvBuf = (uint8_t *)RTMemAlloc(cbData);
    AssertPtrReturn(pFrame->pvBuf, VERR_NO_MEMORY);
    pFrame->cbBuf = cbData;

    memcpy(pFrame->pvBuf, pvData, cbData);

    pBlock->pvData       = pFrame;
    pBlock->cbData       = sizeof(VIDEORECAUDIOFRAME) + cbData;
    pBlock->cRefs        = (uint16_t)pCtx->vecStreams.size(); /* All streams need the same audio data. */
    pBlock->uTimeStampMs = uTimeStampMs;

    int rc = RTCritSectEnter(&pCtx->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    try
    {
        VideoRecBlockMap::iterator itBlocks = pCtx->mapBlocksCommon.find(uTimeStampMs);
        if (itBlocks == pCtx->mapBlocksCommon.end())
        {
            VideoRecBlocks *pVideoRecBlocks = new VideoRecBlocks();
            pVideoRecBlocks->List.push_back(pBlock);

            pCtx->mapBlocksCommon.insert(std::make_pair(uTimeStampMs, pVideoRecBlocks));
        }
        else
            itBlocks->second->List.push_back(pBlock);
    }
    catch (const std::exception &ex)
    {
        RT_NOREF(ex);
        rc = VERR_NO_MEMORY;
    }

    int rc2 = RTCritSectLeave(&pCtx->CritSect);
    AssertRC(rc2);

    if (RT_SUCCESS(rc))
        rc = videoRecThreadNotify(pCtx);

    return rc;
#else
    RT_NOREF(pCtx, pvData, cbData, uTimeStampMs);
    return VINF_SUCCESS;
#endif
}

/**
 * Copies a source video frame to the intermediate RGB buffer.
 * This function is executed only once per time.
 *
 * @thread  EMT
 *
 * @returns IPRT status code.
 * @param   pCtx               Pointer to the video recording context.
 * @param   uScreen            Screen number.
 * @param   x                  Starting x coordinate of the video frame.
 * @param   y                  Starting y coordinate of the video frame.
 * @param   uPixelFormat       Pixel format.
 * @param   uBPP               Bits Per Pixel (BPP).
 * @param   uBytesPerLine      Bytes per scanline.
 * @param   uSrcWidth          Width of the video frame.
 * @param   uSrcHeight         Height of the video frame.
 * @param   puSrcData          Pointer to video frame data.
 * @param   uTimeStampMs       Time stamp (in ms).
 */
int VideoRecSendVideoFrame(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint32_t x, uint32_t y,
                           uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                           uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData,
                           uint64_t uTimeStampMs)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertReturn(uSrcWidth,  VERR_INVALID_PARAMETER);
    AssertReturn(uSrcHeight, VERR_INVALID_PARAMETER);
    AssertReturn(puSrcData,  VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pCtx->CritSect);
    AssertRC(rc);

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (!pStream)
    {
        rc = RTCritSectLeave(&pCtx->CritSect);
        AssertRC(rc);

        return VERR_NOT_FOUND;
    }

    videoRecStreamLock(pStream);

    PVIDEORECVIDEOFRAME pFrame = NULL;

    do
    {
        if (!pStream->fEnabled)
        {
            rc = VINF_TRY_AGAIN; /* Not (yet) enabled. */
            break;
        }

        if (uTimeStampMs < pStream->Video.uLastTimeStampMs + pStream->Video.uDelayMs)
        {
            rc = VINF_TRY_AGAIN; /* Respect maximum frames per second. */
            break;
        }

        pStream->Video.uLastTimeStampMs = uTimeStampMs;

        int xDiff = ((int)pStream->Video.uWidth - (int)uSrcWidth) / 2;
        uint32_t w = uSrcWidth;
        if ((int)w + xDiff + (int)x <= 0)  /* Nothing visible. */
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        uint32_t destX;
        if ((int)x < -xDiff)
        {
            w += xDiff + x;
            x = -xDiff;
            destX = 0;
        }
        else
            destX = x + xDiff;

        uint32_t h = uSrcHeight;
        int yDiff = ((int)pStream->Video.uHeight - (int)uSrcHeight) / 2;
        if ((int)h + yDiff + (int)y <= 0)  /* Nothing visible. */
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        uint32_t destY;
        if ((int)y < -yDiff)
        {
            h += yDiff + (int)y;
            y = -yDiff;
            destY = 0;
        }
        else
            destY = y + yDiff;

        if (   destX > pStream->Video.uWidth
            || destY > pStream->Video.uHeight)
        {
            rc = VERR_INVALID_PARAMETER;  /* Nothing visible. */
            break;
        }

        if (destX + w > pStream->Video.uWidth)
            w = pStream->Video.uWidth - destX;

        if (destY + h > pStream->Video.uHeight)
            h = pStream->Video.uHeight - destY;

        pFrame = (PVIDEORECVIDEOFRAME)RTMemAllocZ(sizeof(VIDEORECVIDEOFRAME));
        AssertBreakStmt(pFrame, rc = VERR_NO_MEMORY);

        /* Calculate bytes per pixel and set pixel format. */
        const unsigned uBytesPerPixel = uBPP / 8;
        if (uPixelFormat == BitmapFormat_BGR)
        {
            switch (uBPP)
            {
                case 32:
                    pFrame->uPixelFormat = VIDEORECPIXELFMT_RGB32;
                    break;
                case 24:
                    pFrame->uPixelFormat = VIDEORECPIXELFMT_RGB24;
                    break;
                case 16:
                    pFrame->uPixelFormat = VIDEORECPIXELFMT_RGB565;
                    break;
                default:
                    AssertMsgFailed(("Unknown color depth (%RU32)\n", uBPP));
                    break;
            }
        }
        else
            AssertMsgFailed(("Unknown pixel format (%RU32)\n", uPixelFormat));

        const size_t cbRGBBuf =   pStream->Video.uWidth
                                * pStream->Video.uHeight
                                * uBytesPerPixel;
        AssertBreakStmt(cbRGBBuf, rc = VERR_INVALID_PARAMETER);

        pFrame->pu8RGBBuf = (uint8_t *)RTMemAlloc(cbRGBBuf);
        AssertBreakStmt(pFrame->pu8RGBBuf, rc = VERR_NO_MEMORY);
        pFrame->cbRGBBuf  = cbRGBBuf;
        pFrame->uWidth    = uSrcWidth;
        pFrame->uHeight   = uSrcHeight;

        /* If the current video frame is smaller than video resolution we're going to encode,
         * clear the frame beforehand to prevent artifacts. */
        if (   uSrcWidth  < pStream->Video.uWidth
            || uSrcHeight < pStream->Video.uHeight)
        {
            RT_BZERO(pFrame->pu8RGBBuf, pFrame->cbRGBBuf);
        }

        /* Calculate start offset in source and destination buffers. */
        uint32_t offSrc = y * uBytesPerLine + x * uBytesPerPixel;
        uint32_t offDst = (destY * pStream->Video.uWidth + destX) * uBytesPerPixel;

#ifdef VBOX_VIDEOREC_DUMP
        VIDEORECBMPHDR bmpHdr;
        RT_ZERO(bmpHdr);

        VIDEORECBMPDIBHDR bmpDIBHdr;
        RT_ZERO(bmpDIBHdr);

        bmpHdr.u16Magic   = 0x4d42; /* Magic */
        bmpHdr.u32Size    = (uint32_t)(sizeof(VIDEORECBMPHDR) + sizeof(VIDEORECBMPDIBHDR) + (w * h * uBytesPerPixel));
        bmpHdr.u32OffBits = (uint32_t)(sizeof(VIDEORECBMPHDR) + sizeof(VIDEORECBMPDIBHDR));

        bmpDIBHdr.u32Size          = sizeof(VIDEORECBMPDIBHDR);
        bmpDIBHdr.u32Width         = w;
        bmpDIBHdr.u32Height        = h;
        bmpDIBHdr.u16Planes        = 1;
        bmpDIBHdr.u16BitCount      = uBPP;
        bmpDIBHdr.u32XPelsPerMeter = 5000;
        bmpDIBHdr.u32YPelsPerMeter = 5000;

        char szFileName[RTPATH_MAX];
        RTStrPrintf2(szFileName, sizeof(szFileName), "/tmp/VideoRecFrame-%RU32.bmp", uScreen);

        RTFILE fh;
        int rc2 = RTFileOpen(&fh, szFileName,
                             RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc2))
        {
            RTFileWrite(fh, &bmpHdr,    sizeof(bmpHdr),    NULL);
            RTFileWrite(fh, &bmpDIBHdr, sizeof(bmpDIBHdr), NULL);
        }
#endif
        Assert(pFrame->cbRGBBuf >= w * h * uBytesPerPixel);

        /* Do the copy. */
        for (unsigned int i = 0; i < h; i++)
        {
            /* Overflow check. */
            Assert(offSrc + w * uBytesPerPixel <= uSrcHeight * uBytesPerLine);
            Assert(offDst + w * uBytesPerPixel <= pStream->Video.uHeight * pStream->Video.uWidth * uBytesPerPixel);

            memcpy(pFrame->pu8RGBBuf + offDst, puSrcData + offSrc, w * uBytesPerPixel);

#ifdef VBOX_VIDEOREC_DUMP
            if (RT_SUCCESS(rc2))
                RTFileWrite(fh, pFrame->pu8RGBBuf + offDst, w * uBytesPerPixel, NULL);
#endif
            offSrc += uBytesPerLine;
            offDst += pStream->Video.uWidth * uBytesPerPixel;
        }

#ifdef VBOX_VIDEOREC_DUMP
        if (RT_SUCCESS(rc2))
            RTFileClose(fh);
#endif

    } while (0);

    if (rc == VINF_SUCCESS) /* Note: Also could be VINF_TRY_AGAIN. */
    {
        PVIDEORECBLOCK pBlock = (PVIDEORECBLOCK)RTMemAlloc(sizeof(VIDEORECBLOCK));
        if (pBlock)
        {
            AssertPtr(pFrame);

            pBlock->enmType = VIDEORECBLOCKTYPE_VIDEO;
            pBlock->pvData  = pFrame;
            pBlock->cbData  = sizeof(VIDEORECVIDEOFRAME) + pFrame->cbRGBBuf;

            try
            {
                VideoRecBlocks *pVideoRecBlocks = new VideoRecBlocks();
                pVideoRecBlocks->List.push_back(pBlock);

                Assert(pStream->Blocks.Map.find(uTimeStampMs) == pStream->Blocks.Map.end());
                pStream->Blocks.Map.insert(std::make_pair(uTimeStampMs, pVideoRecBlocks));
            }
            catch (const std::exception &ex)
            {
                RT_NOREF(ex);

                RTMemFree(pBlock);
                rc = VERR_NO_MEMORY;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        VideoRecVideoFrameFree(pFrame);

    videoRecStreamUnlock(pStream);

    int rc2 = RTCritSectLeave(&pCtx->CritSect);
    AssertRC(rc2);

    if (   RT_SUCCESS(rc)
        && rc != VINF_TRY_AGAIN) /* Only signal the thread if operation was successful. */
    {
        videoRecThreadNotify(pCtx);
    }

    return rc;
}

