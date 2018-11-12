/* $Id$ */
/** @file
 * Recording stream code.
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

#include "Recording.h"
#include "RecordingStream.h"
#include "RecordingUtils.h"
#include "WebMWriter.h"


RecordingStream::RecordingStream(RecordingContext *a_pCtx)
    : pCtx(a_pCtx)
    , enmState(RECORDINGSTREAMSTATE_UNINITIALIZED)
    , tsStartMs(0)
{
    File.pWEBM = NULL;
    File.hFile = NIL_RTFILE;
}

RecordingStream::RecordingStream(RecordingContext *a_pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings)
    : enmState(RECORDINGSTREAMSTATE_UNINITIALIZED)
    , tsStartMs(0)
{
    File.pWEBM = NULL;
    File.hFile = NIL_RTFILE;

    int rc2 = initInternal(a_pCtx, uScreen, Settings);
    if (RT_FAILURE(rc2))
        throw rc2;
}

RecordingStream::~RecordingStream(void)
{
    int rc2 = uninitInternal();
    AssertRC(rc2);
}

/**
 * Opens a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::open(const settings::RecordingScreenSettings &Settings)
{
    /* Sanity. */
    Assert(Settings.enmDest != RecordingDestination_None);

    int rc;

    switch (Settings.enmDest)
    {
        case RecordingDestination_File:
        {
            Assert(Settings.File.strName.isNotEmpty());

            char *pszAbsPath = RTPathAbsDup(Settings.File.strName.c_str());
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

            if (this->uScreenID > 0)
                rc = RTStrAPrintf(&pszFile, "%s-%u%s", pszAbsPath, this->uScreenID + 1, pszSuff);
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

                    if (this->uScreenID > 0)
                        rc = RTStrAPrintf(&pszFile, "%s-%04d-%02u-%02uT%02u-%02u-%02u-%09uZ-%u%s",
                                          pszAbsPath, time.i32Year, time.u8Month, time.u8MonthDay,
                                          time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond,
                                          this->uScreenID + 1, pszSuff);
                    else
                        rc = RTStrAPrintf(&pszFile, "%s-%04d-%02u-%02uT%02u-%02u-%02u-%09uZ%s",
                                          pszAbsPath, time.i32Year, time.u8Month, time.u8MonthDay,
                                          time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond,
                                          pszSuff);

                    if (RT_SUCCESS(rc))
                        rc = RTFileOpen(&hFile, pszFile, fOpen);
                }

                try
                {
                    Assert(File.pWEBM == NULL);
                    File.pWEBM = new WebMWriter();
                }
                catch (std::bad_alloc &)
               {
                    rc = VERR_NO_MEMORY;
                }

                if (RT_SUCCESS(rc))
                {
                    this->File.hFile   = hFile;
                    this->File.strName = pszFile;
                }
            }

            RTStrFree(pszSuff);
            RTStrFree(pszAbsPath);

            if (RT_FAILURE(rc))
            {
                LogRel(("Recording: Failed to open file '%s' for screen %RU32, rc=%Rrc\n",
                        pszFile ? pszFile : "<Unnamed>", this->uScreenID, rc));
            }

            RTStrFree(pszFile);
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
 * Parses an options string to configure advanced / hidden / experimental features of a recording stream.
 * Unknown values will be skipped.
 *
 * @returns IPRT status code.
 * @param   strOptions          Options string to parse.
 */
int RecordingStream::parseOptionsString(const com::Utf8Str &strOptions)
{
    size_t pos = 0;
    com::Utf8Str key, value;
    while ((pos = strOptions.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("vc_quality", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_LIBVPX
            Assert(this->ScreenSettings.Video.ulFPS);
            if (value.compare("realtime", Utf8Str::CaseInsensitive) == 0)
                this->Video.Codec.VPX.uEncoderDeadline = VPX_DL_REALTIME;
            else if (value.compare("good", Utf8Str::CaseInsensitive) == 0)
                this->Video.Codec.VPX.uEncoderDeadline = 1000000 / this->ScreenSettings.Video.ulFPS;
            else if (value.compare("best", Utf8Str::CaseInsensitive) == 0)
                this->Video.Codec.VPX.uEncoderDeadline = VPX_DL_BEST_QUALITY;
            else
            {
                this->Video.Codec.VPX.uEncoderDeadline = value.toUInt32();
#endif
            }
        }
        else if (key.compare("vc_enabled", Utf8Str::CaseInsensitive) == 0)
        {
            if (value.compare("false", Utf8Str::CaseInsensitive) == 0)
                this->ScreenSettings.featureMap[RecordingFeature_Video] = false;
        }
        else if (key.compare("ac_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_RECORDING
            if (value.compare("true", Utf8Str::CaseInsensitive) == 0)
                this->ScreenSettings.featureMap[RecordingFeature_Audio] = true;
#endif
        }
        else if (key.compare("ac_profile", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_RECORDING
            if (value.compare("low", Utf8Str::CaseInsensitive) == 0)
            {
                this->ScreenSettings.Audio.uHz       = 8000;
                this->ScreenSettings.Audio.cBits     = 16;
                this->ScreenSettings.Audio.cChannels = 1;
            }
            else if (value.startsWith("med" /* "med[ium]" */, Utf8Str::CaseInsensitive) == 0)
            {
                /* Stay with the default set above. */
            }
            else if (value.compare("high", Utf8Str::CaseInsensitive) == 0)
            {
                this->ScreenSettings.Audio.uHz       = 48000;
                this->ScreenSettings.Audio.cBits     = 16;
                this->ScreenSettings.Audio.cChannels = 2;
            }
#endif
        }
        else
            LogRel(("Recording: Unknown option '%s' (value '%s'), skipping\n", key.c_str(), value.c_str()));

    } /* while */

    return VINF_SUCCESS;
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
 * Checks if a specified limit for a recording stream has been reached.
 *
 * @returns true if any limit has been reached.
 * @param   tsNowMs             Current time stamp (in ms).
 */
bool RecordingStream::IsLimitReached(uint64_t tsNowMs) const
{
    if (!IsReady())
        return true;

    if (   this->ScreenSettings.ulMaxTimeS
        && tsNowMs >= this->tsStartMs + (this->ScreenSettings.ulMaxTimeS * RT_MS_1SEC))
    {
        return true;
    }

    if (this->ScreenSettings.enmDest == RecordingDestination_File)
    {

        if (this->ScreenSettings.File.ulMaxSizeMB)
        {
            uint64_t sizeInMB = this->File.pWEBM->GetFileSize() / _1M;
            if(sizeInMB >= this->ScreenSettings.File.ulMaxSizeMB)
                return true;
        }

        /* Check for available free disk space */
        if (   this->File.pWEBM
            && this->File.pWEBM->GetAvailableSpace() < 0x100000) /** @todo r=andy WTF? Fix this. */
        {
            LogRel(("Recording: Not enough free storage space available, stopping video capture\n"));
            return true;
        }
    }

    return false;
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
 */
int RecordingStream::Process(RecordingBlockMap &mapBlocksCommon)
{
    lock();

    if (!this->ScreenSettings.fEnabled)
    {
        unlock();
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;

    RecordingBlockMap::iterator itStreamBlocks = Blocks.Map.begin();
    while (itStreamBlocks != Blocks.Map.end())
    {
        const uint64_t         uTimeStampMs = itStreamBlocks->first;
              RecordingBlocks *pBlocks      = itStreamBlocks->second;

        AssertPtr(pBlocks);

        while (!pBlocks->List.empty())
        {
            PRECORDINGBLOCK pBlock = pBlocks->List.front();
            AssertPtr(pBlock);

#ifdef VBOX_WITH_LIBVPX
            if (pBlock->enmType == RECORDINGBLOCKTYPE_VIDEO)
            {
                PRECORDINGVIDEOFRAME pVideoFrame  = (PRECORDINGVIDEOFRAME)pBlock->pvData;

                rc = recordingRGBToYUV(pVideoFrame->uPixelFormat,
                                       /* Destination */
                                       this->Video.Codec.VPX.pu8YuvBuf, pVideoFrame->uWidth, pVideoFrame->uHeight,
                                       /* Source */
                                       pVideoFrame->pu8RGBBuf, this->ScreenSettings.Video.ulWidth, this->ScreenSettings.Video.ulHeight);
                if (RT_SUCCESS(rc))
                {
                    rc = writeVideoVPX(uTimeStampMs, pVideoFrame);
                }
                else
                    break;
            }
#endif
            RecordingBlockFree(pBlock);
            pBlock = NULL;

            pBlocks->List.pop_front();
        }

        ++itStreamBlocks;
    }

#ifdef VBOX_WITH_AUDIO_RECORDING
    AssertPtr(pCtx);

    /* As each (enabled) screen has to get the same audio data, look for common (audio) data which needs to be
     * written to the screen's assigned recording stream. */
    RecordingBlockMap::iterator itCommonBlocks = mapBlocksCommon.begin();
    while (itCommonBlocks != mapBlocksCommon.end())
    {
        RECORDINGBLOCKList::iterator itBlock = itCommonBlocks->second->List.begin();
        while (itBlock != itCommonBlocks->second->List.end())
        {
            PRECORDINGBLOCK pBlockCommon = (PRECORDINGBLOCK)(*itBlock);
            switch (pBlockCommon->enmType)
            {
                case RECORDINGBLOCKTYPE_AUDIO:
                {
                    PRECORDINGAUDIOFRAME pAudioFrame = (PRECORDINGAUDIOFRAME)pBlockCommon->pvData;
                    AssertPtr(pAudioFrame);
                    AssertPtr(pAudioFrame->pvBuf);
                    Assert(pAudioFrame->cbBuf);

                    WebMWriter::BlockData_Opus blockData = { pAudioFrame->pvBuf, pAudioFrame->cbBuf,
                                                             pBlockCommon->uTimeStampMs };
                    AssertPtr(this->File.pWEBM);
                    rc = this->File.pWEBM->WriteBlock(this->uTrackAudio, &blockData, sizeof(blockData));
                    break;
                }

                default:
                    AssertFailed();
                    break;
            }

            if (RT_FAILURE(rc))
                break;

            Assert(pBlockCommon->cRefs);
            pBlockCommon->cRefs--;
            if (pBlockCommon->cRefs == 0)
            {
                RecordingBlockFree(pBlockCommon);
                itCommonBlocks->second->List.erase(itBlock);
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

        if (RT_FAILURE(rc))
            break;
    }
#endif

    unlock();

    return rc;
}

/**
 * Sends a raw (e.g. not yet encoded) video frame to the recording stream.
 *
 * @returns IPRT status code.
 * @param   x                   Upper left (X) coordinate where the video frame starts.
 * @param   y                   Upper left (Y) coordinate where the video frame starts.
 * @param   uPixelFormat        Pixel format of the video frame.
 * @param   uBPP                Bits per pixel (BPP) of the video frame.
 * @param   uBytesPerLine       Bytes per line  of the video frame.
 * @param   uSrcWidth           Width (in pixels) of the video frame.
 * @param   uSrcHeight          Height (in pixels) of the video frame.
 * @param   puSrcData           Actual pixel data of the video frame.
 * @param   uTimeStampMs        Timestamp (in ms) as PTS.
 */
int RecordingStream::SendVideoFrame(uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                                    uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData, uint64_t uTimeStampMs)
{
    lock();

    PRECORDINGVIDEOFRAME pFrame = NULL;

    int rc = VINF_SUCCESS;

    do
    {
        if (!this->fEnabled)
        {
            rc = VINF_TRY_AGAIN; /* Not (yet) enabled. */
            break;
        }

        if (uTimeStampMs < this->Video.uLastTimeStampMs + this->Video.uDelayMs)
        {
            rc = VINF_TRY_AGAIN; /* Respect maximum frames per second. */
            break;
        }

        this->Video.uLastTimeStampMs = uTimeStampMs;

        int xDiff = ((int)this->ScreenSettings.Video.ulWidth - (int)uSrcWidth) / 2;
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
        int yDiff = ((int)this->ScreenSettings.Video.ulHeight - (int)uSrcHeight) / 2;
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

        if (   destX > this->ScreenSettings.Video.ulWidth
            || destY > this->ScreenSettings.Video.ulHeight)
        {
            rc = VERR_INVALID_PARAMETER;  /* Nothing visible. */
            break;
        }

        if (destX + w > this->ScreenSettings.Video.ulWidth)
            w = this->ScreenSettings.Video.ulWidth - destX;

        if (destY + h > this->ScreenSettings.Video.ulHeight)
            h = this->ScreenSettings.Video.ulHeight - destY;

        pFrame = (PRECORDINGVIDEOFRAME)RTMemAllocZ(sizeof(RECORDINGVIDEOFRAME));
        AssertBreakStmt(pFrame, rc = VERR_NO_MEMORY);

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
                    AssertMsgFailedBreakStmt(("Unknown color depth (%RU32)\n", uBPP), rc = VERR_NOT_SUPPORTED);
                    break;
            }
        }
        else
            AssertMsgFailedBreakStmt(("Unknown pixel format (%RU32)\n", uPixelFormat), rc = VERR_NOT_SUPPORTED);

        const size_t cbRGBBuf =   this->ScreenSettings.Video.ulWidth
                                * this->ScreenSettings.Video.ulHeight
                                * uBytesPerPixel;
        AssertBreakStmt(cbRGBBuf, rc = VERR_INVALID_PARAMETER);

        pFrame->pu8RGBBuf = (uint8_t *)RTMemAlloc(cbRGBBuf);
        AssertBreakStmt(pFrame->pu8RGBBuf, rc = VERR_NO_MEMORY);
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
        RECORDINGBMPHDR bmpHdr;
        RT_ZERO(bmpHdr);

        RECORDINGBMPDIBHDR bmpDIBHdr;
        RT_ZERO(bmpDIBHdr);

        bmpHdr.u16Magic   = 0x4d42; /* Magic */
        bmpHdr.u32Size    = (uint32_t)(sizeof(RECORDINGBMPHDR) + sizeof(RECORDINGBMPDIBHDR) + (w * h * uBytesPerPixel));
        bmpHdr.u32OffBits = (uint32_t)(sizeof(RECORDINGBMPHDR) + sizeof(RECORDINGBMPDIBHDR));

        bmpDIBHdr.u32Size          = sizeof(RECORDINGBMPDIBHDR);
        bmpDIBHdr.u32Width         = w;
        bmpDIBHdr.u32Height        = h;
        bmpDIBHdr.u16Planes        = 1;
        bmpDIBHdr.u16BitCount      = uBPP;
        bmpDIBHdr.u32XPelsPerMeter = 5000;
        bmpDIBHdr.u32YPelsPerMeter = 5000;

        char szFileName[RTPATH_MAX];
        RTStrPrintf2(szFileName, sizeof(szFileName), "/tmp/VideoRecFrame-%RU32.bmp", this->uScreenID);

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
        if (RT_SUCCESS(rc2))
            RTFileClose(fh);
#endif

    } while (0);

    if (rc == VINF_SUCCESS) /* Note: Also could be VINF_TRY_AGAIN. */
    {
        PRECORDINGBLOCK pBlock = (PRECORDINGBLOCK)RTMemAlloc(sizeof(RECORDINGBLOCK));
        if (pBlock)
        {
            AssertPtr(pFrame);

            pBlock->enmType = RECORDINGBLOCKTYPE_VIDEO;
            pBlock->pvData  = pFrame;
            pBlock->cbData  = sizeof(RECORDINGVIDEOFRAME) + pFrame->cbRGBBuf;

            try
            {
                RecordingBlocks *pRECORDINGBLOCKs = new RecordingBlocks();
                pRECORDINGBLOCKs->List.push_back(pBlock);

                Assert(this->Blocks.Map.find(uTimeStampMs) == this->Blocks.Map.end());
                this->Blocks.Map.insert(std::make_pair(uTimeStampMs, pRECORDINGBLOCKs));
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
        RecordingVideoFrameFree(pFrame);

    unlock();

    return rc;
}

/**
 * Initializes a recording stream.
 *
 * @returns IPRT status code.
 * @param   a_pCtx              Pointer to recording context.
 * @param   uScreen             Screen number to use for this recording stream.
 * @param   Settings            Capturing configuration to use for initialization.
 */
int RecordingStream::Init(RecordingContext *a_pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings)
{
    return initInternal(a_pCtx, uScreen, Settings);
}

/**
 * Initializes a recording stream, internal version.
 *
 * @returns IPRT status code.
 * @param   a_pCtx              Pointer to recording context.
 * @param   uScreen             Screen number to use for this recording stream.
 * @param   Settings            Capturing configuration to use for initialization.
 */
int RecordingStream::initInternal(RecordingContext *a_pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings)
{
    int rc = parseOptionsString(Settings.strOptions);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTCritSectInit(&this->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    rc = open(Settings);
    if (RT_FAILURE(rc))
        return rc;

    const bool fVideoEnabled = Settings.isFeatureEnabled(RecordingFeature_Video);
    const bool fAudioEnabled = Settings.isFeatureEnabled(RecordingFeature_Audio);

    if (fVideoEnabled)
    {
        rc = initVideo();
        if (RT_FAILURE(rc))
            return rc;
    }

    if (fAudioEnabled)
    {
        rc = initAudio();
        if (RT_FAILURE(rc))
            return rc;
    }

    switch (this->ScreenSettings.enmDest)
    {
        case RecordingDestination_File:
        {
            const char *pszFile = this->ScreenSettings.File.strName.c_str();

            AssertPtr(File.pWEBM);
            rc = File.pWEBM->OpenEx(pszFile, &this->File.hFile,
#ifdef VBOX_WITH_AUDIO_RECORDING
                                      Settings.isFeatureEnabled(RecordingFeature_Audio)
                                    ? WebMWriter::AudioCodec_Opus : WebMWriter::AudioCodec_None,
#else
                                      WebMWriter::AudioCodec_None,
#endif
                                      Settings.isFeatureEnabled(RecordingFeature_Video)
                                    ? WebMWriter::VideoCodec_VP8 : WebMWriter::VideoCodec_None);
            if (RT_FAILURE(rc))
            {
                LogRel(("Recording: Failed to create output file '%s' (%Rrc)\n", pszFile, rc));
                break;
            }

            if (fVideoEnabled)
            {
                rc = this->File.pWEBM->AddVideoTrack(Settings.Video.ulWidth, Settings.Video.ulHeight,
                                                     Settings.Video.ulFPS, &this->uTrackVideo);
                if (RT_FAILURE(rc))
                {
                    LogRel(("Recording: Failed to add video track to output file '%s' (%Rrc)\n", pszFile, rc));
                    break;
                }

                LogRel(("Recording: Recording video of screen #%u with %RU32x%RU32 @ %RU32 kbps, %RU32 FPS (track #%RU8)\n",
                        this->uScreenID, Settings.Video.ulWidth, Settings.Video.ulHeight, Settings.Video.ulRate,
                        Settings.Video.ulFPS, this->uTrackVideo));
            }

#ifdef VBOX_WITH_AUDIO_RECORDING
            if (fAudioEnabled)
            {
                rc = this->File.pWEBM->AddAudioTrack(Settings.Audio.uHz, Settings.Audio.cChannels, Settings.Audio.cBits,
                                                     &this->uTrackAudio);
                if (RT_FAILURE(rc))
                {
                    LogRel(("Recording: Failed to add audio track to output file '%s' (%Rrc)\n", pszFile, rc));
                    break;
                }

                LogRel(("Recording: Recording audio of screen #%u in %RU16Hz, %RU8 bit, %RU8 %s (track #%RU8)\n",
                        this->uScreenID, Settings.Audio.uHz, Settings.Audio.cBits, Settings.Audio.cChannels,
                        Settings.Audio.cChannels ? "channels" : "channel", this->uTrackAudio));
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
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        this->pCtx           = a_pCtx;
        this->enmState       = RECORDINGSTREAMSTATE_INITIALIZED;
        this->fEnabled       = true;
        this->uScreenID      = uScreen;
        this->tsStartMs      = RTTimeMilliTS();
        this->ScreenSettings = Settings;
    }
    else
    {
        int rc2 = uninitInternal();
        AssertRC(rc2);
        return rc;
    }

    return VINF_SUCCESS;
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
    int rc = VINF_SUCCESS;

    if (this->fEnabled)
    {
        switch (this->ScreenSettings.enmDest)
        {
            case RecordingDestination_File:
            {
                if (this->File.pWEBM)
                    rc = this->File.pWEBM->Close();
                break;
            }

            default:
                AssertFailed(); /* Should never happen. */
                break;
        }

        this->Blocks.Clear();

        LogRel(("Recording: Recording screen #%u stopped\n", this->uScreenID));
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("Recording: Error stopping recording screen #%u, rc=%Rrc\n", this->uScreenID, rc));
        return rc;
    }

    switch (this->ScreenSettings.enmDest)
    {
        case RecordingDestination_File:
        {
            if (RTFileIsValid(this->File.hFile))
            {
                rc = RTFileClose(this->File.hFile);
                if (RT_SUCCESS(rc))
                {
                    LogRel(("Recording: Closed file '%s'\n", this->ScreenSettings.File.strName.c_str()));
                }
                else
                {
                    LogRel(("Recording: Error closing file '%s', rc=%Rrc\n", this->ScreenSettings.File.strName.c_str(), rc));
                    break;
                }
            }

            if (this->File.pWEBM)
            {
                delete this->File.pWEBM;
                this->File.pWEBM = NULL;
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

    int rc = close();
    if (RT_FAILURE(rc))
        return rc;

    if (this->ScreenSettings.isFeatureEnabled(RecordingFeature_Video))
    {
        int rc2 = unitVideo();
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    RTCritSectDelete(&this->CritSect);

    this->enmState = RECORDINGSTREAMSTATE_UNINITIALIZED;
    this->fEnabled = false;

    return rc;
}

/**
 * Uninitializes video recording for a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::unitVideo(void)
{
#ifdef VBOX_WITH_LIBVPX
    /* At the moment we only have VPX. */
    return uninitVideoVPX();
#else
    return VERR_NOT_SUPPORTED;
#endif
}

#ifdef VBOX_WITH_LIBVPX
/**
 * Uninitializes the VPX codec for a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::uninitVideoVPX(void)
{
    PRECORDINGVIDEOCODEC pCodec = &this->Video.Codec;
    vpx_img_free(&pCodec->VPX.RawImage);
    pCodec->VPX.pu8YuvBuf = NULL; /* Was pointing to VPX.RawImage. */

    vpx_codec_err_t rcv = vpx_codec_destroy(&this->Video.Codec.VPX.Ctx);
    Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

    return VINF_SUCCESS;
}
#endif

/**
 * Initializes the video recording for a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::initVideo(void)
{
    /* Sanity. */
    AssertReturn(this->ScreenSettings.Video.ulRate,   VERR_INVALID_PARAMETER);
    AssertReturn(this->ScreenSettings.Video.ulWidth,  VERR_INVALID_PARAMETER);
    AssertReturn(this->ScreenSettings.Video.ulHeight, VERR_INVALID_PARAMETER);
    AssertReturn(this->ScreenSettings.Video.ulFPS,    VERR_INVALID_PARAMETER);

    this->Video.cFailedEncodingFrames = 0;
    this->Video.uDelayMs = RT_MS_1SEC / this->ScreenSettings.Video.ulFPS;

    int rc;

#ifdef VBOX_WITH_LIBVPX
    /* At the moment we only have VPX. */
    rc = initVideoVPX();
#else
    rc = VINF_SUCCESS;
#endif

    if (RT_FAILURE(rc))
        LogRel(("Recording: Failed to initialize video encoding (%Rrc)\n", rc));

    return rc;
}

#ifdef VBOX_WITH_LIBVPX
/**
 * Initializes the VPX codec for a recording stream.
 *
 * @returns IPRT status code.
 */
int RecordingStream::initVideoVPX(void)
{
# ifdef VBOX_WITH_LIBVPX_VP9
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp9_cx();
# else /* Default is using VP8. */
    vpx_codec_iface_t *pCodecIface = vpx_codec_vp8_cx();
# endif

    PRECORDINGVIDEOCODEC pCodec = &this->Video.Codec;

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(pCodecIface, &pCodec->VPX.Cfg, 0 /* Reserved */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("Recording: Failed to get default config for VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_AVREC_CODEC_INIT_FAILED;
    }

    /* Target bitrate in kilobits per second. */
    pCodec->VPX.Cfg.rc_target_bitrate = this->ScreenSettings.Video.ulRate;
    /* Frame width. */
    pCodec->VPX.Cfg.g_w = this->ScreenSettings.Video.ulWidth;
    /* Frame height. */
    pCodec->VPX.Cfg.g_h = this->ScreenSettings.Video.ulHeight;
    /* 1ms per frame. */
    pCodec->VPX.Cfg.g_timebase.num = 1;
    pCodec->VPX.Cfg.g_timebase.den = 1000;
    /* Disable multithreading. */
    pCodec->VPX.Cfg.g_threads = 0;

    /* Initialize codec. */
    rcv = vpx_codec_enc_init(&pCodec->VPX.Ctx, pCodecIface, &pCodec->VPX.Cfg, 0 /* Flags */);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("Recording: Failed to initialize VPX encoder: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_AVREC_CODEC_INIT_FAILED;
    }

    if (!vpx_img_alloc(&pCodec->VPX.RawImage, VPX_IMG_FMT_I420,
                       this->ScreenSettings.Video.ulWidth, this->ScreenSettings.Video.ulHeight, 1))
    {
        LogRel(("Recording: Failed to allocate image %RU32x%RU32\n",
                this->ScreenSettings.Video.ulWidth, this->ScreenSettings.Video.ulHeight));
        return VERR_NO_MEMORY;
    }

    /* Save a pointer to the first raw YUV plane. */
    pCodec->VPX.pu8YuvBuf = pCodec->VPX.RawImage.planes[0];

    return VINF_SUCCESS;
}
#endif

/**
 * Initializes the audio part of a recording stream,
 *
 * @returns IPRT status code.
 */
int RecordingStream::initAudio(void)
{
#ifdef VBOX_WITH_AUDIO_RECORDING
    if (this->ScreenSettings.isFeatureEnabled(RecordingFeature_Audio))
    {
        /* Sanity. */
        AssertReturn(this->ScreenSettings.Audio.uHz,       VERR_INVALID_PARAMETER);
        AssertReturn(this->ScreenSettings.Audio.cBits,     VERR_INVALID_PARAMETER);
        AssertReturn(this->ScreenSettings.Audio.cChannels, VERR_INVALID_PARAMETER);
    }
#endif

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_LIBVPX
/**
 * Encodes the source image and write the encoded image to the stream's destination.
 *
 * @returns IPRT status code.
 * @param   uTimeStampMs        Absolute timestamp (PTS) of frame (in ms) to encode.
 * @param   pFrame              Frame to encode and submit.
 */
int RecordingStream::writeVideoVPX(uint64_t uTimeStampMs, PRECORDINGVIDEOFRAME pFrame)
{
    AssertPtrReturn(pFrame, VERR_INVALID_POINTER);

    int rc;

    PRECORDINGVIDEOCODEC pCodec = &this->Video.Codec;

    /* Presentation Time Stamp (PTS). */
    vpx_codec_pts_t pts = uTimeStampMs;
    vpx_codec_err_t rcv = vpx_codec_encode(&pCodec->VPX.Ctx,
                                           &pCodec->VPX.RawImage,
                                           pts                          /* Time stamp */,
                                           this->Video.uDelayMs         /* How long to show this frame */,
                                           0                            /* Flags */,
                                           pCodec->VPX.uEncoderDeadline /* Quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        if (this->Video.cFailedEncodingFrames++ < 64) /** @todo Make this configurable. */
        {
            LogRel(("Recording: Failed to encode video frame: %s\n", vpx_codec_err_to_string(rcv)));
            return VERR_GENERAL_FAILURE;
        }
    }

    this->Video.cFailedEncodingFrames = 0;

    vpx_codec_iter_t iter = NULL;
    rc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pPacket = vpx_codec_get_cx_data(&pCodec->VPX.Ctx, &iter);
        if (!pPacket)
            break;

        switch (pPacket->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
            {
                WebMWriter::BlockData_VP8 blockData = { &pCodec->VPX.Cfg, pPacket };
                rc = this->File.pWEBM->WriteBlock(this->uTrackVideo, &blockData, sizeof(blockData));
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

/**
 * Locks a recording stream.
 */
void RecordingStream::lock(void)
{
    int rc = RTCritSectEnter(&CritSect);
    AssertRC(rc);
}

/**
 * Unlocks a locked recording stream.
 */
void RecordingStream::unlock(void)
{
    int rc = RTCritSectLeave(&CritSect);
    AssertRC(rc);
}

