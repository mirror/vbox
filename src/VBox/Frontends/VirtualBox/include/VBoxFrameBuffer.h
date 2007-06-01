/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer class and subclasses declarations
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VBoxFrameBuffer_h__
#define __VBoxFrameBuffer_h__

#include "COMDefs.h"

class VBoxConsoleView;

#include <qmutex.h>
#include <qevent.h>
#include <qpixmap.h>
#include <qimage.h>

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

//#define VBOX_GUI_FRAMEBUF_STAT

#if defined (VBOX_GUI_DEBUG) && defined (VBOX_GUI_FRAMEBUF_STAT)
#define FRAMEBUF_DEBUG_START(prefix) \
    uint64_t prefix##elapsed = VMCPUTimer::ticks();
#define FRAMEBUF_DEBUG_STOP(prefix,w,h) { \
        prefix##elapsed = VMCPUTimer::ticks() - prefix##elapsed; \
        V_DEBUG(( "Last update: %04d x %04d px, %03.3f ms, %.0f ticks", \
            (w), (h), \
            (double) prefix##elapsed / (double) VMCPUTimer::ticksPerMsec(), \
            (double) prefix##elapsed \
        )); \
    }
#else
#define FRAMEBUF_DEBUG_START(prefix) {}
#define FRAMEBUF_DEBUG_STOP(prefix,w,h) {}
#endif

/////////////////////////////////////////////////////////////////////////////

/**
 *  Frame buffer resize event.
 */
class VBoxResizeEvent : public QEvent
{
public:
    VBoxResizeEvent (FramebufferPixelFormat_T f, uchar *v, unsigned l, int w, int h) :
        QEvent ((QEvent::Type) VBoxDefs::ResizeEventType), fmt (f), vr (v), lsz (l), wdt (w), hgt (h) {}
    FramebufferPixelFormat_T  pixelFormat() { return fmt; }
    uchar *vram() { return vr; }
    unsigned lineSize() { return lsz; }
    int width() { return wdt; }
    int height() { return hgt; }
private:
    FramebufferPixelFormat_T fmt;
    uchar *vr;
    unsigned lsz;
    int wdt;
    int hgt;
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

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_REFRESH_TIMER)

/**
 *  Copies the current VM video buffer contents to the pixmap referenced
 *  by the argument. The return value indicates whether the
 *  buffer has been updated since the last call to this method or not.
 *
 *  The copy operation is atomic (guarded by a mutex).
 *
 *  This method is intentionally inlined for faster execution and should be
 *  called only by VBoxConsoleView members.
 *
 *  @return true if the pixmap is updated, and false otherwise.
 */
inline bool display_to_pixmap( const CConsole &c, QPixmap &pm )
{
    CDisplay display = c.GetDisplay();

    uint8_t *addr = (uint8_t *) display.LockFramebuffer();
    AssertMsg (addr, ("The buffer address must not be null"));

    bool rc = pm.convertFromImage (QImage (addr,
                                           display.GetWidth(), display.GetHeight(),
                                           display.GetColorDepth(),
                                           0, 0, QImage::LittleEndian));
    AssertMsg (rc, ("convertFromImage() must always return true"));

    display.UnlockFramebuffer();

    return rc;
}

#endif

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

class VBoxFrameBuffer : public IFramebuffer
{
public:

    VBoxFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxFrameBuffer();

    NS_DECL_ISUPPORTS

#if defined (Q_OS_WIN32)
    STDMETHOD_(ULONG, AddRef)() {
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
    STDMETHOD(COMGETTER(ColorDepth)) (ULONG *aColorDepth);
    STDMETHOD(COMGETTER(LineSize)) (ULONG *aLineSize);
    STDMETHOD(COMGETTER(PixelFormat)) (FramebufferPixelFormat_T *aPixelFormat);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *aHeightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);

    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();

    STDMETHOD(RequestResize) (FramebufferPixelFormat_T aPixelFormat,
                              BYTE *aVRAM, ULONG aLineSize,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished);

    STDMETHOD(OperationSupported)(FramebufferAccelerationOperation_T aOperation,
                                  BOOL *aSupported);
    STDMETHOD(VideoModeSupported) (ULONG aWidth, ULONG aHeight, ULONG aBPP,
                                   BOOL *aSupported);
    STDMETHOD(SolidFill) (ULONG aX, ULONG aY, ULONG aWidth, ULONG aHeight,
                          ULONG aColor, BOOL *aHandled);
    STDMETHOD(CopyScreenBits) (ULONG aXDst, ULONG aYDst, ULONG aXSrc, ULONG aYSrc,
                               ULONG aWidth, ULONG aHeight, BOOL *aHandled);

    // Helper functions
    int width() { return mWdt; }
    int height() { return mHgt; }

    virtual FramebufferPixelFormat_T pixelFormat()
    {
        return FramebufferPixelFormat_PixelFormatDefault;
    }

    void lock() { mMutex->lock(); }
    void unlock() { mMutex->unlock(); }

    virtual uchar *address() = 0;
    virtual int colorDepth() = 0;
    virtual int lineSize() = 0;

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
    QMutex *mMutex;
    int mWdt;
    int mHgt;

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

    uchar *address() { return mImg.bits(); }
    int colorDepth() { return mImg.depth(); }
    int lineSize() { return mImg.bytesPerLine(); }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    QPixmap mPM;
    QImage mImg;
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
        if (mSurfVRAM)
        {
            return (uchar*) (mScreen ? (uintptr_t) mSurfVRAM->pixels : 0);
        }
        else
        {
            return (uchar*) (mScreen ? (uintptr_t) mScreen->pixels : 0);
        }
    }

    int colorDepth()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM: mScreen;
        return surf ? surf->format->BitsPerPixel : 0;
    }

    int lineSize()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM: mScreen;
        return surf ? surf->pitch : 0;
    }

    FramebufferPixelFormat_T pixelFormat()
    {
        return mPixelFormat;
    }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    SDL_Surface *mScreen;
    SDL_Surface *mSurfVRAM;

    uchar *mPtrVRAM;
    ULONG mLineSize;
    FramebufferPixelFormat_T mPixelFormat;
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

    uchar *address() { return (uchar *)mSurfaceDesc.lpSurface; }
    int colorDepth() { return mSurfaceDesc.ddpfPixelFormat.dwRGBBitCount; }
    int lineSize() { return mSurfaceDesc.lPitch; }

    FramebufferPixelFormat_T pixelFormat() { return mPixelFormat; };

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);
    void moveEvent (QMoveEvent *me);

private:
    void releaseObjects();

    void setupSurface (FramebufferPixelFormat_T pixelFormat, uchar *pvVRAM, ULONG lineSize, ULONG w, ULONG h);
    void recreateSurface (FramebufferPixelFormat_T pixelFormat, uchar *pvVRAM, ULONG lineSize, ULONG w, ULONG h);
    void deleteSurface ();
    void drawRect (ULONG x, ULONG y, ULONG w, ULONG h);
    void getWindowPosition (void);

    LPDIRECTDRAW7        mDDRAW;
    LPDIRECTDRAWCLIPPER  mClipper;
    LPDIRECTDRAWSURFACE7 mSurface;
    DDSURFACEDESC2       mSurfaceDesc;
    LPDIRECTDRAWSURFACE7 mPrimarySurface;

    FramebufferPixelFormat_T mPixelFormat;

    BOOL mGuestVRAMSurface;

    int mWndX;
    int mWndY;

    BOOL mSynchronousUpdates;
};

#endif

#endif // __VBoxFrameBuffer_h__
