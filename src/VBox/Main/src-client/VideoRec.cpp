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

#ifdef VBOX_WITH_LIBVPX
# define VPX_CODEC_DISABLE_COMPAT 1
# include "vpx/vp8cx.h"
# include "vpx/vpx_image.h"
# include "vpx/vpx_encoder.h"
#endif /* VBOX_WITH_LIBVPX */

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

/**
 * Enumeration for supported pixel formats.
 */
enum VIDEORECPIXELFMT
{
    /** Unknown pixel format. */
    VIDEORECPIXELFMT_UNKNOWN    = 0,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB24      = 1,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB32      = 2,
    /** RGB 565. */
    VIDEORECPIXELFMT_RGB565     = 3,
    /** The usual 32-bit hack. */
    VIDEORECPIXELFMT_32BIT_HACK = 0x7fffffff
};

/**
 * Structure for keeping specific video recording codec data.
 */
typedef struct VIDEORECVIDEOCODEC
{
    union
    {
#ifdef VBOX_WITH_LIBVPX
        struct
        {
            /** VPX codec context. */
            vpx_codec_ctx_t     Ctx;
            /** VPX codec configuration. */
            vpx_codec_enc_cfg_t Cfg;
            /** VPX image context. */
            vpx_image_t         RawImage;
        } VPX;
#endif /* VBOX_WITH_LIBVPX */
    };
} VIDEORECVIDEOCODEC, *PVIDEORECVIDEOCODEC;

/**
 * Structure for keeping a single video recording video frame.
 */
typedef struct VIDEORECVIDEOFRAME
{
    /** X resolution of this frame. */
    uint32_t            uWidth;
    /** Y resolution of this frame. */
    uint32_t            uHeight;
    /** Pixel format of this frame. */
    uint32_t            uPixelFormat;
    /** RGB buffer containing the unmodified frame buffer data from Main's display. */
    uint8_t            *pu8RGBBuf;
    /** Size (in bytes) of the RGB buffer. */
    size_t              cbRGBBuf;
} VIDEORECVIDEOFRAME, *PVIDEORECVIDEOFRAME;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
/**
 * Structure for keeping a single video recording audio frame.
 */
typedef struct VIDEORECAUDIOFRAME
{
    /** Pointer to audio data. */
    uint8_t            *pvBuf;
    /** Size (in bytes) of audio data. */
    size_t              cbBuf;
} VIDEORECAUDIOFRAME, *PVIDEORECAUDIOFRAME;
#endif

/**
 * Enumeration for specifying a video recording block type.
 */
typedef enum VIDEORECBLOCKTYPE
{
    /** Uknown block type, do not use. */
    VIDEORECBLOCKTYPE_UNKNOWN = 0,
    /** The block is a video frame. */
    VIDEORECBLOCKTYPE_VIDEO,
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    /** The block is an audio frame. */
    VIDEORECBLOCKTYPE_AUDIO
#endif
} VIDEORECBLOCKTYPE;

/**
 * Generic structure for keeping a single video recording (data) block.
 */
typedef struct VIDEORECBLOCK
{
    /** The block's type. */
    VIDEORECBLOCKTYPE  enmType;
    /** Number of references held of this block. */
    uint16_t           cRefs;
    /** The (absolute) time stamp (in ms, PTS) of this block. */
    uint64_t           uTimeStampMs;
    /** Opaque data block to the actual block data, depending on the block's type. */
    void              *pvData;
    /** Size (in bytes) of the (opaque) data block. */
    size_t             cbData;
} VIDEORECBLOCK, *PVIDEORECBLOCK;

/** List for keeping video recording (data) blocks. */
typedef std::list<PVIDEORECBLOCK> VideoRecBlockList;

static void videoRecBlockFree(PVIDEORECBLOCK pBlock);

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
            videoRecBlockFree(pBlock);
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
struct VIDEORECSTREAM
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
        /** Minimal delay (in ms) between two frames. */
        uint32_t            uDelayMs;
        /** Target X resolution (in pixels). */
        uint32_t            uWidth;
        /** Target Y resolution (in pixels). */
        uint32_t            uHeight;
        /** Time stamp (in ms) of the last video frame we encoded. */
        uint64_t            uLastTimeStampMs;
        /** Pointer to the codec's internal YUV buffer. */
        uint8_t            *pu8YuvBuf;
        /** Number of failed attempts to encode the current video frame in a row. */
        uint16_t            cFailedEncodingFrames;
    } Video;

    /** Common set of video recording (data) blocks, needed for
     *  multiplexing to all recording streams. */
    VideoRecBlockSet Blocks;
};

/** Vector of video recording streams. */
typedef std::vector <PVIDEORECSTREAM> VideoRecStreams;

/**
 * Structure for keeping a video recording context.
 */
struct VIDEORECCONTEXT
{
    /** Used recording configuration. */
    VIDEORECCFG         Cfg;
    /** The current state. */
    uint32_t            enmState;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT          WaitEvent;
    /** Whether this conext is in started state or not. */
    bool                fStarted;
    /** Shutdown indicator. */
    bool                fShutdown;
    /** Worker thread. */
    RTTHREAD            Thread;
    /** Vector of current recording streams.
     *  Per VM screen (display) one recording stream is being used. */
    VideoRecStreams     vecStreams;
    /** Timestamp (in ms) of when recording has been started. */
    uint64_t            tsStartMs;
    /** Block map of common blocks which need to get multiplexed
     *  to all recording streams. This common block maps should help
     *  reducing the time spent in EMT and avoid doing the (expensive)
     *  multiplexing work in there.
     *
     *  For now this only affects audio, e.g. all recording streams
     *  need to have the same audio data at a specific point in time. */
    VideoRecBlockMap    mapBlocksCommon;
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
 * Iterator class for running through a BGRA32 image buffer and converting
 * it to RGB.
 */
class ColorConvBGRA32Iter
{
private:
    enum { PIX_SIZE = 4 };
public:
    ColorConvBGRA32Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuf)
    {
        LogFlow(("width = %d height=%d aBuf=%lx\n", aWidth, aHeight, aBuf));
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuf = aBuf;
    }
    /**
     * Convert the next pixel to RGB.
     * @returns true on success, false if we have reached the end of the buffer
     * @param   aRed    where to store the red value
     * @param   aGreen  where to store the green value
     * @param   aBlue   where to store the blue value
     */
    bool getRGB(unsigned *aRed, unsigned *aGreen, unsigned *aBlue)
    {
        bool rc = false;
        if (mPos + PIX_SIZE <= mSize)
        {
            *aRed   = mBuf[mPos + 2];
            *aGreen = mBuf[mPos + 1];
            *aBlue  = mBuf[mPos    ];
            mPos += PIX_SIZE;
            rc = true;
        }
        return rc;
    }

    /**
     * Skip forward by a certain number of pixels
     * @param aPixels  how many pixels to skip
     */
    void skip(unsigned aPixels)
    {
        mPos += PIX_SIZE * aPixels;
    }
private:
    /** Size of the picture buffer */
    unsigned mSize;
    /** Current position in the picture buffer */
    unsigned mPos;
    /** Address of the picture buffer */
    uint8_t *mBuf;
};

/**
 * Iterator class for running through an BGR24 image buffer and converting
 * it to RGB.
 */
class ColorConvBGR24Iter
{
private:
    enum { PIX_SIZE = 3 };
public:
    ColorConvBGR24Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuf)
    {
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuf = aBuf;
    }
    /**
     * Convert the next pixel to RGB.
     * @returns true on success, false if we have reached the end of the buffer
     * @param   aRed    where to store the red value
     * @param   aGreen  where to store the green value
     * @param   aBlue   where to store the blue value
     */
    bool getRGB(unsigned *aRed, unsigned *aGreen, unsigned *aBlue)
    {
        bool rc = false;
        if (mPos + PIX_SIZE <= mSize)
        {
            *aRed   = mBuf[mPos + 2];
            *aGreen = mBuf[mPos + 1];
            *aBlue  = mBuf[mPos    ];
            mPos += PIX_SIZE;
            rc = true;
        }
        return rc;
    }

    /**
     * Skip forward by a certain number of pixels
     * @param aPixels  how many pixels to skip
     */
    void skip(unsigned aPixels)
    {
        mPos += PIX_SIZE * aPixels;
    }
private:
    /** Size of the picture buffer */
    unsigned mSize;
    /** Current position in the picture buffer */
    unsigned mPos;
    /** Address of the picture buffer */
    uint8_t *mBuf;
};

/**
 * Iterator class for running through an BGR565 image buffer and converting
 * it to RGB.
 */
class ColorConvBGR565Iter
{
private:
    enum { PIX_SIZE = 2 };
public:
    ColorConvBGR565Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuf)
    {
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuf = aBuf;
    }
    /**
     * Convert the next pixel to RGB.
     * @returns true on success, false if we have reached the end of the buffer
     * @param   aRed    where to store the red value
     * @param   aGreen  where to store the green value
     * @param   aBlue   where to store the blue value
     */
    bool getRGB(unsigned *aRed, unsigned *aGreen, unsigned *aBlue)
    {
        bool rc = false;
        if (mPos + PIX_SIZE <= mSize)
        {
            unsigned uFull =  (((unsigned) mBuf[mPos + 1]) << 8)
                             | ((unsigned) mBuf[mPos]);
            *aRed   = (uFull >> 8) & ~7;
            *aGreen = (uFull >> 3) & ~3 & 0xff;
            *aBlue  = (uFull << 3) & ~7 & 0xff;
            mPos += PIX_SIZE;
            rc = true;
        }
        return rc;
    }

    /**
     * Skip forward by a certain number of pixels
     * @param aPixels  how many pixels to skip
     */
    void skip(unsigned aPixels)
    {
        mPos += PIX_SIZE * aPixels;
    }
private:
    /** Size of the picture buffer */
    unsigned mSize;
    /** Current position in the picture buffer */
    unsigned mPos;
    /** Address of the picture buffer */
    uint8_t *mBuf;
};

#ifdef VBOX_WITH_AUDIO_VIDEOREC
static void videoRecAudioFrameFree(PVIDEORECAUDIOFRAME pFrame);
#endif
static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStream, uint64_t uTimeStampMs, PVIDEORECVIDEOFRAME pFrame);
static int videoRecRGBToYUV(uint32_t uPixelFormat,
                            uint8_t *paDst, uint32_t uDstWidth, uint32_t uDstHeight,
                            uint8_t *paSrc, uint32_t uSrcWidth, uint32_t uSrcHeight);
static int videoRecStreamCloseFile(PVIDEORECSTREAM pStream);
static void videoRecStreamLock(PVIDEORECSTREAM pStream);
static void videoRecStreamUnlock(PVIDEORECSTREAM pStream);
static void videoRecVideoFrameFree(PVIDEORECVIDEOFRAME pFrame);

/**
 * Convert an image to YUV420p format.
 *
 * @return true on success, false on failure.
 * @param  aDstBuf              The destination image buffer.
 * @param  aDstWidth            Width (in pixel) of destination buffer.
 * @param  aDstHeight           Height (in pixel) of destination buffer.
 * @param  aSrcBuf              The source image buffer.
 * @param  aSrcWidth            Width (in pixel) of source buffer.
 * @param  aSrcHeight           Height (in pixel) of source buffer.
 */
template <class T>
inline bool colorConvWriteYUV420p(uint8_t *aDstBuf, unsigned aDstWidth, unsigned aDstHeight,
                                  uint8_t *aSrcBuf, unsigned aSrcWidth, unsigned aSrcHeight)
{
    RT_NOREF(aDstWidth, aDstHeight);

    AssertReturn(!(aSrcWidth & 1),  false);
    AssertReturn(!(aSrcHeight & 1), false);

    bool fRc = true;
    T iter1(aSrcWidth, aSrcHeight, aSrcBuf);
    T iter2 = iter1;
    iter2.skip(aSrcWidth);
    unsigned cPixels = aSrcWidth * aSrcHeight;
    unsigned offY = 0;
    unsigned offU = cPixels;
    unsigned offV = cPixels + cPixels / 4;
    unsigned const cyHalf = aSrcHeight / 2;
    unsigned const cxHalf = aSrcWidth  / 2;
    for (unsigned i = 0; i < cyHalf && fRc; ++i)
    {
        for (unsigned j = 0; j < cxHalf; ++j)
        {
            unsigned red, green, blue;
            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            unsigned u = (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            unsigned v = (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + aSrcWidth] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + aSrcWidth + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            aDstBuf[offU] = u;
            aDstBuf[offV] = v;
            offY += 2;
            ++offU;
            ++offV;
        }

        iter1.skip(aSrcWidth);
        iter2.skip(aSrcWidth);
        offY += aSrcWidth;
    }

    return true;
}

/**
 * Convert an image to RGB24 format
 * @returns true on success, false on failure
 * @param aWidth    width of image
 * @param aHeight   height of image
 * @param aDestBuf  an allocated memory buffer large enough to hold the
 *                  destination image (i.e. width * height * 12bits)
 * @param aSrcBuf   the source image as an array of bytes
 */
template <class T>
inline bool colorConvWriteRGB24(unsigned aWidth, unsigned aHeight,
                                uint8_t *aDestBuf, uint8_t *aSrcBuf)
{
    enum { PIX_SIZE = 3 };
    bool rc = true;
    AssertReturn(0 == (aWidth & 1), false);
    AssertReturn(0 == (aHeight & 1), false);
    T iter(aWidth, aHeight, aSrcBuf);
    unsigned cPixels = aWidth * aHeight;
    for (unsigned i = 0; i < cPixels && rc; ++i)
    {
        unsigned red, green, blue;
        rc = iter.getRGB(&red, &green, &blue);
        if (rc)
        {
            aDestBuf[i * PIX_SIZE    ] = red;
            aDestBuf[i * PIX_SIZE + 1] = green;
            aDestBuf[i * PIX_SIZE + 2] = blue;
        }
    }
    return rc;
}

#ifdef VBOX_WITH_AUDIO_VIDEOREC
/**
 * Frees a previously allocated video recording audio frame.
 *
 * @param   pFrame              Audio frame to free. The pointer will be invalid after return.
 */
static void videoRecAudioFrameFree(PVIDEORECAUDIOFRAME pFrame)
{
    if (!pFrame)
        return;

    if (pFrame->pvBuf)
    {
        Assert(pFrame->cbBuf);
        RTMemFree(pFrame->pvBuf);
    }
    RTMemFree(pFrame);
    pFrame = NULL;
}
#endif

/**
 * Frees a video recording (data) block.
 *
 * @returns IPRT status code.
 * @param   pBlock              Video recording (data) block to free. The pointer will be invalid after return.
 */
static void videoRecBlockFree(PVIDEORECBLOCK pBlock)
{
    if (!pBlock)
        return;

    switch (pBlock->enmType)
    {
        case VIDEORECBLOCKTYPE_VIDEO:
            videoRecVideoFrameFree((PVIDEORECVIDEOFRAME)pBlock->pvData);
            break;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
        case VIDEORECBLOCKTYPE_AUDIO:
            videoRecAudioFrameFree((PVIDEORECAUDIOFRAME)pBlock->pvData);
            break;
#endif
        default:
            AssertFailed();
            break;
    }

    RTMemFree(pBlock);
    pBlock = NULL;
}

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

                    if (pBlock->enmType == VIDEORECBLOCKTYPE_VIDEO)
                    {
                        PVIDEORECVIDEOFRAME pVideoFrame  = (PVIDEORECVIDEOFRAME)pBlock->pvData;

                        rc = videoRecRGBToYUV(pVideoFrame->uPixelFormat,
                                              /* Destination */
                                              pStream->Video.pu8YuvBuf, pVideoFrame->uWidth, pVideoFrame->uHeight,
                                              /* Source */
                                              pVideoFrame->pu8RGBBuf, pStream->Video.uWidth, pStream->Video.uHeight);
                        if (RT_SUCCESS(rc))
                            rc = videoRecEncodeAndWrite(pStream, uTimeStampMs, pVideoFrame);
                    }

                    videoRecBlockFree(pBlock);
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
                            videoRecBlockFree(pBlockCommon);
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
            rc = RTThreadUserWait(pCtx->Thread, 30 * 1000 /* 30s timeout */);

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

            if (pStream->fEnabled)
            {
                switch (pStream->enmDst)
                {
                    case VIDEORECDEST_FILE:
                    {
                        if (pStream->File.pWEBM)
                            pStream->File.pWEBM->Close();
                        break;
                    }

                    default:
                        AssertFailed(); /* Should never happen. */
                        break;
                }

                vpx_img_free(&pStream->Video.Codec.VPX.RawImage);
                vpx_codec_err_t rcv = vpx_codec_destroy(&pStream->Video.Codec.VPX.Ctx);
                Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

                pStream->Blocks.Clear();

                LogRel(("VideoRec: Recording screen #%u stopped\n", pStream->uScreenID));
            }

            switch (pStream->enmDst)
            {
                case VIDEORECDEST_FILE:
                {
                    int rc2 = videoRecStreamCloseFile(pStream);
                    AssertRC(rc2);

                    if (pStream->File.pWEBM)
                    {
                        delete pStream->File.pWEBM;
                        pStream->File.pWEBM = NULL;
                    }
                    break;
                }

                default:
                    AssertFailed(); /* Should never happen. */
                    break;
            }

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
 * Retrieves a specific recording stream of a recording context.
 *
 * @returns Pointer to recording stream if found, or NULL if not found.
 * @param   pCtx                Recording context to look up stream for.
 * @param   uScreen             Screen number of recording stream to look up.
 */
DECLINLINE(PVIDEORECSTREAM) videoRecStreamGet(PVIDEORECCONTEXT pCtx, uint32_t uScreen)
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
static void videoRecStreamLock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectEnter(&pStream->CritSect);
    AssertRC(rc);
}

/**
 * Unlocks a locked recording stream.
 *
 * @param   pStream             Recording stream to unlock.
 */
static void videoRecStreamUnlock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectLeave(&pStream->CritSect);
    AssertRC(rc);
}

/**
 * Opens a file for a given recording stream to capture to.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to open file for.
 * @param   pCfg                Recording configuration to use.
 */
static int videoRecStreamOpenFile(PVIDEORECSTREAM pStream, PVIDEORECCFG pCfg)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,    VERR_INVALID_POINTER);

    Assert(pStream->enmDst == VIDEORECDEST_INVALID);
    Assert(pCfg->enmDst    == VIDEORECDEST_FILE);

    Assert(pCfg->File.strName.isNotEmpty());

    char *pszAbsPath = RTPathAbsDup(com::Utf8Str(pCfg->File.strName).c_str());
    AssertPtrReturn(pszAbsPath, VERR_NO_MEMORY);

    RTPathStripSuffix(pszAbsPath);

    char *pszSuff = RTStrDup(".webm");
    if (!pszSuff)
    {
        RTStrFree(pszAbsPath);
        return VERR_NO_MEMORY;
    }

    char *pszFile = NULL;

    int rc;
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

    return rc;
}

/**
 * Closes a recording stream's file again.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to close file for.
 */
static int videoRecStreamCloseFile(PVIDEORECSTREAM pStream)
{
    Assert(pStream->enmDst == VIDEORECDEST_FILE);

    pStream->enmDst = VIDEORECDEST_INVALID;

    AssertPtr(pStream->File.pszFile);

    if (RTFileIsValid(pStream->File.hFile))
    {
        RTFileClose(pStream->File.hFile);
        LogRel(("VideoRec: Closed file '%s'\n", pStream->File.pszFile));
    }

    RTStrFree(pStream->File.pszFile);
    pStream->File.pszFile = NULL;

    return VINF_SUCCESS;
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

    int rc = videoRecStreamOpenFile(pStream, &pCtx->Cfg);
    if (RT_FAILURE(rc))
        return rc;

    pStream->pCtx = pCtx;

    /** @todo Make the following parameters configurable on a per-stream basis? */
    pStream->Video.uWidth                = pCfg->Video.uWidth;
    pStream->Video.uHeight               = pCfg->Video.uHeight;
    pStream->Video.cFailedEncodingFrames = 0;

    PVIDEORECVIDEOCODEC pVC = &pStream->Video.Codec;

    pStream->Video.uDelayMs = 1000 / pCfg->Video.uFPS;

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

                LogRel(("VideoRec: Recording screen #%u with %RU32x%RU32 @ %RU32 kbps, %RU32 FPS\n",
                        uScreen, pCfg->Video.uWidth, pCfg->Video.uHeight, pCfg->Video.uRate, pCfg->Video.uFPS));
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

                LogRel(("VideoRec: Recording audio in %RU16Hz, %RU8 bit, %RU8 %s\n",
                        pCfg->Audio.uHz, pCfg->Audio.cBits, pCfg->Audio.cChannels, pCfg->Audio.cChannels ? "channel" : "channels"));
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
        return rc;

#ifdef VBOX_WITH_LIBVPX
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
    pStream->Video.pu8YuvBuf = pVC->VPX.RawImage.planes[0];
#endif
    pStream->fEnabled = true;

    return VINF_SUCCESS;
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
 * Checks if recording engine is ready to accept a new frame for the given screen.
 *
 * @returns true if recording engine is ready.
 * @param   pCtx                Pointer to video recording context.
 * @param   uScreen             Screen ID.
 * @param   uTimeStampMs        Current time stamp (in ms).
 */
bool VideoRecIsReady(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t uTimeStampMs)
{
    AssertPtrReturn(pCtx, false);

    if (ASMAtomicReadU32(&pCtx->enmState) != VIDEORECSTS_INITIALIZED)
        return false;

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (   !pStream
        || !pStream->fEnabled)
    {
        return false;
    }

    if (uTimeStampMs < pStream->Video.uLastTimeStampMs + pStream->Video.uDelayMs)
        return false;

    return true;
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
        && tsNowMs >= pCtx->tsStartMs + (pCfg->uMaxTimeS * 1000))
    {
        return true;
    }

    if (pCfg->enmDst == VIDEORECDEST_FILE)
    {

        if (pCfg->File.uMaxSizeMB)
        {
            uint64_t sizeInMB = pStream->File.pWEBM->GetFileSize() / (1024 * 1024);
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
 * Encodes the source image and write the encoded image to the stream's destination.
 *
 * @returns IPRT status code.
 * @param   pStream             Stream to encode and submit to.
 * @param   uTimeStampMs        Absolute timestamp (PTS) of frame (in ms) to encode.
 * @param   pFrame              Frame to encode and submit.
 */
static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStream, uint64_t uTimeStampMs, PVIDEORECVIDEOFRAME pFrame)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pFrame,  VERR_INVALID_POINTER);

    int rc;

    AssertPtr(pStream->pCtx);
    PVIDEORECCFG        pCfg = &pStream->pCtx->Cfg;
    PVIDEORECVIDEOCODEC pVC  = &pStream->Video.Codec;
#ifdef VBOX_WITH_LIBVPX
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
#else
    RT_NOREF(pStream);
    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_LIBVPX */
    return rc;
}

/**
 * Converts a RGB to YUV buffer.
 *
 * @returns IPRT status code.
 * TODO
 */
static int videoRecRGBToYUV(uint32_t uPixelFormat,
                            uint8_t *paDst, uint32_t uDstWidth, uint32_t uDstHeight,
                            uint8_t *paSrc, uint32_t uSrcWidth, uint32_t uSrcHeight)
{
    switch (uPixelFormat)
    {
        case VIDEORECPIXELFMT_RGB32:
            if (!colorConvWriteYUV420p<ColorConvBGRA32Iter>(paDst, uDstWidth, uDstHeight,
                                                            paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB24:
            if (!colorConvWriteYUV420p<ColorConvBGR24Iter>(paDst, uDstWidth, uDstHeight,
                                                           paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB565:
            if (!colorConvWriteYUV420p<ColorConvBGR565Iter>(paDst, uDstWidth, uDstHeight,
                                                            paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        default:
            AssertFailed();
            return VERR_NOT_SUPPORTED;
    }
    return VINF_SUCCESS;
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

        RTFILE fh;
        int rc2 = RTFileOpen(&fh, "/tmp/VideoRecFrame.bmp",
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
        videoRecVideoFrameFree(pFrame);

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

/**
 * Frees a video recording video frame.
 *
 * @returns IPRT status code.
 * @param   pFrame              Pointer to video frame to free. The pointer will be invalid after return.
 */
static void videoRecVideoFrameFree(PVIDEORECVIDEOFRAME pFrame)
{
    if (!pFrame)
        return;

    if (pFrame->pu8RGBBuf)
    {
        Assert(pFrame->cbRGBBuf);
        RTMemFree(pFrame->pu8RGBBuf);
    }
    RTMemFree(pFrame);
}

