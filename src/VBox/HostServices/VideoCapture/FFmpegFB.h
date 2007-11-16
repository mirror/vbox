/** @file
 *
 * VBox Remote Desktop Protocol.
 * FFmpeg framebuffer interface.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#ifndef _H_FFMPEGFB
#define _H_FFMPEGFB

#include <VBox/com/VirtualBox.h>

#include <iprt/uuid.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include <iprt/runtime.h>
#include <iprt/critsect.h>

#ifdef DEBUG
# define VBOX_DEBUG_FF DEBUG
# include <avcodec.h>
# include <avformat.h>
# define DEBUG VBOX_DEBUG_FF
#else /* DEBUG not defined */
# include <avcodec.h>
# include <avformat.h>
#endif /* DEBUG not defined */

class FFmpegFB : public IFramebuffer
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
    /** An AVPicture structure containing information about the
      * guest framebuffer */
    AVPicture mGuestPicture;
    /** ffmpeg pixel format of guest framebuffer */
    int mFFMPEGPixelFormat;
    /** An AVPicture structure containing information about the
      * MPEG frame framebuffer */
    AVPicture mFramePicture;
    /** Since we are building without exception support, we use this
        to signal allocation failure in the constructor */
    bool mOutOfMemory;
    /** A hack: ffmpeg mpeg2 only writes a frame if something has
        changed.  So we flip the low luminance bit of the first
        pixel every frame. */
    bool mToggle;

    HRESULT setup_library();
    HRESULT setup_output_format();
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


#endif /* !_H_FFMPEGFB */
