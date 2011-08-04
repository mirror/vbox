/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QDesktopWidget>
#include <QTimer>
#include <QPainter>
#include <QScrollBar>
#include <VBox/VBoxVideo.h>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIFrameBuffer.h"
#include "UIFrameBufferQGL.h"
#include "UIFrameBufferQImage.h"
#include "UIFrameBufferQuartz2D.h"
#include "UIFrameBufferSDL.h"
#include "VBoxFBOverlay.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewNormal.h"
#include "UIMachineViewFullscreen.h"
#include "UIMachineViewSeamless.h"
#include "UIMachineViewScale.h"

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/XKBlib.h>
# ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#  undef KeyRelease
#  undef KeyPress
#  undef FocusOut
#  undef FocusIn
# endif
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "DockIconPreview.h"
# include "DarwinKeyboard.h"
# include "UICocoaApplication.h"
# include <VBox/err.h>
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

class UIViewport: public QWidget
{
public:

    UIViewport(QWidget *pParent) : QWidget(pParent)
    {
        /* No need for background drawing: */
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    QPaintEngine *paintEngine() const
    {
        if (testAttribute(Qt::WA_PaintOnScreen))
            return NULL;
        else
            return QWidget::paintEngine();
    }
};

UIMachineView* UIMachineView::create(  UIMachineWindow *pMachineWindow
                                     , ulong uScreenId
                                     , UIVisualStateType visualStateType
#ifdef VBOX_WITH_VIDEOHWACCEL
                                     , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                     )
{
    UIMachineView *pMachineView = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            pMachineView = new UIMachineViewNormal(  pMachineWindow
                                                   , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                   , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                   );
            break;
        case UIVisualStateType_Fullscreen:
            pMachineView = new UIMachineViewFullscreen(  pMachineWindow
                                                       , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                       , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                       );
            break;
        case UIVisualStateType_Seamless:
            pMachineView = new UIMachineViewSeamless(  pMachineWindow
                                                     , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                     , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                     );
            break;
        case UIVisualStateType_Scale:
            pMachineView = new UIMachineViewScale(  pMachineWindow
                                                  , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                  , bAccelerate2DVideo
#endif
                                                  );
            break;
        default:
            break;
    }
    return pMachineView;
}

void UIMachineView::destroy(UIMachineView *pMachineView)
{
    delete pMachineView;
}

double UIMachineView::aspectRatio() const
{
    return frameBuffer() ? (double)(frameBuffer()->width()) / frameBuffer()->height() : 0;
}

void UIMachineView::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            if (   vboxGlobal().vmRenderMode() != VBoxDefs::TimerMode
                && m_pFrameBuffer
                &&
                (   state           != KMachineState_TeleportingPausedVM
                 || m_previousState != KMachineState_Teleporting))
            {
                takePauseShotLive();
                /* Fully repaint to pick up m_pauseShot: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Restoring:
        {
            /* Only works with the primary screen currently. */
            if (screenId() == 0)
            {
                takePauseShotSnapshot();
                /* Fully repaint to pick up m_pauseShot: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Running:
        {
            if (   m_previousState == KMachineState_Paused
                || m_previousState == KMachineState_TeleportingPausedVM
                || m_previousState == KMachineState_Restoring)
            {
                if (vboxGlobal().vmRenderMode() != VBoxDefs::TimerMode && m_pFrameBuffer)
                {
                    /* Reset the pixmap to free memory: */
                    resetPauseShot();
                    /* Ask for full guest display update (it will also update
                     * the viewport through IFramebuffer::NotifyUpdate): */
                    CDisplay dsp = session().GetConsole().GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
            break;
        }
        default:
            break;
    }

    m_previousState = state;
}

UIMachineView::UIMachineView(  UIMachineWindow *pMachineWindow
                             , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                             , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                             )
    : QAbstractScrollArea(pMachineWindow->machineWindow())
    , m_pMachineWindow(pMachineWindow)
    , m_uScreenId(uScreenId)
    , m_pFrameBuffer(0)
    , m_previousState(KMachineState_Null)
    , m_desktopGeometryType(DesktopGeo_Invalid)
    , m_bIsMachineWindowResizeIgnored(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_fAccelerate2DVideo(bAccelerate2DVideo)
#endif /* VBOX_WITH_VIDEOHWACCEL */
{
}

UIMachineView::~UIMachineView()
{
}

void UIMachineView::prepareViewport()
{
    /* Prepare viewport: */
#ifdef VBOX_GUI_USE_QGLFB
    QWidget *pViewport = 0;
    switch (vboxGlobal().vmRenderMode())
    {
        case VBoxDefs::QGLMode:
            pViewport = new VBoxGLWidget(session().GetConsole(), this, NULL);
            break;
        default:
            pViewport = new UIViewport(this);
    }
#else /* VBOX_GUI_USE_QGLFB */
    UIViewport *pViewport = new UIViewport(this);
#endif /* !VBOX_GUI_USE_QGLFB */
    setViewport(pViewport);
}

void UIMachineView::prepareFrameBuffer()
{
    /* Prepare frame-buffer depending on render-mode: */
    switch (vboxGlobal().vmRenderMode())
    {
#ifdef VBOX_GUI_USE_QIMAGE
        case VBoxDefs::QImageMode:
# ifdef VBOX_WITH_VIDEOHWACCEL
            if (m_fAccelerate2DVideo)
            {
                UIFrameBuffer* pFramebuffer = uisession()->frameBuffer(screenId());
                if (pFramebuffer)
                    pFramebuffer->setView(this);
                else
                {
                    /* these two additional template args is a workaround to this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFramebuffer = new VBoxOverlayFrameBuffer<UIFrameBufferQImage, UIMachineView, UIResizeEvent>(this, &machineWindowWrapper()->session(), (uint32_t)screenId());
                    uisession()->setFrameBuffer(screenId(), pFramebuffer);
                }
                m_pFrameBuffer = pFramebuffer;
            }
            else
                m_pFrameBuffer = new UIFrameBufferQImage(this);
# else /* VBOX_WITH_VIDEOHWACCEL */
            m_pFrameBuffer = new UIFrameBufferQImage(this);
# endif /* !VBOX_WITH_VIDEOHWACCEL */
            break;
#endif /* VBOX_GUI_USE_QIMAGE */
#ifdef VBOX_GUI_USE_QGLFB
        case VBoxDefs::QGLMode:
            m_pFrameBuffer = new UIFrameBufferQGL(this);
            break;
//        case VBoxDefs::QGLOverlayMode:
//            m_pFrameBuffer = new UIQGLOverlayFrameBuffer(this);
//            break;
#endif /* VBOX_GUI_USE_QGLFB */
#ifdef VBOX_GUI_USE_SDL
        case VBoxDefs::SDLMode:
            /* Indicate that we are doing all drawing stuff ourself: */
            // TODO_NEW_CORE
            viewport()->setAttribute(Qt::WA_PaintOnScreen);
# ifdef Q_WS_X11
            /* This is somehow necessary to prevent strange X11 warnings on i386 and segfaults on x86_64: */
            XFlush(QX11Info::display());
# endif /* Q_WS_X11 */
# if defined(VBOX_WITH_VIDEOHWACCEL) && defined(DEBUG_misha) /* not tested yet */
            if (m_fAccelerate2DVideo)
            {
                class UIFrameBuffer* pFramebuffer = uisession()->frameBuffer(screenId());
                if (pFramebuffer)
                    pFramebuffer->setView(this);
                else
                {
                    /* these two additional template args is a workaround to this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFramebuffer = new VBoxOverlayFrameBuffer<UIFrameBufferSDL, UIMachineView, UIResizeEvent>(this, &machineWindowWrapper()->session(), (uint32_t)screenId());
                    uisession()->setFrameBuffer(screenId(), pFramebuffer);
                }
                m_pFrameBuffer = pFramebuffer;
            }
            else
                m_pFrameBuffer = new UIFrameBufferSDL(this);
# else
            m_pFrameBuffer = new UIFrameBufferSDL(this);
# endif
            /* Disable scrollbars because we cannot correctly draw in a scrolled window using SDL: */
            horizontalScrollBar()->setEnabled(false);
            verticalScrollBar()->setEnabled(false);
            break;
#endif /* VBOX_GUI_USE_SDL */
#if 0 // TODO: Enable DDraw frame buffer!
#ifdef VBOX_GUI_USE_DDRAW
        case VBoxDefs::DDRAWMode:
            m_pFrameBuffer = new UIDDRAWFrameBuffer(this);
            if (!m_pFrameBuffer || m_pFrameBuffer->address() == NULL)
            {
                if (m_pFrameBuffer)
                    delete m_pFrameBuffer;
                m_mode = VBoxDefs::QImageMode;
                m_pFrameBuffer = new UIFrameBufferQImage(this);
            }
            break;
#endif /* VBOX_GUI_USE_DDRAW */
#endif
#ifdef VBOX_GUI_USE_QUARTZ2D
        case VBoxDefs::Quartz2DMode:
            /* Indicate that we are doing all drawing stuff ourself: */
            viewport()->setAttribute(Qt::WA_PaintOnScreen);
# ifdef VBOX_WITH_VIDEOHWACCEL
            if (m_fAccelerate2DVideo)
            {
                UIFrameBuffer* pFramebuffer = uisession()->frameBuffer(screenId());
                if (pFramebuffer)
                    pFramebuffer->setView(this);
                else
                {
                    /* these two additional template args is a workaround to this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFramebuffer = new VBoxOverlayFrameBuffer<UIFrameBufferQuartz2D, UIMachineView, UIResizeEvent>(this, &machineWindowWrapper()->session(), (uint32_t)screenId());
                    uisession()->setFrameBuffer(screenId(), pFramebuffer);
                }
                m_pFrameBuffer = pFramebuffer;
            }
            else
                m_pFrameBuffer = new UIFrameBufferQuartz2D(this);
# else /* VBOX_WITH_VIDEOHWACCEL */
            m_pFrameBuffer = new UIFrameBufferQuartz2D(this);
# endif /* !VBOX_WITH_VIDEOHWACCEL */
            break;
#endif /* VBOX_GUI_USE_QUARTZ2D */
        default:
            AssertReleaseMsgFailed(("Render mode must be valid: %d\n", vboxGlobal().vmRenderMode()));
            LogRel(("Invalid render mode: %d\n", vboxGlobal().vmRenderMode()));
            qApp->exit(1);
            break;
    }

    /* If frame-buffer was prepared: */
    if (m_pFrameBuffer)
    {
        /* Prepare display: */
        CDisplay display = session().GetConsole().GetDisplay();
        Assert(!display.isNull());
#ifdef VBOX_WITH_VIDEOHWACCEL
        CFramebuffer fb(NULL);
        if (m_fAccelerate2DVideo)
        {
            LONG XOrigin, YOrigin;
            /* Check if the framebuffer is already assigned;
             * in this case we do not need to re-assign it neither do we need to AddRef. */
            display.GetFramebuffer(m_uScreenId, fb, XOrigin, YOrigin);
        }
        if (fb.raw() != m_pFrameBuffer) /* <-this will evaluate to true iff m_fAccelerate2DVideo is disabled or iff no framebuffer is yet assigned */
#endif /* VBOX_WITH_VIDEOHWACCEL */
        {
            m_pFrameBuffer->AddRef();
        }
        /* Always perform SetFramebuffer to ensure 3D gets notified: */
        display.SetFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));
    }

    QSize size;
#ifdef Q_WS_X11
    /* Processing pseudo resize-event to synchronize frame-buffer with stored
     * framebuffer size. On X11 this will be additional done when the machine
     * state was 'saved'. */
    if (session().GetMachine().GetState() == KMachineState_Saved)
        size = guestSizeHint();
#endif /* Q_WS_X11 */
    /* If there is a preview image saved, we will resize the framebuffer to the
     * size of that image. */
    ULONG buffer = 0, width = 0, height = 0;
    CMachine machine = session().GetMachine();
    machine.QuerySavedScreenshotPNGSize(0, buffer, width, height);
    if (buffer > 0)
    {
        /* Init with the screenshot size */
        size = QSize(width, height);
        /* Try to get the real guest dimensions from the save state */
        ULONG guestWidth = 0, guestHeight = 0;
        machine.QuerySavedGuestSize(0, guestWidth, guestHeight);
        if (   guestWidth  > 0
            && guestHeight > 0)
            size = QSize(guestWidth, guestHeight);
    }
    /* If we have a valid size, resize the framebuffer. */
    if (   size.width() > 0
        && size.height() > 0)
    {
        UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, size.width(), size.height());
        frameBuffer()->resizeEvent(&event);
    }
}

void UIMachineView::prepareCommon()
{
    /* Prepare view frame: */
    setFrameStyle(QFrame::NoFrame);

    /* Setup palette: */
    QPalette palette(viewport()->palette());
    palette.setColor(viewport()->backgroundRole(), Qt::black);
    viewport()->setPalette(palette);

    /* Setup focus policy: */
    setFocusPolicy(Qt::WheelFocus);
}

void UIMachineView::prepareFilters()
{
    /* Enable MouseMove events: */
    viewport()->setMouseTracking(true);

    /* QScrollView does the below on its own, but let's
     * do it anyway for the case it will not do it in the future: */
    viewport()->installEventFilter(this);

    /* We want to be notified on some parent's events: */
    machineWindowWrapper()->machineWindow()->installEventFilter(this);
}

void UIMachineView::prepareConsoleConnections()
{
    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));
}

void UIMachineView::loadMachineViewSettings()
{
    /* Global settings: */
    {
        /* Remember the desktop geometry and register for geometry
         * change events for telling the guest about video modes we like: */
        QString desktopGeometry = vboxGlobal().settings().publicProperty("GUI/MaxGuestResolution");
        if ((desktopGeometry == QString::null) || (desktopGeometry == "auto"))
            setDesktopGeometry(DesktopGeo_Automatic, 0, 0);
        else if (desktopGeometry == "any")
            setDesktopGeometry(DesktopGeo_Any, 0, 0);
        else
        {
            int width = desktopGeometry.section(',', 0, 0).toInt();
            int height = desktopGeometry.section(',', 1, 1).toInt();
            setDesktopGeometry(DesktopGeo_Fixed, width, height);
        }
    }
}

void UIMachineView::cleanupFrameBuffer()
{
    if (m_pFrameBuffer)
    {
        /* Process pending frame-buffer resize events: */
        QApplication::sendPostedEvents(this, VBoxDefs::ResizeEventType);
#ifdef VBOX_WITH_VIDEOHWACCEL
        if (m_fAccelerate2DVideo)
        {
            /* When 2D is enabled we do not re-create Framebuffers. This is done to
             * 1. avoid 2D command loss during the time slot when no framebuffer is assigned to the display
             * 2. make it easier to preserve the current 2D state */
            Assert(m_pFrameBuffer == uisession()->frameBuffer(screenId()));
            m_pFrameBuffer->setView(NULL);
        }
        else
#endif /* VBOX_WITH_VIDEOHWACCEL */
        {
            /* Warn framebuffer about its no more necessary: */
            m_pFrameBuffer->setDeleted(true);
            /* Detach framebuffer from Display: */
            CDisplay display = session().GetConsole().GetDisplay();
            display.SetFramebuffer(m_uScreenId, CFramebuffer(NULL));
            /* Release the reference: */
            m_pFrameBuffer->Release();
//          delete m_pFrameBuffer; // TODO_NEW_CORE: possibly necessary to really cleanup
            m_pFrameBuffer = NULL;
        }
    }
}

UIMachineLogic* UIMachineView::machineLogic() const
{
    return machineWindowWrapper()->machineLogic();
}

UISession* UIMachineView::uisession() const
{
    return machineLogic()->uisession();
}

CSession& UIMachineView::session()
{
    return uisession()->session();
}

QSize UIMachineView::sizeHint() const
{
#ifdef VBOX_WITH_DEBUGGER
    // TODO: Fix all DEBUGGER stuff!
    /* HACK ALERT! Really ugly workaround for the resizing to 9x1 done by DevVGA if provoked before power on. */
    QSize fb(m_pFrameBuffer->width(), m_pFrameBuffer->height());
    if (fb.width() < 16 || fb.height() < 16)
    {
        CMachine machine = uisession()->session().GetMachine();
        if (   vboxGlobal().isStartPausedEnabled()
            || vboxGlobal().isDebuggerAutoShowEnabled(machine))
        fb = QSize(640, 480);
    }
    return QSize(fb.width() + frameWidth() * 2, fb.height() + frameWidth() * 2);
#else /* VBOX_WITH_DEBUGGER */
    return QSize(m_pFrameBuffer->width() + frameWidth() * 2, m_pFrameBuffer->height() + frameWidth() * 2);
#endif /* !VBOX_WITH_DEBUGGER */
}

int UIMachineView::contentsX() const
{
    return horizontalScrollBar()->value();
}

int UIMachineView::contentsY() const
{
    return verticalScrollBar()->value();
}

int UIMachineView::contentsWidth() const
{
    return m_pFrameBuffer->width();
}

int UIMachineView::contentsHeight() const
{
    return m_pFrameBuffer->height();
}

int UIMachineView::visibleWidth() const
{
    return horizontalScrollBar()->pageStep();
}

int UIMachineView::visibleHeight() const
{
    return verticalScrollBar()->pageStep();
}

QSize UIMachineView::desktopGeometry() const
{
    QSize geometry;
    switch (m_desktopGeometryType)
    {
        case DesktopGeo_Fixed:
        case DesktopGeo_Automatic:
            geometry = QSize(qMax(m_desktopGeometry.width(), m_storedConsoleSize.width()),
                             qMax(m_desktopGeometry.height(), m_storedConsoleSize.height()));
            break;
        case DesktopGeo_Any:
            geometry = QSize(0, 0);
            break;
        default:
            AssertMsgFailed(("Bad geometry type %d!\n", m_desktopGeometryType));
    }
    return geometry;
}

QSize UIMachineView::guestSizeHint()
{
    /* Result: */
    QSize sizeHint;

    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Load machine view hint: */
    QString strKey = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastGuestSizeHint) :
                     QString("%1%2").arg(VBoxDefs::GUI_LastGuestSizeHint).arg(m_uScreenId);
    QString strValue = machine.GetExtraData(strKey);

    bool ok = true;
    int width = 0, height = 0;
    if (ok)
        width = strValue.section(',', 0, 0).toInt(&ok);
    if (ok)
        height = strValue.section(',', 1, 1).toInt(&ok);

    if (ok /* If previous parameters were read correctly! */)
    {
        /* Compose guest size hint from loaded values: */
        sizeHint = QSize(width, height);
    }
    else
    {
        /* Compose guest size hint from default attributes: */
        sizeHint = QSize(800, 600);
    }

    /* Return result: */
    return sizeHint;
}

void UIMachineView::setDesktopGeometry(DesktopGeo geometry, int aWidth, int aHeight)
{
    switch (geometry)
    {
        case DesktopGeo_Fixed:
            m_desktopGeometryType = DesktopGeo_Fixed;
            if (aWidth != 0 && aHeight != 0)
                m_desktopGeometry = QSize(aWidth, aHeight);
            else
                m_desktopGeometry = QSize(0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Automatic:
            m_desktopGeometryType = DesktopGeo_Automatic;
            m_desktopGeometry = QSize(0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Any:
            m_desktopGeometryType = DesktopGeo_Any;
            m_desktopGeometry = QSize(0, 0);
            break;
        default:
            AssertMsgFailed(("Invalid desktop geometry type %d\n", geometry));
            m_desktopGeometryType = DesktopGeo_Invalid;
    }
}

void UIMachineView::storeConsoleSize(int iWidth, int iHeight)
{
    m_storedConsoleSize = QSize(iWidth, iHeight);
}

void UIMachineView::storeGuestSizeHint(const QSize &sizeHint)
{
    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Save machine view hint: */
    QString strKey = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastGuestSizeHint) :
                     QString("%1%2").arg(VBoxDefs::GUI_LastGuestSizeHint).arg(m_uScreenId);
    QString strValue = QString("%1,%2").arg(sizeHint.width()).arg(sizeHint.height());
    machine.SetExtraData(strKey, strValue);
}

void UIMachineView::takePauseShotLive()
{
    /* Take a screen snapshot. Note that TakeScreenShot() always needs a 32bpp image: */
    QImage shot = QImage(m_pFrameBuffer->width(), m_pFrameBuffer->height(), QImage::Format_RGB32);
    /* If TakeScreenShot fails or returns no image, just show a black image. */
    shot.fill(0);
    CDisplay dsp = session().GetConsole().GetDisplay();
    dsp.TakeScreenShot(screenId(), shot.bits(), shot.width(), shot.height());
    /* TakeScreenShot() may fail if, e.g. the Paused notification was delivered
     * after the machine execution was resumed. It's not fatal: */
    if (dsp.isOk())
        dimImage(shot);
    m_pauseShot = QPixmap::fromImage(shot);
}

void UIMachineView::takePauseShotSnapshot()
{
    CMachine machine = session().GetMachine();
    ULONG width = 0, height = 0;
    QVector<BYTE> screenData = machine.ReadSavedScreenshotPNGToArray(0, width, height);
    if (screenData.size() != 0)
    {
        ULONG guestWidth = 0, guestHeight = 0;
        machine.QuerySavedGuestSize(0, guestWidth, guestHeight);
        QImage shot = QImage::fromData(screenData.data(), screenData.size(), "PNG").scaled(guestWidth > 0 ? QSize(guestWidth, guestHeight) : guestSizeHint());
        dimImage(shot);
        m_pauseShot = QPixmap::fromImage(shot);
    }
}

void UIMachineView::updateSliders()
{
    QSize p = viewport()->size();
    QSize m = maximumViewportSize();

    QSize v = QSize(frameBuffer()->width(), frameBuffer()->height());
    /* No scroll bars needed: */
    if (m.expandedTo(v) == m)
        p = m;

    horizontalScrollBar()->setRange(0, v.width() - p.width());
    verticalScrollBar()->setRange(0, v.height() - p.height());
    horizontalScrollBar()->setPageStep(p.width());
    verticalScrollBar()->setPageStep(p.height());
}

QPoint UIMachineView::viewportToContents(const QPoint &vp) const
{
    return QPoint(vp.x() + contentsX(), vp.y() + contentsY());
}

void UIMachineView::scrollBy(int dx, int dy)
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
}

void UIMachineView::dimImage(QImage &img)
{
    for (int y = 0; y < img.height(); ++ y)
    {
        if (y % 2)
        {
            if (img.depth() == 32)
            {
                for (int x = 0; x < img.width(); ++ x)
                {
                    int gray = qGray(img.pixel (x, y)) / 2;
                    img.setPixel(x, y, qRgb (gray, gray, gray));
                }
            }
            else
            {
                ::memset(img.scanLine (y), 0, img.bytesPerLine());
            }
        }
        else
        {
            if (img.depth() == 32)
            {
                for (int x = 0; x < img.width(); ++ x)
                {
                    int gray = (2 * qGray (img.pixel (x, y))) / 3;
                    img.setPixel(x, y, qRgb (gray, gray, gray));
                }
            }
        }
    }
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void UIMachineView::scrollContentsBy(int dx, int dy)
{
    if (m_pFrameBuffer)
    {
        m_pFrameBuffer->viewportScrolled(dx, dy);
    }
    QAbstractScrollArea::scrollContentsBy(dx, dy);
}
#endif /* VBOX_WITH_VIDEOHWACCEL */

#ifdef Q_WS_MAC
void UIMachineView::updateDockIcon()
{
    machineLogic()->updateDockIcon();
}

CGImageRef UIMachineView::vmContentImage()
{
    if (!m_pauseShot.isNull())
    {
        CGImageRef pauseImg = ::darwinToCGImageRef(&m_pauseShot);
        /* Use the pause image as background */
        return pauseImg;
    }
    else
    {
# ifdef VBOX_GUI_USE_QUARTZ2D
        if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode)
        {
            /* If the render mode is Quartz2D we could use the CGImageRef
             * of the framebuffer for the dock icon creation. This saves
             * some conversion time. */
            CGImageRef image = static_cast<UIFrameBufferQuartz2D*>(m_pFrameBuffer)->imageRef();
            CGImageRetain(image); /* Retain it, cause the consumer will release it. */
            return image;
        }
        else
# endif /* VBOX_GUI_USE_QUARTZ2D */
            /* In image mode we have to create the image ref out of the
             * framebuffer */
            return frameBuffertoCGImageRef(m_pFrameBuffer);
    }
    return 0;
}

CGImageRef UIMachineView::frameBuffertoCGImageRef(UIFrameBuffer *pFrameBuffer)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert(cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData(pFrameBuffer, pFrameBuffer->address(), pFrameBuffer->bitsPerPixel() / 8 * pFrameBuffer->width() * pFrameBuffer->height(), NULL);
    Assert(dp);
    CGImageRef ir = CGImageCreate(pFrameBuffer->width(), pFrameBuffer->height(), 8, 32, pFrameBuffer->bytesPerLine(), cs,
                                  kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                  kCGRenderingIntentDefault);
    Assert(ir);
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);

    return ir;
}
#endif /* Q_WS_MAC */

bool UIMachineView::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            viewport()->update(pPaintEvent->x() - contentsX(), pPaintEvent->y() - contentsY(),
                                pPaintEvent->width(), pPaintEvent->height());
            return true;
        }

#ifdef Q_WS_MAC
        /* Event posted OnShowWindow: */
        case VBoxDefs::ShowWindowEventType:
        {
            /* Dunno what Qt3 thinks a window that has minimized to the dock should be - it is not hidden,
             * neither is it minimized. OTOH it is marked shown and visible, but not activated.
             * This latter isn't of much help though, since at this point nothing is marked activated.
             * I might have overlooked something, but I'm buggered what if I know what. So, I'll just always
             * show & activate the stupid window to make it get out of the dock when the user wishes to show a VM: */
            window()->show();
            window()->activateWindow();
            return true;
        }
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBoxDefs::VHWACommandProcessType:
        {
            m_pFrameBuffer->doProcessVHWACommand(pEvent);
            return true;
        }
#endif /* VBOX_WITH_VIDEOHWACCEL */

        default:
            break;
    }

    return QAbstractScrollArea::event(pEvent);
}

bool UIMachineView::eventFilter(QObject *pWatched, QEvent *pEvent)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    if (pWatched == viewport())
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                if (m_pFrameBuffer)
                    m_pFrameBuffer->viewportResized(static_cast<QResizeEvent*>(pEvent));
                break;
            }
            default:
                break;
        }
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */
    if (pWatched == machineWindowWrapper()->machineWindow())
    {
        switch (pEvent->type())
        {
            case QEvent::WindowStateChange:
            {
                /* During minimizing and state restoring machineWindowWrapper() gets
                 * the focus which belongs to console view window, so returning it properly. */
                QWindowStateChangeEvent *pWindowEvent = static_cast<QWindowStateChangeEvent*>(pEvent);
                if (pWindowEvent->oldState() & Qt::WindowMinimized)
                {
                    if (QApplication::focusWidget())
                    {
                        QApplication::focusWidget()->clearFocus();
                        qApp->processEvents();
                    }
                    QTimer::singleShot(0, this, SLOT(setFocus()));
                }
                break;
            }
            default:
                break;
        }
    }

    return QAbstractScrollArea::eventFilter(pWatched, pEvent);
}

void UIMachineView::resizeEvent(QResizeEvent *pEvent)
{
    updateSliders();
    return QAbstractScrollArea::resizeEvent(pEvent);
}

void UIMachineView::moveEvent(QMoveEvent *pEvent)
{
    return QAbstractScrollArea::moveEvent(pEvent);
}

void UIMachineView::paintEvent(QPaintEvent *pPaintEvent)
{
    if (m_pauseShot.isNull())
    {
        /* Delegate the paint function to the VBoxFrameBuffer interface: */
        if (m_pFrameBuffer && !uisession()->isTurnedOff())
            m_pFrameBuffer->paintEvent(pPaintEvent);
#ifdef Q_WS_MAC
        /* Update the dock icon if we are in the running state */
        if (uisession()->isRunning())
            updateDockIcon();
#endif /* Q_WS_MAC */
        return;
    }

#ifdef VBOX_GUI_USE_QUARTZ2D
    if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode && m_pFrameBuffer)
    {
        m_pFrameBuffer->paintEvent(pPaintEvent);
        updateDockIcon();
    }
    else
#endif /* VBOX_GUI_USE_QUARTZ2D */
    {
        /* We have a snapshot for the paused state: */
        QRect r = pPaintEvent->rect().intersect(viewport()->rect());
        /* We have to disable paint on screen if we are using the regular painter: */
        bool paintOnScreen = viewport()->testAttribute(Qt::WA_PaintOnScreen);
        viewport()->setAttribute(Qt::WA_PaintOnScreen, false);
        QPainter pnt(viewport());
        pnt.drawPixmap(r, m_pauseShot, QRect(r.x() + contentsX(), r.y() + contentsY(), r.width(), r.height()));
        /* Restore the attribute to its previous state: */
        viewport()->setAttribute(Qt::WA_PaintOnScreen, paintOnScreen);
#ifdef Q_WS_MAC
        updateDockIcon();
#endif /* Q_WS_MAC */
    }
}

#if defined(Q_WS_WIN)

bool UIMachineView::winEvent(MSG *pMsg, long* /* piResult */)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            /* Filter using keyboard-filter: */
            bool fKeyboardFilteringResult = machineLogic()->keyboardHandler()->winEventFilter(pMsg, screenId());
            /* Keyboard filter rules the result: */
            fResult = fKeyboardFilteringResult;
            break;
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#elif defined(Q_WS_X11)

bool UIMachineView::x11Event(XEvent *pEvent)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pEvent->type)
    {
        case XFocusOut:
        case XFocusIn:
        case XKeyPress:
        case XKeyRelease:
        {
            /* Filter using keyboard-filter: */
            bool fKeyboardFilteringResult = machineLogic()->keyboardHandler()->x11EventFilter(pEvent, screenId());
            /* Filter using mouse-filter: */
            bool fMouseFilteringResult = machineLogic()->mouseHandler()->x11EventFilter(pEvent, screenId());
            /* If at least one of filters wants to filter event out then the result is 'true': */
            fResult = fKeyboardFilteringResult || fMouseFilteringResult;
            break;
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#endif
