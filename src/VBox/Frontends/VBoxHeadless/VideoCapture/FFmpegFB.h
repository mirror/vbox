/** @file
 *
 * VBox Remote Desktop Protocol.
 * FFmpeg framebuffer interface.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef _H_FFMPEGFB
#define _H_FFMPEGFB

#include <VBox/com/VirtualBox.h>

#include <iprt/uuid.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include <iprt/initterm.h>
#include <iprt/critsect.h>

#ifdef DEBUG
# define VBOX_DEBUG_FF DEBUG
# include <avcodec.h>
# include <avformat.h>
# undef  DEBUG
# define DEBUG VBOX_DEBUG_FF
#else /* DEBUG not defined */
# include <avcodec.h>
# include <avformat.h>
#endif /* DEBUG not defined */

class FFmpegFB : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:
    FFmpegFB(ULONG width, ULONG height, ULONG bitrate, com::Bstr filename);
    virtual ~FFmpegFB();

#ifndef VBOX_WITH_XPCOM
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IFramebuffer)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    // public methods only for internal purposes
    HRESULT init ();

    STDMETHOD(COMGETTER(Width))(ULONG *width);
    STDMETHOD(COMGETTER(Height))(ULONG *height);
    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();
    STDMETHOD(COMGETTER(Address))(BYTE **address);
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *bitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *bytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *pixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *usesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *heightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId);

    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y,
                            ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(RequestResize)(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                             ULONG bitsPerPixel, ULONG bytesPerLine,
                             ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(OperationSupported)(FramebufferAccelerationOperation_T operation, BOOL *supported);
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported);
    STDMETHOD(SolidFill)(ULONG x, ULONG y, ULONG width, ULONG height,
                         ULONG color, BOOL *handled);
    STDMETHOD(CopyScreenBits)(ULONG xDst, ULONG yDst, ULONG xSrc, ULONG ySrc,
                              ULONG width, ULONG height, BOOL *handled);
    STDMETHOD(GetVisibleRegion)(BYTE *rectangles, ULONG count, ULONG *countCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *rectangles, ULONG count);

private:
    /** true if url_fopen actually succeeded */
    bool mfUrlOpen;
    /** Guest framebuffer width */
    ULONG mGuestWidth;
    /** Guest framebuffer height */
    ULONG mGuestHeight;
    /** Bit rate used for encoding */
    ULONG mBitRate;
    /** Guest framebuffer pixel format */
    ULONG mPixelFormat;
    /** Guest framebuffer color depth */
    ULONG mBitsPerPixel;
    /** Name of the file we will write to */
    com::Bstr mFileName;
    /** Guest framebuffer line length */
    ULONG mBytesPerLine;
    /** MPEG frame framebuffer width */
    ULONG mFrameWidth;
    /** MPEG frame framebuffer height */
    ULONG mFrameHeight;
    /** The size of one YUV frame */
    ULONG mYUVFrameSize;
    /** If we can't use the video RAM directly, we allocate our own
      * buffer */
    uint8_t *mRGBBuffer;
    /** The address of the buffer - can be either mRGBBuffer or the
      * guests VRAM (HC address) if we can handle that directly */
    uint8_t *mBufferAddress;
    /** An intermediary RGB buffer with the same dimensions */
    uint8_t *mTempRGBBuffer;
    /** Frame buffer translated into YUV420 for the mpeg codec */
    uint8_t *mYUVBuffer;
    /** Temporary buffer into which the codec writes frames to be
      * written into the file */
    uint8_t *mOutBuf;
    RTCRITSECT mCritSect;
    /** File where we store the mpeg stream */
    RTFILE mFile;
    /** time at which the last "real" frame was created */
    int64_t mLastTime;
    /** Pointer to ffmpeg's format information context */
    AVFormatContext *mpFormatContext;
    /** ffmpeg context containing information about the stream */
    AVStream *mpStream;
    /** Information for ffmpeg describing the current frame */
    AVFrame *mFrame;
    /** ffmpeg pixel format of guest framebuffer */
    int mFFMPEGPixelFormat;
    /** Since we are building without exception support, we use this
        to signal allocation failure in the constructor */
    bool mOutOfMemory;
    /** A hack: ffmpeg mpeg2 only writes a frame if something has
        changed.  So we flip the low luminance bit of the first
        pixel every frame. */
    bool mToggle;

    HRESULT setup_library();
    HRESULT setup_output_format();
    HRESULT list_formats();
    HRESULT open_codec();
    HRESULT open_output_file();
    void copy_to_intermediate_buffer(ULONG x, ULONG y, ULONG w, ULONG h);
    HRESULT do_rgb_to_yuv_conversion();
    HRESULT do_encoding_and_write();
    HRESULT write_png();
#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};

/**
 * Iterator class for running through an BGRA32 image buffer and converting
 * it to RGB.
 */
class FFmpegBGRA32Iter
{
private:
    enum { PIX_SIZE = 4 };
public:
    FFmpegBGRA32Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuffer)
    {
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuffer = aBuffer;
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
            *aRed = mBuffer[mPos + 2];
            *aGreen = mBuffer[mPos + 1];
            *aBlue = mBuffer[mPos];
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
    uint8_t *mBuffer;
};

/**
 * Iterator class for running through an BGR24 image buffer and converting
 * it to RGB.
 */
class FFmpegBGR24Iter
{
private:
    enum { PIX_SIZE = 3 };
public:
    FFmpegBGR24Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuffer)
    {
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuffer = aBuffer;
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
            *aRed = mBuffer[mPos + 2];
            *aGreen = mBuffer[mPos + 1];
            *aBlue = mBuffer[mPos];
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
    uint8_t *mBuffer;
};

/**
 * Iterator class for running through an BGR565 image buffer and converting
 * it to RGB.
 */
class FFmpegBGR565Iter
{
private:
    enum { PIX_SIZE = 2 };
public:
    FFmpegBGR565Iter(unsigned aWidth, unsigned aHeight, uint8_t *aBuffer)
    {
        mPos = 0;
        mSize = aWidth * aHeight * PIX_SIZE;
        mBuffer = aBuffer;
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
            unsigned uFull =   (((unsigned) mBuffer[mPos + 1]) << 8)
                             | ((unsigned) mBuffer[mPos]);
            *aRed = (uFull >> 8) & ~7;
            *aGreen = (uFull >> 3) & ~3 & 0xff;
            *aBlue = (uFull << 3) & ~7 & 0xff;
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
    uint8_t *mBuffer;
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
inline bool FFmpegWriteYUV420p(unsigned aWidth, unsigned aHeight, uint8_t *aDestBuf,
                        uint8_t *aSrcBuf)
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
inline bool FFmpegWriteRGB24(unsigned aWidth, unsigned aHeight, uint8_t *aDestBuf,
                        uint8_t *aSrcBuf)
{
    enum { PIX_SIZE = 3 };
    bool rc = true;
    AssertReturn(0 == (aWidth & 1), false);
    AssertReturn(0 == (aHeight & 1), false);
    T iter(aWidth, aHeight, aSrcBuf);
    unsigned cPixels = aWidth * aHeight;
    for (unsigned i = 0; (i < cPixels) && rc; ++i)
    {
        unsigned red, green, blue;
        rc = iter.getRGB(&red, &green, &blue);
        if (rc)
        {
            aDestBuf[i * PIX_SIZE] = red;
            aDestBuf[i * PIX_SIZE + 1] = green;
            aDestBuf[i * PIX_SIZE + 2] = blue;
        }
    }
    return rc;
}

#endif /* !_H_FFMPEGFB */
