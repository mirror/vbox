/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer class and subclasses implementation
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

#include "VBoxFrameBuffer.h"

#include "VBoxConsoleView.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"

#include <qapplication.h>
//Added by qt3to4:
#include <QPaintEvent>

//
// VBoxFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

/** @class VBoxFrameBuffer
 *
 *  Base class for all frame buffer implementations.
 */

#if !defined (Q_OS_WIN32)
NS_DECL_CLASSINFO (VBoxFrameBuffer)
NS_IMPL_ISUPPORTS1_CI (VBoxFrameBuffer, IFramebuffer)
#endif

VBoxFrameBuffer::VBoxFrameBuffer (VBoxConsoleView *aView)
    : mView (aView), mMutex (new QMutex (true))
    , mWdt (0), mHgt (0)
#if defined (Q_OS_WIN32)
    , refcnt (0)
#endif
{
    AssertMsg (mView, ("VBoxConsoleView must not be null\n"));
}

VBoxFrameBuffer::~VBoxFrameBuffer()
{
    delete mMutex;
}

// IFramebuffer implementation methods.
// Just forwarders to relevant class methods.

STDMETHODIMP VBoxFrameBuffer::COMGETTER(Address) (BYTE **aAddress)
{
    if (!aAddress)
        return E_POINTER;
    *aAddress = address();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(Width) (ULONG *aWidth)
{
    if (!aWidth)
        return E_POINTER;
    *aWidth = (ULONG) width();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(Height) (ULONG *aHeight)
{
    if (!aHeight)
        return E_POINTER;
    *aHeight = (ULONG) height();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(BitsPerPixel) (ULONG *aBitsPerPixel)
{
    if (!aBitsPerPixel)
        return E_POINTER;
    *aBitsPerPixel = bitsPerPixel();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(BytesPerLine) (ULONG *aBytesPerLine)
{
    if (!aBytesPerLine)
        return E_POINTER;
    *aBytesPerLine = bytesPerLine();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(PixelFormat) (
    ULONG *aPixelFormat)
{
    if (!aPixelFormat)
        return E_POINTER;
    *aPixelFormat = pixelFormat();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(UsesGuestVRAM) (
    BOOL *aUsesGuestVRAM)
{
    if (!aUsesGuestVRAM)
        return E_POINTER;
    *aUsesGuestVRAM = usesGuestVRAM();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(HeightReduction) (ULONG *aHeightReduction)
{
    if (!aHeightReduction)
        return E_POINTER;
    /* no reduction */
    *aHeightReduction = 0;
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
{
    if (!aOverlay)
        return E_POINTER;
    /* not yet implemented */
    *aOverlay = 0;
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::Lock()
{
    this->lock();
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::Unlock()
{
    this->unlock();
    return S_OK;
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxFrameBuffer::RequestResize (ULONG aScreenId, ULONG aPixelFormat,
                                             BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                                             ULONG aWidth, ULONG aHeight,
                                             BOOL *aFinished)
{
    NOREF(aScreenId);
    QApplication::postEvent (mView,
                             new VBoxResizeEvent (aPixelFormat, aVRAM, aBitsPerPixel,
                                                  aBytesPerLine, aWidth, aHeight));

#ifdef DEBUG_sunlover
    LogFlowFunc (("pixelFormat=%d, vram=%p, bpp=%d, bpl=%d, w=%d, h=%d\n",
          aPixelFormat, aVRAM, aBitsPerPixel, aBytesPerLine, aWidth, aHeight));
#endif /* DEBUG_sunlover */

    /*
     *  the resize has been postponed, return FALSE
     *  to cause the VM thread to sleep until IDisplay::ResizeFinished()
     *  is called from the VBoxResizeEvent event handler.
     */

    *aFinished = FALSE;
    return S_OK;
}

STDMETHODIMP
VBoxFrameBuffer::OperationSupported (FramebufferAccelerationOperation_T aOperation,
                                     BOOL *aSupported)
{
    NOREF(aOperation);
    if (!aSupported)
        return E_POINTER;
    *aSupported = FALSE;
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
 */
STDMETHODIMP VBoxFrameBuffer::VideoModeSupported (ULONG aWidth, ULONG aHeight,
                                                  ULONG aBPP, BOOL *aSupported)
{
    NOREF(aWidth);
    NOREF(aHeight);
    NOREF(aBPP);
    if (!aSupported)
        return E_POINTER;
    /* for now, we swallow everything */
    *aSupported = TRUE;
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::SolidFill (ULONG aX, ULONG aY,
                                         ULONG aWidth, ULONG aHeight,
                                         ULONG aColor, BOOL *aHandled)
{
    NOREF(aX);
    NOREF(aY);
    NOREF(aWidth);
    NOREF(aHeight);
    NOREF(aColor);
    if (!aHandled)
        return E_POINTER;
    *aHandled = FALSE;
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::CopyScreenBits (ULONG aXDst, ULONG aYDst,
                                              ULONG aXSrc, ULONG aYSrc,
                                              ULONG aWidth, ULONG aHeight,
                                              BOOL *aHandled)
{
    NOREF(aXDst);
    NOREF(aYDst);
    NOREF(aXSrc);
    NOREF(aYSrc);
    NOREF(aWidth);
    NOREF(aHeight);
    if (!aHandled)
        return E_POINTER;
    *aHandled = FALSE;
    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                               ULONG *aCountCopied)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo what and why?

    NOREF(aCount);
    NOREF(aCountCopied);

    return S_OK;
}

STDMETHODIMP VBoxFrameBuffer::SetVisibleRegion (BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    QRegion reg;
    for (ULONG ind = 0; ind < aCount; ++ ind)
    {
        QRect rect;
        rect.setLeft (rects->xLeft);
        rect.setTop (rects->yTop);
        /* QRect are inclusive */
        rect.setRight (rects->xRight - 1);
        rect.setBottom (rects->yBottom - 1);
        reg += rect;
        ++ rects;
    }
    QApplication::postEvent (mView, new VBoxSetRegionEvent (reg));

    return S_OK;
}

//
// VBoxQImageFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QIMAGE)

/** @class VBoxQImageFrameBuffer
 *
 *  The VBoxQImageFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */

VBoxQImageFrameBuffer::VBoxQImageFrameBuffer (VBoxConsoleView *aView) :
    VBoxFrameBuffer (aView)
{
    resizeEvent (new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
                                      NULL, 0, 0, 640, 480));
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxQImageFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                                  ULONG aW, ULONG aH,
                                                  BOOL *aFinished)
{
#if !defined (Q_WS_WIN) && !defined (Q_WS_PM)
    /* we're not on the GUI thread and update() isn't thread safe in Qt 3.3.x
       on the Mac (4.2.x is), on Linux (didn't check Qt 4.x there) and
       probably on other non-DOS platforms, so post the event instead. */
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));
#else
    /* we're not on the GUI thread, so update() instead of repaint()! */
    mView->viewport()->update (aX - mView->contentsX(),
                               aY - mView->contentsY(),
                               aW, aH);
#endif
    /* the update has been finished, return TRUE */
    *aFinished = TRUE;
    return S_OK;
}

void VBoxQImageFrameBuffer::paintEvent (QPaintEvent *pe)
{
    const QRect &r = pe->rect().intersect (mView->viewport()->rect());

    /*  some outdated rectangle during processing VBoxResizeEvent */
    if (r.isEmpty())
        return;

#if 0
    LogFlowFunc (("%dx%d-%dx%d (img=%dx%d)\n",
                  r.x(), r.y(), r.width(), r.height(),
                  img.width(), img.height()));
#endif

    FRAMEBUF_DEBUG_START (xxx);

    if (r.width() < mWdt * 2 / 3)
    {
        /* this method is faster for narrow updates */
        mPM.convertFromImage (mImg.copy (r.x() + mView->contentsX(),
                                         r.y() + mView->contentsY(),
                                         r.width(), r.height()));

#warning port me
//        ::bitBlt (mView->viewport(), r.x(), r.y(),
//                  &mPM, 0, 0,
//                  r.width(), r.height(),
//                  Qt::CopyROP, TRUE);
    }
    else
    {
        /* this method is faster for wide updates */
        mPM.convertFromImage (QImage (mImg.scanLine (r.y() + mView->contentsY()),
                                      mImg.width(), r.height(), mImg.depth(),
                                      0, 0, QImage::LittleEndian));

#warning port me
//        ::bitBlt (mView->viewport(), r.x(), r.y(),
//                  &mPM, r.x() + mView->contentsX(), 0,
//                  r.width(), r.height(),
//                  Qt::CopyROP, TRUE);
    }

    FRAMEBUF_DEBUG_STOP (xxx, r.width(), r.height());
}

void VBoxQImageFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
#if 0
    LogFlowFunc (("fmt=%d, vram=%p, bpp=%d, bpl=%d, width=%d, height=%d\n",
                  re->pixelFormat(), re->VRAM(),
                  re->bitsPerPixel(), re->bytesPerLine(),
                  re->width(), re->height()));
#endif

    mWdt = re->width();
    mHgt = re->height();

    bool remind = false;
    bool fallback = false;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (re->bitsPerPixel())
        {
            /* 32-, 8- and 1-bpp are the only depths suported by QImage */
            case 32:
                break;
            case 8:
            case 1:
                remind = true;
                break;
            default:
                remind = true;
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* QImage only supports 32-bit aligned scan lines */
            fallback = re->bytesPerLine() !=
                ((mWdt * re->bitsPerPixel() + 31) / 32) * 4;
            Assert (!fallback);
            if (!fallback)
            {
                mImg = QImage ((uchar *) re->VRAM(), mWdt, mHgt,
                               re->bitsPerPixel(), NULL, 0, QImage::LittleEndian);
                mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
                mUsesGuestVRAM = true;
            }
        }
    }
    else
    {
        fallback = true;
    }

    if (fallback)
    {
        /* we don't support either the pixel format or the color depth,
         * fallback to a self-provided 32bpp RGB buffer */
        mImg = QImage (mWdt, mHgt, 32, 0, QImage::LittleEndian);
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        mUsesGuestVRAM = false;
    }

    if (remind)
    {
        class RemindEvent : public VBoxAsyncEvent
        {
            ulong mRealBPP;
        public:
            RemindEvent (ulong aRealBPP)
                : mRealBPP (aRealBPP) {}
            void handle()
            {
                vboxProblem().remindAboutWrongColorDepth (mRealBPP, 32);
            }
        };
        (new RemindEvent (re->bitsPerPixel()))->post();
    }
}

#endif

//
// VBoxSDLFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_SDL)

/** @class VBoxSDLFrameBuffer
 *
 *  The VBoxSDLFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses SDL to store and render VM display data.
 */

VBoxSDLFrameBuffer::VBoxSDLFrameBuffer (VBoxConsoleView *aView) :
    VBoxFrameBuffer (aView)
{
    mScreen = NULL;
    mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    mSurfVRAM = NULL;

    resizeEvent (new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
                                      NULL, 0, 0, 640, 480));
}

VBoxSDLFrameBuffer::~VBoxSDLFrameBuffer()
{
    if (mSurfVRAM)
    {
        SDL_FreeSurface (mSurfVRAM);
        mSurfVRAM = NULL;
    }
    SDL_QuitSubSystem (SDL_INIT_VIDEO);
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxSDLFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                               ULONG aW, ULONG aH,
                                               BOOL *aFinished)
{
#if !defined (Q_WS_WIN) && !defined (Q_WS_PM)
    /* we're not on the GUI thread and update() isn't thread safe in Qt 3.3.x
       on the Mac (4.2.x is), on Linux (didn't check Qt 4.x there) and
       probably on other non-DOS platforms, so post the event instead. */
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));
#else
    /* we're not on the GUI thread, so update() instead of repaint()! */
    mView->viewport()->update (aX - mView->contentsX(),
                               aY - mView->contentsY(),
                               aW, aH);
#endif
    /* the update has been finished, return TRUE */
    *aFinished = TRUE;
    return S_OK;
}

void VBoxSDLFrameBuffer::paintEvent (QPaintEvent *pe)
{
    if (mScreen)
    {
#ifdef Q_WS_X11
        /* make sure we don't conflict with Qt's drawing */
        //XSync(QPaintDevice::x11Display(), FALSE);
#endif
        if (mScreen->pixels)
        {
            QRect r = pe->rect();

            if (mSurfVRAM)
            {
                SDL_Rect rect = { (Sint16) r.x(), (Sint16) r.y(),
                                  (Sint16) r.width(), (Sint16) r.height() };
                SDL_BlitSurface (mSurfVRAM, &rect, mScreen, &rect);
                /** @todo may be: if ((mScreen->flags & SDL_HWSURFACE) == 0) */
                SDL_UpdateRect (mScreen, r.x(), r.y(), r.width(), r.height());
            }
            else
            {
                SDL_UpdateRect (mScreen, r.x(), r.y(), r.width(), r.height());
            }
        }
    }
}

void VBoxSDLFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
    mWdt = re->width();
    mHgt = re->height();

    /* close SDL so we can init it again */
    if (mSurfVRAM)
    {
        SDL_FreeSurface(mSurfVRAM);
        mSurfVRAM = NULL;
    }
    if (mScreen)
    {
        SDL_QuitSubSystem (SDL_INIT_VIDEO);
        mScreen = NULL;
    }

    /*
     *  initialize the SDL library, use its super hack to integrate it with our
     *  client window
     */
    static char sdlHack[64];
    LogFlowFunc (("Using client window 0x%08lX to initialize SDL\n",
                  mView->viewport()->winId()));
    /* Note: SDL_WINDOWID must be decimal (not hex) to work on Win32 */
    sprintf (sdlHack, "SDL_WINDOWID=%lu", mView->viewport()->winId());
    putenv (sdlHack);
    int rc = SDL_InitSubSystem (SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    AssertMsg (rc == 0, ("SDL initialization failed: %s\n", SDL_GetError()));
    NOREF(rc);

#ifdef Q_WS_X11
    /* undo signal redirections from SDL, it'd steal keyboard events from us! */
    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);
#endif

    LogFlowFunc (("Setting SDL video mode to %d x %d\n", mWdt, mHgt));

    bool fallback = false;

    Uint32 Rmask = 0;
    Uint32 Gmask = 0;
    Uint32 Bmask = 0;

    if (re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (re->bitsPerPixel())
        {
            case 32:
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
                break;
            case 24:
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
                break;
            case 16:
                Rmask = 0xF800;
                Gmask = 0x07E0;
                Bmask = 0x001F;
            default:
                /* Unsupported format leads to the indirect buffer */
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* Create a source surface from guest VRAM. */
            mSurfVRAM = SDL_CreateRGBSurfaceFrom(re->VRAM(), mWdt, mHgt,
                                                 re->bitsPerPixel(),
                                                 re->bytesPerLine(),
                                                 Rmask, Gmask, Bmask, 0);
            LogFlowFunc (("Created VRAM surface %p\n", mSurfVRAM));
            if (mSurfVRAM == NULL)
                fallback = true;
        }
    }
    else
    {
        /* Unsupported format leads to the indirect buffer */
        fallback = true;
    }

    /* Pixel format is RGB in any case */
    mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;

    mScreen = SDL_SetVideoMode (mWdt, mHgt, 0,
                                SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL);
    AssertMsg (mScreen, ("SDL video mode could not be set!\n"));
}

#endif

//
// VBoxDDRAWFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_DDRAW)

/* The class is defined in VBoxFBDDRAW.cpp */

#endif

//
// VBoxQuartz2DFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QUARTZ2D)

/* The class is defined in VBoxQuartz2D.cpp */

#endif
