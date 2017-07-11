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

static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStrm);
static int videoRecRGBToYUV(PVIDEORECSTREAM pStrm);

using namespace com;

/**
 * Enumeration for a video recording state.
 */
enum VIDEORECSTS
{
    /** Not initialized. */
    VIDEORECSTS_UNINITIALIZED = 0,
    /** Initialized, idle. */
    VIDEORECSTS_IDLE          = 1,
    /** Currently busy, delay termination. */
    VIDEORECSTS_BUSY          = 2,
    /** The usual 32-bit hack. */
    VIDEORECSTS_32BIT_HACK    = 0x7fffffff
};

/**
 * Enumeration for supported pixel formats.
 */
enum VIDEORECPIXELFMT
{
    /** Unknown pixel format. */
    VIDEORECPIXELFMT_UNKNOWN = 0,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB24   = 1,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB32   = 2,
    /** RGB 565. */
    VIDEORECPIXELFMT_RGB565  = 3
};

/**
 * Structure for keeping specific video recording codec data.
 */
typedef struct VIDEORECCODEC
{
    union
    {
#ifdef VBOX_WITH_LIBVPX
        struct
        {
            /** VPX codec context. */
            vpx_codec_ctx_t     CodecCtx;
            /** VPX codec configuration. */
            vpx_codec_enc_cfg_t Config;
            /** VPX image context. */
            vpx_image_t         RawImage;
        } VPX;
#endif /* VBOX_WITH_LIBVPX */
    };
} VIDEORECCODEC, *PVIDEORECCODEC;

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
    /** Codec data. */
    VIDEORECCODEC       Codec;
    /** Screen ID. */
    uint16_t            uScreen;
    /** Whether video recording is enabled or not. */
    bool                fEnabled;
    /** Time stamp (in ms) of the last frame we encoded. */
    uint64_t            uLastTimeStampMs;
    /** Time stamp (in ms) of the current frame. */
    uint64_t            uCurTimeStampMs;

    /** Whether the RGB buffer is filled or not. */
    bool                fHasVideoData;

    struct
    {
        /** Target X resolution (in pixels). */
        uint32_t            uDstWidth;
        /** Target Y resolution (in pixels). */
        uint32_t            uDstHeight;
        /** X resolution of the last encoded frame. */
        uint32_t            uSrcLastWidth;
        /** Y resolution of the last encoded frame. */
        uint32_t            uSrcLastHeight;
        /** RGB buffer containing the most recent frame of Main's framebuffer. */
        uint8_t            *pu8RgbBuf;
        /** YUV buffer the encode function fetches the frame from. */
        uint8_t            *pu8YuvBuf;
        /** Pixel format of the current frame. */
        uint32_t            uPixelFormat;
        /** Minimal delay (in ms) between two frames. */
        uint32_t            uDelayMs;
        /** Encoder deadline. */
        unsigned int        uEncoderDeadline;
    } Video;
} VIDEORECSTREAM, *PVIDEORECSTREAM;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
typedef struct VIDEORECAUDIOFRAME
{
    uint8_t             abBuf[_64K]; /** @todo Fix! */
    uint32_t            cbBuf;
    /** Time stamp (in ms). */
    uint64_t            uTimeStampMs;
} VIDEORECAUDIOFRAME, *PVIDEORECAUDIOFRAME;
#endif

/** Vector of video recording streams. */
typedef std::vector <PVIDEORECSTREAM> VideoRecStreams;

/**
 * Structure for keeping a video recording context.
 */
typedef struct VIDEORECCONTEXT
{
    /** The current state. */
    uint32_t            enmState;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT          WaitEvent;
    /** Whether video recording is enabled or not. */
    bool                fEnabled;
    /** Shutdown indicator. */
    bool                fShutdown;
    /** Worker thread. */
    RTTHREAD            Thread;
    /** Maximal time (in ms) to record. */
    uint64_t            uMaxTimeMs;
    /** Maximal file size (in MB) to record. */
    uint32_t            uMaxSizeMB;
    /** Vector of current video recording stream contexts. */
    VideoRecStreams     vecStreams;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    bool                fHasAudioData;
    VIDEORECAUDIOFRAME  Audio;
#endif
} VIDEORECCONTEXT, *PVIDEORECCONTEXT;


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
 * Convert an image to YUV420p format
 * @returns true on success, false on failure
 * @param aWidth    width of image
 * @param aHeight   height of image
 * @param aDestBuf  an allocated memory buffer large enough to hold the
 *                  destination image (i.e. width * height * 12bits)
 * @param aSrcBuf   the source image as an array of bytes
 */
template <class T>
inline bool colorConvWriteYUV420p(unsigned aWidth, unsigned aHeight, uint8_t *aDestBuf, uint8_t *aSrcBuf)
{
    AssertReturn(!(aWidth & 1), false);
    AssertReturn(!(aHeight & 1), false);
    bool fRc = true;
    T iter1(aWidth, aHeight, aSrcBuf);
    T iter2 = iter1;
    iter2.skip(aWidth);
    unsigned cPixels = aWidth * aHeight;
    unsigned offY = 0;
    unsigned offU = cPixels;
    unsigned offV = cPixels + cPixels / 4;
    unsigned const cyHalf = aHeight / 2;
    unsigned const cxHalf = aWidth  / 2;
    for (unsigned i = 0; i < cyHalf && fRc; ++i)
    {
        for (unsigned j = 0; j < cxHalf; ++j)
        {
            unsigned red, green, blue;
            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDestBuf[offY] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            unsigned u = (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            unsigned v = (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDestBuf[offY + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDestBuf[offY + aWidth] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDestBuf[offY + aWidth + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            aDestBuf[offU] = u;
            aDestBuf[offV] = v;
            offY += 2;
            ++offU;
            ++offV;
        }

        iter1.skip(aWidth);
        iter2.skip(aWidth);
        offY += aWidth;
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
        const bool fHasAudioData = ASMAtomicReadBool(&pCtx->fHasAudioData);
#endif
        /** @todo r=andy This is inefficient -- as we already wake up this thread
         *               for every screen from Main, we here go again (on every wake up) through
         *               all screens.  */
        for (VideoRecStreams::iterator it = pCtx->vecStreams.begin(); it != pCtx->vecStreams.end(); it++)
        {
            PVIDEORECSTREAM pStream = (*it);

            if (!pStream->fEnabled)
                continue;

            if (ASMAtomicReadBool(&pStream->fHasVideoData))
            {
                rc = videoRecRGBToYUV(pStream);

                ASMAtomicWriteBool(&pStream->fHasVideoData, false);

                if (RT_SUCCESS(rc))
                    rc = videoRecEncodeAndWrite(pStream);

                if (RT_FAILURE(rc))
                {
                    static unsigned s_cErrEnc = 100;
                    if (s_cErrEnc > 0)
                    {
                        LogRel(("VideoRec: Error %Rrc encoding / writing video frame\n", rc));
                        s_cErrEnc--;
                    }
                }
            }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
            /* Each (enabled) screen has to get the audio data. */
            if (fHasAudioData)
            {
                WebMWriter::BlockData_Opus blockData = { pCtx->Audio.abBuf, pCtx->Audio.cbBuf, pCtx->Audio.uTimeStampMs };
                rc = pStream->pEBML->WriteBlock(pStream->uTrackAudio, &blockData, sizeof(blockData));
            }
#endif
        } /* for */

#ifdef VBOX_WITH_AUDIO_VIDEOREC
        if (fHasAudioData)
            ASMAtomicWriteBool(&pCtx->fHasAudioData, false);
#endif
    }

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

    int rc = VINF_SUCCESS;

    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)RTMemAllocZ(sizeof(VIDEORECCONTEXT));
    if (!pCtx)
        return VERR_NO_MEMORY;

    for (uint32_t uScreen = 0; uScreen < cScreens; uScreen++)
    {
        PVIDEORECSTREAM pStream = (PVIDEORECSTREAM)RTMemAllocZ(sizeof(VIDEORECSTREAM));
        if (!pStream)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        try
        {
            pStream->uScreen = uScreen;

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
            pCtx->enmState = VIDEORECSTS_IDLE;
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

            vpx_img_free(&pStream->Codec.VPX.RawImage);
            vpx_codec_err_t rcv = vpx_codec_destroy(&pStream->Codec.VPX.CodecCtx);
            Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);

            if (pStream->Video.pu8RgbBuf)
            {
                RTMemFree(pStream->Video.pu8RgbBuf);
                pStream->Video.pu8RgbBuf = NULL;
            }

            LogRel(("VideoRec: Recording screen #%u stopped\n", pStream->uScreen));
        }

        if (pStream->pEBML)
        {
            delete pStream->pEBML;
            pStream->pEBML = NULL;
        }

        it = pCtx->vecStreams.erase(it);

        RTMemFree(pStream);
        pStream = NULL;
    }

    Assert(pCtx->vecStreams.empty());

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

    pStream->Video.uDstWidth  = uWidth;
    pStream->Video.uDstHeight = uHeight;
    pStream->Video.pu8RgbBuf = (uint8_t *)RTMemAllocZ(uWidth * uHeight * 4);
    AssertReturn(pStream->Video.pu8RgbBuf, VERR_NO_MEMORY);

    /* Play safe: the file must not exist, overwriting is potentially
     * hazardous as nothing prevents the user from picking a file name of some
     * other important file, causing unintentional data loss. */

#ifdef VBOX_WITH_LIBVPX
    pStream->Video.uEncoderDeadline = VPX_DL_REALTIME;

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(DEFAULTCODEC, &pStream->Codec.VPX.Config, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogRel(("VideoRec: Failed to get default configuration for VPX codec: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }
#endif

    com::Utf8Str options(pszOptions);
    size_t pos = 0;

    /* By default we enable everything (if available). */
    bool fHasVideoTrack = true;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    bool fHasAudioTrack = true;
#endif

    com::Utf8Str key, value;
    while ((pos = options.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
    {
        if (key.compare("vc_quality", Utf8Str::CaseInsensitive) == 0)
        {
            if (value.compare("realtime", Utf8Str::CaseInsensitive) == 0)
            {
#ifdef VBOX_WITH_LIBVPX
                pStream->Video.uEncoderDeadline = VPX_DL_REALTIME;
#endif
            }
            else if (value.compare("good", Utf8Str::CaseInsensitive) == 0)
            {
                pStream->Video.uEncoderDeadline = 1000000 / uFPS;
            }
            else if (value.compare("best", Utf8Str::CaseInsensitive) == 0)
            {
#ifdef VBOX_WITH_LIBVPX
                pStream->Video.uEncoderDeadline = VPX_DL_BEST_QUALITY;
#endif
            }
            else
            {
                LogRel(("VideoRec: Setting quality deadline to '%s'\n", value.c_str()));
                pStream->Video.uEncoderDeadline = value.toUInt32();
            }
        }
        else if (key.compare("vc_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (value.compare("false", Utf8Str::CaseInsensitive) == 0) /* Disable audio. */
            {
                fHasVideoTrack = false;
                LogRel(("VideoRec: Only audio will be recorded\n"));
            }
#endif
        }
        else if (key.compare("ac_enabled", Utf8Str::CaseInsensitive) == 0)
        {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
            if (value.compare("false", Utf8Str::CaseInsensitive)) /* Disable audio. */
            {
                fHasAudioTrack = false;
                LogRel(("VideoRec: Only video will be recorded\n"));
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
    fOpen |= RTFILE_O_CREATE;
#endif

    int rc = pStream->pEBML->Create(pszFile, fOpen, WebMWriter::AudioCodec_Opus, WebMWriter::VideoCodec_VP8);
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

        LogRel(("VideoRec: Recording screen #%u with %ux%u @ %u kbps, %u fps to '%s'\n",
                uScreen, uWidth, uHeight, uRate, uFPS, pszFile));
    }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
    if (fHasAudioTrack)
    {
        rc = pStream->pEBML->AddAudioTrack(48000, 2, 16, &pStream->uTrackAudio); /** @todo Make this configurable. */
        if (RT_FAILURE(rc))
        {
            LogRel(("VideoRec: Failed to add audio track to output file '%s' (%Rrc)\n", pszFile, rc));
            return rc;
        }

        LogRel(("VideoRec: Recording audio enabled\n"));
    }
#endif

#ifdef VBOX_WITH_LIBVPX
    /* Target bitrate in kilobits per second. */
    pStream->Codec.VPX.Config.rc_target_bitrate = uRate;
    /* Frame width. */
    pStream->Codec.VPX.Config.g_w = uWidth;
    /* Frame height. */
    pStream->Codec.VPX.Config.g_h = uHeight;
    /* 1ms per frame. */
    pStream->Codec.VPX.Config.g_timebase.num = 1;
    pStream->Codec.VPX.Config.g_timebase.den = 1000;
    /* Disable multithreading. */
    pStream->Codec.VPX.Config.g_threads = 0;

    /* Initialize codec. */
    rcv = vpx_codec_enc_init(&pStream->Codec.VPX.CodecCtx, DEFAULTCODEC, &pStream->Codec.VPX.Config, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to initialize VP8 encoder %s", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }

    if (!vpx_img_alloc(&pStream->Codec.VPX.RawImage, VPX_IMG_FMT_I420, uWidth, uHeight, 1))
    {
        LogFlow(("Failed to allocate image %dx%d", uWidth, uHeight));
        return VERR_NO_MEMORY;
    }

    pStream->Video.pu8YuvBuf = pStream->Codec.VPX.RawImage.planes[0];
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

    uint32_t enmState = ASMAtomicReadU32(&pCtx->enmState);

    return (   enmState == VIDEORECSTS_IDLE
            || enmState == VIDEORECSTS_BUSY);
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

    uint32_t enmState = ASMAtomicReadU32(&pCtx->enmState);
    if (enmState != VIDEORECSTS_IDLE)
        return false;

    PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
    if (   !pStream
        || !pStream->fEnabled)
    {
        return false;
    }

    if (uTimeStampMs < pStream->uLastTimeStampMs + pStream->Video.uDelayMs)
        return false;

    if (   ASMAtomicReadBool(&pStream->fHasVideoData)
#ifdef VBOX_WITH_AUDIO_VIDEOREC
        /* Check if we have audio data left for the current frame. */
        || ASMAtomicReadBool(&pCtx->fHasAudioData)
#endif
       )
    {
        return false;
    }

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
    if (pStream->pEBML->GetAvailableSpace() < 0x100000)
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
 * @param   pStream             Stream to encode and write.
 */
static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStream)
{
    int rc;

#ifdef VBOX_WITH_LIBVPX
    /* Presentation Time Stamp (PTS). */
    vpx_codec_pts_t pts = pStream->uCurTimeStampMs;
    vpx_codec_err_t rcv = vpx_codec_encode(&pStream->Codec.VPX.CodecCtx,
                                           &pStream->Codec.VPX.RawImage,
                                           pts                           /* Time stamp */,
                                           pStream->Video.uDelayMs               /* How long to show this frame */,
                                           0                             /* Flags */,
                                           pStream->Video.uEncoderDeadline     /* Quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        LogFunc(("Failed to encode video frame: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

    vpx_codec_iter_t iter = NULL;
    rc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pPacket = vpx_codec_get_cx_data(&pStream->Codec.VPX.CodecCtx, &iter);
        if (!pPacket)
            break;

        switch (pPacket->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
            {
                WebMWriter::BlockData_VP8 blockData = { &pStream->Codec.VPX.Config, pPacket };
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
 * @param   pStream             Recording stream to convert RGB to YUV video frame buffer for.
 */
static int videoRecRGBToYUV(PVIDEORECSTREAM pStream)
{
    switch (pStream->Video.uPixelFormat)
    {
        case VIDEORECPIXELFMT_RGB32:
            LogFlow(("32 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGRA32Iter>(pStream->Video.uDstWidth,
                                                            pStream->Video.uDstHeight,
                                                            pStream->Video.pu8YuvBuf,
                                                            pStream->Video.pu8RgbBuf))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB24:
            LogFlow(("24 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR24Iter>(pStream->Video.uDstWidth,
                                                           pStream->Video.uDstHeight,
                                                           pStream->Video.pu8YuvBuf,
                                                           pStream->Video.pu8RgbBuf))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB565:
            LogFlow(("565 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR565Iter>(pStream->Video.uDstWidth,
                                                            pStream->Video.uDstHeight,
                                                            pStream->Video.pu8YuvBuf,
                                                            pStream->Video.pu8RgbBuf))
                return VERR_INVALID_PARAMETER;
            break;
        default:
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

    /* Do not execute during termination and guard against termination. */
    if (!ASMAtomicCmpXchgU32(&pCtx->enmState, VIDEORECSTS_BUSY, VIDEORECSTS_IDLE))
        return VINF_TRY_AGAIN;

    /* To save time spent in EMT, do the required audio multiplexing in the encoding thread.
     *
     * The multiplexing is needed to supply all recorded (enabled) screens with the same
     * audio data at the same given point in time.
     */

    if (ASMAtomicReadBool(&pCtx->fHasAudioData))
        return VERR_TRY_AGAIN; /* Previous frame not yet encoded. */

    memcpy(pCtx->Audio.abBuf, pvData, RT_MIN(_64K, cbData));

    pCtx->Audio.cbBuf        = cbData;
    pCtx->Audio.uTimeStampMs = uTimeStampMs;

    ASMAtomicWriteBool(&pCtx->fHasAudioData, true);
    RTSemEventSignal(pCtx->WaitEvent);
#else
    RT_NOREF(pCtx, pvData, cbData, uTimeStampMs);
#endif
    return VINF_SUCCESS;
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
    /* Do not execute during termination and guard against termination. */
    if (!ASMAtomicCmpXchgU32(&pCtx->enmState, VIDEORECSTS_BUSY, VIDEORECSTS_IDLE))
        return VINF_TRY_AGAIN;

    int rc = VINF_SUCCESS;
    do
    {
        AssertPtrBreakStmt(pCtx,      rc = VERR_INVALID_POINTER);
        AssertBreakStmt(uSrcWidth,    rc = VERR_INVALID_PARAMETER);
        AssertBreakStmt(uSrcHeight,   rc = VERR_INVALID_PARAMETER);
        AssertPtrBreakStmt(puSrcData, rc = VERR_INVALID_POINTER);

        PVIDEORECSTREAM pStream = videoRecStreamGet(pCtx, uScreen);
        if (!pStream)
        {
            rc = VERR_NOT_FOUND;
            break;
        }

        if (!pStream->fEnabled)
        {
            rc = VINF_TRY_AGAIN; /* Not (yet) enabled. */
            break;
        }

        if (uTimeStampMs < pStream->uLastTimeStampMs + pStream->Video.uDelayMs)
        {
            rc = VINF_TRY_AGAIN; /* Respect maximum frames per second. */
            break;
        }

        if (ASMAtomicReadBool(&pStream->fHasVideoData))
        {
            rc = VERR_TRY_AGAIN; /* Previous frame not yet encoded. */
            break;
        }

        pStream->uLastTimeStampMs = uTimeStampMs;

        int xDiff = ((int)pStream->Video.uDstWidth - (int)uSrcWidth) / 2;
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
        int yDiff = ((int)pStream->Video.uDstHeight - (int)uSrcHeight) / 2;
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

        if (   destX > pStream->Video.uDstWidth
            || destY > pStream->Video.uDstHeight)
        {
            rc = VERR_INVALID_PARAMETER;  /* Nothing visible. */
            break;
        }

        if (destX + w > pStream->Video.uDstWidth)
            w = pStream->Video.uDstWidth - destX;

        if (destY + h > pStream->Video.uDstHeight)
            h = pStream->Video.uDstHeight - destY;

        /* Calculate bytes per pixel. */
        uint32_t bpp = 1;
        if (uPixelFormat == BitmapFormat_BGR)
        {
            switch (uBPP)
            {
                case 32:
                    pStream->Video.uPixelFormat = VIDEORECPIXELFMT_RGB32;
                    bpp = 4;
                    break;
                case 24:
                    pStream->Video.uPixelFormat = VIDEORECPIXELFMT_RGB24;
                    bpp = 3;
                    break;
                case 16:
                    pStream->Video.uPixelFormat = VIDEORECPIXELFMT_RGB565;
                    bpp = 2;
                    break;
                default:
                    AssertMsgFailed(("Unknown color depth! mBitsPerPixel=%d\n", uBPP));
                    break;
            }
        }
        else
            AssertMsgFailed(("Unknown pixel format! mPixelFormat=%d\n", pStream->Video.uPixelFormat));

        /* One of the dimensions of the current frame is smaller than before so
         * clear the entire buffer to prevent artifacts from the previous frame. */
        if (   uSrcWidth  < pStream->Video.uSrcLastWidth
            || uSrcHeight < pStream->Video.uSrcLastHeight)
            memset(pStream->Video.pu8RgbBuf, 0, pStream->Video.uDstWidth * pStream->Video.uDstHeight * 4);

        pStream->Video.uSrcLastWidth  = uSrcWidth;
        pStream->Video.uSrcLastHeight = uSrcHeight;

        /* Calculate start offset in source and destination buffers. */
        uint32_t offSrc = y * uBytesPerLine + x * bpp;
        uint32_t offDst = (destY * pStream->Video.uDstWidth + destX) * bpp;

        /* Do the copy. */
        for (unsigned int i = 0; i < h; i++)
        {
            /* Overflow check. */
            Assert(offSrc + w * bpp <= uSrcHeight * uBytesPerLine);
            Assert(offDst + w * bpp <= pStream->Video.uDstHeight * pStream->Video.uDstWidth * bpp);

            memcpy(pStream->Video.pu8RgbBuf + offDst, puSrcData + offSrc, w * bpp);

            offSrc += uBytesPerLine;
            offDst += pStream->Video.uDstWidth * bpp;
        }

        pStream->uCurTimeStampMs = uTimeStampMs;

        ASMAtomicWriteBool(&pStream->fHasVideoData, true);
        RTSemEventSignal(pCtx->WaitEvent);

    } while (0);

    ASMAtomicCmpXchgU32(&pCtx->enmState, VIDEORECSTS_IDLE, VIDEORECSTS_BUSY);

    return rc;
}
