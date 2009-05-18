/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer class and subclasses declarations
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

#ifndef ___VBoxFrameBuffer_h___
#define ___VBoxFrameBuffer_h___

#include "COMDefs.h"
#include <iprt/critsect.h>

/* Qt includes */
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QPaintEvent>
#include <QMoveEvent>

#if defined (VBOX_GUI_USE_SDL)
#include <SDL.h>
#include <signal.h>
#endif

#if defined (Q_WS_WIN) && defined (VBOX_GUI_USE_DDRAW)
// VBox/cdefs.h defines these:
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <ddraw.h>
#endif

class VBoxConsoleView;

/////////////////////////////////////////////////////////////////////////////

/**
 *  Frame buffer resize event.
 */
class VBoxResizeEvent : public QEvent
{
public:

    VBoxResizeEvent (ulong aPixelFormat, uchar *aVRAM,
                     ulong aBitsPerPixel, ulong aBytesPerLine,
                     ulong aWidth, ulong aHeight) :
        QEvent ((QEvent::Type) VBoxDefs::ResizeEventType),
        mPixelFormat (aPixelFormat), mVRAM (aVRAM), mBitsPerPixel (aBitsPerPixel),
        mBytesPerLine (aBytesPerLine), mWidth (aWidth), mHeight (aHeight) {}
    ulong pixelFormat() { return mPixelFormat; }
    uchar *VRAM() { return mVRAM; }
    ulong bitsPerPixel() { return mBitsPerPixel; }
    ulong bytesPerLine() { return mBytesPerLine; }
    ulong width() { return mWidth; }
    ulong height() { return mHeight; }

private:

    ulong mPixelFormat;
    uchar *mVRAM;
    ulong mBitsPerPixel;
    ulong mBytesPerLine;
    ulong mWidth;
    ulong mHeight;
};

/**
 *  Frame buffer repaint event.
 */
class VBoxRepaintEvent : public QEvent
{
public:
    VBoxRepaintEvent (int x, int y, int w, int h) :
        QEvent ((QEvent::Type) VBoxDefs::RepaintEventType),
        ex (x), ey (y), ew (w), eh (h)
    {}
    int x() { return ex; }
    int y() { return ey; }
    int width() { return ew; }
    int height() { return eh; }
private:
    int ex, ey, ew, eh;
};

/**
 *  Frame buffer set region event.
 */
class VBoxSetRegionEvent : public QEvent
{
public:
    VBoxSetRegionEvent (const QRegion &aReg)
        : QEvent ((QEvent::Type) VBoxDefs::SetRegionEventType)
        , mReg (aReg) {}
    QRegion region() { return mReg; }
private:
    QRegion mReg;
};

/////////////////////////////////////////////////////////////////////////////

/**
 *  Common IFramebuffer implementation for all methods used by GUI to maintain
 *  the VM display video memory.
 *
 *  Note that although this class can be called from multiple threads
 *  (in particular, the GUI thread and EMT) it doesn't protect access to every
 *  data field using its mutex lock. This is because all synchronization between
 *  the GUI and the EMT thread is supposed to be done using the
 *  IFramebuffer::NotifyUpdate() and IFramebuffer::RequestResize() methods
 *  (in particular, the \a aFinished parameter of these methods is responsible
 *  for the synchronization). These methods are always called on EMT and
 *  therefore always follow one another but never in parallel.
 *
 *  Using this object's mutex lock (exposed also in IFramebuffer::Lock() and
 *  IFramebuffer::Unlock() implementations) usually makes sense only if some
 *  third-party thread (i.e. other than GUI or EMT) needs to make sure that
 *  *no* VM display update or resize event can occur while it is accessing
 *  IFramebuffer properties or the underlying display memory storage area.
 *
 *  See IFramebuffer documentation for more info.
 */

class VBoxFrameBuffer : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:

    VBoxFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxFrameBuffer();

    NS_DECL_ISUPPORTS

#if defined (Q_OS_WIN32)

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
        if (riid == IID_IUnknown) {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IFramebuffer) {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }

#endif

    // IFramebuffer COM methods
    STDMETHOD(COMGETTER(Address)) (BYTE **aAddress);
    STDMETHOD(COMGETTER(Width)) (ULONG *aWidth);
    STDMETHOD(COMGETTER(Height)) (ULONG *aHeight);
    STDMETHOD(COMGETTER(BitsPerPixel)) (ULONG *aBitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine)) (ULONG *aBytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *aPixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *aUsesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *aHeightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId);

    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();

    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished);

    STDMETHOD(VideoModeSupported) (ULONG aWidth, ULONG aHeight, ULONG aBPP,
                                   BOOL *aSupported);

    STDMETHOD(GetVisibleRegion)(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *aRectangles, ULONG aCount);

    ulong width() { return mWdt; }
    ulong height() { return mHgt; }

    virtual ulong pixelFormat()
    {
        return FramebufferPixelFormat_FOURCC_RGB;
    }

    virtual bool usesGuestVRAM()
    {
        return false;
    }

    void lock() { RTCritSectEnter(&mCritSect); }
    void unlock() { RTCritSectLeave(&mCritSect); }

    virtual uchar *address() = 0;
    virtual ulong bitsPerPixel() = 0;
    virtual ulong bytesPerLine() = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when some part of the
     *  VM display viewport needs to be repainted on the host screen.
     */
    virtual void paintEvent (QPaintEvent *pe) = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) after it gets a
     *  VBoxResizeEvent posted from the RequestResize() method implementation.
     */
    virtual void resizeEvent (VBoxResizeEvent *re)
    {
        mWdt = re->width();
        mHgt = re->height();
    }

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when the VM console
     *  window is moved.
     */
    virtual void moveEvent (QMoveEvent * /*me*/ ) {}

protected:

    VBoxConsoleView *mView;
    RTCRITSECT mCritSect;
    int mWdt;
    int mHgt;
    uint64_t mWinId;

#if defined (Q_OS_WIN32)
private:
    long refcnt;
#endif
};

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QIMAGE)

class VBoxQImageFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxQImageFrameBuffer (VBoxConsoleView *aView);

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH,
                             BOOL *aFinished);

    ulong pixelFormat() { return mPixelFormat; }
    bool usesGuestVRAM() { return mUsesGuestVRAM; }

    uchar *address() { return mImg.bits(); }
    ulong bitsPerPixel() { return mImg.depth(); }
    ulong bytesPerLine() { return mImg.bytesPerLine(); }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    QPixmap mPM;
    QImage mImg;
    ulong mPixelFormat;
    bool mUsesGuestVRAM;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_SDL)

class VBoxSDLFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxSDLFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxSDLFrameBuffer();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH,
                             BOOL *aFinished);

    uchar *address()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? (uchar *) (uintptr_t) surf->pixels : 0;
    }

    ulong bitsPerPixel()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? surf->format->BitsPerPixel : 0;
    }

    ulong bytesPerLine()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? surf->pitch : 0;
    }

    ulong pixelFormat()
    {
        return mPixelFormat;
    }

    bool usesGuestVRAM()
    {
        return mSurfVRAM != NULL;
    }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    SDL_Surface *mScreen;
    SDL_Surface *mSurfVRAM;

    ulong mPixelFormat;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_DDRAW)

class VBoxDDRAWFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxDDRAWFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxDDRAWFrameBuffer();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH,
                             BOOL *aFinished);

    uchar *address() { return (uchar *) mSurfaceDesc.lpSurface; }
    ulong bitsPerPixel() { return mSurfaceDesc.ddpfPixelFormat.dwRGBBitCount; }
    ulong bytesPerLine() { return (ulong) mSurfaceDesc.lPitch; }

    ulong pixelFormat() { return mPixelFormat; };

    bool usesGuestVRAM() { return mUsesGuestVRAM; }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);
    void moveEvent (QMoveEvent *me);

private:

    void releaseObjects();

    bool createSurface (ULONG aPixelFormat, uchar *pvVRAM,
                        ULONG aBitsPerPixel, ULONG aBytesPerLine,
                        ULONG aWidth, ULONG aHeight);
    void deleteSurface();
    void drawRect (ULONG x, ULONG y, ULONG w, ULONG h);
    void getWindowPosition (void);

    LPDIRECTDRAW7        mDDRAW;
    LPDIRECTDRAWCLIPPER  mClipper;
    LPDIRECTDRAWSURFACE7 mSurface;
    DDSURFACEDESC2       mSurfaceDesc;
    LPDIRECTDRAWSURFACE7 mPrimarySurface;

    ulong mPixelFormat;

    bool mUsesGuestVRAM;

    int mWndX;
    int mWndY;

    bool mSynchronousUpdates;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (Q_WS_MAC) && defined (VBOX_GUI_USE_QUARTZ2D)

#include <Carbon/Carbon.h>

class VBoxQuartz2DFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxQuartz2DFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxQuartz2DFrameBuffer ();

    STDMETHOD (NotifyUpdate) (ULONG aX, ULONG aY,
                              ULONG aW, ULONG aH,
                              BOOL *aFinished);
    STDMETHOD (SetVisibleRegion) (BYTE *aRectangles, ULONG aCount);

    uchar *address() { return mDataAddress; }
    ulong bitsPerPixel() { return CGImageGetBitsPerPixel (mImage); }
    ulong bytesPerLine() { return CGImageGetBytesPerRow (mImage); }
    ulong pixelFormat() { return mPixelFormat; };
    bool usesGuestVRAM() { return mBitmapData == NULL; }

    const CGImageRef imageRef() const { return mImage; }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    void clean();

    uchar *mDataAddress;
    void *mBitmapData;
    ulong mPixelFormat;
    CGImageRef mImage;
    typedef struct
    {
        /** The size of this structure expressed in rcts entries. */
        ULONG allocated;
        /** The number of entries in the rcts array. */
        ULONG used;
        /** Variable sized array of the rectangle that makes up the region. */
        CGRect rcts[1];
    } RegionRects;
    /** The current valid region, all access is by atomic cmpxchg or atomic xchg.
     *
     * The protocol for updating and using this has to take into account that
     * the producer (SetVisibleRegion) and consumer (paintEvent) are running
     * on different threads. Therefore the producer will create a new RegionRects
     * structure before atomically replace the existing one. While the consumer
     * will read the value by atomically replace it by NULL, and then when its
     * done try restore it by cmpxchg. If the producer has already put a new
     * region there, it will be discarded (see mRegionUnused).
     */
    RegionRects volatile *mRegion;
    /** For keeping the unused region and thus avoid some RTMemAlloc/RTMemFree calls.
     * This is operated with atomic cmpxchg and atomic xchg. */
    RegionRects volatile *mRegionUnused;
};

#endif /* Q_WS_MAC && VBOX_GUI_USE_QUARTZ2D */

#endif // !___VBoxFrameBuffer_h___
