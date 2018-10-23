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

#include "VideoRecInternals.h"

class WebMWriter;

struct VIDEORECCFG;
typedef struct VIDEORECCFG *PVIDEORECCFG;

struct VIDEORECCONTEXT;
typedef struct VIDEORECCONTEXT *PVIDEORECCONTEXT;


/** Structure for queuing all blocks bound to a single timecode.
 *  This can happen if multiple tracks are being involved. */
struct VideoRecBlocks
{
    virtual ~VideoRecBlocks()
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
typedef std::map<uint64_t, VideoRecBlocks *> VideoRecBlockMap;

/**
 * Structure for holding a set of video recording (data) blocks.
 */
struct VideoRecBlockSet
{
    virtual ~VideoRecBlockSet()
    {
        Clear();
    }

    /**
     * Resets a video recording block set by removing (destroying)
     * all current elements.
     */
    void Clear()
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
 * Structure for maintaining a video recording stream.
 */
typedef struct VIDEORECSTREAM
{
    /** Video recording context this stream is associated to. */
    PVIDEORECCONTEXT            pCtx;
    /** Destination where to write the stream to. */
    VIDEORECDEST                enmDst;
    union
    {
        struct
        {
            /** File handle to use for writing. */
            RTFILE              hFile;
            /** File name being used for this stream. */
            char               *pszFile;
            /** Pointer to WebM writer instance being used. */
            WebMWriter         *pWEBM;
        } File;
    };
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    /** Track number of audio stream. */
    uint8_t             uTrackAudio;
#endif
    /** Track number of video stream. */
    uint8_t             uTrackVideo;
    /** Screen ID. */
    uint16_t            uScreenID;
    /** Whether video recording is enabled or not. */
    bool                fEnabled;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;

    struct
    {
        /** Codec-specific data. */
        VIDEORECVIDEOCODEC  Codec;
        /** Minimal delay (in ms) between two video frames.
         *  This value is based on the configured FPS rate. */
        uint32_t            uDelayMs;
        /** Target X resolution (in pixels). */
        uint32_t            uWidth;
        /** Target Y resolution (in pixels). */
        uint32_t            uHeight;
        /** Time stamp (in ms) of the last video frame we encoded. */
        uint64_t            uLastTimeStampMs;
        /** Number of failed attempts to encode the current video frame in a row. */
        uint16_t            cFailedEncodingFrames;
    } Video;

    /** Common set of video recording (data) blocks, needed for
     *  multiplexing to all recording streams. */
    VideoRecBlockSet Blocks;
} VIDEORECSTREAM, *PVIDEORECSTREAM;

/** Vector of video recording streams. */
typedef std::vector <PVIDEORECSTREAM> VideoRecStreams;

int videoRecStreamClose(PVIDEORECSTREAM pStream);
PVIDEORECSTREAM videoRecStreamGet(PVIDEORECCONTEXT pCtx, uint32_t uScreen);
int videoRecStreamOpen(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg);
int videoRecStreamUninit(PVIDEORECSTREAM pStream);
int videoRecStreamUnitVideo(PVIDEORECSTREAM pStream);
int videoRecStreamInitVideo(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg);
#ifdef VBOX_WITH_LIBVPX
int videoRecStreamInitVideoVPX(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg);
int videoRecStreamUninitVideoVPX(PVIDEORECSTREAM pStream);
int videoRecStreamWriteVideoVPX(PVIDEORECSTREAM pStream, uint64_t uTimeStampMs, PVIDEORECVIDEOFRAME pFrame);
#endif
void videoRecStreamLock(PVIDEORECSTREAM pStream);
void videoRecStreamUnlock(PVIDEORECSTREAM pStream);

#endif /* ____H_VIDEOREC_STREAM */

