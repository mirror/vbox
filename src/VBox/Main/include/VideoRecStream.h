/* $Id$ */
/** @file
 * Video recording stream code header.
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

#ifndef ____H_VIDEOREC_STREAM
#define ____H_VIDEOREC_STREAM

#include <map>
#include <vector>

#include <iprt/critsect.h>

#include <VBox/com/array.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/err.h>
#include <VBox/settings.h>

#include "VideoRecInternals.h"

class WebMWriter;
class CaptureContext;

/** Structure for queuing all blocks bound to a single timecode.
 *  This can happen if multiple tracks are being involved. */
struct CaptureBlocks
{
    virtual ~CaptureBlocks()
    {
        Clear();
    }

    /**
     * Resets a video recording block list by removing (destroying)
     * all current elements.
     */
    void Clear()
    {
        while (!List.empty())
        {
            PVIDEORECBLOCK pBlock = List.front();
            VideoRecBlockFree(pBlock);
            List.pop_front();
        }

        Assert(List.size() == 0);
    }

    /** The actual block list for this timecode. */
    VideoRecBlockList List;
};

/** A block map containing all currently queued blocks.
 *  The key specifies a unique timecode, whereas the value
 *  is a list of blocks which all correlate to the same key (timecode). */
typedef std::map<uint64_t, CaptureBlocks *> VideoRecBlockMap;

/**
 * Structure for holding a set of recording (data) blocks.
 */
struct CaptureBlockSet
{
    virtual ~CaptureBlockSet()
    {
        Clear();
    }

    /**
     * Resets a recording block set by removing (destroying)
     * all current elements.
     */
    void Clear(void)
    {
        VideoRecBlockMap::iterator it = Map.begin();
        while (it != Map.end())
        {
            it->second->Clear();
            delete it->second;
            Map.erase(it);
            it = Map.begin();
        }

        Assert(Map.size() == 0);
    }

    /** Timestamp (in ms) when this set was last processed. */
    uint64_t         tsLastProcessedMs;
    /** All blocks related to this block set. */
    VideoRecBlockMap Map;
};

/**
 * Class for managing a recording stream.
 */
class CaptureStream
{
public:

    CaptureStream(void);

    CaptureStream(uint32_t uScreen, const settings::CaptureScreenSettings &Settings);

    virtual ~CaptureStream(void);

public:

    int Init(uint32_t uScreen, const settings::CaptureScreenSettings &Settings);
    int Uninit(void);

    int Process(VideoRecBlockMap &mapBlocksCommon);
    int SendVideoFrame(uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                       uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData, uint64_t uTimeStampMs);

    const settings::CaptureScreenSettings &GetConfig(void) const;
    bool IsLimitReached(uint64_t tsNowMs) const;
    bool IsReady(void) const;

protected:

    int open(void);
    int close(void);

    int initInternal(uint32_t uScreen, const settings::CaptureScreenSettings &Settings);
    int uninitInternal(void);

    int initVideo(void);
    int unitVideo(void);

    int initAudio(void);

#ifdef VBOX_WITH_LIBVPX
    int initVideoVPX(void);
    int uninitVideoVPX(void);
    int writeVideoVPX(uint64_t uTimeStampMs, PVIDEORECVIDEOFRAME pFrame);
#endif
    void lock(void);
    void unlock(void);

    int parseOptionsString(const com::Utf8Str &strOptions);

protected:

    /** Recording context this stream is associated to. */
    CaptureContext             *pCtx;
    struct
    {
        /** File handle to use for writing. */
        RTFILE              hFile;
        /** File name being used for this stream. */
        Utf8Str             strName;
        /** Pointer to WebM writer instance being used. */
        WebMWriter         *pWEBM;
    } File;
    bool                fEnabled;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    /** Track number of audio stream. */
    uint8_t             uTrackAudio;
#endif
    /** Track number of video stream. */
    uint8_t             uTrackVideo;
    /** Screen ID. */
    uint16_t            uScreenID;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;
    /** Timestamp (in ms) of when recording has been start. */
    uint64_t            tsStartMs;

    struct
    {
        /** Minimal delay (in ms) between two video frames.
         *  This value is based on the configured FPS rate. */
        uint32_t            uDelayMs;
        /** Time stamp (in ms) of the last video frame we encoded. */
        uint64_t            uLastTimeStampMs;
        /** Number of failed attempts to encode the current video frame in a row. */
        uint16_t            cFailedEncodingFrames;
        VIDEORECVIDEOCODEC  Codec;
    } Video;

    settings::CaptureScreenSettings ScreenSettings;
    /** Common set of video recording (data) blocks, needed for
     *  multiplexing to all recording streams. */
    CaptureBlockSet                 Blocks;
};

/** Vector of video recording streams. */
typedef std::vector <CaptureStream *> VideoRecStreams;

#endif /* ____H_VIDEOREC_STREAM */

