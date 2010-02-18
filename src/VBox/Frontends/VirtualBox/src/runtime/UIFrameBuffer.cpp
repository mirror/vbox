/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBuffer class and subclasses implementation
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
#include <QPainter>
/* Local includes */
#include "UIMachineView.h"
#include "UIFrameBuffer.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#if defined (Q_OS_WIN32)
static CComModule _Module;
#else
NS_DECL_CLASSINFO (UIFrameBuffer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (UIFrameBuffer, IFramebuffer)
#endif

UIFrameBuffer::UIFrameBuffer(UIMachineView *pMachineView)
    : m_pMachineView(pMachineView)
    , m_width(0), m_height(0)
#if defined (Q_OS_WIN32)
    , m_iRefCnt(0)
#endif
{
    AssertMsg(m_pMachineView, ("UIMachineView must not be null\n"));
    m_uWinId = (m_pMachineView && m_pMachineView->viewport()) ? (ULONG64)m_pMachineView->viewport()->winId() : 0;
    int rc = RTCritSectInit(&m_critSect);
    AssertRC(rc);
}

UIFrameBuffer::~UIFrameBuffer()
{
    RTCritSectDelete(&m_critSect);
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Address) (BYTE **ppAddress)
{
    if (!ppAddress)
        return E_POINTER;
    *ppAddress = address();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Width) (ULONG *puWidth)
{
    if (!puWidth)
        return E_POINTER;
    *puWidth = (ULONG)width();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Height) (ULONG *puHeight)
{
    if (!puHeight)
        return E_POINTER;
    *puHeight = (ULONG)height();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BitsPerPixel) (ULONG *puBitsPerPixel)
{
    if (!puBitsPerPixel)
        return E_POINTER;
    *puBitsPerPixel = bitsPerPixel();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BytesPerLine) (ULONG *puBytesPerLine)
{
    if (!puBytesPerLine)
        return E_POINTER;
    *puBytesPerLine = bytesPerLine();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(PixelFormat) (ULONG *puPixelFormat)
{
    if (!puPixelFormat)
        return E_POINTER;
    *puPixelFormat = pixelFormat();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(UsesGuestVRAM) (BOOL *pbUsesGuestVRAM)
{
    if (!pbUsesGuestVRAM)
        return E_POINTER;
    *pbUsesGuestVRAM = usesGuestVRAM();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(HeightReduction) (ULONG *puHeightReduction)
{
    if (!puHeightReduction)
        return E_POINTER;
    *puHeightReduction = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Overlay) (IFramebufferOverlay **ppOverlay)
{
    if (!ppOverlay)
        return E_POINTER;
    /* not yet implemented */
    *ppOverlay = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(WinId) (ULONG64 *puWinId)
{
    if (!puWinId)
        return E_POINTER;
    *puWinId = m_uWinId;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::Lock()
{
    this->lock();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::Unlock()
{
    this->unlock();
    return S_OK;
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBuffer::RequestResize(ULONG uScreenId, ULONG uPixelFormat,
                                          BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                                          ULONG uWidth, ULONG uHeight,
                                          BOOL *pbFinished)
{
    NOREF(uScreenId);
    QApplication::postEvent (m_pMachineView,
                             new UIResizeEvent(uPixelFormat, pVRAM, uBitsPerPixel,
                                               uBytesPerLine, uWidth, uHeight));

    *pbFinished = FALSE;
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
STDMETHODIMP UIFrameBuffer::VideoModeSupported(ULONG uWidth, ULONG uHeight, ULONG uBPP, BOOL *pbSupported)
{
    NOREF(uBPP);
    LogFlowThisFunc(("width=%lu, height=%lu, BPP=%lu\n",
                    (unsigned long)uWidth, (unsigned long)uHeight, (unsigned long)uBPP));
    if (!pbSupported)
        return E_POINTER;
    *pbSupported = TRUE;
    QRect screen = m_pMachineView->desktopGeometry();
    if ((screen.width() != 0) && (uWidth > (ULONG)screen.width()))
        *pbSupported = FALSE;
    if ((screen.height() != 0) && (uHeight > (ULONG)screen.height()))
        *pbSupported = FALSE;
    LogFlowThisFunc(("screenW=%lu, screenH=%lu -> aSupported=%s\n",
                    screen.width(), screen.height(), *pbSupported ? "TRUE" : "FALSE"));
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::GetVisibleRegion(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied)
{
    PRTRECT rects = (PRTRECT)pRectangles;

    if (!rects)
        return E_POINTER;

    NOREF(uCount);
    NOREF(puCountCopied);

    return S_OK;
}

STDMETHODIMP UIFrameBuffer::SetVisibleRegion(BYTE *pRectangles, ULONG uCount)
{
    PRTRECT rects = (PRTRECT)pRectangles;

    if (!rects)
        return E_POINTER;

    QRegion reg;
    for (ULONG ind = 0; ind < uCount; ++ ind)
    {
        QRect rect;
        rect.setLeft(rects->xLeft);
        rect.setTop(rects->yTop);
        /* QRect are inclusive */
        rect.setRight(rects->xRight - 1);
        rect.setBottom(rects->yBottom - 1);
        reg += rect;
        ++ rects;
    }
    QApplication::postEvent(m_pMachineView, new UISetRegionEvent(reg));

    return S_OK;
}

STDMETHODIMP UIFrameBuffer::ProcessVHWACommand(BYTE *pCommand)
{
    Q_UNUSED(pCommand);
    return E_NOTIMPL;
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void UIFrameBuffer::doProcessVHWACommand(QEvent *pEvent)
{
    Q_UNUSED(pEvent);
    /* should never be here */
    AssertBreakpoint();
}
#endif

#if defined (VBOX_GUI_USE_QIMAGE)
/** @class UIFrameBufferQImage
 *
 *  The UIFrameBufferQImage class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */
UIFrameBufferQImage::UIFrameBufferQImage(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
{
    /* Initialize the framebuffer the first time */
    resizeEvent(new UIResizeEvent(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480));
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferQImage::NotifyUpdate(ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
    /* We're not on the GUI thread and update() isn't thread safe in
     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
     * on Linux (didn't check Qt 4.x there) and probably on other
     * non-DOS platforms, so post the event instead. */
    QApplication::postEvent(m_pMachineView, new UIRepaintEvent(uX, uY, uW, uH));

    return S_OK;
}

void UIFrameBufferQImage::paintEvent(QPaintEvent *pEvent)
{
    const QRect &r = pEvent->rect().intersected(m_pMachineView->viewport()->rect());

    /* Some outdated rectangle during processing UIResizeEvent */
    if (r.isEmpty())
        return;

#if 0
    LogFlowFunc (("%dx%d-%dx%d (img=%dx%d)\n", r.x(), r.y(), r.width(), r.height(), img.width(), img.height()));
#endif

    QPainter painter(m_pMachineView->viewport());

    if (r.width() < m_width * 2 / 3)
    {
        /* This method is faster for narrow updates */
        m_PM = QPixmap::fromImage(m_img.copy(r.x() + m_pMachineView->contentsX(),
                                             r.y() + m_pMachineView->contentsY(),
                                             r.width(), r.height()));
        painter.drawPixmap(r.x(), r.y(), m_PM);
    }
    else
    {
        /* This method is faster for wide updates */
        m_PM = QPixmap::fromImage(QImage(m_img.scanLine (r.y() + m_pMachineView->contentsY()),
                                  m_img.width(), r.height(), m_img.bytesPerLine(),
                                  QImage::Format_RGB32));
        painter.drawPixmap(r.x(), r.y(), m_PM, r.x() + m_pMachineView->contentsX(), 0, 0, 0);
    }
}

void UIFrameBufferQImage::resizeEvent(UIResizeEvent *pEvent)
{
#if 0
    LogFlowFunc (("fmt=%d, vram=%p, bpp=%d, bpl=%d, width=%d, height=%d\n",
                  pEvent->pixelFormat(), pEvent->VRAM(),
                  pEvent->bitsPerPixel(), pEvent->bytesPerLine(),
                  pEvent->width(), pEvent->height()));
#endif

    m_width = pEvent->width();
    m_height = pEvent->height();

    bool bRemind = false;
    bool bFallback = false;
    ulong bitsPerLine = pEvent->bytesPerLine() * 8;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (pEvent->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        QImage::Format format;
        switch (pEvent->bitsPerPixel())
        {
            /* 32-, 8- and 1-bpp are the only depths supported by QImage */
            case 32:
                format = QImage::Format_RGB32;
                break;
            case 8:
                format = QImage::Format_Indexed8;
                bRemind = true;
                break;
            case 1:
                format = QImage::Format_Mono;
                bRemind = true;
                break;
            default:
                format = QImage::Format_Invalid; /* set it to something so gcc keeps quiet. */
                bRemind = true;
                bFallback = true;
                break;
        }

        if (!bFallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((pEvent->bytesPerLine() & 3) == 0);
            bFallback = ((pEvent->bytesPerLine() & 3) != 0);
        }
        if (!bFallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (pEvent->bitsPerPixel() - 1)) == 0);
            bFallback = ((bitsPerLine & (pEvent->bitsPerPixel() - 1)) != 0);
        }
        if (!bFallback)
        {
            ulong virtWdt = bitsPerLine / pEvent->bitsPerPixel();
            m_img = QImage ((uchar *) pEvent->VRAM(), virtWdt, m_height, format);
            m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            m_bUsesGuestVRAM = true;
        }
    }
    else
    {
        bFallback = true;
    }

    if (bFallback)
    {
        /* we don't support either the pixel format or the color depth,
         * bFallback to a self-provided 32bpp RGB buffer */
        m_img = QImage (m_width, m_height, QImage::Format_RGB32);
        m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        m_bUsesGuestVRAM = false;
    }

    if (bRemind)
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
        (new RemindEvent (pEvent->bitsPerPixel()))->post();
    }
}
#endif

#if 0
#if defined (VBOX_GUI_USE_QGL)
UIFrameBufferQGL::UIFrameBufferQGL(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
    , m_cmdPipe(pMachineView)
{
#ifndef VBOXQGL_PROF_BASE
    resizeEvent(new UIResizeEvent(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480));
#else
    resizeEvent(new UIResizeEvent(FramebufferPixelFormat_Opaque, NULL, 0, 0, VBOXQGL_PROF_WIDTH, VBOXQGL_PROF_HEIGHT));
#endif
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferQGL::NotifyUpdate (ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
#ifdef VBOXQGL_PROF_BASE
    QApplication::postEvent(m_pMachineView, new UIRepaintEvent(uX, uY, uW, uH));
#else
    QRect r(uX, uY, uW, uH);
    m_cmdPipe.postCmd(VBOXVHWA_PIPECMD_PAINT, &r, 0);
#endif
    return S_OK;
}

#ifdef VBOXQGL_PROF_BASE
STDMETHODIMP UIFrameBufferQGL::RequestResize(ULONG uScreenId, ULONG uPixelFormat,
                                             BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                                             ULONG uWidth, ULONG uHeight,
                                             BOOL *pbFinished)
{
    uWidth = VBOXQGL_PROF_WIDTH;
    uHeight = VBOXQGL_PROF_HEIGHT;
    UIFrameBuffer::RequestResize(uScreenId, uPixelFormat, aVRAM, uBitsPerPixel, uBytesPerLine, uWidth, uHeight, pbFinished);

    for(;;)
    {
        ULONG uX = 0;
        ULONG uY = 0;
        ULONG uW = aWidth;
        ULONG uH = aHeight;
        NotifyUpdate(uX, uY, uW, uH);
        RTThreadSleep(40);
    }
    return S_OK;
}
#endif

VBoxGLWidget* UIFrameBufferQGL::vboxWidget()
{
    return (VBoxGLWidget*)m_pMachineView->viewport();
}

void UIFrameBufferQGL::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    VBoxGLWidget *pWidget = vboxWidget();
    pWidget->makeCurrent();

    QRect vp(m_pMachineView->contentsX(), m_pMachineView->contentsY(), pWidget->width(), pWidget->height());
    if (vp != pWidget->vboxViewport())
    {
        pWidget->vboxDoUpdateViewport(vp);
    }

    pWidget->performDisplayAndSwap(true);
}

void UIFrameBufferQGL::resizeEvent(UIResizeEvent *pEvent)
{
    m_width = pEvent->width();
    m_height = pEvent->height();

    vboxWidget()->vboxResizeEvent(pEvent);
}

/* processing the VHWA command, called from the GUI thread */
void UIFrameBufferQGL::doProcessVHWACommand(QEvent *pEvent)
{
    Q_UNUSED(pEvent);
    vboxWidget()->vboxProcessVHWACommands(&m_cmdPipe);
}

#ifdef VBOX_WITH_VIDEOHWACCEL
STDMETHODIMP UIFrameBufferQGL::ProcessVHWACommand(BYTE *pCommand)
{
    VBOXVHWACMD *pCmd = (VBOXVHWACMD*)pCommand;
    /* indicate that we process and complete the command asynchronously */
    pCmd->Flags |= VBOXVHWACMD_FLAG_HG_ASYNCH;
    /* post the command to the GUI thread for processing */
    m_cmdPipe.postCmd(VBOXVHWA_PIPECMD_VHWA, pCmd, 0);
    return S_OK;
}
#endif
#endif

#if defined (VBOX_GUI_USE_SDL)
#include "VBoxX11Helper.h"

/** @class UIFrameBufferSDL
 *
 *  The UIFrameBufferSDL class is a class that implements the IFrameBuffer
 *  interface and uses SDL to store and render VM display data.
 */
UIFrameBufferSDL::UIFrameBufferSDL(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
{
    m_pScreen = NULL;
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    m_pSurfVRAM = NULL;

    X11ScreenSaverSettingsInit();
    resizeEvent(new UIResizeEvent(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480));
}

UIFrameBufferSDL::~UIFrameBufferSDL()
{
    if (m_pSurfVRAM)
    {
        SDL_FreeSurface(m_pSurfVRAM);
        m_pSurfVRAM = NULL;
    }
    X11ScreenSaverSettingsSave();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    X11ScreenSaverSettingsRestore();
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferSDL::NotifyUpdate(ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
#if !defined (Q_WS_WIN) && !defined (Q_WS_PM)
    /* we're not on the GUI thread and update() isn't thread safe in Qt 3.3.x
       on the Mac (4.2.x is), on Linux (didn't check Qt 4.x there) and
       probably on other non-DOS platforms, so post the event instead. */
    QApplication::postEvent (m_pMachineView, new UIRepaintEvent(uX, uY, uW, UH));
#else
    /* we're not on the GUI thread, so update() instead of repaint()! */
    m_pMachineView->viewport()->update(uX - m_pMachineView->contentsX(), uY - m_pMachineView->contentsY(), uW, uH);
#endif
    return S_OK;
}

void UIFrameBufferSDL::paintEvent(QPaintEvent *pEvent)
{
    if (m_pScreen)
    {
        if (m_pScreen->pixels)
        {
            QRect r = pEvent->rect();

            if (m_pSurfVRAM)
            {
                SDL_Rect rect = { (Sint16)r.x(), (Sint16)r.y(), (Sint16)r.width(), (Sint16)r.height() };
                SDL_BlitSurface(m_pSurfVRAM, &rect, m_pScreen, &rect);
                /** @todo may be: if ((m_pScreen->flags & SDL_HWSURFACE) == 0) */
                SDL_UpdateRect(m_pScreen, r.x(), r.y(), r.width(), r.height());
            }
            else
            {
                SDL_UpdateRect(m_pScreen, r.x(), r.y(), r.width(), r.height());
            }
        }
    }
}

void UIFrameBufferSDL::resizeEvent(UIResizeEvent *pEvent)
{
    /* Check whether the guest resolution has not been changed. */
    bool fSameResolutionRequested = (width()  == pEvent->width() && height() == pEvent->height());

    /* Check if the guest VRAM can be used as the source bitmap. */
    bool bFallback = false;

    Uint32 Rmask = 0;
    Uint32 Gmask = 0;
    Uint32 Bmask = 0;

    if (pEvent->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (pEvent->bitsPerPixel())
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
                bFallback = true;
                break;
        }
    }
    else
    {
        /* Unsupported format leads to the indirect buffer */
        bFallback = true;
    }

    m_width = pEvent->width();
    m_height = pEvent->height();

    /* Recreate the source surface. */
    if (m_pSurfVRAM)
    {
        SDL_FreeSurface(m_pSurfVRAM);
        m_pSurfVRAM = NULL;
    }

    if (!bFallback)
    {
        /* It is OK to create the source surface from the guest VRAM. */
        m_pSurfVRAM = SDL_CreateRGBSurfaceFrom(pEvent->VRAM(), pEvent->width(), pEvent->height(),
                                               pEvent->bitsPerPixel(),
                                               pEvent->bytesPerLine(),
                                               Rmask, Gmask, Bmask, 0);
        LogFlowFunc(("Created VRAM surface %p\n", m_pSurfVRAM));
        if (m_pSurfVRAM == NULL)
        {
            bFallback = true;
        }
    }

    if (fSameResolutionRequested)
    {
        LogFlowFunc(("the same resolution requested, skipping the resize.\n"));
        return;
    }

    /* close SDL so we can init it again */
    if (m_pScreen)
    {
        X11ScreenSaverSettingsSave();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        X11ScreenSaverSettingsRestore();
        m_pScreen = NULL;
    }

    /*
     *  initialize the SDL library, use its super hack to integrate it with our client window
     */
    static char sdlHack[64];
    LogFlowFunc(("Using client window 0x%08lX to initialize SDL\n", m_pMachineView->viewport()->winId()));
    /* Note: SDL_WINDOWID must be decimal (not hex) to work on Win32 */
    sprintf(sdlHack, "SDL_WINDOWID=%lu", m_pMachineView->viewport()->winId());
    putenv(sdlHack);
    X11ScreenSaverSettingsSave();
    int rc = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    X11ScreenSaverSettingsRestore();
    AssertMsg(rc == 0, ("SDL initialization failed: %s\n", SDL_GetError()));
    NOREF(rc);

#ifdef Q_WS_X11
    /* undo signal redirections from SDL, it'd steal keyboard events from us! */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
#endif

    LogFlowFunc(("Setting SDL video mode to %d x %d\n", m_width, m_height));

    /* Pixel format is RGB in any case */
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;

    m_pScreen = SDL_SetVideoMode(m_width, m_height, 0, SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL);
    AssertMsg(m_pScreen, ("SDL video mode could not be set!\n"));
}
#endif

#if defined (VBOX_GUI_USE_DDRAW)
/* The class is defined in VBoxFBDDRAW.cpp */
#endif

#if defined (VBOX_GUI_USE_QUARTZ2D)
/* The class is defined in VBoxQuartz2D.cpp */
#endif
#endif
