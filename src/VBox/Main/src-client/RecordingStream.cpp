/* $Id$ */
/** @file
 * Recording stream code.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_RECORDING
#include "LoggingNew.h"

#include <iprt/path.h>

#ifdef VBOX_RECORDING_DUMP
# include <iprt/formats/bmp.h>
#endif

#ifdef VBOX_WITH_AUDIO_RECORDING
# include <VBox/vmm/pdmaudioinline.h>
#endif

#include "Recording.h"
#include "RecordingUtils.h"
#include "WebMWriter.h"


RecordingStream::RecordingStream(RecordingContext *a_pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings)
    : enmState(RECORDINGSTREAMSTATE_UNINITIALIZED)
{
    int vrc2 = initInternal(a_pCtx, uScreen, Settings);
    if (RT_FAILURE(vrc2))
        throw vrc2;
}

RecordingStream::~RecordingStream(void)
{
    int vrc2 = uninitInternal();
    AssertRC(vrc2);
}

/**
 * Opens a recording stream.
 *
 * @returns IPRT status code.
 * @param   screenSettings      Recording settings to use.
 */
int RecordingStream::open(const settings::RecordingScreenSettings &screenSettings)
{
    /* Sanity. */
    Assert(screenSettings.enmDest != RecordingDestination_None);

    int vrc;

    switch (screenSettings.enmDest)
    {
        case RecordingDestination_File:
        {
            Assert(screenSettings.File.strName.isNotEmpty());

            char *pszAbsPath = RTPathAbsDup(screenSettings.File.strName.c_str());
            AssertPtrReturn(pszAbsPath, VERR_NO_MEMORY);

            RTPathStripSuffix(pszAbsPath);

            char *pszSuff = RTStrDup(".webm");
            if (!pszSuff)
            {
                RTStrFree(pszAbsPath);
                vrc = VERR_NO_MEMORY;
                break;
            }

            char *pszFile = NULL;

            vrc = RTStrAPrintf(&pszFile, "%s-%u%s", pszAbsPath, this->uScreenID, pszSuff);
            if (RT_SUCCESS(vrc))
            {
#ifdef DEBUG_andy
                uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE;
#else
                uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_WRITE;

                /* Play safe: the file must not exist, overwriting is potentially
                 * hazardous as nothing prevents the user from picking a file name of some
                 * other important file, causing unintentional data loss. */
                fOpen |= RTFILE_O_CREATE;
#endif
                RTFILE hFile;
                vrc = RTFileOpen(&hFile, pszFile, fOpen);
                if (vrc == VERR_ALREADY_EXISTS)
                {
                    RTStrFree(pszFile);
                    pszFile = NULL;

                    RTTIMESPEC ts;
                    RTTimeNow(&ts);
                    RTTIME time;
                    RTTimeExplode(&time, &ts);

                    vrc = RTStrAPrintf(&pszFile, "%s-%04d-%02u-%02uT%02u-%02u-%02u-%09uZ-%u%s",
                                       pszAbsPath, time.i32Year, time.u8Month, time.u8MonthDay,
                                       time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond,
                                       this->uScreenID, pszSuff);
                    if (RT_SUCCESS(vrc))
                        vrc = RTFileOpen(&hFile, pszFile, fOpen);
                }

                if (RT_FAILURE(vrc))
                    break;

                LogRel2(("Recording: Opened file '%s'\n", pszFile));

                try
                {
                    Assert(File.pWEBM == NULL);
                    File.pWEBM = new WebMWriter();
                }
                catch (std::bad_alloc &)
                {
                    vrc = VERR_NO_MEMORY;
                }

                if (RT_SUCCESS(vrc))
                {
                    this->File.hFile = hFile;
                    this->ScreenSettings.File.strName = pszFile;
                }
            }

            RTStrFree(pszSuff);
            RTStrFree(pszAbsPath);

            if (RT_FAILURE(vrc))
            {
                LogRel(("Recording: Failed to open file '%s' for screen %RU32, vrc=%Rrc\n",
                        pszFile ? pszFile : "<Unnamed>", this->uScreenID, vrc));
            }

            RTStrFree(pszFile);
            break;
        }

        default:
            vrc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Returns the recording stream's used configuration.
 *
 * @returns The recording stream's used configuration.
 */
const settings::RecordingScreenSettings &RecordingStream::GetConfig(void) const
{
    return this->ScreenSettings;
}

/**
 * Checks if a specified limit for a recording stream has been reached, internal version.
 *
 * @returns true if any limit has been reached.
 * @param   msTimestamp     Timestamp (in ms) to check for.
 */
bool RecordingStream::isLimitReachedInternal(uint64_t msTimestamp) const
{
    LogFlowThisFunc(("msTimestamp=%RU64, ulMaxTimeS=%RU32, tsStartMs=%RU64\n",
                     msTimestamp, this->ScreenSettings.ulMaxTimeS, this->tsStartMs));

    if (   this->ScreenSettings.ulMaxTimeS
        && msTimestamp >= this->tsStartMs + (this->ScreenSettings.ulMaxTimeS * RT_MS_1SEC))
    {
        LogRel(("Recording: Time limit for stream #%RU16 has been reached (%RU32s)\n",
                this->uScreenID, this->ScreenSettings.ulMaxTimeS));
        return true;
    }

    if (this->ScreenSettings.enmDest == RecordingDestination_File)
    {
        if (this->ScreenSettings.File.ulMaxSizeMB)
        {
            uint64_t sizeInMB = this->File.pWEBM->GetFileSize() / _1M;
            if(sizeInMB >= this->ScreenSettings.File.ulMaxSizeMB)
            {
                LogRel(("Recording: File size limit for stream #%RU16 has been reached (%RU64MB)\n",
                        this->uScreenID, this->ScreenSettings.File.ulMaxSizeMB));
                return true;
            }
        }

        /* Check for available free disk space */
        if (   this->File.pWEBM
            && this->File.pWEBM->GetAvailableSpace() < 0x100000) /** @todo r=andy WTF? Fix this. */
        {
            LogRel(("Recording: Not enough free storage space available, stopping recording\n"));
            return true;
        }
    }

    return false;
}

/**
 * Internal iteration main loop.
 * Does housekeeping and recording context notification.
 *
 * @returns IPRT status code.
 * @param   msTimestamp         Current timestamp (in ms).
 */
int RecordingStream::iterateInternal(uint64_t msTimestamp)
{
    if (!this->fEnabled)
        return VINF_SUCCESS;

    int vrc;

    if (isLimitReachedInternal(msTimestamp))
    {
        vrc = VINF_RECORDING_LIMIT_REACHED;
    }
    else
        vrc = VINF_SUCCESS;

    AssertPtr(this->m_pCtx);

    switch (vrc)
    {
        case VINF_RECORDING_LIMIT_REACHED:
        {
            this->fEnabled = false;

            int vrc2 = this->m_pCtx->OnLimitReached(this->uScreenID, VINF_SUCCESS /* rc */);
            AssertRC(vrc2);
            break;
        }

        default:
            break;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Checks if a specified limit for a recording stream has been reached.
 *
 * @returns true if any limit has been reached.
 * @param   msTimestamp         Timestamp (in ms) to check for.
 */
bool RecordingStream::IsLimitReached(uint64_t msTimestamp) const
{
    if (!IsReady())
        return true;

    return isLimitReachedInternal(msTimestamp);
}

/**
 * Returns whether a recording stream is ready (e.g. enabled and active) or not.
 *
 * @returns \c true if ready, \c false if not.
 */
bool RecordingStream::IsReady(void) const
{
    return this->fEnabled;
}

/**
 * Processes a recording stream.
 * This function takes care of the actual encoding and writing of a certain stream.
 * As this can be very CPU intensive, this function usually is called from a separate thread.
 *
 * @returns IPRT status code.
 * @param   mapBlocksCommon     Map of common block to process for this stream.
 *
 * @note    Runs in recording thread.
 */
int RecordingStream::Process(RecordingBlockMap &mapBlocksCommon)
{
    LogFlowFuncEnter();

    lock();

    if (!this->ScreenSettings.fEnabled)
    {
        unlock();
        return VINF_SUCCESS;
    }

    int vrc = VINF_SUCCESS;

    RecordingBlockMap::iterator itStreamBlocks = Blocks.Map.begin();
    while (itStreamBlocks != Blocks.Map.end())
    {
        uint64_t const   msTimestamp = itStreamBlocks->first;
        RecordingBlocks *pBlocks     = itStreamBlocks->second;

        AssertPtr(pBlocks);

        while (!pBlocks->List.empty())
        {
            RecordingBlock *pBlock = pBlocks->List.front();
            AssertPtr(pBlock);

            if (pBlock->enmType == RECORDINGBLOCKTYPE_VIDEO)
            {
                RECORDINGFRAME Frame;
                Frame.VideoPtr    = (PRECORDINGVIDEOFRAME)pBlock->pvData;
                Frame.msTimestamp = msTimestamp;

                int vrc2 = recordingCodecEncode(&this->CodecVideo, &Frame, NULL, NULL);
                AssertRC(vrc2);
                if (RT_SUCCESS(vrc))
                    vrc = vrc2;
            }

            pBlocks->List.pop_front();
            delete pBlock;
        }

        Assert(pBlocks->List.empty());
        delete pBlocks;

        Blocks.Map.erase(itStreamBlocks);
        itStreamBlocks = Blocks.Map.begin();
    }

#ifdef VBOX_WITH_AUDIO_RECORDING
    /* Do we need to multiplex the common audio data to this stream? */
    if (this->ScreenSettings.isFeatureEnabled(RecordingFeature_Audio))
    {
        /* As each (enabled) screen has to get the same audio data, look for common (audio) data which needs to be
         * written to the screen's assigned recording stream. */
        RecordingBlockMap::iterator itCommonBlocks = mapBlocksCommon.begin();
        while (itCommonBlocks != mapBlocksCommon.end())
        {
            RecordingBlockList::iterator itBlock = itCommonBlocks->second->List.begin();
            while (itBlock != itCommonBlocks->second->List.end())
            {
                RecordingBlock *pBlockCommon = (RecordingBlock *)(*itBlock);
                switch (pBlockCommon->enmType)
                {
                    case RECORDINGBLOCKTYPE_AUDIO:
                    {
                        PRECORDINGAUDIOFRAME pAudioFrame = (PRECORDINGAUDIOFRAME)pBlockCommon->pvData;
                        AssertPtr(pAudioFrame);
                        AssertPtr(pAudioFrame->pvBuf);
                        Assert(pAudioFrame->cbBuf);

                        AssertPtr(this->File.pWEBM);
                        int vrc2 = this->File.pWEBM->WriteBlock(this->uTrackAudio, pAudioFrame->pvBuf, pAudioFrame->cbBuf, pBlockCommon->msTimestamp, pBlockCommon->uFlags);
                        AssertRC(vrc2);
                        if (RT_SUCCESS(vrc))
                            vrc = vrc2;
                        break;
                    }

                    default:
                        AssertFailed();
                        break;
                }

                Assert(pBlockCommon->cRefs);
                pBlockCommon->cRefs--;
                if (pBlockCommon->cRefs == 0)
                {
                    itCommonBlocks->second->List.erase(itBlock);
                    delete pBlockCommon;
                    itBlock = itCommonBlocks->second->List.begin();
                }
                else
                    ++itBlock;
            }

            /* If no entries are left over in the block map, remove it altogether. */
            if (itCommonBlocks->second->List.empty())
            {
                delete itCommonBlocks->second;
                mapBlocksCommon.erase(itCommonBlocks);
                itCommonBlocks = mapBlocksCommon.begin();
            }
            else
                ++itCommonBlocks;

            LogFunc(("Common blocks: %zu\n", mapBlocksCommon.size()));
        }
    }
#else
    RT_NOREF(mapBlocksCommon);
#endif /* VBOX_WITH_AUDIO_RECORDING */

    unlock();

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Sends a raw (e.g. not yet encoded) video frame to the recording stream.
 *
 * @returns IPRT status code. Will return VINF_RECORDING_LIMIT_REACHED if the stream's recording
 *          limit has been reached or VINF_RECORDING_THROTTLED if the frame is too early for the current
 *          FPS setting.
 * @param   x                   Upper left (X) coordinate where the video frame starts.
 * @param   y                   Upper left (Y) coordinate where the video frame starts.
 * @param   uPixelFormat        Pixel format of the video frame.
 * @param   uBPP                Bits per pixel (BPP) of the video frame.
 * @param   uBytesPerLine       Bytes per line  of the video frame.
 * @param   uSrcWidth           Width (in pixels) of the video frame.
 * @param   uSrcHeight          Height (in pixels) of the video frame.
 * @param   puSrcData           Actual pixel data of the video frame.
 * @param   msTimestamp         Absolute PTS timestamp (in ms).
 */
int RecordingStream::SendVideoFrame(uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                                    uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData, uint64_t msTimestamp)
{
    lock();

    LogFlowFunc(("tsAbsPTSMs=%RU64\n", msTimestamp));

    PRECORDINGCODEC pCodec = &this->CodecVideo;

    PRECORDINGVIDEOFRAME pFrame = NULL;

    int vrc = iterateInternal(msTimestamp);
    if (vrc != VINF_SUCCESS) /* Can return VINF_RECORDING_LIMIT_REACHED. */
    {
        unlock();
        return vrc;
    }

    do
    {
        if (msTimestamp < pCodec->State.tsLastWrittenMs + pCodec->Parms.Video.uDelayMs)
        {
            vrc = VINF_RECORDING_THROTTLED; /* Respect maximum frames per second. */
            break;
        }

        pCodec->State.tsLastWrittenMs = msTimestamp;

        int xDiff = ((int)this->ScreenSettings.Video.ulWidth - (int)uSrcWidth) / 2;
        uint32_t w = uSrcWidth;
        if ((int)w + xDiff + (int)x <= 0)  /* Nothing visible. */
        {
            vrc = VERR_INVALID_PARAMETER;
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
        int yDiff = ((int)this->ScreenSettings.Video.ulHeight - (int)uSrcHeight) / 2;
        if ((int)h + yDiff + (int)y <= 0)  /* Nothing visible. */
        {
            vrc = VERR_INVALID_PARAMETER;
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

        if (   destX > this->ScreenSettings.Video.ulWidth
            || destY > this->ScreenSettings.Video.ulHeight)
        {
            vrc = VERR_INVALID_PARAMETER;  /* Nothing visible. */
            break;
        }

        if (destX + w > this->ScreenSettings.Video.ulWidth)
            w = this->ScreenSettings.Video.ulWidth - destX;

        if (destY + h > this->ScreenSettings.Video.ulHeight)
            h = this->ScreenSettings.Video.ulHeight - destY;

        pFrame = (PRECORDINGVIDEOFRAME)RTMemAllocZ(sizeof(RECORDINGVIDEOFRAME));
        AssertBreakStmt(pFrame, vrc = VERR_NO_MEMORY);

        /* Calculate bytes per pixel and set pixel format. */
        const unsigned uBytesPerPixel = uBPP / 8;
        if (uPixelFormat == BitmapFormat_BGR)
        {
            switch (uBPP)
            {
                case 32:
                    pFrame->uPixelFormat = RECORDINGPIXELFMT_RGB32;
                    break;
                case 24:
                    pFrame->uPixelFormat = RECORDINGPIXELFMT_RGB24;
                    break;
                case 16:
                    pFrame->uPixelFormat = RECORDINGPIXELFMT_RGB565;
                    break;
                default:
                    AssertMsgFailedBreakStmt(("Unknown color depth (%RU32)\n", uBPP), vrc = VERR_NOT_SUPPORTED);
                    break;
            }
        }
        else
            AssertMsgFailedBreakStmt(("Unknown pixel format (%RU32)\n", uPixelFormat), vrc = VERR_NOT_SUPPORTED);

        const size_t cbRGBBuf =   this->ScreenSettings.Video.ulWidth
                                * this->ScreenSettings.Video.ulHeight
                                * uBytesPerPixel;
        AssertBreakStmt(cbRGBBuf, vrc = VERR_INVALID_PARAMETER);

        pFrame->pu8RGBBuf = (uint8_t *)RTMemAlloc(cbRGBBuf);
        AssertBreakStmt(pFrame->pu8RGBBuf, vrc = VERR_NO_MEMORY);
        pFrame->cbRGBBuf  = cbRGBBuf;
        pFrame->uWidth    = uSrcWidth;
        pFrame->uHeight   = uSrcHeight;

        /* If the current video frame is smaller than video resolution we're going to encode,
         * clear the frame beforehand to prevent artifacts. */
        if (   uSrcWidth  < this->ScreenSettings.Video.ulWidth
            || uSrcHeight < this->ScreenSettings.Video.ulHeight)
        {
            RT_BZERO(pFrame->pu8RGBBuf, pFrame->cbRGBBuf);
        }

        /* Calculate start offset in source and destination buffers. */
        uint32_t offSrc = y * uBytesPerLine + x * uBytesPerPixel;
        uint32_t offDst = (destY * this->ScreenSettings.Video.ulWidth + destX) * uBytesPerPixel;

#ifdef VBOX_RECORDING_DUMP
        BMPFILEHDR fileHdr;
        RT_ZERO(fileHdr);

        BMPWIN3XINFOHDR coreHdr;
        RT_ZERO(coreHdr);

        fileHdr.uType       = BMP_HDR_MAGIC;
        fileHdr.cbFileSize = (uint32_t)(sizeof(BMPFILEHDR) + sizeof(BMPWIN3XINFOHDR) + (w * h * uBytesPerPixel));
        fileHdr.offBits    = (uint32_t)(sizeof(BMPFILEHDR) + sizeof(BMPWIN3XINFOHDR));

        coreHdr.cbSize         = sizeof(BMPWIN3XINFOHDR);
        coreHdr.uWidth         = w;
        coreHdr.uHeight        = h;
        coreHdr.cPlanes        = 1;
        coreHdr.cBits          = uBPP;
        coreHdr.uXPelsPerMeter = 5000;
        coreHdr.uYPelsPerMeter = 5000;

        char szFileName[RTPATH_MAX];
        RTStrPrintf2(szFileName, sizeof(szFileName), "/tmp/VideoRecFrame-%RU32.bmp", this->uScreenID);

        RTFILE fh;
        int vrc2 = RTFileOpen(&fh, szFileName,
                              RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(vrc2))
        {
            RTFileWrite(fh, &fileHdr,    sizeof(fileHdr),    NULL);
            RTFileWrite(fh, &coreHdr, sizeof(coreHdr), NULL);
        }
#endif
        Assert(pFrame->cbRGBBuf >= w * h * uBytesPerPixel);

        /* Do the copy. */
        for (unsigned int i = 0; i < h; i++)
        {
            /* Overflow check. */
            Assert(offSrc + w * uBytesPerPixel <= uSrcHeight * uBytesPerLine);
            Assert(offDst + w * uBytesPerPixel <= this->ScreenSettings.Video.ulHeight * this->ScreenSettings.Video.ulWidth * uBytesPerPixel);

            memcpy(pFrame->pu8RGBBuf + offDst, puSrcData + offSrc, w * uBytesPerPixel);

#ifdef VBOX_RECORDING_DUMP
            if (RT_SUCCESS(rc2))
                RTFileWrite(fh, pFrame->pu8RGBBuf + offDst, w * uBytesPerPixel, NULL);
#endif
            offSrc += uBytesPerLine;
            offDst += this->ScreenSettings.Video.ulWidth * uBytesPerPixel;
        }

#ifdef VBOX_RECORDING_DUMP
        if (RT_SUCCESS(vrc2))
            RTFileClose(fh);
#endif

    } while (0);

    if (vrc == VINF_SUCCESS) /* Note: Also could be VINF_TRY_AGAIN. */
    {
        RecordingBlock *pBlock = new RecordingBlock();
        if (pBlock)
        {
            AssertPtr(pFrame);

            pBlock->enmType = RECORDINGBLOCKTYPE_VIDEO;
            pBlock->pvData  = pFrame;
            pBlock->cbData  = sizeof(RECORDINGVIDEOFRAME) + pFrame->cbRGBBuf;

            try
            {
                RecordingBlocks *pRecordingBlocks = new RecordingBlocks();
                pRecordingBlocks->List.push_back(pBlock);

                Assert(this->Blocks.Map.find(msTimestamp) == this->Blocks.Map.end());
                this->Blocks.Map.insert(std::make_pair(msTimestamp, pRecordingBlocks));
            }
            catch (const std::exception &ex)
            {
                RT_NOREF(ex);

                delete pBlock;
                vrc = VERR_NO_MEMORY;
            }
        }
        else
            vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        RecordingVideoFrameFree(pFrame);

    unlock();

    return vrc;
}

/**
 * Initializes a recording stream.
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to recording context.
 * @param   uScreen             Screen number to use for this recording stream.
 * @param   Settings            Recording screen configuration to use for initialization.
 */
int RecordingStream::Init(RecordingContext *pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings)
{
    return initInternal(pCtx, uScreen, Settings);
}

/**
 * Initializes a recording stream, internal version.
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to recording context.
 * @param   uScreen             Screen number to use for this recording stream.
 * @param   screenSettings      Recording screen configuration to use for initialization.
 */
int RecordingStream::initInternal(RecordingContext *pCtx, uint32_t uScreen,
                                  const settings::RecordingScreenSettings &screenSettings)
{
    AssertReturn(enmState == RECORDINGSTREAMSTATE_UNINITIALIZED, VERR_WRONG_ORDER);

    this->m_pCtx         = pCtx;
    this->uTrackAudio    = UINT8_MAX;
    this->uTrackVideo    = UINT8_MAX;
    this->tsStartMs      = 0;
    this->uScreenID      = uScreen;
#ifdef VBOX_WITH_AUDIO_RECORDING
    /* We use the codec from the recording context, as this stream only receives multiplexed data (same audio for all streams). */
    this->pCodecAudio    = m_pCtx->GetCodecAudio();
#endif
    this->ScreenSettings = screenSettings;

    settings::RecordingScreenSettings *pSettings = &this->ScreenSettings;

    int vrc = RTCritSectInit(&this->CritSect);
    if (RT_FAILURE(vrc))
        return vrc;

    this->File.pWEBM = NULL;
    this->File.hFile = NIL_RTFILE;

    vrc = open(*pSettings);
    if (RT_FAILURE(vrc))
        return vrc;

    const bool fVideoEnabled = pSettings->isFeatureEnabled(RecordingFeature_Video);
    const bool fAudioEnabled = pSettings->isFeatureEnabled(RecordingFeature_Audio);

    if (fVideoEnabled)
    {
        vrc = initVideo(*pSettings);
        if (RT_FAILURE(vrc))
            return vrc;
    }

    switch (pSettings->enmDest)
    {
        case RecordingDestination_File:
        {
            Assert(pSettings->File.strName.isNotEmpty());
            const char *pszFile = pSettings->File.strName.c_str();

            AssertPtr(File.pWEBM);
            vrc = File.pWEBM->OpenEx(pszFile, &this->File.hFile,
#ifdef VBOX_WITH_AUDIO_RECORDING
                                     fAudioEnabled ? pSettings->Audio.enmCodec : RecordingAudioCodec_None,
#else
                                     RecordingAudioCodec_None,
#endif
                                     fVideoEnabled ? pSettings->Video.enmCodec : RecordingVideoCodec_None);
            if (RT_FAILURE(vrc))
            {
                LogRel(("Recording: Failed to create output file '%s' (%Rrc)\n", pszFile, vrc));
                break;
            }

            if (fVideoEnabled)
            {
                vrc = this->File.pWEBM->AddVideoTrack(&this->CodecVideo,
                                                      pSettings->Video.ulWidth, pSettings->Video.ulHeight, pSettings->Video.ulFPS,
                                                      &this->uTrackVideo);
                if (RT_FAILURE(vrc))
                {
                    LogRel(("Recording: Failed to add video track to output file '%s' (%Rrc)\n", pszFile, vrc));
                    break;
                }

                LogRel(("Recording: Recording video of screen #%u with %RU32x%RU32 @ %RU32 kbps, %RU32 FPS (track #%RU8)\n",
                        this->uScreenID, pSettings->Video.ulWidth, pSettings->Video.ulHeight,
                        pSettings->Video.ulRate, pSettings->Video.ulFPS, this->uTrackVideo));
            }

#ifdef VBOX_WITH_AUDIO_RECORDING
            if (fAudioEnabled)
            {
                AssertPtr(this->pCodecAudio);
                vrc = this->File.pWEBM->AddAudioTrack(this->pCodecAudio,
                                                      pSettings->Audio.uHz, pSettings->Audio.cChannels, pSettings->Audio.cBits,
                                                      &this->uTrackAudio);
                if (RT_FAILURE(vrc))
                {
                    LogRel(("Recording: Failed to add audio track to output file '%s' (%Rrc)\n", pszFile, vrc));
                    break;
                }

                LogRel(("Recording: Recording audio of screen #%u in %RU16Hz, %RU8 bit, %RU8 %s (track #%RU8)\n",
                        this->uScreenID, pSettings->Audio.uHz, pSettings->Audio.cBits, pSettings->Audio.cChannels,
                        pSettings->Audio.cChannels ? "channels" : "channel", this->uTrackAudio));
            }
#endif

            if (   fVideoEnabled
#ifdef VBOX_WITH_AUDIO_RECORDING
                || fAudioEnabled
#endif
               )
            {
                char szWhat[32] = { 0 };
                if (fVideoEnabled)
                    RTStrCat(szWhat, sizeof(szWhat), "video");
#ifdef VBOX_WITH_AUDIO_RECORDING
                if (fAudioEnabled)
                {
                    if (fVideoEnabled)
                        RTStrCat(szWhat, sizeof(szWhat), " + ");
                    RTStrCat(szWhat, sizeof(szWhat), "audio");
                }
#endif
                LogRel(("Recording: Recording %s of screen #%u to '%s'\n", szWhat, this->uScreenID, pszFile));
            }

            break;
        }

        default:
            AssertFailed(); /* Should never happen. */
            vrc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(vrc))
    {
        this->enmState  = RECORDINGSTREAMSTATE_INITIALIZED;
        this->fEnabled  = true;
        this->tsStartMs = RTTimeProgramMilliTS();

        return VINF_SUCCESS;
    }

    int vrc2 = uninitInternal();
    AssertRC(vrc2);

    LogRel(("Recording: Stream #%RU32 initialization failed with %Rrc\n", uScreen, vrc));
    return vrc;
}

/**
 * Closes a recording stream.
 * Depending on the stream's recording destination, this function closes all associated handles
 * and finalizes recording.
 *
 * @returns IPRT status code.
 */
int RecordingStream::close(void)
{
    int vrc = VINF_SUCCESS;

    switch (this->ScreenSettings.enmDest)
    {
        case RecordingDestination_File:
        {
            if (this->File.pWEBM)
                vrc = this->File.pWEBM->Close();
            break;
        }

        default:
            AssertFailed(); /* Should never happen. */
            break;
    }

    this->Blocks.Clear();

    LogRel(("Recording: Recording screen #%u stopped\n", this->uScreenID));

    if (RT_FAILURE(vrc))
    {
        LogRel(("Recording: Error stopping recording screen #%u, vrc=%Rrc\n", this->uScreenID, vrc));
        return vrc;
    }

    switch (this->ScreenSettings.enmDest)
    {
        case RecordingDestination_File:
        {
            if (RTFileIsValid(this->File.hFile))
            {
                vrc = RTFileClose(this->File.hFile);
                if (RT_SUCCESS(vrc))
                {
                    LogRel(("Recording: Closed file '%s'\n", this->ScreenSettings.File.strName.c_str()));
                }
                else
                {
                    LogRel(("Recording: Error closing file '%s', rc=%Rrc\n", this->ScreenSettings.File.strName.c_str(), vrc));
                    break;
                }
            }

            WebMWriter *pWebMWriter = this->File.pWEBM;
            AssertPtr(pWebMWriter);

            if (pWebMWriter)
            {
                /* If no clusters (= data) was written, delete the file again. */
                if (pWebMWriter->GetClusters() == 0)
                {
                    int vrc2 = RTFileDelete(this->ScreenSettings.File.strName.c_str());
                    AssertRC(vrc2); /* Ignore rc on non-debug builds. */
                }

                delete pWebMWriter;
                pWebMWriter = NULL;

                this->File.pWEBM = NULL;
            }
            break;
        }

        default:
            vrc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Uninitializes a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::Uninit(void)
{
    return uninitInternal();
}

/**
 * Uninitializes a recording stream, internal version.
 *
 * @returns IPRT status code.
 */
int RecordingStream::uninitInternal(void)
{
    if (this->enmState != RECORDINGSTREAMSTATE_INITIALIZED)
        return VINF_SUCCESS;

    int vrc = close();
    if (RT_FAILURE(vrc))
        return vrc;

#ifdef VBOX_WITH_AUDIO_RECORDING
    this->pCodecAudio = NULL;
#endif

    if (this->ScreenSettings.isFeatureEnabled(RecordingFeature_Video))
    {
        vrc = recordingCodecFinalize(&this->CodecVideo);
        if (RT_SUCCESS(vrc))
            vrc = recordingCodecDestroy(&this->CodecVideo);
    }

    if (RT_SUCCESS(vrc))
    {
        RTCritSectDelete(&this->CritSect);

        this->enmState = RECORDINGSTREAMSTATE_UNINITIALIZED;
        this->fEnabled = false;
    }

    return vrc;
}

/**
 * Writes encoded data to a WebM file instance.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec which has encoded the data.
 * @param   pvData              Encoded data to write.
 * @param   cbData              Size (in bytes) of \a pvData.
 * @param   msAbsPTS            Absolute PTS (in ms) of written data.
 * @param   uFlags              Encoding flags of type RECORDINGCODEC_ENC_F_XXX.
 */
int RecordingStream::codecWriteToWebM(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData,
                                      uint64_t msAbsPTS, uint32_t uFlags)
{
    AssertPtr(this->File.pWEBM);
    AssertPtr(pvData);
    Assert   (cbData);

    WebMWriter::WebMBlockFlags blockFlags = VBOX_WEBM_BLOCK_FLAG_NONE;
    if (RT_LIKELY(uFlags != RECORDINGCODEC_ENC_F_NONE))
    {
        /* All set. */
    }
    else
    {
        if (uFlags & RECORDINGCODEC_ENC_F_BLOCK_IS_KEY)
            blockFlags |= VBOX_WEBM_BLOCK_FLAG_KEY_FRAME;
        if (uFlags & RECORDINGCODEC_ENC_F_BLOCK_IS_INVISIBLE)
            blockFlags |= VBOX_WEBM_BLOCK_FLAG_INVISIBLE;
    }

    return this->File.pWEBM->WriteBlock(  pCodec->Parms.enmType == RECORDINGCODECTYPE_AUDIO
                                        ? this->uTrackAudio : this->uTrackVideo,
                                        pvData, cbData, msAbsPTS, blockFlags);
}

/**
 * Codec callback for writing encoded data to a recording stream.
 *
 * @returns VBox status code.
 * @param   pCodec              Codec which has encoded the data.
 * @param   pvData              Encoded data to write.
 * @param   cbData              Size (in bytes) of \a pvData.
 * @param   msAbsPTS            Absolute PTS (in ms) of written data.
 * @param   uFlags              Encoding flags of type RECORDINGCODEC_ENC_F_XXX.
 */
/* static */
DECLCALLBACK(int) RecordingStream::codecWriteDataCallback(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData,
                                                          uint64_t msAbsPTS, uint32_t uFlags, void *pvUser)
{
    RecordingStream *pThis = (RecordingStream *)pvUser;
    AssertPtr(pThis);

    /** @todo For now this is hardcoded to always write to a WebM file. Add other stuff later. */
    return pThis->codecWriteToWebM(pCodec, pvData, cbData, msAbsPTS, uFlags);
}

/**
 * Initializes the video recording for a recording stream.
 *
 * @returns VBox status code.
 * @param   screenSettings      Screen settings to use.
 */
int RecordingStream::initVideo(const settings::RecordingScreenSettings &screenSettings)
{
    /* Sanity. */
    AssertReturn(screenSettings.Video.ulRate,   VERR_INVALID_PARAMETER);
    AssertReturn(screenSettings.Video.ulWidth,  VERR_INVALID_PARAMETER);
    AssertReturn(screenSettings.Video.ulHeight, VERR_INVALID_PARAMETER);
    AssertReturn(screenSettings.Video.ulFPS,    VERR_INVALID_PARAMETER);

    PRECORDINGCODEC pCodec = &this->CodecVideo;

    RECORDINGCODECCALLBACKS Callbacks;
    Callbacks.pvUser       = this;
    Callbacks.pfnWriteData = RecordingStream::codecWriteDataCallback;

    int vrc = recordingCodecCreateVideo(pCodec, screenSettings.Video.enmCodec);
    if (RT_SUCCESS(vrc))
        vrc = recordingCodecInit(pCodec, &Callbacks, screenSettings);

    if (RT_FAILURE(vrc))
        LogRel(("Recording: Initializing video codec failed with %Rrc\n", vrc));

    return vrc;
}

/**
 * Locks a recording stream.
 */
void RecordingStream::lock(void)
{
    int vrc = RTCritSectEnter(&CritSect);
    AssertRC(vrc);
}

/**
 * Unlocks a locked recording stream.
 */
void RecordingStream::unlock(void)
{
    int vrc = RTCritSectLeave(&CritSect);
    AssertRC(vrc);
}

