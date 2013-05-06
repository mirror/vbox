/* $Id$ */
/** @file
 * Encodes the screen content in VPX format.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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

static int videoRecEncodeAndWrite(PVIDEORECCONTEXT pVideoRecCtx);
static int videoRecRGBToYUV(PVIDEORECCONTEXT pVideoRecCtx);

/* RGB buffer */
enum
{
    /* RGB buffer empty */
    VIDREC_RGB_EMPTY = 0,
    /* RGB buffer filled */
    VIDREC_RGB_FILLED
};

/* encoding */
enum
{
    VIDREC_UNINITIALIZED = 0,
    /* initialized */
    VIDREC_INITIALIZED = 1,
    /* signal that we are terminating */
    VIDREC_TERMINATING = 2,
    /* confirmation that the worker thread terminated */
    VIDREC_TERMINATED = 3
};

typedef struct VIDEORECCONTEXT
{
    /* container context */
    EbmlGlobal          ebml;
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
    /* semaphore */
    RTSEMEVENT          WaitEvent;
    /* true if video recording is enabled */
    bool                fEnabled;
    /* worker thread */
    RTTHREAD            Thread;
    /* see VIDREC_xxx */
    uint32_t            u32State;
    /* true if the RGB buffer is filled */
    bool                fRgbFilled;
    /* pixel format of the current frame */
    uint32_t            u32PixelFormat;
    /* maximum number of frames per second */
    uint32_t            uDelay;
    uint64_t            u64LastTimeStamp;
    /* time stamp of the current frame */
    uint64_t            u64TimeStamp;
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
inline bool colorConvWriteYUV420p(unsigned aWidth, unsigned aHeight,
                                  uint8_t *aDestBuf, uint8_t *aSrcBuf)
{
    AssertReturn(0 == (aWidth & 1), false);
    AssertReturn(0 == (aHeight & 1), false);
    bool rc = true;
    T iter1(aWidth, aHeight, aSrcBuf);
    T iter2 = iter1;
    iter2.skip(aWidth);
    unsigned cPixels = aWidth * aHeight;
    unsigned offY = 0;
    unsigned offU = cPixels;
    unsigned offV = cPixels + cPixels / 4;
    for (unsigned i = 0; (i < aHeight / 2) && rc; ++i)
    {
        for (unsigned j = 0; (j < aWidth / 2) && rc; ++j)
        {
            unsigned red, green, blue, u, v;
            rc = iter1.getRGB(&red, &green, &blue);
            if (rc)
            {
                aDestBuf[offY] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
                u = (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
                v = (((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128) / 4;
                rc = iter1.getRGB(&red, &green, &blue);
            }
            if (rc)
            {
                aDestBuf[offY + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
                u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
                v += (((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128) / 4;
                rc = iter2.getRGB(&red, &green, &blue);
            }
            if (rc)
            {
                aDestBuf[offY + aWidth] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
                u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
                v += (((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128) / 4;
                rc = iter2.getRGB(&red, &green, &blue);
            }
            if (rc)
            {
                aDestBuf[offY + aWidth + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
                u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
                v += (((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128) / 4;
                aDestBuf[offU] = u;
                aDestBuf[offV] = v;
                offY += 2;
                ++offU;
                ++offV;
            }
        }
        if (rc)
        {
            iter1.skip(aWidth);
            iter2.skip(aWidth);
            offY += aWidth;
        }
    }
    return rc;
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
 * VideoRec utility function to create video recording context.
 *
 * @returns IPRT status code.
 * @param   ppVideoRecCtx video recording context
 */
int VideoRecContextCreate(PVIDEORECCONTEXT *ppVideoRecCtx)
{
    PVIDEORECCONTEXT pVideoRecCtx = (PVIDEORECCONTEXT)RTMemAllocZ(sizeof(VIDEORECCONTEXT));
    *ppVideoRecCtx = pVideoRecCtx;
    AssertReturn(pVideoRecCtx, VERR_NO_MEMORY);

    pVideoRecCtx->ebml.last_pts_ms = -1;
    return VINF_SUCCESS;
}

/**
 * Worker thread.
 *
 * RGB/YUV conversion and encoding.
 */
DECLCALLBACK(int) VideoRecThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVIDEORECCONTEXT pVideoRecCtx = (PVIDEORECCONTEXT)pvUser;
    for (;;)
    {
        int rc = RTSemEventWait(pVideoRecCtx->WaitEvent, RT_INDEFINITE_WAIT);
        AssertRCBreak(rc);

        if (ASMAtomicReadU32(&pVideoRecCtx->u32State) == VIDREC_TERMINATING)
            break;
        else if (ASMAtomicReadBool(&pVideoRecCtx->fRgbFilled))
        {
            rc = videoRecRGBToYUV(pVideoRecCtx);
            ASMAtomicWriteBool(&pVideoRecCtx->fRgbFilled, false);
            if (RT_SUCCESS(rc))
                rc = videoRecEncodeAndWrite(pVideoRecCtx);
           if (RT_FAILURE(rc))
                LogRel(("Error %Rrc encoding video frame\n", rc));
        }
    }

    ASMAtomicWriteU32(&pVideoRecCtx->u32State, VIDREC_TERMINATED);
    RTThreadUserSignal(ThreadSelf);
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to initialize video recording context.
 *
 * @returns IPRT status code.
 * @param   pVideoRecCtx        Pointer to video recording context to initialize Framebuffer width.
 * @param   strFile             File to save the recorded data
 * @param   uTargetWidth        Width of the target image in the video recoriding file (movie)
 * @param   uTargetHeight       Height of the target image in video recording file.
 */
int VideoRecContextInit(PVIDEORECCONTEXT pVideoRecCtx, com::Bstr strFile,
                        uint32_t uWidth, uint32_t uHeight, uint32_t uRate, uint32_t uFps)
{
    pVideoRecCtx->uTargetWidth  = uWidth;
    pVideoRecCtx->uTargetHeight = uHeight;
    pVideoRecCtx->pu8RgbBuf = (uint8_t *)RTMemAllocZ(uWidth * uHeight * 4);
    AssertReturn(pVideoRecCtx->pu8RgbBuf, VERR_NO_MEMORY);

    int rc = RTFileOpen(&pVideoRecCtx->ebml.file,
                        com::Utf8Str(strFile).c_str(),
                        RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
    {
        LogFlow(("Failed to open the output File \n"));
        return VERR_GENERAL_FAILURE;
    }

    vpx_codec_err_t rcv = vpx_codec_enc_config_default(DEFAULTCODEC, &pVideoRecCtx->VpxConfig, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to configure codec \n", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

    /* target bitrate in kilobits per second */
    pVideoRecCtx->VpxConfig.rc_target_bitrate = uRate;
    /* frame width */
    pVideoRecCtx->VpxConfig.g_w = uWidth;
    /* frame height */
    pVideoRecCtx->VpxConfig.g_h = uHeight;
    /* 1ms per frame */
    pVideoRecCtx->VpxConfig.g_timebase.num = 1;
    pVideoRecCtx->VpxConfig.g_timebase.den = 1000;
    /* disable multithreading */
    pVideoRecCtx->VpxConfig.g_threads = 0;
    pVideoRecCtx->uDelay = 1000 / uFps;

    struct vpx_rational arg_framerate = {30, 1};
    rc = Ebml_WriteWebMFileHeader(&pVideoRecCtx->ebml, &pVideoRecCtx->VpxConfig, &arg_framerate);
    AssertRCReturn(rc, rc);

    /* Initialize codec */
    rcv = vpx_codec_enc_init(&pVideoRecCtx->VpxCodec, DEFAULTCODEC,
                             &pVideoRecCtx->VpxConfig, 0);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to initialize encoder %s", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

    ASMAtomicWriteU32(&pVideoRecCtx->u32State, VIDREC_INITIALIZED);

    if (!vpx_img_alloc(&pVideoRecCtx->VpxRawImage, VPX_IMG_FMT_I420, uWidth, uHeight, 1))
    {
        LogFlow(("Failed to allocate image %dx%d", uWidth, uHeight));
        return VERR_NO_MEMORY;
    }
    pVideoRecCtx->pu8YuvBuf = pVideoRecCtx->VpxRawImage.planes[0];

    int vrc = RTSemEventCreate(&pVideoRecCtx->WaitEvent);
    AssertRCReturn(vrc, vrc);

    vrc = RTThreadCreate(&pVideoRecCtx->Thread, VideoRecThread,
                         (void*)pVideoRecCtx, 0,
                         RTTHREADTYPE_MAIN_WORKER, 0, "VideoRec");
    AssertRCReturn(vrc, vrc);

    pVideoRecCtx->fEnabled = true;
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to close the video recording context.
 *
 * @param   pVideoRecCtx   Pointer to video recording context.
 */
void VideoRecContextClose(PVIDEORECCONTEXT pVideoRecCtx)
{
    if (ASMAtomicReadU32(&pVideoRecCtx->u32State) == VIDREC_UNINITIALIZED)
        return;

    if (pVideoRecCtx->ebml.file != NIL_RTFILE)
    {
        int rc = Ebml_WriteWebMFileFooter(&pVideoRecCtx->ebml, 0);
        AssertRC(rc);
        RTFileClose(pVideoRecCtx->ebml.file);
        pVideoRecCtx->ebml.file = NIL_RTFILE;
    }
    if (pVideoRecCtx->ebml.cue_list)
    {
        RTMemFree(pVideoRecCtx->ebml.cue_list);
        pVideoRecCtx->ebml.cue_list = NULL;
    }
    if (pVideoRecCtx->fEnabled)
    {
        ASMAtomicWriteU32(&pVideoRecCtx->u32State, VIDREC_TERMINATING);
        RTSemEventSignal(pVideoRecCtx->WaitEvent);
        RTThreadUserWait(pVideoRecCtx->Thread, 10000);
        RTSemEventDestroy(pVideoRecCtx->WaitEvent);
        vpx_img_free(&pVideoRecCtx->VpxRawImage);
        vpx_codec_destroy(&pVideoRecCtx->VpxCodec);
        RTMemFree(pVideoRecCtx->pu8RgbBuf);
        pVideoRecCtx->pu8RgbBuf = NULL;
    }
}

/**
 * VideoRec utility function to check if recording is enabled.
 *
 * @returns true if recording is enabled
 * @param   pVideoRecCtx   Pointer to video recording context.
 */
bool VideoRecIsEnabled(PVIDEORECCONTEXT pVideoRecCtx)
{
    AssertPtr(pVideoRecCtx);
    return pVideoRecCtx->fEnabled;
}

/**
 * VideoRec utility function to encode the source image and write the encoded
 * image to target file.
 *
 * @returns IPRT status code.
 * @param   pVideoRecCtx  Pointer to video recording context.
 * @param   uSourceWidth      Width of the source image.
 * @param   uSourceHeight     Height of the source image.
 */
static int videoRecEncodeAndWrite(PVIDEORECCONTEXT pVideoRecCtx)
{
    /* presentation time stamp */
    vpx_codec_pts_t pts = pVideoRecCtx->u64TimeStamp;
    vpx_codec_err_t rcv = vpx_codec_encode(&pVideoRecCtx->VpxCodec,
                                           &pVideoRecCtx->VpxRawImage,
                                           pts /* time stamp */,
                                           10  /* how long to show this frame */,
                                           0   /* flags */,
                                           VPX_DL_REALTIME /* deadline */);
    if (rcv != VPX_CODEC_OK)
    {
        LogFlow(("Failed to encode:%s\n", vpx_codec_err_to_string(rcv)));
        return VERR_GENERAL_FAILURE;
    }

    vpx_codec_iter_t iter = NULL;
    int rc = VERR_NO_DATA;
    for (;;)
    {
        const vpx_codec_cx_pkt_t *pkt = vpx_codec_get_cx_data(&pVideoRecCtx->VpxCodec, &iter);
        if (!pkt)
            break;
        switch (pkt->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
                rc = Ebml_WriteWebMBlock(&pVideoRecCtx->ebml, &pVideoRecCtx->VpxConfig, pkt);
                break;
            default:
                LogFlow(("Unexpected CODEC Packet.\n"));
                break;
        }
    }

    pVideoRecCtx->cFrame++;
    return rc;
}

/**
 * VideoRec utility function to convert RGB to YUV.
 *
 * @returns IPRT status code.
 * @param   pVideoRecCtx      Pointer to video recording context.
 */
static int videoRecRGBToYUV(PVIDEORECCONTEXT pVideoRecCtx)
{
    switch (pVideoRecCtx->u32PixelFormat)
    {
        case VPX_IMG_FMT_RGB32:
            LogFlow(("32 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGRA32Iter>(pVideoRecCtx->uTargetWidth,
                                                            pVideoRecCtx->uTargetHeight,
                                                            pVideoRecCtx->pu8YuvBuf,
                                                            pVideoRecCtx->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        case VPX_IMG_FMT_RGB24:
            LogFlow(("24 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR24Iter>(pVideoRecCtx->uTargetWidth,
                                                           pVideoRecCtx->uTargetHeight,
                                                           pVideoRecCtx->pu8YuvBuf,
                                                           pVideoRecCtx->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        case VPX_IMG_FMT_RGB565:
            LogFlow(("565 bit\n"));
            if (!colorConvWriteYUV420p<ColorConvBGR565Iter>(pVideoRecCtx->uTargetWidth,
                                                            pVideoRecCtx->uTargetHeight,
                                                            pVideoRecCtx->pu8YuvBuf,
                                                            pVideoRecCtx->pu8RgbBuf))
                return VERR_GENERAL_FAILURE;
            break;
        default:
            return VERR_GENERAL_FAILURE;
    }
    return VINF_SUCCESS;
}

/**
 * VideoRec utility function to copy source image (FrameBuf) to
 * intermediate RGB buffer.
 *
 * @returns IPRT status code.
 * @param   pVideoRecCtx       Pointer to video recording context.
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
int VideoRecCopyToIntBuf(PVIDEORECCONTEXT pVideoRecCtx, uint32_t x, uint32_t y,
                         uint32_t uPixelFormat, uint32_t uBitsPerPixel, uint32_t uBytesPerLine,
                         uint32_t uSourceWidth, uint32_t uSourceHeight, uint8_t *pu8BufAddr,
                         uint64_t u64TimeStamp)
{
    AssertPtrReturn(pu8BufAddr, VERR_INVALID_PARAMETER);
    AssertReturn(uSourceWidth, VERR_INVALID_PARAMETER);
    AssertReturn(uSourceHeight, VERR_INVALID_PARAMETER);

    if (u64TimeStamp < pVideoRecCtx->u64LastTimeStamp + pVideoRecCtx->uDelay)
        return VINF_TRY_AGAIN;
    pVideoRecCtx->u64LastTimeStamp = u64TimeStamp;

    if (ASMAtomicReadBool(&pVideoRecCtx->fRgbFilled))
        return VERR_TRY_AGAIN;

    int xDiff = ((int)pVideoRecCtx->uTargetWidth - (int)uSourceWidth) / 2;
    uint32_t w = uSourceWidth;
    if ((int)w + xDiff + (int)x <= 0)  /* nothing visible */
        return VERR_INVALID_PARAMETER;

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
    int yDiff = ((int)pVideoRecCtx->uTargetHeight - (int)uSourceHeight) / 2;
    if ((int)h + yDiff + (int)y <= 0)  /* nothing visible */
        return VERR_INVALID_PARAMETER;

    uint32_t destY;
    if ((int)y < -yDiff)
    {
        h += yDiff + (int)y;
        y = -yDiff;
        destY = 0;
    }
    else
        destY = y + yDiff;

    if (   destX > pVideoRecCtx->uTargetWidth
        || destY > pVideoRecCtx->uTargetHeight)
        return VERR_INVALID_PARAMETER;  /* nothing visible */

    if (destX + w > pVideoRecCtx->uTargetWidth)
        w = pVideoRecCtx->uTargetWidth - destX;

    if (destY + h > pVideoRecCtx->uTargetHeight)
        h = pVideoRecCtx->uTargetHeight - destY;

    /* Calculate bytes per pixel */
    uint32_t bpp = 1;
    if (uPixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (uBitsPerPixel)
        {
            case 32:
                pVideoRecCtx->u32PixelFormat = VPX_IMG_FMT_RGB32;
                bpp = 4;
                break;
            case 24:
                pVideoRecCtx->u32PixelFormat = VPX_IMG_FMT_RGB24;
                bpp = 3;
                break;
            case 16:
                pVideoRecCtx->u32PixelFormat = VPX_IMG_FMT_RGB565;
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
    if (   uSourceWidth  < pVideoRecCtx->uLastSourceWidth
        || uSourceHeight < pVideoRecCtx->uLastSourceHeight)
    {
        memset(pVideoRecCtx->pu8RgbBuf, 0,
               pVideoRecCtx->uTargetWidth * pVideoRecCtx->uTargetHeight * 4);
    }
    pVideoRecCtx->uLastSourceWidth  = uSourceWidth;
    pVideoRecCtx->uLastSourceHeight = uSourceHeight;

    /* Calculate start offset in source and destination buffers */
    uint32_t offSrc = y * uBytesPerLine + x * bpp;
    uint32_t offDst = (destY * pVideoRecCtx->uTargetWidth + destX) * bpp;
    /* do the copy */
    for (unsigned int i = 0; i < h; i++)
    {
        /* Overflow check */
        Assert(offSrc + w * bpp <= uSourceHeight * uBytesPerLine);
        Assert(offDst + w * bpp <= pVideoRecCtx->uTargetHeight * pVideoRecCtx->uTargetWidth * bpp);
        memcpy(pVideoRecCtx->pu8RgbBuf + offDst, pu8BufAddr + offSrc, w * bpp);
        offSrc += uBytesPerLine;
        offDst += pVideoRecCtx->uTargetWidth * bpp;
    }

    pVideoRecCtx->u64TimeStamp = u64TimeStamp;

    ASMAtomicWriteBool(&pVideoRecCtx->fRgbFilled, true);
    RTSemEventSignal(pVideoRecCtx->WaitEvent);

    return VINF_SUCCESS;
}
