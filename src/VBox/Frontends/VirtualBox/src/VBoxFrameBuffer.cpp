/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer class and subclasses implementation
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

#include "VBoxFrameBuffer.h"

#include "VBoxConsoleView.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QPainter>
#if defined (VBOX_GUI_USE_QGL)
#include <QGLWidget>

#include <iprt/asm.h>
#endif

//
// VBoxFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

/** @class VBoxFrameBuffer
 *
 *  Base class for all frame buffer implementations.
 */

#if !defined (Q_OS_WIN32)
NS_DECL_CLASSINFO (VBoxFrameBuffer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (VBoxFrameBuffer, IFramebuffer)
#endif

VBoxFrameBuffer::VBoxFrameBuffer (VBoxConsoleView *aView)
    : mView (aView)
    , mWdt (0), mHgt (0)
#if defined (Q_OS_WIN32)
    , refcnt (0)
#endif
{
    AssertMsg (mView, ("VBoxConsoleView must not be null\n"));
    mWinId = (mView && mView->viewport()) ? (ULONG64) mView->viewport()->winId() : 0;
    int rc = RTCritSectInit(&mCritSect);
    AssertRC(rc);
}

VBoxFrameBuffer::~VBoxFrameBuffer()
{
    RTCritSectDelete(&mCritSect);
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

STDMETHODIMP VBoxFrameBuffer::COMGETTER(WinId) (ULONG64 *winId)
{
    if (!winId)
        return E_POINTER;
    *winId = mWinId;
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
    NOREF(aBPP);
    LogFlowThisFunc(("aWidth=%lu, aHeight=%lu, aBPP=%lu\n",
                     (unsigned long) aWidth,  (unsigned long) aHeight,
                      (unsigned long) aBPP));
    if (!aSupported)
        return E_POINTER;
    *aSupported = TRUE;
    QRect screen = mView->desktopGeometry();
    if (   (screen.width() != 0)
        && (aWidth > (ULONG) screen.width())
       )
        *aSupported = FALSE;
    if (   (screen.height() != 0)
        && (aHeight > (ULONG) screen.height())
       )
        *aSupported = FALSE;
    LogFlowThisFunc(("screenW=%lu, screenH=%lu -> aSupported=%s\n",
                    screen.width(), screen.height(), *aSupported ? "TRUE" : "FALSE"));
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

STDMETHODIMP VBoxFrameBuffer::ProcessVHWACommand(BYTE *pCommand)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    QApplication::postEvent (mView,
                             new VBoxVHWACommandProcessEvent ((struct _VBOXVHWACMD*)pCommand));
    return S_OK;
#else
    Q_UNUSED (pCommand);
    return E_NOTIMPL;
#endif
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
                                                  ULONG aW, ULONG aH)
{
    /* We're not on the GUI thread and update() isn't thread safe in
     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
     * on Linux (didn't check Qt 4.x there) and probably on other
     * non-DOS platforms, so post the event instead. */
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));

    return S_OK;
}

void VBoxQImageFrameBuffer::paintEvent (QPaintEvent *pe)
{
    const QRect &r = pe->rect().intersected (mView->viewport()->rect());

    /* Some outdated rectangle during processing VBoxResizeEvent */
    if (r.isEmpty())
        return;

#if 0
    LogFlowFunc (("%dx%d-%dx%d (img=%dx%d)\n",
                  r.x(), r.y(), r.width(), r.height(),
                  img.width(), img.height()));
#endif

    QPainter painter (mView->viewport());

    if (r.width() < mWdt * 2 / 3)
    {
        /* This method is faster for narrow updates */
        mPM = QPixmap::fromImage (mImg.copy (r.x() + mView->contentsX(),
                                             r.y() + mView->contentsY(),
                                             r.width(), r.height()));
        painter.drawPixmap (r.x(), r.y(), mPM);
    }
    else
    {
        /* This method is faster for wide updates */
        mPM = QPixmap::fromImage (QImage (mImg.scanLine (r.y() + mView->contentsY()),
                                  mImg.width(), r.height(), mImg.bytesPerLine(),
                                  QImage::Format_RGB32));
        painter.drawPixmap (r.x(), r.y(), mPM,
                            r.x() + mView->contentsX(), 0, 0, 0);
    }
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
    ulong bitsPerLine = re->bytesPerLine() * 8;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        QImage::Format format;
        switch (re->bitsPerPixel())
        {
            /* 32-, 8- and 1-bpp are the only depths suported by QImage */
            case 32:
                format = QImage::Format_RGB32;
                break;
            case 8:
                format = QImage::Format_Indexed8;
                remind = true;
                break;
            case 1:
                format = QImage::Format_Mono;
                remind = true;
                break;
            default:
                remind = true;
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((re->bytesPerLine() & 3) == 0);
            fallback = ((re->bytesPerLine() & 3) != 0);
        }
        if (!fallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (re->bitsPerPixel() - 1)) == 0);
            fallback = ((bitsPerLine & (re->bitsPerPixel() - 1)) != 0);
        }
        if (!fallback)
        {
            ulong virtWdt = bitsPerLine / re->bitsPerPixel();
            mImg = QImage ((uchar *) re->VRAM(), virtWdt, mHgt, format);
            mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            mUsesGuestVRAM = true;
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
        mImg = QImage (mWdt, mHgt, QImage::Format_RGB32);
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
// VBoxQGLFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QGL)

#define VBOXQGLLOG(_m)

/** @class VBoxQGLFrameBuffer
 *
 *  The VBoxQImageFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */

VBoxQGLFrameBuffer::VBoxQGLFrameBuffer (VBoxConsoleView *aView) :
    VBoxFrameBuffer (aView)
{
//    mWidget = new GLWidget(aView->viewport());
    resizeEvent (new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
                                      NULL, 0, 0, 640, 480));
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxQGLFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                                  ULONG aW, ULONG aH)
{
    /* We're not on the GUI thread and update() isn't thread safe in
     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
     * on Linux (didn't check Qt 4.x there) and probably on other
     * non-DOS platforms, so post the event instead. */
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));

    return S_OK;
}

void VBoxQGLFrameBuffer::vboxMakeCurrent()
{
    ((QGLWidget*)mView->viewport())->makeCurrent();
}

VBoxGLWidget* VBoxQGLFrameBuffer::vboxWidget()
{
    return (VBoxGLWidget*)mView->viewport();
}

void VBoxQGLFrameBuffer::paintEvent (QPaintEvent *pe)
{
    vboxWidget()->vboxPaintEvent(pe);
}

static GLsizei  makePowerOf2(GLsizei val)
{
    int last = ASMBitLastSetS32(val);
    if(last>1)
    {
        last--;
        if((1 << last) != val)
        {
            Assert((1 << last) < val);
            val = (1 << (last+1));
        }
    }
    return val;
}

void VBoxGLWidget::vboxResizeEvent (VBoxResizeEvent *re)
{
    VBOXQGLLOG(("-->resizing:  pixelFormat(%d), VRAM(0x%x), bitsPerPixel(%d), bytesPerLine(%d), width(%d),  height(%d)\n",
            re->pixelFormat(),
            re->VRAM(),
            re->bitsPerPixel(),
            re->bytesPerLine(),
            re->width(),
            re->height()));
    mRe = re;
    updateGL();
    mRe = NULL;
}

void VBoxGLWidget::vboxPaintEvent (QPaintEvent *pe)
{
    mpIntersectionRect = &pe->rect();
    updateGL();
    mpIntersectionRect = NULL;
}

void VBoxGLWidget::paintGL()
{
    if(mRe)
    {
        vboxDoResize(mRe);
    }
    else
    {
        vboxDoPaint(mpIntersectionRect);
    }
}

void VBoxGLWidget::initializeGL()
{
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &mTexture);
    VBOXQGL_ASSERTNOERR();

    glBindTexture(GL_TEXTURE_2D, mTexture);
    VBOXQGL_ASSERTNOERR();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    VBOXQGL_ASSERTNOERR();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    VBOXQGL_ASSERTNOERR();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    VBOXQGL_ASSERTNOERR();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    VBOXQGL_ASSERTNOERR();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    VBOXQGL_ASSERTNOERR();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    VBOXQGL_ASSERTNOERR();

    vboxDoInitDisplay();
}

void VBoxGLWidget::vboxDoPaint(const QRect *rec)
{
    Assert(glIsTexture(mTexture));
    glBindTexture(GL_TEXTURE_2D, mTexture);

    int x = rec->x();
    int y = rec->y();
    int width = rec->width();
    int height = rec->height();

    VBOXQGLLOG(("x(%d), y(%d), width(%d), height(%d)\n", x, y, width, height));
    uchar * address = mAddress + y*mBytesPerLine + x*mBytesPerPixel;

    VBOXQGL_CHECKERR(
            glPixelStorei(GL_PACK_ROW_LENGTH, mDisplayWidth);
            );
    VBOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mDisplayWidth);
            );
    VBOXQGL_CHECKERR(
            glTexSubImage2D(GL_TEXTURE_2D,
                0,
                x, y, width, height,
                mFormat,
                mType,
                address);
            );

    vboxDoPerformDisplay();
}

void VBoxGLWidget::vboxDoResize(VBoxResizeEvent *re)
{
    bool remind = false;
    bool fallback = false;

    /* clean the old values first */
    if(mAddress)
    {
        if(!mUsesGuestVRAM)
        {
            if(mAddress)
            {
                free(mAddress);
            }
        }
    }

    GLenum type;
    GLint internalformat;
    GLenum format;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        mBitsPerPixel = re->bitsPerPixel();
        mBytesPerLine = re->bytesPerLine();
        ulong bitsPerLine = mBytesPerLine * 8;

        switch (mBitsPerPixel)
        {
            case 32:
                internalformat = 3;//GL_RGB;
                format = GL_BGRA_EXT;//GL_RGBA;
                type = GL_UNSIGNED_BYTE;
                break;
            case 24:
                Assert(0);
                internalformat = 3;//GL_RGB;
                format = GL_BGR_EXT;
                type = GL_UNSIGNED_BYTE;
                remind = true;
                break;
            case 8:
                Assert(0);
                internalformat = 1;//GL_RGB;
                format = GL_BLUE;//GL_RGB;
                type = GL_UNSIGNED_BYTE;
                remind = true;
                break;
            case 1:
                Assert(0);
                internalformat = 1;
                format = GL_COLOR_INDEX;
                type = GL_BITMAP;
                remind = true;
                break;
            default:
                Assert(0);
                remind = true;
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((re->bytesPerLine() & 3) == 0);
            fallback = ((re->bytesPerLine() & 3) != 0);
        }
        if (!fallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (re->bitsPerPixel() - 1)) == 0);
            fallback = ((bitsPerLine & (re->bitsPerPixel() - 1)) != 0);
        }
        if (!fallback)
        {
            ulong virtWdt = bitsPerLine / re->bitsPerPixel();
            mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            mUsesGuestVRAM = true;
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
        mBitsPerPixel = 32;
        mBytesPerLine = re->width()*mBitsPerPixel/8;
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        internalformat = 3;//GL_RGB;
        format = GL_BGRA_EXT;//GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        mUsesGuestVRAM = false;
    }

    mFormat = format;
    mType = type;
    mInternalFormat = internalformat;
    mBytesPerPixel = mBitsPerPixel/8;
    mDisplayWidth = mBytesPerLine/mBytesPerPixel;
    mDisplayHeight = re->height();

    GLsizei wdt = makePowerOf2(mDisplayWidth);
    GLsizei hgt = makePowerOf2(mDisplayHeight);
    Assert(wdt >= (GLsizei)mDisplayWidth);
    Assert(hgt >= (GLsizei)mDisplayHeight);

    int size = wdt * hgt * (mBitsPerPixel/8);
    uchar * address = (uchar *)malloc(size);
    memset(address, 0, size);

    glBindTexture(GL_TEXTURE_2D, mTexture);
    VBOXQGL_CHECKERR(
        glTexImage2D(GL_TEXTURE_2D,
                0,
                  internalformat,
                  wdt,
                  hgt,
                  0,
                  format,
                  type,
                  (GLvoid *)address);
        );
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(((double)mDisplayWidth)/wdt, ((double)mDisplayHeight)/hgt, 1.0f);
    glMatrixMode(GL_MODELVIEW);

    if(mUsesGuestVRAM)
    {
        mAddress = re->VRAM();
        free(address);
    }
    else
    {
        mAddress = address;
    }

    glViewport(0, 0, mDisplayWidth, mDisplayHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(0., (GLdouble)mDisplayWidth, 0., (GLdouble)mDisplayHeight, 0., 0.);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

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

void VBoxGLWidget::vboxDoInitDisplay()
{
    vboxDoDeleteDisplay();
    VBOXQGL_ASSERTNOERR();

    mDisplay = glGenLists(1);
    VBOXQGL_ASSERTNOERR();
    glNewList(mDisplay, GL_COMPILE);

    glBindTexture(GL_TEXTURE_2D, mTexture);

    glBegin(GL_QUADS);
    glTexCoord2d(0, 0);
    glVertex2d(-1.0, 1.0);
    glTexCoord2d(0, 1);
    glVertex2d(-1.0, -1.0);
    glTexCoord2d(1, 1);
    glVertex2d(1.0, -1.0);
    glTexCoord2d(1, 0);
    glVertex2d(1.0, 1.0);
    glEnd();

    glEndList();
    VBOXQGL_ASSERTNOERR();
    mDisplayInitialized = true;
}

void VBoxGLWidget::vboxDoDeleteDisplay()
{
    if(mDisplayInitialized)
    {
        glDeleteLists(mDisplay, 1);
        mDisplayInitialized = false;
    }
}

void VBoxQGLFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
    mWdt = re->width();
    mHgt = re->height();

    vboxWidget()->vboxResizeEvent(re);
}

#endif

//
// VBoxSDLFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_SDL)

#include "VBoxX11Helper.h"

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

    X11ScreenSaverSettingsInit();
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
    X11ScreenSaverSettingsSave();
    SDL_QuitSubSystem (SDL_INIT_VIDEO);
    X11ScreenSaverSettingsRestore();
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxSDLFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                               ULONG aW, ULONG aH)
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
    /* Check whether the guest resolution has not been changed. */
    bool fSameResolutionRequested = (   width()  == re->width()
                                     && height() == re->height()
                                    );

    /* Check if the guest VRAM can be used as the source bitmap. */
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
                break;
            default:
                /* Unsupported format leads to the indirect buffer */
                fallback = true;
                break;
        }
    }
    else
    {
        /* Unsupported format leads to the indirect buffer */
        fallback = true;
    }

    mWdt = re->width();
    mHgt = re->height();

    /* Recreate the source surface. */
    if (mSurfVRAM)
    {
        SDL_FreeSurface(mSurfVRAM);
        mSurfVRAM = NULL;
    }

    if (!fallback)
    {
        /* It is OK to create the source surface from the guest VRAM. */
        mSurfVRAM = SDL_CreateRGBSurfaceFrom(re->VRAM(), re->width(), re->height(),
                                             re->bitsPerPixel(),
                                             re->bytesPerLine(),
                                             Rmask, Gmask, Bmask, 0);
        LogFlowFunc (("Created VRAM surface %p\n", mSurfVRAM));
        if (mSurfVRAM == NULL)
        {
            fallback = true;
        }
    }

    if (fSameResolutionRequested)
    {
        LogFlowFunc(("the same resolution requested, skipping the resize.\n"));
        return;
    }

    /* close SDL so we can init it again */
    if (mScreen)
    {
        X11ScreenSaverSettingsSave();
        SDL_QuitSubSystem (SDL_INIT_VIDEO);
        X11ScreenSaverSettingsRestore();
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
    X11ScreenSaverSettingsSave();
    int rc = SDL_InitSubSystem (SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    X11ScreenSaverSettingsRestore();
    AssertMsg (rc == 0, ("SDL initialization failed: %s\n", SDL_GetError()));
    NOREF(rc);

#ifdef Q_WS_X11
    /* undo signal redirections from SDL, it'd steal keyboard events from us! */
    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);
#endif

    LogFlowFunc (("Setting SDL video mode to %d x %d\n", mWdt, mHgt));

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
