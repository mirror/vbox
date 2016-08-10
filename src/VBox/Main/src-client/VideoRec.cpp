/* $Id$ */
/** @file
 * Encodes the screen content in VPX format.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN
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

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vp8cx.h>
#include <vpx/vpx_image.h>

/** Default VPX codec to use */
#define DEFAULTCODEC (vpx_codec_vp8_cx())

static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStrm);
static int videoRecRGBToYUV(PVIDEORECSTREAM pStrm);

/* state to synchronized between threads */
enum
{
    VIDREC_UNINITIALIZED = 0,
    /* initialized, idle */
    VIDREC_IDLE = 1,
    /* currently in VideoRecCopyToIntBuf(), delay termination */
    VIDREC_COPYING = 2,
    /* signal that we are terminating */
    VIDREC_TERMINATING = 3
};

/* Must be always accessible and therefore cannot be part of VIDEORECCONTEXT */
static uint32_t g_enmState = VIDREC_UNINITIALIZED;


typedef struct VIDEORECSTREAM
{
    /* container context */
    WebMWriter          Ebml;
    /* VPX codec context */
    vpx_codec_ctx_t     VpxCodec;
    /* VPX configuration */
    vpx_codec_enc_cfg_t VpxConfig;
    /* X resolution */
    uint32_t            uTargetWidth;
    /* Y resolution */
    uint32_t            uTargetHeight;
    /* X resolution of the last encoded picture */
    uint32_t            uLastSourceWidth;
    /* Y resolution of the last encoded picture */
    uint32_t            uLastSourceHeight;
    /* current frame number */
    uint32_t            cFrame;
    /* RGB buffer containing the most recent frame of the framebuffer */
    uint8_t             *pu8RgbBuf;
    /* YUV buffer the encode function fetches the frame from */
    uint8_t             *pu8YuvBuf;
    /* VPX image context */
    vpx_image_t         VpxRawImage;
    /* true if video recording is enabled */
    bool                fEnabled;
    /* true if the RGB buffer is filled */
    bool                fRgbFilled;
    /* pixel format of the current frame */
    uint32_t            u32PixelFormat;
    /* minimal delay between two frames */
    uint32_t            uDelay;
    /* time stamp of the last frame we encoded */
    uint64_t            u64LastTimeStamp;
    /* time stamp of the current frame */
    uint64_t            u64TimeStamp;
    /* encoder deadline */
    unsigned int        uEncoderDeadline;
} VIDEORECSTREAM;

typedef struct VIDEORECCONTEXT
{
    /* semaphore to signal the encoding worker thread */
    RTSEMEVENT          WaitEvent;
    /* semaphore required during termination */
    RTSEMEVENT          TermEvent;
    /* true if video recording is enabled */
    bool                fEnabled;
    /* worker thread */
    RTTHREAD            Thread;
    /* number of stream contexts */
    uint32_t            cScreens;
    /* maximal time stamp */
    uint64_t            u64MaxTimeStamp;
    /* maximal file size in MB */
    uint32_t            uMaxFileSize;
    /* video recording stream contexts */
    VIDEORECSTREAM      Strm[1];
} VIDEORECCONTEXT;


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
 * Worker thread for all streams.
 *
 * RGB/YUV conversion and encoding.
 */
static DECLCALLBACK(int) videoRecThread(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf);
    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)pvUser;
    for (;;)
    {
        int rc = RTSemEventWait(pCtx->WaitEvent, RT_INDEFINITE_WAIT);
        AssertRCBreak(rc);

        if (ASMAtomicReadU32(&g_enmState) == VIDREC_TERMINATING)
            break;
        for (unsigned uScreen = 0; uScreen < pCtx->cScreens; uScreen++)
        {
            PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
            if (   pStrm->fEnabled
                && ASMAtomicReadBool(&pStrm->fRgbFilled))
            {
                rc = videoRecRGBToYUV(pStrm);
                ASMAtomicWriteBool(&pStrm->fRgbFilled, false);
                if (RT_SUCCESS(rc))
                    rc = videoRecEncodeAndWrite(pStrm);
                if (RT_FAILURE(rc))
                {
                    static unsigned cErrors = 100;
                    if (cErrors > 0)
                    {
                        LogRel(("Error %Rrc encoding / writing video frame\n", rc));
                        cErrors--;
                    }
                }
            }
        }
    }

    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to create video recording context.
 *
 * @returns IPRT status code.
 * @param   ppCtx            Video recording context
 * @param   cScreens         Number of screens.
 */
int VideoRecContextCreate(PVIDEORECCONTEXT *ppCtx, uint32_t cScreens)
{
    Assert(ASMAtomicReadU32(&g_enmState) == VIDREC_UNINITIALIZED);

    PVIDEORECCONTEXT pCtx = (PVIDEORECCONTEXT)RTMemAllocZ(RT_OFFSETOF(VIDEORECCONTEXT, Strm[cScreens]));
    *ppCtx = pCtx;
    AssertPtrReturn(pCtx, VERR_NO_MEMORY);

    pCtx->cScreens = cScreens;
    for (unsigned uScreen = 0; uScreen < cScreens; uScreen++)
    {
        /* Since we allocate without using standard C++ new mechanism
         * it is required to call placement new for correct initialization
         * of the object. */
        new (&pCtx->Strm[uScreen] + RT_OFFSETOF(VIDEORECSTREAM, Ebml)) WebMWriter();
    }

    int rc = RTSemEventCreate(&pCtx->WaitEvent);
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pCtx->TermEvent);
    AssertRCReturn(rc, rc);

    rc = RTThreadCreate(&pCtx->Thread, videoRecThread, (void*)pCtx, 0,
                        RTTHREADTYPE_MAIN_WORKER, RTTHREADFLAGS_WAITABLE, "VideoRec");
    AssertRCReturn(rc, rc);

    ASMAtomicWriteU32(&g_enmState, VIDREC_IDLE);
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to initialize video recording context.
 *
 * @returns IPRT status code.
 * @param   pCtx                Pointer to video recording context to initialize Framebuffer width.
 * @param   uScreeen            Screen number.
 * @param   strFile             File to save the recorded data
 * @param   uTargetWidth        Width of the target image in the video recoriding file (movie)
 * @param   uTargetHeight       Height of the target image in video recording file.
 */
int VideoRecStrmInit(PVIDEORECCONTEXT pCtx, uint32_t uScreen, const char *pszFile,
                     uint32_t uWidth, uint32_t uHeight, uint32_t uRate, uint32_t uFps,
                     uint32_t uMaxTime, uint32_t uMaxFileSize, const char *pszOptions)
{
    AssertPtrReturn(pCtx, VERR_INVALID_PARAMETER);
    AssertReturn(uScreen < pCtx->cScreens, VERR_INVALID_PARAMETER);

    pCtx->u64MaxTimeStamp = (uMaxTime > 0 ? RTTimeProgramMilliTS() + uMaxTime * 1000 : 0);
    pCtx->uMaxFileSize = uMaxFileSize;

    PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
    pStrm->uTargetWidth  = uWidth;
    pStrm->uTargetHeight = uHeight;
    pStrm->pu8RgbBuf = (uint8_t *)RTMemAllocZ(uWidth * uHeight * 4);
    AssertReturn(pStrm->pu8RgbBuf, VERR_NO_MEMORY);
    pStrm->uEncoderDeadline = VPX_DL_REALTIME;

    /* Play safe: the file must not exist, overwriting is potentially
     * hazardous as nothing prevents the user from picking a file name of some
     * other important file, causing unintentional data loss. */

    int rc = pStrm->Ebml.create(pszFile);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to create the video capture output file \"%s\" (%Rrc)\n", pszFile, rc));
        return rc;
    }

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(DEFAULTCODEC, &pStrm->VpxConfig, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to configure codec: %s\n", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }

    com::Utf8Str options(pszOptions);
    size_t pos = 0;

    do {

        com::Utf8Str key, value;
        pos = options.parseKeyValue(key, value, pos);

        if (key == "quality")
        {
            if (value == "realtime")
            {
                pStrm->uEncoderDeadline = VPX_DL_REALTIME;
            }
            else if (value == "good")
            {
                pStrm->uEncoderDeadline = 1000000 / uFps;
            }
            else if (value == "best")
            {
                pStrm->uEncoderDeadline = VPX_DL_BEST_QUALITY;
            }
            else
            {
                LogRel(("Settings quality deadline to = %s\n", value.c_str()));
                pStrm->uEncoderDeadline = value.toUInt32();
            }
        }
        else LogRel(("Getting unknown option: %s=%s\n", key.c_str(), value.c_str()));

    } while(pos != com::Utf8Str::npos);

    /* target bitrate in kilobits per second */
    pStrm->VpxConfig.rc_target_bitrate = uRate;
    /* frame width */
    pStrm->VpxConfig.g_w = uWidth;
    /* frame height */
    pStrm->VpxConfig.g_h = uHeight;
    /* 1ms per frame */
    pStrm->VpxConfig.g_timebase.num = 1;
    pStrm->VpxConfig.g_timebase.den = 1000;
    /* disable multithreading */
    pStrm->VpxConfig.g_threads = 0;

    pStrm->uDelay = 1000 / uFps;

    struct vpx_rational arg_framerate = { (int)uFps, 1 };
    rc = pStrm->Ebml.writeHeader(&pStrm->VpxConfig, &arg_framerate);
    AssertRCReturn(rc, rc);

    /* Initialize codec */
    rcv = vpx_codec_enc_init(&pStrm->VpxCodec, DEFAULTCODEC, &pStrm->VpxConfig, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to initialize VP8 encoder %s", vpx_codec_err_to_string(rcv)));
        return VERR_INVALID_PARAMETER;
    }

    if (!vpx_img_alloc(&pStrm->VpxRawImage, VPX_IMG_FMT_I420, uWidth, uHeight, 1))
    {
        LogFlow(("Failed to allocate image %dx%d", uWidth, uHeight));
        return VERR_NO_MEMORY;
    }
    pStrm->pu8YuvBuf = pStrm->VpxRawImage.planes[0];

    pCtx->fEnabled = true;
    pStrm->fEnabled = true;
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to close the video recording context.
 *
 * @param   pCtx   Pointer to video recording context.
 */
void VideoRecContextClose(PVIDEORECCONTEXT pCtx)
{
    if (!pCtx)
        return;

    uint32_t enmState = VIDREC_IDLE;
    for (;;)
    {
        if (ASMAtomicCmpXchgExU32(&g_enmState, VIDREC_TERMINATING, enmState, &enmState))
            break;
        if (enmState == VIDREC_UNINITIALIZED)
            return;
    }
    if (enmState == VIDREC_COPYING)
    {
        int rc = RTSemEventWait(pCtx->TermEvent, RT_INDEFINITE_WAIT);
        AssertRC(rc);
    }

    RTSemEventSignal(pCtx->WaitEvent);
    RTThreadWait(pCtx->Thread, 10000, NULL);
    RTSemEventDestroy(pCtx->WaitEvent);
    RTSemEventDestroy(pCtx->TermEvent);

    for (unsigned uScreen = 0; uScreen < pCtx->cScreens; uScreen++)
    {
        PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
        if (pStrm->fEnabled)
        {
            int rc = pStrm->Ebml.writeFooter(0);
            AssertRC(rc);
            pStrm->Ebml.close();
            vpx_img_free(&pStrm->VpxRawImage);
            vpx_codec_err_t rcv = vpx_codec_destroy(&pStrm->VpxCodec);
            Assert(rcv == VPX_CODEC_OK); RT_NOREF(rcv);
            RTMemFree(pStrm->pu8RgbBuf);
            pStrm->pu8RgbBuf = NULL;
        }
        /* Explicitly deinitilize Ebml object since it was created using placement new. */
        pStrm->Ebml.~WebMWriter();
    }

    RTMemFree(pCtx);

    ASMAtomicWriteU32(&g_enmState, VIDREC_UNINITIALIZED);
}

/**
 * VideoRec utility function to check if recording is enabled.
 *
 * @returns true if recording is enabled
 * @param   pCtx   Pointer to video recording context.
 */
bool VideoRecIsEnabled(PVIDEORECCONTEXT pCtx)
{
    RT_NOREF(pCtx);
    uint32_t enmState = ASMAtomicReadU32(&g_enmState);
    return (   enmState == VIDREC_IDLE
            || enmState == VIDREC_COPYING);
}

/**
 * VideoRec utility function to check if recording engine is ready to accept a new frame
 * for the given screen.
 *
 * @returns true if recording engine is ready
 * @param   pCtx   Pointer to video recording context.
 * @param   uScreen screen id.
 * @param   u64TimeStamp current time stamp
 */
bool VideoRecIsReady(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t u64TimeStamp)
{
    uint32_t enmState = ASMAtomicReadU32(&g_enmState);
    if (enmState != VIDREC_IDLE)
        return false;

    PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
    if (!pStrm->fEnabled)
        return false;

    if (u64TimeStamp < pStrm->u64LastTimeStamp + pStrm->uDelay)
        return false;

    if (ASMAtomicReadBool(&pStrm->fRgbFilled))
        return false;

    return true;
}

/**
 * VideoRec utility function to check if the file size has reached
 * specified limits (if any).
 *
 * @returns true if any limit has been reached
 * @param   pCtx Pointer to video recording context
 * @param   uScreen screen id
 * @param   u64TimeStamp current time stamp
 */

bool VideoRecIsFull(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t u64TimeStamp)
{
    PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
    if(!pStrm->fEnabled)
        return false;

    if(pCtx->u64MaxTimeStamp > 0 && u64TimeStamp >= pCtx->u64MaxTimeStamp)
        return true;

    if (pCtx->uMaxFileSize > 0)
    {
        uint64_t sizeInMB = pStrm->Ebml.getFileSize() / (1024 * 1024);
        if(sizeInMB >= pCtx->uMaxFileSize)
            return true;
    }
    /* Check for available free disk space */
    if (pStrm->Ebml.getAvailableSpace() < 0x100000)
    {
        LogRel(("Storage has not enough free space available, stopping video capture\n"));
        return true;
    }

    return false;
}

/**
 * VideoRec utility function to encode the source image and write the encoded
 * image to target file.
 *
 * @returns IPRT status code.
 * @param   pCtx  Pointer to video recording context.
 * @param   uSourceWidth      Width of the source image.
 * @param   uSourceHeight     Height of the source image.
 */
static int videoRecEncodeAndWrite(PVIDEORECSTREAM pStrm)
{
    /* presentation time stamp */
    vpx_codec_pts_t pts = pStrm->u64TimeStamp;
    vpx_codec_err_t rcv = vpx_codec_encode(&pStrm->VpxCodec,
                                           &pStrm->VpxRawImage,
                                           pts /* time stamp */,
                                           pStrm->uDelay  /* how long to show this frame */,
                                           0   /* flags */,
                                           pStrm->uEncoderDeadline /* quality setting */);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to encode:%s\n", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

    vpx_codec_iter_t iter = NULL;
    int rc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pkt = vpx_codec_get_cx_data(&pStrm->VpxCodec, &iter);
        if (!pkt)
            break;
        switch (pkt->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
                rc = pStrm->Ebml.writeBlock(&pStrm->VpxConfig, pkt);
                break;
            default:
                LogFlow(("Unexpected CODEC Packet.\n"));
                break;
        }
    }

    pStrm->cFrame++;
    return rc;
}

/**
 * VideoRec utility function to convert RGB to YUV.
 *
 * @returns IPRT status code.
 * @param   pCtx      Pointer to video recording context.
 */
static int videoRecRGBToYUV(PVIDEORECSTREAM pStrm)
{
    switch (pStrm->u32PixelFormat)
    {
        case VPX_IMG_FMT_RGB32:
            LogFlow(("32 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGRA32Iter>(pStrm->uTargetWidth,
                                                            pStrm->uTargetHeight,
                                                            pStrm->pu8YuvBuf,
                                                            pStrm->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        case VPX_IMG_FMT_RGB24:
            LogFlow(("24 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR24Iter>(pStrm->uTargetWidth,
                                                           pStrm->uTargetHeight,
                                                           pStrm->pu8YuvBuf,
                                                           pStrm->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        case VPX_IMG_FMT_RGB565:
            LogFlow(("565 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR565Iter>(pStrm->uTargetWidth,
                                                            pStrm->uTargetHeight,
                                                            pStrm->pu8YuvBuf,
                                                            pStrm->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        default:
            return VERR_GENERAL_FAILURE;
    }
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to copy a source image (FrameBuf) to the intermediate
 * RGB buffer. This function is executed only once per time.
 *
 * @thread  EMT
 *
 * @returns IPRT status code.
 * @param   pCtx               Pointer to the video recording context.
 * @param   uScreen            Screen number.
 * @param   x                  Starting x coordinate of the source buffer (Framebuffer).
 * @param   y                  Starting y coordinate of the source buffer (Framebuffer).
 * @param   uPixelFormat       Pixel Format.
 * @param   uBitsPerPixel      Bits Per Pixel
 * @param   uBytesPerLine      Bytes per source scanlineName.
 * @param   uSourceWidth       Width of the source image (framebuffer).
 * @param   uSourceHeight      Height of the source image (framebuffer).
 * @param   pu8BufAddr         Pointer to source image(framebuffer).
 * @param   u64TimeStamp       Time stamp (milliseconds).
 */
int VideoRecCopyToIntBuf(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint32_t x, uint32_t y,
                         uint32_t uPixelFormat, uint32_t uBitsPerPixel, uint32_t uBytesPerLine,
                         uint32_t uSourceWidth, uint32_t uSourceHeight, uint8_t *pu8BufAddr,
                         uint64_t u64TimeStamp)
{
    /* Do not execute during termination and guard against termination */
    if (!ASMAtomicCmpXchgU32(&g_enmState, VIDREC_COPYING, VIDREC_IDLE))
        return VINF_TRY_AGAIN;

    int rc = VINF_SUCCESS;
    do
    {
        AssertPtrBreakStmt(pu8BufAddr, rc = VERR_INVALID_PARAMETER);
        AssertBreakStmt(uSourceWidth, rc = VERR_INVALID_PARAMETER);
        AssertBreakStmt(uSourceHeight, rc = VERR_INVALID_PARAMETER);
        AssertBreakStmt(uScreen < pCtx->cScreens, rc = VERR_INVALID_PARAMETER);

        PVIDEORECSTREAM pStrm = &pCtx->Strm[uScreen];
        if (!pStrm->fEnabled)
        {
            rc = VINF_TRY_AGAIN; /* not (yet) enabled */
            break;
        }
        if (u64TimeStamp < pStrm->u64LastTimeStamp + pStrm->uDelay)
        {
            rc = VINF_TRY_AGAIN; /* respect maximum frames per second */
            break;
        }
        if (ASMAtomicReadBool(&pStrm->fRgbFilled))
        {
            rc = VERR_TRY_AGAIN; /* previous frame not yet encoded */
            break;
        }

        pStrm->u64LastTimeStamp = u64TimeStamp;

        int xDiff = ((int)pStrm->uTargetWidth - (int)uSourceWidth) / 2;
        uint32_t w = uSourceWidth;
        if ((int)w + xDiff + (int)x <= 0)  /* nothing visible */
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

        uint32_t h = uSourceHeight;
        int yDiff = ((int)pStrm->uTargetHeight - (int)uSourceHeight) / 2;
        if ((int)h + yDiff + (int)y <= 0)  /* nothing visible */
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

        if (   destX > pStrm->uTargetWidth
            || destY > pStrm->uTargetHeight)
        {
            rc = VERR_INVALID_PARAMETER;  /* nothing visible */
            break;
        }

        if (destX + w > pStrm->uTargetWidth)
            w = pStrm->uTargetWidth - destX;

        if (destY + h > pStrm->uTargetHeight)
            h = pStrm->uTargetHeight - destY;

        /* Calculate bytes per pixel */
        uint32_t bpp = 1;
        if (uPixelFormat == BitmapFormat_BGR)
        {
            switch (uBitsPerPixel)
            {
                case 32:
                    pStrm->u32PixelFormat = VPX_IMG_FMT_RGB32;
                    bpp = 4;
                    break;
                case 24:
                    pStrm->u32PixelFormat = VPX_IMG_FMT_RGB24;
                    bpp = 3;
                    break;
                case 16:
                    pStrm->u32PixelFormat = VPX_IMG_FMT_RGB565;
                    bpp = 2;
                    break;
                default:
                    AssertMsgFailed(("Unknown color depth! mBitsPerPixel=%d\n", uBitsPerPixel));
                    break;
            }
        }
        else
            AssertMsgFailed(("Unknown pixel format! mPixelFormat=%d\n", uPixelFormat));

        /* One of the dimensions of the current frame is smaller than before so
         * clear the entire buffer to prevent artifacts from the previous frame */
        if (   uSourceWidth  < pStrm->uLastSourceWidth
            || uSourceHeight < pStrm->uLastSourceHeight)
            memset(pStrm->pu8RgbBuf, 0, pStrm->uTargetWidth * pStrm->uTargetHeight * 4);

        pStrm->uLastSourceWidth  = uSourceWidth;
        pStrm->uLastSourceHeight = uSourceHeight;

        /* Calculate start offset in source and destination buffers */
        uint32_t offSrc = y * uBytesPerLine + x * bpp;
        uint32_t offDst = (destY * pStrm->uTargetWidth + destX) * bpp;
        /* do the copy */
        for (unsigned int i = 0; i < h; i++)
        {
            /* Overflow check */
            Assert(offSrc + w * bpp <= uSourceHeight * uBytesPerLine);
            Assert(offDst + w * bpp <= pStrm->uTargetHeight * pStrm->uTargetWidth * bpp);
            memcpy(pStrm->pu8RgbBuf + offDst, pu8BufAddr + offSrc, w * bpp);
            offSrc += uBytesPerLine;
            offDst += pStrm->uTargetWidth * bpp;
        }

        pStrm->u64TimeStamp = u64TimeStamp;

        ASMAtomicWriteBool(&pStrm->fRgbFilled, true);
        RTSemEventSignal(pCtx->WaitEvent);
    } while (0);

    if (!ASMAtomicCmpXchgU32(&g_enmState, VIDREC_IDLE, VIDREC_COPYING))
    {
        rc = RTSemEventSignal(pCtx->TermEvent);
        AssertRC(rc);
    }

    return rc;
}
