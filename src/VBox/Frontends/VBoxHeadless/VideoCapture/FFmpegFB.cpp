/** @file
 *
 * Framebuffer implementation that interfaces with FFmpeg
 * to create a video of the guest.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_GUI

#include "FFmpegFB.h"

#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <png.h>

// external constructor for dynamic loading
/////////////////////////////////////////////////////////////////////////////

/**
 * Callback function to register an ffmpeg framebuffer.
 *
 * @returns COM status code.
 * @param   width        Framebuffer width.
 * @param   height       Framebuffer height.
 * @param   bitrate      Bitrate of mpeg file to be created.
 * @param   filename     Name of mpeg file to be created
 * @retval  retVal       The new framebuffer
 */
extern "C" DECLEXPORT(HRESULT) VBoxRegisterFFmpegFB(ULONG width,
                                     ULONG height, ULONG bitrate,
                                     com::Bstr filename,
                                     IFramebuffer **retVal)
{
    Log2(("VBoxRegisterFFmpegFB: called\n"));
    FFmpegFB *pFramebuffer = new FFmpegFB(width, height, bitrate, filename);
    int rc = pFramebuffer->init();
    AssertMsg(rc == S_OK,
              ("failed to initialise the FFmpeg framebuffer, rc = %d\n",
               rc));
    if (rc == S_OK)
    {
        *retVal = pFramebuffer;
        return S_OK;
    }
    delete pFramebuffer;
    return rc;
}





// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

/**
 * Perform parts of initialisation which are guaranteed not to fail
 * unless we run out of memory.  In this case, we just set the guest
 * buffer to 0 so that RequestResize() does not free it the first time
 * it is called.
 */
FFmpegFB::FFmpegFB(ULONG width, ULONG height, ULONG bitrate,
                   com::Bstr filename) :
    mBitRate(bitrate),
    mPixelFormat(FramebufferPixelFormat_Opaque),
    mBitsPerPixel(0),
    mFileName(filename),
    mBytesPerLine(0),
    mFrameWidth(width), mFrameHeight(height),
    mYUVFrameSize(width * height * 3 / 2),
    mRGBBuffer(0), mpFormatContext(0), mpStream(0),
    mOutOfMemory(false), mToggle(false)
{
    ULONG cPixels = width * height;

    LogFlow(("Creating FFmpegFB object %p, width=%lu, height=%lu\n",
             this, width, height));
    Assert(width % 2 == 0 && height % 2 == 0);
    /* For temporary RGB frame we allocate enough memory to deal with
       RGB16 to RGB32 */
    mTempRGBBuffer = reinterpret_cast<uint8_t *>(av_malloc(cPixels * 4));
    if (mTempRGBBuffer == 0)
        goto nomem_temp_rgb_buffer;
    mYUVBuffer = reinterpret_cast<uint8_t *>(av_malloc(mYUVFrameSize));
    if (mYUVBuffer == 0)
        goto nomem_yuv_buffer;
    mFrame = avcodec_alloc_frame();
    if (mFrame == 0)
        goto nomem_mframe;
    mOutBuf = reinterpret_cast<uint8_t *>(av_malloc(mYUVFrameSize * 2));
    if (mOutBuf == 0)
        goto nomem_moutbuf;

    return;

    /* C-based memory allocation and how to deal with it in C++ :) */
nomem_moutbuf:
    Log(("Failed to allocate memory for mOutBuf\n"));
    av_free(mFrame);
nomem_mframe:
    Log(("Failed to allocate memory for mFrame\n"));
    av_free(mYUVBuffer);
nomem_yuv_buffer:
    Log(("Failed to allocate memory for mYUVBuffer\n"));
    av_free(mTempRGBBuffer);
nomem_temp_rgb_buffer:
    Log(("Failed to allocate memory for mTempRGBBuffer\n"));
    mOutOfMemory = true;
}


/**
 * Write the last frame to disk and free allocated memory
 */
FFmpegFB::~FFmpegFB()
{
    LogFlow(("Destroying FFmpegFB object %p\n", this));
    if (mpFormatContext != 0)
    {
        /* Dummy update to make sure we get all the frame (timing). */
        BOOL dummy;
        NotifyUpdate(0, 0, 0, 0, &dummy);
        /* Write the last pending frame before exiting */
        int rc = do_rgb_to_yuv_conversion();
        if (rc == S_OK)
            do_encoding_and_write();
#if 1
        /* Add another 10 seconds. */
        for (int i = 10*25; i > 0; i--)
            do_encoding_and_write();
#endif
        /* write a png file of the last frame */
        write_png();
        avcodec_close(mpStream->codec);
        av_write_trailer(mpFormatContext);
        /* free the streams */
        for(int i = 0; i < mpFormatContext->nb_streams; i++) {
            av_freep(&mpFormatContext->streams[i]->codec);
            av_freep(&mpFormatContext->streams[i]);
        }
        url_fclose(&mpFormatContext->pb);
        av_free(mpFormatContext);
    }
    RTCritSectDelete(&mCritSect);
    /* We have already freed the stream above */
    mpStream = 0;
    if (mTempRGBBuffer != 0)
        av_free(mTempRGBBuffer);
    if (mYUVBuffer != 0)
        av_free(mYUVBuffer);
    if (mFrame != 0)
        av_free(mFrame);
    if (mOutBuf != 0)
        av_free(mOutBuf);
    if (mRGBBuffer != 0)
        RTMemFree(mRGBBuffer);
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Perform any parts of the initialisation which could potentially fail
 * for reasons other than "out of memory".
 *
 * @returns COM status code
 * @param width  width to be used for MPEG frame framebuffer and initially
 *               for the guest frame buffer - must be a multiple of two
 * @param height height to be used for MPEG frame framebuffer and
 *               initially for the guest framebuffer - must be a multiple
 *               of two
 * @param depth  depth to be used initially for the guest framebuffer
 */
HRESULT FFmpegFB::init()
{
    LogFlow(("Initialising FFmpegFB object %p\n", this));
    if (mOutOfMemory == true)
        return E_OUTOFMEMORY;
    int rc = RTCritSectInit(&mCritSect);
    AssertReturn(rc == VINF_SUCCESS, E_UNEXPECTED);
    int rcSetupLibrary = setup_library();
    AssertReturn(rcSetupLibrary == S_OK, rcSetupLibrary);
    int rcSetupFormat = setup_output_format();
    AssertReturn(rcSetupFormat == S_OK, rcSetupFormat);
    int rcOpenCodec = open_codec();
    AssertReturn(rcOpenCodec == S_OK, rcOpenCodec);
    int rcOpenFile = open_output_file();
    AssertReturn(rcOpenFile == S_OK, rcOpenFile);
    /* Fill in the picture data for the AVFrame - not particularly
       elegant, but that is the API. */
    avpicture_fill((AVPicture *) mFrame, mYUVBuffer, PIX_FMT_YUV420P,
                   mFrameWidth, mFrameHeight);
    /* Fill in the AVPicture structure describing the MPEG frame
       framebuffer - in fact another representation of the same data */
    avpicture_fill(&mFramePicture, mYUVBuffer, PIX_FMT_YUV420P,
                   mFrameWidth, mFrameHeight);
    /* Set the initial framebuffer size to the mpeg frame dimensions */
    BOOL finished;
    RequestResize(0, FramebufferPixelFormat_Opaque, NULL, 0, 0,
                  mFrameWidth, mFrameHeight, &finished);
    /* Start counting time */
    mLastTime = RTTimeMilliTS();
    mLastTime = mLastTime - mLastTime % 40;
    return rc;
}

// IFramebuffer properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Return the address of the frame buffer for the virtual VGA device to
 * write to.  If COMGETTER(UsesGuestVRAM) returns FLASE (or if this address
 * is not the same as the guests VRAM buffer), the device will perform
 * translation.
 *
 * @returns         COM status code
 * @retval  address The address of the buffer
 */
STDMETHODIMP FFmpegFB::COMGETTER(Address) (BYTE **address)
{
    if (!address)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Address): returning address %p\n", mBufferAddress));
    *address = mBufferAddress;
    return S_OK;
}

/**
 * Return the width of our frame buffer.
 *
 * @returns       COM status code
 * @retval  width The width of the frame buffer
 */
STDMETHODIMP FFmpegFB::COMGETTER(Width) (ULONG *width)
{
    if (!width)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Width): returning width %lu\n", mGuestWidth));
    *width = mGuestWidth;
    return S_OK;
}

/**
 * Return the height of our frame buffer.
 *
 * @returns        COM status code
 * @retval  height The height of the frame buffer
 */
STDMETHODIMP FFmpegFB::COMGETTER(Height) (ULONG *height)
{
    if (!height)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Height): returning height %lu\n", mGuestHeight));
    *height = mGuestHeight;
    return S_OK;
}

/**
 * Return the colour depth of our frame buffer.  Note that we actually
 * store the pixel format, not the colour depth internally, since
 * when display sets FramebufferPixelFormat_Opaque, it
 * wants to retreive FramebufferPixelFormat_Opaque and
 * nothing else.
 *
 * @returns            COM status code
 * @retval  bitsPerPixel The colour depth of the frame buffer
 */
STDMETHODIMP FFmpegFB::COMGETTER(BitsPerPixel) (ULONG *bitsPerPixel)
{
    if (!bitsPerPixel)
        return E_POINTER;
    *bitsPerPixel = mBitsPerPixel;
    LogFlow(("FFmpeg::COMGETTER(BitsPerPixel): returning depth %lu\n",
             *bitsPerPixel));
    return S_OK;
}

/**
 * Return the number of bytes per line in our frame buffer.
 *
 * @returns          COM status code
 * @retval  bytesPerLine The number of bytes per line
 */
STDMETHODIMP FFmpegFB::COMGETTER(BytesPerLine) (ULONG *bytesPerLine)
{
    if (!bytesPerLine)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(BytesPerLine): returning line size %lu\n", mBytesPerLine));
    *bytesPerLine = mBytesPerLine;
    return S_OK;
}

/**
 * Return the pixel layout of our frame buffer.
 *
 * @returns             COM status code
 * @retval  pixelFormat The pixel layout
 */
STDMETHODIMP FFmpegFB::COMGETTER(PixelFormat) (ULONG *pixelFormat)
{
    if (!pixelFormat)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(PixelFormat): returning pixel format: %d\n",
             mPixelFormat));
    *pixelFormat = mPixelFormat;
    return S_OK;
}

/**
 * Return whether we use the guest VRAM directly.
 *
 * @returns             COM status code
 * @retval  pixelFormat The pixel layout
 */
STDMETHODIMP FFmpegFB::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
{
    if (!usesGuestVRAM)
        return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(UsesGuestVRAM): uses guest VRAM? %d\n",
             FALSE));
    *usesGuestVRAM = mRGBBuffer == NULL;
    return S_OK;
}

/**
 * Return the number of lines of our frame buffer which can not be used
 * (e.g. for status lines etc?).
 *
 * @returns                 COM status code
 * @retval  heightReduction The number of unused lines
 */
STDMETHODIMP FFmpegFB::COMGETTER(HeightReduction) (ULONG *heightReduction)
{
    if (!heightReduction)
        return E_POINTER;
    /* no reduction */
    *heightReduction = 0;
    LogFlow(("FFmpeg::COMGETTER(HeightReduction): returning 0\n"));
    return S_OK;
}

/**
 * Return a pointer to the alpha-blended overlay used to render status icons
 * etc above the framebuffer.
 *
 * @returns          COM status code
 * @retval  aOverlay The overlay framebuffer
 */
STDMETHODIMP FFmpegFB::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
{
    if (!aOverlay)
        return E_POINTER;
    /* not yet implemented */
    *aOverlay = 0;
    LogFlow(("FFmpeg::COMGETTER(Overlay): returning 0\n"));
    return S_OK;
}

// IFramebuffer methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FFmpegFB::Lock()
{
    LogFlow(("FFmpeg::Lock: called\n"));
    int rc = RTCritSectEnter(&mCritSect);
    AssertRC(rc);
    if (rc == VINF_SUCCESS)
        return S_OK;
    return E_UNEXPECTED;
}

STDMETHODIMP FFmpegFB::Unlock()
{
    LogFlow(("FFmpeg::Unlock: called\n"));
    RTCritSectLeave(&mCritSect);
    return S_OK;
}


/**
 * This method is used to notify us that an area of the guest framebuffer
 * has been updated.
 *
 * @returns        COM status code
 * @param x        X co-ordinate of the upper left-hand corner of the
 *                 area which has been updated
 * @param y        Y co-ordinate of the upper left-hand corner of the
 *                 area which has been updated
 * @param w        width of the area which has been updated
 * @param h        height of the area which has been updated
 * @param finished
 */
STDMETHODIMP FFmpegFB::NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h,
                                    BOOL *finished)
{
    int rc;
    int64_t iCurrentTime = RTTimeMilliTS();

    LogFlow(("FFmpeg::NotifyUpdate called: x=%lu, y=%lu, w=%lu, h=%lu\n",
             x, y, w, h));
    if (!finished)
        return E_POINTER;
    /* For now we will do things synchronously */
    *finished = true;
    /* We always leave at least one frame update pending, which we
       process when the time until the next frame has elapsed. */
    if (iCurrentTime - mLastTime >= 40)
    {
        rc = do_rgb_to_yuv_conversion();
        if (rc != S_OK)
        {
            copy_to_intermediate_buffer(x, y, w, h);
            return rc;
        }
        rc = do_encoding_and_write();
        if (rc != S_OK)
        {
            copy_to_intermediate_buffer(x, y, w, h);
            return rc;
        }
        mLastTime = mLastTime + 40;
        /* Write frames for the time in-between.  Not a good way
           to handle this. */
        while (iCurrentTime - mLastTime >= 40)
        {
/*            rc = do_rgb_to_yuv_conversion();
            if (rc != S_OK)
            {
                copy_to_intermediate_buffer(x, y, w, h);
                return rc;
            }
*/          rc = do_encoding_and_write();
            if (rc != S_OK)
            {
                copy_to_intermediate_buffer(x, y, w, h);
                return rc;
            }
            mLastTime = mLastTime + 40;
        }
    }
    /* Finally we copy the updated data to the intermediate buffer,
       ready for the next update. */
    copy_to_intermediate_buffer(x, y, w, h);
    return S_OK;
}


/**
 * Requests a resize of our "screen".
 *
 * @returns COM status code
 * @param   pixelFormat Layout of the guest video RAM (i.e. 16, 24,
 *                      32 bpp)
 * @param   vram        host context pointer to the guest video RAM,
 *                      in case we can cope with the format
 * @param   bitsPerPixel color depth of the guest video RAM
 * @param   bytesPerLine length of a screen line in the guest video RAM
 * @param   w           video mode width in pixels
 * @param   h           video mode height in pixels
 * @retval  finished    set to true if the method is synchronous and
 *                      to false otherwise
 *
 * This method is called when the guest attempts to resize the virtual
 * screen.  The pointer to the guest's video RAM is supplied in case
 * the framebuffer can handle the pixel format.  If it can't, it should
 * allocate a memory buffer itself, and the virtual VGA device will copy
 * the guest VRAM to that in a format we can handle.  The
 * COMGETTER(UsesGuestVRAM) method is used to tell the VGA device which method
 * we have chosen, and the other COMGETTER methods tell the device about
 * the layout of our buffer.  We currently handle all VRAM layouts except
 * FramebufferPixelFormat_Opaque (which cannot be handled by
 * definition).
 */
STDMETHODIMP FFmpegFB::RequestResize(ULONG aScreenId, ULONG pixelFormat,
                                     BYTE *vram, ULONG bitsPerPixel,
                                     ULONG bytesPerLine,
                                     ULONG w, ULONG h, BOOL *finished)
{
    NOREF(aScreenId);
    if (!finished)
        return E_POINTER;
    LogFlow(("FFmpeg::RequestResize called: pixelFormat=%d, vram=%lu, "
             "bpp=%lu bpl=%lu, w=%lu, h=%lu\n",
             pixelFormat, vram, bitsPerPixel, bytesPerLine, w, h));
    /* For now, we are doing things synchronously */
    *finished = true;

    /* We always reallocate our buffer */
    if (mRGBBuffer)
        RTMemFree(mRGBBuffer);
    mGuestWidth = w;
    mGuestHeight = h;

    bool fallback = false;

    /* See if there are conditions under which we can use the guest's VRAM,
     * fallback to our own memory buffer otherwise */

    if (pixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (bitsPerPixel)
        {
            case 32:
                mFFMPEGPixelFormat = PIX_FMT_RGBA32;
                Log2(("FFmpeg::RequestResize: setting ffmpeg pixel format to PIX_FMT_RGBA32\n"));
                break;
            case 24:
                mFFMPEGPixelFormat = PIX_FMT_RGB24;
                Log2(("FFmpeg::RequestResize: setting ffmpeg pixel format to PIX_FMT_RGB24\n"));
                break;
            case 16:
                mFFMPEGPixelFormat = PIX_FMT_RGB565;
                Log2(("FFmpeg::RequestResize: setting ffmpeg pixel format to PIX_FMT_RGB565\n"));
                break;
            default:
                fallback = true;
                break;
        }
    }
    else
    {
        fallback = true;
    }

    if (!fallback)
    {
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        mBufferAddress = reinterpret_cast<uint8_t *>(vram);
        mBytesPerLine = bytesPerLine;
        mBitsPerPixel = bitsPerPixel;
        mRGBBuffer = 0;
        Log2(("FFmpeg::RequestResize: setting mBufferAddress to vram and mLineSize to %lu\n", mBytesPerLine));
    }
    else
    {
        /* we always fallback to 32bpp RGB */
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        mFFMPEGPixelFormat = PIX_FMT_RGBA32;
        Log2(("FFmpeg::RequestResize: setting ffmpeg pixel format to PIX_FMT_RGBA32\n"));

        mBytesPerLine = w * 4;
        mBitsPerPixel = 32;
        mRGBBuffer = reinterpret_cast<uint8_t *>(RTMemAlloc(mBytesPerLine * h));
        AssertReturn(mRGBBuffer != 0, E_OUTOFMEMORY);
        Log2(("FFmpeg::RequestResize: alloc'ing mBufferAddress and mRGBBuffer to %p and mBytesPerLine to %lu\n", mBufferAddress,
              mBytesPerLine));
        mBufferAddress = mRGBBuffer;
    }

    /* Fill in the structure describing the (intermediate) guest
       framebuffer so that ffmpeg can copy correctly between the two */
    avpicture_fill(&mGuestPicture, mTempRGBBuffer, mFFMPEGPixelFormat,
                   mFrameWidth, mFrameHeight);
    /* Blank out the intermediate frame framebuffer */
    memset(mTempRGBBuffer, 0, mFrameWidth * mFrameHeight * 4);
    return S_OK;
}


/**
 * Queries whether we support a given accelerated opperation.  Since we
 * do not have any way of performing accelerated operations, we always
 * return false in supported.
 *
 * @returns          COM status code
 * @param  operation The operation being queried
 * @retval supported Whether or not we support that operation
 */
STDMETHODIMP FFmpegFB::OperationSupported(FramebufferAccelerationOperation_T operation,
                                                     BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    *supported = false;
    return S_OK;
}

/**
 * Returns whether we like the given video mode.
 *
 * @returns COM status code
 * @param   width     video mode width in pixels
 * @param   height    video mode height in pixels
 * @param   bpp       video mode bit depth in bits per pixel
 * @param   supported pointer to result variable
 *
 * As far as I know, the only restruction we have on video modes is that
 * we have to have an even number of horizontal and vertical pixels.
 * I sincerely doubt that anything else will be requested, and if it
 * is anyway, we will just silently amputate one line when we write to
 * the mpeg file.
 */
STDMETHODIMP FFmpegFB::VideoModeSupported(ULONG width, ULONG height,
                                          ULONG bpp, BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    *supported = true;
    return S_OK;
}

/**
 * Since we currently do not have any way of doing this faster than
 * the VGA device, we simply false in handled.  Behaviour taken from
 * src/VBox/RDP/server/framebuffer.cpp.
 */
STDMETHODIMP FFmpegFB::SolidFill(ULONG x, ULONG y, ULONG width,
                                 ULONG height, ULONG color, BOOL *handled)
{
    LogFlow(("FFmpeg::SolidFill called.\n"));
    if (!handled)
        return E_POINTER;
    *handled = false;
    return S_OK;
}

/**
 * Since we currently do not have any way of doing this faster than
 * the VGA device, we simply false in handled.  Behaviour taken from
 * src/VBox/RDP/server/framebuffer.cpp.
 */
STDMETHODIMP FFmpegFB::CopyScreenBits(ULONG xDst, ULONG yDst, ULONG xSrc,
                                      ULONG ySrc, ULONG width,
                                      ULONG height, BOOL *handled)
{
    LogFlow(("FFmpeg::CopyScreenBits called.\n"));
    if (!handled)
        return E_POINTER;
    *handled = false;
    return S_OK;
}

/** Stubbed */
STDMETHODIMP FFmpegFB::GetVisibleRegion(BYTE *rectangles, ULONG /* count */, ULONG * /* countCopied */)
{
    if (!rectangles)
        return E_POINTER;
    *rectangles = 0;
    return S_OK;
}

/** Stubbed */
STDMETHODIMP FFmpegFB::SetVisibleRegion(BYTE *rectangles, ULONG /* count */)
{
    if (!rectangles)
        return E_POINTER;
    return S_OK;
}


// Private Methods
//////////////////////////////////////////////////////////////////////////
//

HRESULT FFmpegFB::setup_library()
{
    /* Set up the avcodec library */
    avcodec_init();
    /* Register all codecs in the library. */
    avcodec_register_all();
    /* Register all formats in the format library */
    av_register_all();
    mpFormatContext = av_alloc_format_context();
    AssertReturn(mpFormatContext != 0, E_OUTOFMEMORY);
    mpStream = av_new_stream(mpFormatContext, 0);
    AssertReturn(mpStream != 0, E_UNEXPECTED);
    strncpy(mpFormatContext->filename, com::Utf8Str(mFileName),
            sizeof(mpFormatContext->filename));
    return S_OK;
}


/**
 * Determine the correct output format and codec for our MPEG file.
 *
 * @returns COM status code
 *
 * @pre The format context (mpFormatContext) should have already been
 * allocated.
 */
HRESULT FFmpegFB::setup_output_format()
{
    Assert(mpFormatContext != 0);
    AVOutputFormat *pOutFormat = guess_format(0, com::Utf8Str(mFileName),
                                              0);
    AssertMsgReturn(pOutFormat != 0,
                    ("Could not deduce output format from file name\n"),
                    E_INVALIDARG);
    AssertMsgReturn((pOutFormat->flags & AVFMT_RAWPICTURE) == 0,
                    ("Can't handle output format for file\n"),
                    E_INVALIDARG);
    AssertMsgReturn((pOutFormat->flags & AVFMT_NOFILE) == 0,
                    ("pOutFormat->flags=%x, pOutFormat->name=%s\n",
                    pOutFormat->flags, pOutFormat->name), E_UNEXPECTED);
    AssertMsgReturn(pOutFormat->video_codec != CODEC_ID_NONE,
                    ("No video codec available - you have probably selected a non-video file format\n"), E_UNEXPECTED);
    mpFormatContext->oformat = pOutFormat;
    /* Set format specific parameters - requires the format to be set. */
    int rcSetParam = av_set_parameters(mpFormatContext, 0);
    AssertReturn(rcSetParam >= 0, E_UNEXPECTED);
#if 1 /* bird: This works for me on the mac, please review & test elsewhere. */
    /* Fill in any uninitialized parameters like opt_output_file in ffpmeg.c does.
       This fixes most of the buffer underflow warnings:
       http://lists.mplayerhq.hu/pipermail/ffmpeg-devel/2005-June/001699.html */
    if (!mpFormatContext->preload)
        mpFormatContext->preload = (int)(0.5 * AV_TIME_BASE);
    if (!mpFormatContext->max_delay)
        mpFormatContext->max_delay = (int)(0.7 * AV_TIME_BASE);
#endif
    return S_OK;
}


/**
 * Open the FFmpeg codec and set it up (width, etc) for our MPEG file.
 *
 * @returns COM status code
 *
 * @pre The format context (mpFormatContext) and the stream (mpStream)
 * should have already been allocated.
 */
HRESULT FFmpegFB::open_codec()
{
    Assert(mpFormatContext != 0);
    Assert(mpStream != 0);
    AVOutputFormat *pOutFormat = mpFormatContext->oformat;
    AVCodecContext *pCodecContext = mpStream->codec;
    AssertReturn(pCodecContext != 0, E_UNEXPECTED);
    AVCodec *pCodec = avcodec_find_encoder(pOutFormat->video_codec);
    AssertReturn(pCodec != 0, E_UNEXPECTED);
    pCodecContext->codec_id = pOutFormat->video_codec;
    pCodecContext->codec_type = CODEC_TYPE_VIDEO;
    pCodecContext->bit_rate = mBitRate;
    pCodecContext->width = mFrameWidth;
    pCodecContext->height = mFrameHeight;
    pCodecContext->time_base.den = 25;
    pCodecContext->time_base.num = 1;
    pCodecContext->gop_size = 12; /* at most one intra frame in 12 */
    pCodecContext->max_b_frames = 1;
    pCodecContext->pix_fmt = PIX_FMT_YUV420P;
    /* taken from the ffmpeg output example */
    // some formats want stream headers to be seperate
    if (!strcmp(pOutFormat->name, "mp4")
        || !strcmp(pOutFormat->name, "mov")
        || !strcmp(pOutFormat->name, "3gp"))
        pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    /* end output example section */
    int rcOpenCodec = avcodec_open(pCodecContext, pCodec);
    AssertReturn(rcOpenCodec >= 0, E_UNEXPECTED);
    return S_OK;
}


/**
 * Open our MPEG file and write the header.
 *
 * @returns COM status code
 *
 * @pre The format context (mpFormatContext) and the stream (mpStream)
 * should have already been allocated and set up.
 */
HRESULT FFmpegFB::open_output_file()
{
    char szFileName[RTPATH_MAX];
    Assert(mpFormatContext);
    Assert(mpFormatContext->oformat);
    strcpy(szFileName, com::Utf8Str(mFileName));
    int rcUrlFopen = url_fopen(&mpFormatContext->pb,
                               szFileName, URL_WRONLY);
    AssertReturn(rcUrlFopen >= 0, E_UNEXPECTED);
    av_write_header(mpFormatContext);
    return S_OK;
}


/**
 * Copy an area from the output buffer used by the virtual VGA (may
 * just be the guest's VRAM) to our fixed size intermediate buffer.
 * The picture in the intermediate buffer is centred if the guest
 * screen dimensions are smaller and amputated if they are larger than
 * our frame dimensions.
 *
 * @param x        X co-ordinate of the upper left-hand corner of the
 *                 area which has been updated
 * @param y        Y co-ordinate of the upper left-hand corner of the
 *                 area which has been updated
 * @param w        width of the area which has been updated
 * @param h        height of the area which has been updated
 */
void FFmpegFB::copy_to_intermediate_buffer(ULONG x, ULONG y, ULONG w, ULONG h)
{
    Log2(("FFmpegFB::copy_to_intermediate_buffer: x=%lu, y=%lu, w=%lu, h=%lu\n",
          x, y, w, h));
    /* Perform clipping and calculate the destination co-ordinates */
    ULONG destX, destY, bpp;
    LONG xDiff = (LONG(mFrameWidth) - LONG(mGuestWidth)) / 2;
    LONG yDiff = (LONG(mFrameHeight) - LONG(mGuestHeight)) / 2;
    if (LONG(w) + xDiff + LONG(x) <= 0)  /* nothing visible */
        return;
    if (LONG(x) < -xDiff)
    {
        w = LONG(w) + xDiff + x;
        x = -xDiff;
        destX = 0;
    }
    else
        destX = x + xDiff;
    if (LONG(h) + yDiff + LONG(y) <= 0)  /* nothing visible */
        return;
    if (LONG(y) < -yDiff)
    {
        h = LONG(h) + yDiff + LONG(y);
        y = -yDiff;
        destY = 0;
    }
    else
        destY = y + yDiff;
    if (destX > mFrameWidth || destY > mFrameHeight)
        return;  /* nothing visible */
    if (destX + w > mFrameWidth)
        w = mFrameWidth - destX;
    if (destY + h > mFrameHeight)
        h = mFrameHeight - destY;
    /* Calculate bytes per pixel */
    if (mPixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (mBitsPerPixel)
        {
            case 32:
            case 24:
            case 16:
                bpp = mBitsPerPixel / 8;
                break;
            default:
                AssertMsgFailed(("Unknown color depth! mBitsPerPixel=%d\n", mBitsPerPixel));
                bpp = 1;
                break;
        }
    }
    else
    {
        AssertMsgFailed(("Unknown pixel format! mPixelFormat=%d\n", mPixelFormat));
        bpp = 1;
    }
    /* Calculate start offset in source and destination buffers */
    ULONG srcOffs = y * mBytesPerLine + x * bpp;
    ULONG destOffs = (destY * mFrameWidth + destX) * bpp;
    /* do the copy */
    for (unsigned int i = 0; i < h; i++)
    {
        /* Overflow check */
        Assert(srcOffs + w * bpp <= mGuestHeight * mBytesPerLine);
        Assert(destOffs + w * bpp <= mFrameHeight * mFrameWidth * bpp);
        memcpy(mTempRGBBuffer + destOffs, mBufferAddress + srcOffs,
               w * bpp);
        srcOffs = srcOffs + mBytesPerLine;
        destOffs = destOffs + mFrameWidth * bpp;
    }
}


/**
 * Copy the RGB data in the intermediate framebuffer to YUV data in
 * the YUV framebuffer.
 *
 * @returns COM status code
 */
HRESULT FFmpegFB::do_rgb_to_yuv_conversion()
{
    int rc = img_convert(&mFramePicture, PIX_FMT_YUV420P,
                         &mGuestPicture, mFFMPEGPixelFormat,
                         mFrameWidth, mFrameHeight);
    AssertMsg(rc >= 0, ("img_convert() failed.  rc=%d, mFFMPEGPixelFormat=%d\n"
                   "mFrameWidth=%u, mFrameHeight=%u\n", rc,
                   mFFMPEGPixelFormat, mFrameWidth, mFrameHeight));
    if (rc < 0)
        return E_UNEXPECTED;
    return S_OK;
}


/**
 * Encode the YUV framebuffer as an MPEG frame and write it to the file.
 *
 * @returns COM status code
 */
HRESULT FFmpegFB::do_encoding_and_write()
{
    AVCodecContext *pContext = mpStream->codec;

    /* A hack: ffmpeg mpeg2 only writes a frame if something has
        changed.  So we flip the low luminance bit of the first
        pixel every frame. */
    if (mToggle)
        mYUVBuffer[0] |= 1;
    else
        mYUVBuffer[0] &= 0xfe;
    mToggle = !mToggle;
    int cSize = avcodec_encode_video(pContext, mOutBuf, mYUVFrameSize * 2,
                                  mFrame);
    AssertMsgReturn(cSize >= 0,
                    ("avcodec_encode_video() failed with rc=%d.\n"
                     "mFrameWidth=%u, mFrameHeight=%u\n", cSize,
                     mFrameWidth, mFrameHeight), E_UNEXPECTED);
    if (cSize > 0)
    {
        AVPacket Packet;
        av_init_packet(&Packet);
        Packet.pts = av_rescale_q(pContext->coded_frame->pts,
                                 pContext->time_base,
                                 mpStream->time_base);
        if(pContext->coded_frame->key_frame)
            Packet.flags |= PKT_FLAG_KEY;
        Packet.stream_index = mpStream->index;
        Packet.data = mOutBuf;
        Packet.size = cSize;

        /* write the compressed frame in the media file */
        int rcWriteFrame = av_write_frame(mpFormatContext, &Packet);
        AssertReturn(rcWriteFrame == 0, E_UNEXPECTED);
    }
    return S_OK;
}


/**
 * Capture the current (i.e. the last) frame as a PNG file with the
 * same basename as the captured video file.
 */
HRESULT FFmpegFB::write_png()
{
    HRESULT errorCode = E_OUTOFMEMORY;
    png_bytep *row_pointers;
    char PNGFileName[RTPATH_MAX], oldName[RTPATH_MAX];
    png_structp png_ptr;
    png_infop info_ptr;
    AVPicture libPNGPicture;
    uint8_t *PNGBuffer;
    int rc;
    /* Work out the new file name - for some reason, we can't use
       the com::Utf8Str() directly, but have to copy it */
    strcpy(oldName, com::Utf8Str(mFileName));
    int baseLen = strrchr(oldName, '.') - oldName;
    if (baseLen == 0)
        baseLen = strlen(oldName);
    if (baseLen >= RTPATH_MAX - 5)  /* for whatever reason */
        baseLen = RTPATH_MAX - 5;
    memcpy(&PNGFileName[0], oldName, baseLen);
    PNGFileName[baseLen] = '.';
    PNGFileName[baseLen + 1] = 'p';
    PNGFileName[baseLen + 2] = 'n';
    PNGFileName[baseLen + 3] = 'g';
    PNGFileName[baseLen + 4] = 0;
    /* Open output file */
    FILE *fp = fopen(PNGFileName, "wb");
    if (fp == 0)
    {
       errorCode = E_UNEXPECTED;
       goto fopen_failed;
    }
    /* Create libpng basic structures */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
                            0 /* error function */, 0 /* warning function */);
    if (png_ptr == 0)
       goto png_create_write_struct_failed;
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == 0)
    {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        goto png_create_info_struct_failed;
    }
    /* Convert image to standard RGB24 to simplify life */
    PNGBuffer = reinterpret_cast<uint8_t *>(av_malloc(mFrameWidth
                                                      * mFrameHeight * 4));
    if (PNGBuffer == 0)
        goto av_malloc_buffer_failed;
    row_pointers =
        reinterpret_cast<png_bytep *>(av_malloc(mFrameHeight
                                                * sizeof(png_bytep)));
    if (row_pointers == 0)
        goto av_malloc_pointers_failed;
    avpicture_fill(&libPNGPicture, PNGBuffer, PIX_FMT_RGB24,
                   mFrameWidth, mFrameHeight);
    rc = img_convert(&libPNGPicture, PIX_FMT_RGB24,
                         &mGuestPicture, mFFMPEGPixelFormat,
                         mFrameWidth, mFrameHeight);
    if (rc < 0)
        goto setjmp_exception;
    /* libpng exception handling */
    if (setjmp(png_jmpbuf(png_ptr)))
       goto setjmp_exception;
    /* pass libpng the file pointer */
    png_init_io(png_ptr, fp);
    /* set the image properties */
    png_set_IHDR(png_ptr, info_ptr, mFrameWidth, mFrameHeight,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* set up the information about the bitmap for libpng */
    row_pointers[0] = png_bytep(PNGBuffer);
    for (unsigned i = 1; i < mFrameHeight; i++)
        row_pointers[i] = row_pointers[i  - 1] + mFrameWidth * 3;
    png_set_rows(png_ptr, info_ptr, &row_pointers[0]);
    /* and write the thing! */
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
    /* drop through to cleanup */
    errorCode = S_OK;
setjmp_exception:
    av_free(row_pointers);
av_malloc_pointers_failed:
    av_free(PNGBuffer);
av_malloc_buffer_failed:
    png_destroy_write_struct(&png_ptr, &info_ptr);
png_create_info_struct_failed:
png_create_write_struct_failed:
    fclose(fp);
fopen_failed:
    if (errorCode != S_OK)
        Log(("FFmpegFB::write_png: Failed to write .png image of final frame\n"));
    return errorCode;
}


#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(FFmpegFB)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(FFmpegFB, IFramebuffer)
#endif
