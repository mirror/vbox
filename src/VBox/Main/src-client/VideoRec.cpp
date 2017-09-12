/* $Id$ */
/** @file
 * Video capturing utility routines.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_DISPLAY
#include "LoggingNew.h"

#include <stdexcept>
#include <vector>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include "EbmlWriter.h"
#include "VideoRec.h"

#ifdef VBOX_WITH_LIBVPX
# define VPX_CODEC_DISABLE_COMPAT 1
# include <vpx/vp8cx.h>
# include <vpx/vpx_image.h>

/** Default VPX codec to use. */
# define DEFAULTCODEC (vpx_codec_vp8_cx())
#endif /* VBOX_WITH_LIBVPX */

struct VIDEORECVIDEOFRAME;
typedef struct VIDEORECVIDEOFRAME *PVIDEORECVIDEOFRAME;

static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStream, PVIDEORECVIDEOFRAME pFrame);
static int videoRecRGBToYUV(uint32_t uPixelFormat,
                            uint8_t *paDst, uint32_t uDstWidth, uint32_t uDstHeight,
                            uint8_t *paSrc, uint32_t uSrcWidth, uint32_t uSrcHeight);

int videoRecStreamLock(PVIDEORECSTREAM pStream);
int videoRecStreamUnlock(PVIDEORECSTREAM pStream);

using namespace com;

#if 0
/** Enables support for encoding multiple audio / video data frames at once. */
#define VBOX_VIDEOREC_WITH_QUEUE
#endif
#ifdef DEBUG_andy
/** Enables dumping audio / video data for debugging reasons. */
# define VBOX_VIDEOREC_DUMP
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
            /** Encoder deadline. */
            unsigned int        uEncoderDeadline;
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
    /** Time stamp (in ms). */
    uint64_t            uTimeStampMs;
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
    uint8_t             abBuf[_64K]; /** @todo Fix! */
    size_t              cbBuf;
    /** Time stamp (in ms). */
    uint64_t            uTimeStampMs;
} VIDEORECAUDIOFRAME, *PVIDEORECAUDIOFRAME;
#endif

/**
 * Strucutre for maintaining a video recording stream.
 */
typedef struct VIDEORECSTREAM
{
    /** Container context. */
    WebMWriter         *pEBML;
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
#ifdef VBOX_VIDEOREC_WITH_QUEUE
# error "Implement me!"
#else
        VIDEORECVIDEOFRAME  Frame;
        bool                fHasVideoData;
#endif
    } Video;
} VIDEORECSTREAM, *PVIDEORECSTREAM;

/** Vector of video recording streams. */
typedef std::vector <PVIDEORECSTREAM> VideoRecStreams;

/**
 * Structure for keeping a video recording context.
 */
typedef struct VIDEORECCONTEXT
{
    /** The current state. */
    uint32_t            enmState;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT          WaitEvent;
    /** Whether recording is enabled or not. */
    bool                fEnabled;
    /** Shutdown indicator. */
    bool                fShutdown;
    /** Worker thread. */
    RTTHREAD            Thread;
    /** Maximal time (in ms) to record. */
    uint64_t            uMaxTimeMs;
    /** Maximal file size (in MB) to record. */
    uint32_t            uMaxSizeMB;
    /** Vector of current recording stream contexts. */
    VideoRecStreams     vecStreams;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    struct
    {
        bool                fHasAudioData;
        VIDEORECAUDIOFRAME  Frame;
    } Audio;
#endif
} VIDEORECCONTEXT, *PVIDEORECCONTEXT;

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

/**
 * Worker thread for all streams of a video recording context.
 *
 * Does RGB/YUV conversion and encoding.
 */
static DECLCALLBACK(int) videoRecThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)pvUser;

    /* Signal that we're up and rockin'. */
    RTThreadUserSignal(hThreadSelf);

    for (;;)
    {
        int rc = RTSemEventWait(pCtx->WaitEvent, RT_INDEFINITE_WAIT);
        AssertRCBreak(rc);

        if (ASMAtomicReadBool(&pCtx->fShutdown))
            break;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
        VIDEORECAUDIOFRAME audioFrame;
        RT_ZERO(audioFrame);

        int rc2 = RTCritSectEnter(&pCtx->CritSect);
        AssertRC(rc2);

        const bool fEncodeAudio = pCtx->Audio.fHasAudioData;
        if (fEncodeAudio)
        {
            /*
             * Every recording stream needs to get the same audio data at a certain point in time.
             * Do the multiplexing here to not block EMT for too long.
             *
             * For now just doing a simple copy of the current audio frame should be good enough.
             */
            memcpy(&audioFrame, &pCtx->Audio.Frame, sizeof(VIDEORECAUDIOFRAME));

            pCtx->Audio.fHasAudioData = false;
        }

        rc2 = RTCritSectLeave(&pCtx->CritSect);
        AssertRC(rc2);
#endif

        /** @todo r=andy This is inefficient -- as we already wake up this thread
         *               for every screen from Main, we here go again (on every wake up) through
         *               all screens.  */
        for (VideoRecStreams::iterator it = pCtx->vecStreams.begin(); it != pCtx->vecStreams.end(); it++)
        {
            PVIDEORECSTREAM pStream = (*it);

            videoRecStreamLock(pStream);

            if (!pStream->fEnabled)
            {
                videoRecStreamUnlock(pStream);
                continue;
            }

            PVIDEORECVIDEOFRAME pVideoFrame = &pStream->Video.Frame;
            const bool fEncodeVideo = pStream->Video.fHasVideoData;

            if (fEncodeVideo)
            {
                rc = videoRecRGBToYUV(pVideoFrame->uPixelFormat,
                                      /* Destination */
                                      pStream->Video.pu8YuvBuf, pVideoFrame->uWidth, pVideoFrame->uHeight,
                                      /* Source */
                                      pVideoFrame->pu8RGBBuf, pStream->Video.uWidth, pStream->Video.uHeight);
                if (RT_SUCCESS(rc))
                    rc = videoRecEncodeAndWrite(pStream, pVideoFrame);

                pStream->Video.fHasVideoData = false;
            }

            videoRecStreamUnlock(pStream);

            if (RT_FAILURE(rc))
            {
                static unsigned s_cErrEncVideo = 0;
                if (s_cErrEncVideo < 32)
                {
                    LogRel(("VideoRec: Error %Rrc encoding / writing video frame\n", rc));
                    s_cErrEncVideo++;
                }
            }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
            /* Each (enabled) screen has to get the same audio data. */
            if (fEncodeAudio)
            {
                Assert(audioFrame.cbBuf);
                Assert(audioFrame.cbBuf <= _64K); /** @todo Fix. */

                WebMWriter::BlockData_Opus blockData = { audioFrame.abBuf, audioFrame.cbBuf, audioFrame.uTimeStampMs };
                rc = pStream->pEBML->WriteBlock(pStream->uTrackAudio, &blockData, sizeof(blockData));
                if (RT_FAILURE(rc))
                {
                    static unsigned s_cErrEncAudio = 0;
                    if (s_cErrEncAudio < 32)
                    {
                        LogRel(("VideoRec: Error %Rrc encoding audio frame\n", rc));
                        s_cErrEncAudio++;
                    }
                }
            }
#endif
        }

        /* Keep going in case of errors. */

    } /* for */

    return VINF_SUCCESS;
}

/**
 * Creates a video recording context.
 *
 * @returns IPRT status code.
 * @param   cScreens         Number of screens to create context for.
 * @param   ppCtx            Pointer to created video recording context on success.
 */
int VideoRecContextCreate(uint32_t cScreens, PVIDEORECCONTEXT *ppCtx)
{
    AssertReturn(cScreens, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppCtx, VERR_INVALID_POINTER);

    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)RTMemAllocZ(sizeof(VIDEORECCONTEXT));
    if (!pCtx)
        return VERR_NO_MEMORY;

    int rc = RTCritSectInit(&pCtx->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    for (uint32_t uScreen = 0; uScreen < cScreens; uScreen++)
    {
        PVIDEORECSTREAM pStream = (PVIDEORECSTREAM)RTMemAllocZ(sizeof(VIDEORECSTREAM));
        if (!pStream)
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

            pStream->pEBML = new WebMWriter();
        }
        catch (std::bad_alloc)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pCtx->enmState  = VIDEORECSTS_UNINITIALIZED;
        pCtx->fShutdown = false;

        rc = RTSemEventCreate(&pCtx->WaitEvent);
        AssertRCReturn(rc, rc);

        rc = RTThreadCreate(&pCtx->Thread, videoRecThread, (void *)pCtx, 0,
                            RTTHREADTYPE_MAIN_WORKER, RTTHREADFLAGS_WAITABLE, "VideoRec");

        if (RT_SUCCESS(rc)) /* Wait for the thread to start. */
            rc = RTThreadUserWait(pCtx->Thread, 30 * 1000 /* 30s timeout */);

        if (RT_SUCCESS(rc))
        {
            pCtx->enmState = VIDEORECSTS_INITIALIZED;
            pCtx->fEnabled = true;

            if (ppCtx)
                *ppCtx = pCtx;
        }
    }

    if (RT_FAILURE(rc))
    {
        /* Roll back allocations on error. */
        VideoRecStreams::iterator it = pCtx->vecStreams.begin();
        while (it != pCtx->vecStreams.end())
        {
            PVIDEORECSTREAM pStream = (*it);

            if (pStream->pEBML)
                delete pStream->pEBML;

            it = pCtx->vecStreams.erase(it);

            RTMemFree(pStream);
            pStream = NULL;
        }

        Assert(pCtx->vecStreams.empty());
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

    /* Set shutdown indicator. */
    ASMAtomicWriteBool(&pCtx->fShutdown, true);

    /* Signal the thread. */
    RTSemEventSignal(pCtx->WaitEvent);

    int rc = RTThreadWait(pCtx->Thread, 10 * 1000 /* 10s timeout */, NULL);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTSemEventDestroy(pCtx->WaitEvent);
    AssertRC(rc);

    pCtx->WaitEvent = NIL_RTSEMEVENT;

    VideoRecStreams::iterator it = pCtx->vecStreams.begin();
    while (it != pCtx->vecStreams.end())
    {
        PVIDEORECSTREAM pStream = (*it);

        if (pStream->fEnabled)
        {
            AssertPtr(pStream->pEBML);
            pStream->pEBML->Close();

            vpx_img_free(&pStream->Video.Codec.VPX.RawImage);
            vpx_codec_err_t rcv = vpx_codec_destroy(&pStream->Video.Codec.VPX.Ctx);
            Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

#ifdef VBOX_VIDEOREC_WITH_QUEUE
# error "Implement me!"
#else
            PVIDEORECVIDEOFRAME pFrame = &pStream->Video.Frame;
#endif
            if (pFrame->pu8RGBBuf)
            {
                Assert(pFrame->cbRGBBuf);

                RTMemFree(pFrame->pu8RGBBuf);
                pFrame->pu8RGBBuf = NULL;
            }

            pFrame->cbRGBBuf = 0;

            LogRel(("VideoRec: Recording screen #%u stopped\n", pStream->uScreenID));
        }

        if (pStream->pEBML)
        {
            delete pStream->pEBML;
            pStream->pEBML = NULL;
        }

        it = pCtx->vecStreams.erase(it);

        RTCritSectDelete(&pStream->CritSect);

        RTMemFree(pStream);
        pStream = NULL;
    }

    Assert(pCtx->vecStreams.empty());

    RTCritSectDelete(&pCtx->CritSect);

    RTMemFree(pCtx);
    pCtx = NULL;

    return VINF_SUCCESS;
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
    catch (std::out_of_range)
    {
        pStream = NULL;
    }

    return pStream;
}

/**
 * Locks a recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to lock.
 */
int videoRecStreamLock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectEnter(&pStream->CritSect);
    AssertRC(rc);

    return rc;
}

/**
 * Unlocks a locked recording stream.
 *
 * @returns IPRT status code.
 * @param   pStream             Recording stream to unlock.
 */
int videoRecStreamUnlock(PVIDEORECSTREAM pStream)
{
    int rc = RTCritSectLeave(&pStream->CritSect);
    AssertRC(rc);

    return rc;
}

/**
 * VideoRec utility function to initialize video recording context.
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to video recording context.
 * @param   uScreen             Screen number to record.
 * @param   pszFile             File to save recording to.
 * @param   uWidth              Target video resolution (width).
 * @param   uHeight             Target video resolution (height).
 * @param   uRate               Target encoding bit rate.
 * @param   uFPS                Target FPS (Frame Per Second).
 * @param   uMaxTimeS           Maximum time (in s) to record, or 0 for no time limit.
 * @param   uMaxSizeMB          Maximum file size (in MB) to record, or 0 for no limit.
 * @param   pszOptions          Additional options in "key=value" array format. Optional.
 */
int VideoRecStreamInit(PVIDEORECCONTEXT pCtx, uint32_t uScreen, const char *pszFile,
                       uint32_t uWidth, uint32_t uHeight, uint32_t uRate, uint32_t uFPS,
                       uint32_t uMaxTimeS, uint32_t uMaxSizeMB, const char *pszOptions)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(uWidth,     VERR_INVALID_PARAMETER);
    AssertReturn(uHeight,    VERR_INVALID_PARAMETER);
    AssertReturn(uRate,      VERR_INVALID_PARAMETER);
    AssertReturn(uFPS,       VERR_INVALID_PARAMETER);
    /* pszOptions is optional. */

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (!pStream)
        return VERR_NOT_FOUND;

    pCtx->uMaxTimeMs = (uMaxTimeS > 0 ? RTTimeProgramMilliTS() + uMaxTimeS * 1000 : 0);
    pCtx->uMaxSizeMB = uMaxSizeMB;

    pStream->Video.uWidth  = uWidth;
    pStream->Video.uHeight = uHeight;

#ifndef VBOX_VIDEOREC_WITH_QUEUE
    /* When not using a queue, we only use one frame per stream at once.
     * So do the initialization here. */
    PVIDEORECVIDEOFRAME pFrame = &pStream->Video.Frame;

    const size_t cbRGBBuf =   pStream->Video.uWidth
                            * pStream->Video.uHeight
                            * 4 /* 32 BPP maximum */;
    AssertReturn(cbRGBBuf, VERR_INVALID_PARAMETER);

    pFrame->pu8RGBBuf = (uint8_t *)RTMemAllocZ(cbRGBBuf);
    AssertReturn(pFrame->pu8RGBBuf, VERR_NO_MEMORY);
    pFrame->cbRGBBuf  = cbRGBBuf;
#endif

    PVIDEORECVIDEOCODEC pVC = &pStream->Video.Codec;

#ifdef VBOX_WITH_LIBVPX
    pVC->VPX.uEncoderDeadline = VPX_DL_REALTIME;

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(DEFAULTCODEC, &pVC->VPX.Cfg, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("VideoRec: Failed to get default configuration for VPX codec: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }
#endif

    com::Utf8Str options(pszOptions);
    size_t pos = 0;

    /* By default we enable everything (if available). */
    bool     fHasVideoTrack = true;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    bool     fHasAudioTrack = true;
    /* By default we use 48kHz, 16-bit, stereo for the audio track. */
    uint16_t uAudioHz       = 48000;
    uint8_t  uAudioBits     = 16;
    uint8_t  uAudioChannels = 2;
#endif

    com::Utf8Str key, value;
    while ((pos = options.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("vc_quality", Utf8Str::CaseInsensitive) == 0)
        {
            if (value.compare("realtime", Utf8Str::CaseInsensitive) == 0)
            {
#ifdef VBOX_WITH_LIBVPX
                pVC->VPX.uEncoderDeadline = VPX_DL_REALTIME;
#endif
            }
            else if (value.compare("good", Utf8Str::CaseInsensitive) == 0)
            {
                pVC->VPX.uEncoderDeadline = 1000000 / uFPS;
            }
            else if (value.compare("best", Utf8Str::CaseInsensitive) == 0)
            {
#ifdef VBOX_WITH_LIBVPX
                pVC->VPX.uEncoderDeadline = VPX_DL_BEST_QUALITY;
#endif
            }
            else
            {
                LogRel(("VideoRec: Setting quality deadline to '%s'\n", value.c_str()));
                pVC->VPX.uEncoderDeadline = value.toUInt32();
            }
        }
        else if (key.compare("vc_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (value.compare("false", Utf8Str::CaseInsensitive) == 0)
            {
                fHasVideoTrack = false;
                LogRel(("VideoRec: Only audio will be recorded\n"));
            }
#endif
        }
        else if (key.compare("ac_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (value.compare("false", Utf8Str::CaseInsensitive))
            {
                fHasAudioTrack = false;
                LogRel(("VideoRec: Only video will be recorded\n"));
            }
#endif
        }
        else if (key.compare("ac_profile", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (value.compare("low", Utf8Str::CaseInsensitive) == 0)
            {
                uAudioHz       = 8000;
                uAudioBits     = 16;
                uAudioChannels = 1;
            }
            else if (value.startsWith("med" /* "med[ium]" */, Utf8Str::CaseInsensitive) == 0)
            {
                uAudioHz       = 22050;
                uAudioBits     = 16;
                uAudioChannels = 2;
            }
            else if (value.compare("high", Utf8Str::CaseInsensitive) == 0)
            {
                /* Stay with the default set above. */
            }
#endif
        }
        else
            LogRel(("VideoRec: Unknown option '%s' (value '%s'), skipping\n", key.c_str(), value.c_str()));

    } /* while */

    uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_WRITE;
#ifdef DEBUG
    fOpen |= RTFILE_O_CREATE_REPLACE;
#else
    /* Play safe: the file must not exist, overwriting is potentially
     * hazardous as nothing prevents the user from picking a file name of some
     * other important file, causing unintentional data loss. */
    fOpen |= RTFILE_O_CREATE;
#endif

    int rc = pStream->pEBML->Create(pszFile, fOpen,
#ifdef VBOX_WITH_AUDIO_VIDEOREC
                                    fHasAudioTrack ? WebMWriter::AudioCodec_Opus : WebMWriter::AudioCodec_None,
#else
                                    WebMWriter::AudioCodec_None,
#endif
                                    fHasVideoTrack ? WebMWriter::VideoCodec_VP8 : WebMWriter::VideoCodec_None);
    if (RT_FAILURE(rc))
    {
        LogRel(("VideoRec: Failed to create the video capture output file '%s' (%Rrc)\n", pszFile, rc));
        return rc;
    }

    pStream->Video.uDelayMs = 1000 / uFPS;

    if (fHasVideoTrack)
    {
        rc = pStream->pEBML->AddVideoTrack(uWidth, uHeight, uFPS, &pStream->uTrackVideo);
        if (RT_FAILURE(rc))
        {
            LogRel(("VideoRec: Failed to add video track to output file '%s' (%Rrc)\n", pszFile, rc));
            return rc;
        }

        LogRel(("VideoRec: Recording screen #%u with %ux%u @ %u kbps, %u FPS\n",
                uScreen, uWidth, uHeight, uRate, uFPS));
    }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
    if (fHasAudioTrack)
    {
        rc = pStream->pEBML->AddAudioTrack(uAudioHz, uAudioChannels, uAudioBits, &pStream->uTrackAudio);
        if (RT_FAILURE(rc))
        {
            LogRel(("VideoRec: Failed to add audio track to output file '%s' (%Rrc)\n", pszFile, rc));
            return rc;
        }

        LogRel(("VideoRec: Recording audio in %RU16Hz, %RU8 bit, %RU8 %s\n",
                uAudioHz, uAudioBits, uAudioChannels, uAudioChannels == 1 ? "channel" : "channels"));
    }
#endif

    if (   fHasVideoTrack
#ifdef VBOX_WITH_AUDIO_VIDEOREC
        || fHasAudioTrack
#endif
       )
    {
        char szWhat[32] = { 0 };
        if (fHasVideoTrack)
            RTStrCat(szWhat, sizeof(szWhat), "video");
#ifdef VBOX_WITH_AUDIO_VIDEOREC
        if (fHasAudioTrack)
        {
            if (fHasVideoTrack)
                RTStrCat(szWhat, sizeof(szWhat), " + ");
            RTStrCat(szWhat, sizeof(szWhat), "audio");
        }
#endif
        LogRel(("Recording %s to '%s'\n", szWhat, pszFile));
    }

#ifdef VBOX_WITH_LIBVPX
    /* Target bitrate in kilobits per second. */
    pVC->VPX.Cfg.rc_target_bitrate = uRate;
    /* Frame width. */
    pVC->VPX.Cfg.g_w = uWidth;
    /* Frame height. */
    pVC->VPX.Cfg.g_h = uHeight;
    /* 1ms per frame. */
    pVC->VPX.Cfg.g_timebase.num = 1;
    pVC->VPX.Cfg.g_timebase.den = 1000;
    /* Disable multithreading. */
    pVC->VPX.Cfg.g_threads = 0;

    /* Initialize codec. */
    rcv = vpx_codec_enc_init(&pVC->VPX.Ctx, DEFAULTCODEC, &pVC->VPX.Cfg, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to initialize VP8 encoder %s", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }

    if (!vpx_img_alloc(&pVC->VPX.RawImage, VPX_IMG_FMT_I420, uWidth, uHeight, 1))
    {
        LogFlow(("Failed to allocate image %dx%d", uWidth, uHeight));
        return VERR_NO_MEMORY;
    }

    /* Save a pointer to the first raw YUV plane. */
    pStream->Video.pu8YuvBuf = pVC->VPX.RawImage.planes[0];
#endif
    pStream->fEnabled = true;

    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to check if recording is enabled.
 *
 * @returns true if recording is enabled.
 * @param   pCtx                Pointer to video recording context.
 */
bool VideoRecIsEnabled(PVIDEORECCONTEXT pCtx)
{
    if (!pCtx)
        return false;

    return ASMAtomicReadBool(&pCtx->fEnabled);
}

/**
 * VideoRec utility function to check if recording engine is ready to accept a new frame
 * for the given screen.
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

    PVIDEORECVIDEOFRAME pLastFrame = &pStream->Video.Frame;

    if (uTimeStampMs < pLastFrame->uTimeStampMs + pStream->Video.uDelayMs)
        return false;

    return true;
}

/**
 * VideoRec utility function to check if a specified limit for recording
 * has been reached.
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

    if (   pCtx->uMaxTimeMs
        && tsNowMs >= pCtx->uMaxTimeMs)
    {
        return true;
    }

    if (pCtx->uMaxSizeMB)
    {
        uint64_t sizeInMB = pStream->pEBML->GetFileSize() / (1024 * 1024);
        if(sizeInMB >= pCtx->uMaxSizeMB)
            return true;
    }

    /* Check for available free disk space */
    if (   pStream->pEBML
        && pStream->pEBML->GetAvailableSpace() < 0x100000) /**@todo r=andy WTF? Fix this. */
    {
        LogRel(("VideoRec: Not enough free storage space available, stopping video capture\n"));
        return true;
    }

    return false;
}

/**
 * VideoRec utility function to encode the source image and write the encoded
 * image to target file.
 *
 * @returns IPRT status code.
 * @param   pStream             Stream to encode and submit to.
 * @param   pFrame              Frame to encode and submit.
 */
static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStream, PVIDEORECVIDEOFRAME pFrame)
{
    int rc;

    PVIDEORECVIDEOCODEC pVC = &pStream->Video.Codec;

#ifdef VBOX_WITH_LIBVPX
    /* Presentation Time Stamp (PTS). */
    vpx_codec_pts_t pts = pFrame->uTimeStampMs;
    vpx_codec_err_t rcv = vpx_codec_encode(&pVC->VPX.Ctx,
                                           &pVC->VPX.RawImage,
                                           pts                       /* Time stamp */,
                                           pStream->Video.uDelayMs   /* How long to show this frame */,
                                           0                         /* Flags */,
                                           pVC->VPX.uEncoderDeadline /* Quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        LogFunc(("Failed to encode video frame: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

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
                rc = pStream->pEBML->WriteBlock(pStream->uTrackVideo, &blockData, sizeof(blockData));
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
 * VideoRec utility function to convert RGB to YUV.
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
 * @param   cbData              Size (in bytes) of audio frame data.
 * @param   uTimeStampMs        Time stamp (in ms) of audio playback.
 */
int VideoRecSendAudioFrame(PVIDEORECCONTEXT pCtx, const void *pvData, size_t cbData, uint64_t uTimeStampMs)
{
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    AssertReturn(cbData <= _64K, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pCtx->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    /* To save time spent in EMT, do the required audio multiplexing in the encoding thread.
     *
     * The multiplexing is needed to supply all recorded (enabled) screens with the same
     * audio data at the same given point in time.
     */
    PVIDEORECAUDIOFRAME pFrame = &pCtx->Audio.Frame;

    memcpy(pFrame->abBuf, pvData, RT_MIN(_64K /** @todo Fix! */, cbData));

    pFrame->cbBuf        = cbData;
    pFrame->uTimeStampMs = uTimeStampMs;

    pCtx->Audio.fHasAudioData = true;

    rc = RTCritSectLeave(&pCtx->CritSect);
    if (RT_SUCCESS(rc))
        rc = RTSemEventSignal(pCtx->WaitEvent);

    return rc;
#else
    RT_NOREF(pCtx, pvData, cbData, uTimeStampMs);
    return VINF_SUCCESS;
#endif
}

/**
 * VideoRec utility function to copy a source video frame to the intermediate
 * RGB buffer. This function is executed only once per time.
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

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (!pStream)
        return VERR_NOT_FOUND;

    int rc = RTCritSectEnter(&pStream->CritSect);
    if (RT_FAILURE(rc))
        return rc;

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

#ifdef VBOX_VIDEOREC_WITH_QUEUE
# error "Implement me!"
#else
        PVIDEORECVIDEOFRAME pFrame = &pStream->Video.Frame;
#endif
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

#ifndef VBOX_VIDEOREC_WITH_QUEUE
        /* If we don't use a queue then we have to compare the dimensions
         * of the current frame with the previous frame:
         *
         * If it's smaller than before then clear the entire buffer to prevent artifacts
         * from the previous frame. */
        if (   uSrcWidth  < pFrame->uWidth
            || uSrcHeight < pFrame->uHeight)
        {
            /** @todo r=andy Only clear dirty areas. */
            RT_BZERO(pFrame->pu8RGBBuf, pFrame->cbRGBBuf);
        }
#endif
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
        pFrame->uTimeStampMs = uTimeStampMs;
        pFrame->uWidth       = uSrcWidth;
        pFrame->uHeight      = uSrcHeight;

        pStream->Video.fHasVideoData = true;

    } while (0);

    int rc2 = RTCritSectLeave(&pStream->CritSect);
    if (RT_SUCCESS(rc2))
    {
        if (   RT_SUCCESS(rc)
            && rc != VINF_TRY_AGAIN) /* Only signal the thread if operation was successful. */
        {
            rc2 = RTSemEventSignal(pCtx->WaitEvent);
        }
    }

    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}
