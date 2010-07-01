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
#include "VBoxProblemReporter.h"
#include "UIFrameBuffer.h"
#include "UIFrameBufferQGL.h"
#include "UIFrameBufferQImage.h"
#include "UIFrameBufferQuartz2D.h"
#include "UIFrameBufferSDL.h"
#include "VBoxFBOverlay.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineWindowSeamless.h"
#include "UIMachineViewNormal.h"
#include "UIMachineViewFullscreen.h"
#include "UIMachineViewSeamless.h"

#ifdef Q_WS_WIN
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <windows.h>
static UIMachineView *gView = 0;
static HHOOK gKbdHook = 0;
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
# include <QX11Info>
# define XK_XKB_KEYS
# define XK_MISCELLANY
# include <X11/XKBlib.h>
# include <X11/keysym.h>
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
# include "XKeyboard.h"
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "DockIconPreview.h"
# include "DarwinKeyboard.h"
# include "UICocoaApplication.h"
# include <VBox/err.h>
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

class VBoxViewport: public QWidget
{
public:

    VBoxViewport(QWidget *pParent) : QWidget(pParent)
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

enum { KeyExtended = 0x01, KeyPressed = 0x02, KeyPause = 0x04, KeyPrint = 0x08 };
enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };

UIMachineView* UIMachineView::create(  UIMachineWindow *pMachineWindow
                                     , ulong uScreenId
                                     , UIVisualStateType visualStateType
#ifdef VBOX_WITH_VIDEOHWACCEL
                                     , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                     )
{
    UIMachineView *view = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            view = new UIMachineViewNormal(  pMachineWindow
                                           , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                           );
            break;
        case UIVisualStateType_Fullscreen:
            view = new UIMachineViewFullscreen(  pMachineWindow
                                               , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                               , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                               );
            break;
        case UIVisualStateType_Seamless:
            view = new UIMachineViewSeamless(  pMachineWindow
                                             , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                             , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                             );
            break;
        default:
            break;
    }
    return view;
}

void UIMachineView::destroy(UIMachineView *pMachineView)
{
    delete pMachineView;
}

int UIMachineView::keyboardState() const
{
    return (m_bIsKeyboardCaptured ? UIViewStateType_KeyboardCaptured : 0) |
           (m_bIsHostkeyPressed ? UIViewStateType_HostKeyPressed : 0);
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
            if (vboxGlobal().vmRenderMode() != VBoxDefs::TimerMode &&  m_pFrameBuffer &&
                (state != KMachineState_TeleportingPausedVM || m_previousState != KMachineState_Teleporting))
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
                {
                    dimImage(shot);
                }
                m_pauseShot = QPixmap::fromImage(shot);
                /* Fully repaint to pick up m_pauseShot: */
                viewport()->repaint();
            }
            /* Reuse the focus event handler to uncapture keyboard: */
            if (hasFocus())
                focusEvent(false /* aHasFocus*/, false /* aReleaseHostKey */);
            break;
        }
        case KMachineState_Stuck:
        {
            /* Reuse the focus event handler to uncapture keyboard: */
            if (hasFocus())
                focusEvent(false /* aHasFocus*/, false /* aReleaseHostKey */);
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
                    m_pauseShot = QPixmap();
                    /* Ask for full guest display update (it will also update
                     * the viewport through IFramebuffer::NotifyUpdate): */
                    CDisplay dsp = session().GetConsole().GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
            /* Reuse the focus event handler to capture keyboard: */
            if (hasFocus())
                focusEvent(true /* aHasFocus */);
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
    , m_globalSettings(vboxGlobal().settings())
    , m_pFrameBuffer(0)
    , m_previousState(KMachineState_Null)
    , m_desktopGeometryType(DesktopGeo_Invalid)
    , m_bIsKeyboardCaptured(false)
    , m_bIsHostkeyPressed(false)
    , m_bIsHostkeyAlone (false)
    , m_bIsHostkeyInCapture(false)
    , m_bIsMachineWindowResizeIgnored(false)
    , m_fPassCAD(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_fAccelerate2DVideo(bAccelerate2DVideo)
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef Q_WS_MAC
    , m_darwinKeyModifiers(0)
    , m_fKeyboardGrabbed (false)
#endif /* Q_WS_MAC */
{
}

UIMachineView::~UIMachineView()
{
}

void UIMachineView::prepareFrameBuffer()
{
    /* Prepare viewport: */
#ifdef VBOX_GUI_USE_QGLFB
    QWidget *pViewport;
    switch (vboxGlobal().vmRenderMode())
    {
        case VBoxDefs::QGLMode:
            pViewport = new VBoxGLWidget(session().GetConsole(), this, NULL);
            break;
        default:
            pViewport = new VBoxViewport(this);
    }
#else /* VBOX_GUI_USE_QGLFB */
    VBoxViewport *pViewport = new VBoxViewport(this);
#endif /* !VBOX_GUI_USE_QGLFB */
    setViewport(pViewport);

    CDisplay display = session().GetConsole().GetDisplay();
    Assert(!display.isNull());
    m_pFrameBuffer = NULL;

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
    if (m_pFrameBuffer)
    {
#ifdef VBOX_WITH_VIDEOHWACCEL
        CFramebuffer fb(NULL);
        if (m_fAccelerate2DVideo)
        {
            LONG XOrigin, YOrigin;
            /* check if the framebuffer is already assigned
             * in this case we do not need to re-assign it
             * neither do we need to AddRef */
            display.GetFramebuffer(m_uScreenId, fb, XOrigin, YOrigin);
        }
        if (fb.raw() != m_pFrameBuffer) /* <-this will evaluate to true iff m_fAccelerate2DVideo is disabled or iff no framebuffer is yet assigned */
#endif /* VBOX_WITH_VIDEOHWACCEL */
        {
            m_pFrameBuffer->AddRef();
        }

        /* always perform SetFramebuffer to ensure 3D gets notified */
        display.SetFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));
    }

#ifdef Q_WS_X11
    /* Processing pseudo resize-event to synchronize frame-buffer
     * with stored framebuffer size in case of machine state was 'saved': */
    if (session().GetMachine().GetState() == KMachineState_Saved)
    {
        QSize size = guestSizeHint();
        UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, size.width(), size.height());
        frameBuffer()->resizeEvent(&event);
    }
#endif /* Q_WS_X11 */
}

void UIMachineView::prepareCommon()
{
    /* Prepare view frame: */
    setFrameStyle(QFrame::NoFrame);

    /* Pressed keys: */
    ::memset(m_pressedKeys, 0, sizeof(m_pressedKeys));

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
#ifdef Q_WS_X11
        /* Initialize the X keyboard subsystem: */
        initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif /* Q_WS_X11 */

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

    /* Exatra data settings: */
    {
        /* CAD settings: */
        QString passCAD = session().GetConsole().GetMachine().GetExtraData(VBoxDefs::GUI_PassCAD);
        if (!passCAD.isEmpty() && ((passCAD != "false") || (passCAD != "no")))
            m_fPassCAD = true;
    }
}

void UIMachineView::cleanupCommon()
{
#ifdef Q_WS_WIN
    if (gKbdHook)
        UnhookWindowsHookEx(gKbdHook);
    gView = 0;
#endif /* Q_WS_WIN */

#ifdef Q_WS_MAC
    /* We have to make sure the callback for the keyboard events is released
     * when closing this view. */
    if (m_fKeyboardGrabbed)
        darwinGrabKeyboardEvents (false);
#endif /* Q_WS_MAC */
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
    if ((fb.width() < 16 || fb.height() < 16) && (vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled()))
        fb = QSize(640, 480);
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

void UIMachineView::fixModifierState(LONG *piCodes, uint *puCount)
{
    /* Synchronize the views of the host and the guest to the modifier keys.
     * This function will add up to 6 additional keycodes to codes. */

#if defined(Q_WS_X11)

    Window   wDummy1, wDummy2;
    int      iDummy3, iDummy4, iDummy5, iDummy6;
    unsigned uMask;
    unsigned uKeyMaskNum = 0, uKeyMaskCaps = 0, uKeyMaskScroll = 0;

    uKeyMaskCaps          = LockMask;
    XModifierKeymap* map  = XGetModifierMapping(QX11Info::display());
    KeyCode keyCodeNum    = XKeysymToKeycode(QX11Info::display(), XK_Num_Lock);
    KeyCode keyCodeScroll = XKeysymToKeycode(QX11Info::display(), XK_Scroll_Lock);

    for (int i = 0; i < 8; ++ i)
    {
        if (keyCodeNum != NoSymbol && map->modifiermap[map->max_keypermod * i] == keyCodeNum)
            uKeyMaskNum = 1 << i;
        else if (keyCodeScroll != NoSymbol && map->modifiermap[map->max_keypermod * i] == keyCodeScroll)
            uKeyMaskScroll = 1 << i;
    }
    XQueryPointer(QX11Info::display(), DefaultRootWindow(QX11Info::display()), &wDummy1, &wDummy2,
                  &iDummy3, &iDummy4, &iDummy5, &iDummy6, &uMask);
    XFreeModifiermap(map);

    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(uMask & uKeyMaskNum)))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(uMask & uKeyMaskCaps)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(Q_WS_WIN)

    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(GetKeyState(VK_CAPITAL))))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(Q_WS_MAC)

    /* if (uisession()->numLockAdaptionCnt()) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#else

//#warning Adapt UIMachineView::fixModifierState

#endif
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

void UIMachineView::emitKeyboardStateChanged()
{
    emit keyboardStateChanged(keyboardState());
}

void UIMachineView::captureKbd(bool fCapture, bool fEmitSignal /* = true */)
{
    if (m_bIsKeyboardCaptured == fCapture)
        return;

#if defined(Q_WS_WIN)
    /* On Win32, keyboard grabbing is ineffective, a low-level keyboard hook is used instead. */
#elif defined(Q_WS_X11)
    /* On X11, we are using passive XGrabKey for normal (windowed) mode
     * instead of XGrabKeyboard (called by QWidget::grabKeyboard())
     * because XGrabKeyboard causes a problem under metacity - a window cannot be moved
     * using the mouse if it is currently actively grabing the keyboard;
     * For static modes we are using usual (active) keyboard grabbing. */
    switch (machineLogic()->visualStateType())
    {
        /* If window is moveable we are making passive keyboard grab: */
        case UIVisualStateType_Normal:
        {
            if (fCapture)
                XGrabKey(QX11Info::display(), AnyKey, AnyModifier, machineWindowWrapper()->machineWindow()->winId(), False, GrabModeAsync, GrabModeAsync);
            else
                XUngrabKey(QX11Info::display(), AnyKey, AnyModifier, machineWindowWrapper()->machineWindow()->winId());
            break;
        }
        /* If window is NOT moveable we are making active keyboard grab: */
        case UIVisualStateType_Fullscreen:
        case UIVisualStateType_Seamless:
        {
            if (fCapture)
            {
                /* Keyboard grabbing can fail because of some keyboard shortcut is still grabbed by window manager.
                 * We can't be sure this shortcut will be released at all, so we will retry to grab keyboard for 50 times,
                 * and after we will just ignore that issue: */
                int cTriesLeft = 50;
                while (cTriesLeft && XGrabKeyboard(QX11Info::display(), machineWindowWrapper()->machineWindow()->winId(), False, GrabModeAsync, GrabModeAsync, CurrentTime)) { --cTriesLeft; }
            }
            else
                XUngrabKeyboard(QX11Info::display(), CurrentTime);
            break;
        }
        /* Should we try to grab keyboard in default case? I think - NO. */
        default:
            break;
    }
#elif defined(Q_WS_MAC)
    /* On Mac OS X, we use the Qt methods + disabling global hot keys + watching modifiers
     * (for right/left separation). */
    if (fCapture)
    {
        ::DarwinDisableGlobalHotKeys(true);
        grabKeyboard();
    }
    else
    {
        ::DarwinDisableGlobalHotKeys(false);
        releaseKeyboard();
    }
#else
    if (fCapture)
        grabKeyboard();
    else
        releaseKeyboard();
#endif

    m_bIsKeyboardCaptured = fCapture;

    if (fEmitSignal)
        emitKeyboardStateChanged();
}

void UIMachineView::saveKeyStates()
{
    ::memcpy(m_pressedKeysCopy, m_pressedKeys, sizeof(m_pressedKeys));
}

void UIMachineView::releaseAllPressedKeys(bool aReleaseHostKey /* = true */)
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    bool fSentRESEND = false;

    /* Send a dummy scan code (RESEND) to prevent the guest OS from recognizing
     * a single key click (for ex., Alt) and performing an unwanted action
     * (for ex., activating the menu) when we release all pressed keys below.
     * Note, that it's just a guess that sending RESEND will give the desired
     * effect :), but at least it works with NT and W2k guests. */
    for (uint i = 0; i < SIZEOF_ARRAY (m_pressedKeys); i++)
    {
        if (m_pressedKeys[i] & IsKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode (0xFE);
                fSentRESEND = true;
            }
            keyboard.PutScancode(i | 0x80);
        }
        else if (m_pressedKeys[i] & IsExtKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode(0xFE);
                fSentRESEND = true;
            }
            QVector <LONG> codes(2);
            codes[0] = 0xE0;
            codes[1] = i | 0x80;
            keyboard.PutScancodes(codes);
        }
        m_pressedKeys[i] = 0;
    }

    if (aReleaseHostKey)
        m_bIsHostkeyPressed = false;

#ifdef Q_WS_MAC
    /* Clear most of the modifiers: */
    m_darwinKeyModifiers &=
        alphaLock | kEventKeyModifierNumLockMask |
        (aReleaseHostKey ? 0 : ::DarwinKeyCodeToDarwinModifierMask (m_globalSettings.hostKey()));
#endif /* Q_WS_MAC */

    emitKeyboardStateChanged();
}

void UIMachineView::sendChangedKeyStates()
{
    QVector <LONG> codes(2);
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    for (uint i = 0; i < SIZEOF_ARRAY(m_pressedKeys); ++ i)
    {
        uint8_t os = m_pressedKeysCopy[i];
        uint8_t ns = m_pressedKeys[i];
        if ((os & IsKeyPressed) != (ns & IsKeyPressed))
        {
            codes[0] = i;
            if (!(ns & IsKeyPressed))
                codes[0] |= 0x80;
            keyboard.PutScancode(codes[0]);
        }
        else if ((os & IsExtKeyPressed) != (ns & IsExtKeyPressed))
        {
            codes[0] = 0xE0;
            codes[1] = i;
            if (!(ns & IsExtKeyPressed))
                codes[1] |= 0x80;
            keyboard.PutScancodes(codes);
        }
    }
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
        case QEvent::FocusIn:
        {
            if (uisession()->isRunning())
                focusEvent(true);
            break;
        }
        case QEvent::FocusOut:
        {
            if (uisession()->isRunning())
                focusEvent(false);
            else
            {
                /* Release the host key and all other pressed keys too even when paused
                 * (otherwise, we will get stuck keys in the guest when doing sendChangedKeyStates() on resume
                 * because key presses were already recorded in m_pressedKeys but key releases will most likely
                 * not reach us but the new focus window instead): */
                releaseAllPressedKeys(true /* including host key? */);
            }
            break;
        }

        case VBoxDefs::RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            viewport()->repaint(pPaintEvent->x() - contentsX(), pPaintEvent->y() - contentsY(),
                                pPaintEvent->width(), pPaintEvent->height());

            return true;
        }

#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBoxDefs::VHWACommandProcessType:
        {
            m_pFrameBuffer->doProcessVHWACommand(pEvent);
            return true;
        }
#endif /* VBOX_WITH_VIDEOHWACCEL */

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            if (m_bIsHostkeyPressed && pEvent->type() == QEvent::KeyPress)
            {
                /* Passing F1-F12 keys to the guest: */
                if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F12)
                {
                    QVector <LONG> combo(6);
                    combo[0] = 0x1d; /* Ctrl down */
                    combo[1] = 0x38; /* Alt  down */
                    combo[4] = 0xb8; /* Alt  up   */
                    combo[5] = 0x9d; /* Ctrl up   */
                    if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F10)
                    {
                        combo[2] = 0x3b + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 down */
                        combo[3] = 0xbb + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 up   */
                    }
                    /* some scan slice */
                    else if (pKeyEvent->key() >= Qt::Key_F11 && pKeyEvent->key() <= Qt::Key_F12)
                    {
                        combo[2] = 0x57 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 down */
                        combo[3] = 0xd7 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 up   */
                    }
                    else
                        Assert(0);

                    CKeyboard keyboard = session().GetConsole().GetKeyboard();
                    keyboard.PutScancodes(combo);
                }

                /* Process hot keys not processed in keyEvent() (as in case of non-alphanumeric keys): */
                machineWindowWrapper()->machineLogic()->actionsPool()->processHotKey(QKeySequence(pKeyEvent->key()));
            }
            else if (!m_bIsHostkeyPressed && pEvent->type() == QEvent::KeyRelease)
            {
                /* Show a possible warning on key release which seems to be more expected by the end user: */
                if (uisession()->isPaused())
                {
                    /* Iif the reminder is disabled we pass the event to Qt to enable normal
                     * keyboard functionality (for example, menu access with Alt+Letter): */
                    if (!vboxProblem().remindAboutPausedVMInput())
                        break;
                }
            }

            pKeyEvent->accept();
            return true;
        }

#ifdef Q_WS_MAC
        /* posted OnShowWindow */
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
#ifdef Q_WS_WIN
            /* Install/uninstall low-level kbd hook on every activation/deactivation to:
             * a) avoid excess hook calls when we're not active and
             * b) be always in front of any other possible hooks */
            case QEvent::WindowActivate:
            {
                gView = this;
                gKbdHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandle(NULL), 0);
                AssertMsg(gKbdHook, ("SetWindowsHookEx(): err=%d", GetLastError()));
                break;
            }
            case QEvent::WindowDeactivate:
            {
                if (gKbdHook)
                {
                    UnhookWindowsHookEx(gKbdHook);
                    gKbdHook = NULL;
                    if (gView == this)
                        gView = 0;
                }
                break;
            }
#endif /* Q_WS_WIN */
#ifdef Q_WS_MAC
            /* Install/remove the keyboard event handler: */
            case QEvent::WindowActivate:
                darwinGrabKeyboardEvents(true);
                break;
            case QEvent::WindowDeactivate:
                darwinGrabKeyboardEvents(false);
                break;
#endif /* Q_WS_MAC */
            default:
                break;
        }
    }

    return QAbstractScrollArea::eventFilter (pWatched, pEvent);
}

void UIMachineView::focusEvent(bool fHasFocus, bool fReleaseHostKey /* = true */)
{
    if (fHasFocus)
    {
#ifdef Q_WS_WIN
        if (!uisession()->isAutoCaptureDisabled() && m_globalSettings.autoCapture() && GetAncestor(winId(), GA_ROOT) == GetForegroundWindow())
#else /* Q_WS_WIN */
        if (!uisession()->isAutoCaptureDisabled() && m_globalSettings.autoCapture())
#endif /* !Q_WS_WIN */
        {
            captureKbd(true);
        }

        /* Reset the single-time disable capture flag: */
        if (uisession()->isAutoCaptureDisabled())
            uisession()->setAutoCaptureDisabled(false);
    }
    else
    {
        captureKbd(false, false);
        releaseAllPressedKeys(fReleaseHostKey);
    }
}

bool UIMachineView::keyEvent(int iKey, uint8_t uScan, int fFlags, wchar_t *pUniKey /* = NULL */)
{
    const bool isHostKey = iKey == m_globalSettings.hostKey();

    LONG buf[16];
    LONG *codes = buf;
    uint count = 0;
    uint8_t whatPressed = 0;

    if (!isHostKey && !m_bIsHostkeyPressed)
    {
        if (fFlags & KeyPrint)
        {
            static LONG PrintMake[] = { 0xE0, 0x2A, 0xE0, 0x37 };
            static LONG PrintBreak[] = { 0xE0, 0xB7, 0xE0, 0xAA };
            if (fFlags & KeyPressed)
            {
                codes = PrintMake;
                count = SIZEOF_ARRAY(PrintMake);
            }
            else
            {
                codes = PrintBreak;
                count = SIZEOF_ARRAY(PrintBreak);
            }
        }
        else if (fFlags & KeyPause)
        {
            if (fFlags & KeyPressed)
            {
                static LONG Pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
                codes = Pause;
                count = SIZEOF_ARRAY(Pause);
            }
            else
            {
                /* Pause shall not produce a break code */
                return true;
            }
        }
        else
        {
            if (fFlags & KeyPressed)
            {
                /* Check if the guest has the same view on the modifier keys (NumLock,
                 * CapsLock, ScrollLock) as the X server. If not, send KeyPress events
                 * to synchronize the state. */
                fixModifierState(codes, &count);
            }

            /* Check if it's C-A-D and GUI/PassCAD is not true */
            if (!m_fPassCAD &&
                uScan == 0x53 /* Del */ &&
                ((m_pressedKeys[0x38] & IsKeyPressed) /* Alt */ ||
                 (m_pressedKeys[0x38] & IsExtKeyPressed)) &&
                ((m_pressedKeys[0x1d] & IsKeyPressed) /* Ctrl */ ||
                 (m_pressedKeys[0x1d] & IsExtKeyPressed)))
            {
                /* Use the C-A-D combination as a last resort to get the
                 * keyboard and mouse back to the host when the user forgets
                 * the Host Key. Note that it's always possible to send C-A-D
                 * to the guest using the Host+Del combination. BTW, it would
                 * be preferrable to completely ignore C-A-D in guests, but
                 * that's not possible because we cannot predict what other
                 * keys will be pressed next when one of C, A, D is held. */
                if (uisession()->isRunning() && m_bIsKeyboardCaptured)
                {
                    captureKbd(false);
                    if (!(uisession()->isMouseSupportsAbsolute() && uisession()->isMouseIntegrated()))
                        machineLogic()->mouseHandler()->captureMouse(screenId());
                }

                return true;
            }

            /* Process the scancode and update the table of pressed keys: */
            whatPressed = IsKeyPressed;

            if (fFlags & KeyExtended)
            {
                codes[count++] = 0xE0;
                whatPressed = IsExtKeyPressed;
            }

            if (fFlags & KeyPressed)
            {
                codes[count++] = uScan;
                m_pressedKeys[uScan] |= whatPressed;
            }
            else
            {
                /* If we haven't got this key's press message, we ignore its release: */
                if (!(m_pressedKeys[uScan] & whatPressed))
                    return true;
                codes[count++] = uScan | 0x80;
                m_pressedKeys[uScan] &= ~whatPressed;
            }

            if (m_bIsKeyboardCaptured)
                m_pressedKeys[uScan] |= IsKbdCaptured;
            else
                m_pressedKeys[uScan] &= ~IsKbdCaptured;
        }
    }
    else
    {
        /* Currently this is used in winLowKeyboardEvent() only: */
        m_bIsHostkeyInCapture = m_bIsKeyboardCaptured;
    }

    bool emitSignal = false;
    int hotkey = 0;

    /* Process the host key: */
    if (fFlags & KeyPressed)
    {
        if (isHostKey)
        {
            if (!m_bIsHostkeyPressed)
            {
                m_bIsHostkeyPressed = m_bIsHostkeyAlone = true;
                if (uisession()->isRunning())
                    saveKeyStates();
                emitSignal = true;
            }
        }
        else
        {
            if (m_bIsHostkeyPressed)
            {
                if (m_bIsHostkeyAlone)
                {
                    hotkey = iKey;
                    m_bIsHostkeyAlone = false;
                }
            }
        }
    }
    else
    {
        if (isHostKey)
        {
            if (m_bIsHostkeyPressed)
            {
                m_bIsHostkeyPressed = false;

                if (m_bIsHostkeyAlone)
                {
                    if (uisession()->isPaused())
                    {
                        vboxProblem().remindAboutPausedVMInput();
                    }
                    else if (uisession()->isRunning())
                    {
                        bool captured = m_bIsKeyboardCaptured;
                        bool ok = true;
                        if (!captured)
                        {
                            /* temporarily disable auto capture that will take
                             * place after this dialog is dismissed because
                             * the capture state is to be defined by the
                             * dialog result itself */
                            uisession()->setAutoCaptureDisabled(true);
                            bool autoConfirmed = false;
                            ok = vboxProblem().confirmInputCapture (&autoConfirmed);
                            if (autoConfirmed)
                                uisession()->setAutoCaptureDisabled(false);
                            /* otherwise, the disable flag will be reset in
                             * the next console view's foucs in event (since
                             * may happen asynchronously on some platforms,
                             * after we return from this code) */
                        }

                        if (ok)
                        {
                            captureKbd (!captured, false);
                            if (!(uisession()->isMouseSupportsAbsolute() && uisession()->isMouseIntegrated()))
                            {
#ifdef Q_WS_X11
                                /* make sure that pending FocusOut events from the
                                 * previous message box are handled, otherwise the
                                 * mouse is immediately ungrabbed. */
                                qApp->processEvents();
#endif /* Q_WS_X11 */
                                if (m_bIsKeyboardCaptured)
                                    machineLogic()->mouseHandler()->captureMouse(screenId());
                                else
                                    machineLogic()->mouseHandler()->releaseMouse();
                            }
                        }
                    }
                }

                if (uisession()->isRunning())
                    sendChangedKeyStates();

                emitSignal = true;
            }
        }
        else
        {
            if (m_bIsHostkeyPressed)
                m_bIsHostkeyAlone = false;
        }
    }

    /* emit the keyboard state change signal */
    if (emitSignal)
        emitKeyboardStateChanged();

    /* Process Host+<key> shortcuts. currently, <key> is limited to
     * alphanumeric chars. Other Host+<key> combinations are handled in
     * event(). */
    if (hotkey)
    {
        bool processed = false;
#if defined (Q_WS_WIN)
        NOREF(pUniKey);
        int n = GetKeyboardLayoutList(0, NULL);
        Assert (n);
        HKL *list = new HKL[n];
        GetKeyboardLayoutList(n, list);
        for (int i = 0; i < n && !processed; i++)
        {
            wchar_t ch;
            static BYTE keys[256] = {0};
            if (!ToUnicodeEx(hotkey, 0, keys, &ch, 1, 0, list[i]) == 1)
                ch = 0;
            if (ch)
                processed = machineWindowWrapper()->machineLogic()->actionsPool()->processHotKey(QKeySequence((Qt::UNICODE_ACCEL + QChar(ch).toUpper().unicode())));
        }
        delete[] list;
#elif defined (Q_WS_X11)
        NOREF(pUniKey);
        Display *display = QX11Info::display();
        int keysyms_per_keycode = getKeysymsPerKeycode();
        KeyCode kc = XKeysymToKeycode (display, iKey);
        for (int i = 0; i < keysyms_per_keycode && !processed; i += 2)
        {
            KeySym ks = XKeycodeToKeysym(display, kc, i);
            char ch = 0;
            if (!XkbTranslateKeySym(display, &ks, 0, &ch, 1, NULL) == 1)
                ch = 0;
            if (ch)
                QChar c = QString::fromLocal8Bit(&ch, 1)[0];
        }
#elif defined (Q_WS_MAC)
        if (pUniKey && pUniKey[0] && !pUniKey[1])
            processed = machineWindowWrapper()->machineLogic()->actionsPool()->processHotKey(QKeySequence(Qt::UNICODE_ACCEL + QChar(pUniKey[0]).toUpper().unicode()));

        /* Don't consider the hot key as pressed since the guest never saw
         * it. (probably a generic thing) */
        m_pressedKeys[uScan] &= ~whatPressed;
#endif

        /* Grab the key from Qt if processed, or pass it to Qt otherwise
         * in order to process non-alphanumeric keys in event(), after they are
         * converted to Qt virtual keys. */
        return processed;
    }

    /* No more to do, if the host key is in action or the VM is paused: */
    if (m_bIsHostkeyPressed || isHostKey || uisession()->isPaused())
    {
        /* Grab the key from Qt and from VM if it's a host key,
         * otherwise just pass it to Qt */
        return isHostKey;
    }

    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    Assert(!keyboard.isNull());

#ifdef Q_WS_WIN
    /* send pending WM_PAINT events */
    ::UpdateWindow(viewport()->winId());
#endif /* Q_WS_WIN */

    std::vector <LONG> scancodes(codes, &codes[count]);
    keyboard.PutScancodes(QVector<LONG>::fromStdVector(scancodes));

    /* Grab the key from Qt: */
    return true;
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
        if (m_pFrameBuffer)
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
        pnt.drawPixmap(r.x(), r.y(), m_pauseShot, r.x() + contentsX(), r.y() + contentsY(), r.width(), r.height());
        /* Restore the attribute to its previous state: */
        viewport()->setAttribute(Qt::WA_PaintOnScreen, paintOnScreen);
#ifdef Q_WS_MAC
        updateDockIcon();
#endif /* Q_WS_MAC */
    }
}

#if defined(Q_WS_WIN)

LRESULT CALLBACK UIMachineView::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (gView && nCode == HC_ACTION && gView->winLowKeyboardEvent(wParam, *(KBDLLHOOKSTRUCT *)lParam))
        return 1;

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool UIMachineView::winLowKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event)
{
    /* Sometimes it happens that Win inserts additional events on some key
     * press/release. For example, it prepends ALT_GR in German layout with
     * the VK_LCONTROL vkey with curious 0x21D scan code (seems to be necessary
     * to specially treat ALT_GR to enter additional chars to regular apps).
     * These events are definitely unwanted in VM, so filter them out. */
    /* Note (michael): it also sometimes sends the VK_CAPITAL vkey with scan
     * code 0x23a. If this is not passed through then it is impossible to
     * cancel CapsLock on a French keyboard.  I didn't find any other examples
     * of these strange events.  Let's hope we are not missing anything else
     * of importance! */
    if (hasFocus() && (event.scanCode & ~0xFF))
    {
        if (event.vkCode == VK_CAPITAL)
            return false;
        else
            return true;
    }

    if (!m_bIsKeyboardCaptured)
        return false;

    /* it's possible that a key has been pressed while the keyboard was not
     * captured, but is being released under the capture. Detect this situation
     * and return false to let Windows process the message normally and update
     * its key state table (to avoid the stuck key effect). */
    uint8_t what_pressed = (event.flags & 0x01) && (event.vkCode != VK_RSHIFT) ? IsExtKeyPressed : IsKeyPressed;
    if ((event.flags & 0x80) /* released */ &&
        ((event.vkCode == m_globalSettings.hostKey() && !m_bIsHostkeyInCapture) ||
         (m_pressedKeys[event.scanCode] & (IsKbdCaptured | what_pressed)) == what_pressed))
        return false;

    MSG message;
    message.hwnd = winId();
    message.message = msg;
    message.wParam = event.vkCode;
    message.lParam = 1 | (event.scanCode & 0xFF) << 16 | (event.flags & 0xFF) << 24;

    /* Windows sets here the extended bit when the Right Shift key is pressed,
     * which is totally wrong. Undo it. */
    if (event.vkCode == VK_RSHIFT)
        message.lParam &= ~0x1000000;

    /* we suppose here that this hook is always called on the main GUI thread */
    long dummyResult;
    return winEvent(&message, &dummyResult);
}

bool UIMachineView::winEvent(MSG *aMsg, long* /* aResult */)
{
    if (!(aMsg->message == WM_KEYDOWN || aMsg->message == WM_SYSKEYDOWN ||
          aMsg->message == WM_KEYUP || aMsg->message == WM_SYSKEYUP))
        return false;

    /* Check for the special flag possibly set at the end of this function */
    if (aMsg->lParam & (0x1 << 25))
    {
        aMsg->lParam &= ~(0x1 << 25);
        return false;
    }

    int scan = (aMsg->lParam >> 16) & 0x7F;
    /* scancodes 0x80 and 0x00 are ignored */
    if (!scan)
        return true;

    int vkey = aMsg->wParam;

    /* When one of the SHIFT keys is held and one of the cursor movement
     * keys is pressed, Windows duplicates SHIFT press/release messages,
     * but with the virtual key code set to 0xFF. These virtual keys are also
     * sent in some other situations (Pause, PrtScn, etc.). Ignore such
     * messages. */
    if (vkey == 0xFF)
        return true;

    int flags = 0;
    if (aMsg->lParam & 0x1000000)
        flags |= KeyExtended;
    if (!(aMsg->lParam & 0x80000000))
        flags |= KeyPressed;

    switch (vkey)
    {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        {
            /* overcome stupid Win32 modifier key generalization */
            int keyscan = scan;
            if (flags & KeyExtended)
                keyscan |= 0xE000;
            switch (keyscan)
            {
                case 0x002A: vkey = VK_LSHIFT; break;
                case 0x0036: vkey = VK_RSHIFT; break;
                case 0x001D: vkey = VK_LCONTROL; break;
                case 0xE01D: vkey = VK_RCONTROL; break;
                case 0x0038: vkey = VK_LMENU; break;
                case 0xE038: vkey = VK_RMENU; break;
            }
            break;
        }
        case VK_NUMLOCK:
            /* Win32 sets the extended bit for the NumLock key. Reset it. */
            flags &= ~KeyExtended;
            break;
        case VK_SNAPSHOT:
            flags |= KeyPrint;
            break;
        case VK_PAUSE:
            flags |= KeyPause;
            break;
    }

    bool result = keyEvent(vkey, scan, flags);
    if (!result && m_bIsKeyboardCaptured)
    {
        /* keyEvent() returned that it didn't process the message, but since the
         * keyboard is captured, we don't want to pass it to Windows. We just want
         * to let Qt process the message (to handle non-alphanumeric <HOST>+key
         * shortcuts for example). So send it direcltly to the window with the
         * special flag in the reserved area of lParam (to avoid recursion). */
        ::SendMessage(aMsg->hwnd, aMsg->message,
                      aMsg->wParam, aMsg->lParam | (0x1 << 25));
        return true;
    }

    /* These special keys have to be handled by Windows as well to update the
     * internal modifier state and to enable/disable the keyboard LED */
    if (vkey == VK_NUMLOCK || vkey == VK_CAPITAL || vkey == VK_LSHIFT || vkey == VK_RSHIFT)
        return false;

    return result;
}

#elif defined(Q_WS_X11)

static Bool VBoxConsoleViewCompEvent(Display *, XEvent *pEvent, XPointer pvArg)
{
    XEvent *pKeyEvent = (XEvent*)pvArg;
    if ((pEvent->type == XKeyPress) && (pEvent->xkey.keycode == pKeyEvent->xkey.keycode))
        return True;
    else
        return False;
}

bool UIMachineView::x11Event(XEvent *pEvent)
{
    switch (pEvent->type)
    {
        /* We have to handle XFocusOut right here as this event is not passed
         * to UIMachineView::event(). Handling this event is important for
         * releasing the keyboard before the screen saver gets active.
         *
         * See public ticket #3894: Apparently this makes problems with newer
         * versions of Qt and this hack is probably not necessary anymore.
         * So disable it for Qt >= 4.5.0. */
        case XFocusOut:
        case XFocusIn:
            if (uisession()->isRunning())
            {
                if (VBoxGlobal::qtRTVersion() < ((4 << 16) | (5 << 8) | 0))
                {
                    focusEvent(pEvent->type == XFocusIn);
                    if (pEvent->type == XFocusOut)
                        machineLogic()->mouseHandler()->releaseMouse();
                }
            }
            return false;
        case XKeyPress:
        case XKeyRelease:
            break;
        default:
            return false; /* pass the event to Qt */
    }

    /* Translate the keycode to a PC scan code. */
    unsigned scan = handleXKeyEvent(pEvent);

    /* scancodes 0x00 (no valid translation) and 0x80 are ignored */
    if (!scan & 0x7F)
        return true;

    /* Fix for http://www.virtualbox.org/ticket/1296:
     * when X11 sends events for repeated keys, it always inserts an
     * XKeyRelease before the XKeyPress. */
    XEvent returnEvent;
    if ((pEvent->type == XKeyRelease) && (XCheckIfEvent(pEvent->xkey.display, &returnEvent,
        VBoxConsoleViewCompEvent, (XPointer)pEvent) == True))
    {
        XPutBackEvent(pEvent->xkey.display, &returnEvent);
        /* Discard it, don't pass it to Qt. */
        return true;
    }

    KeySym ks = ::XKeycodeToKeysym(pEvent->xkey.display, pEvent->xkey.keycode, 0);

    int flags = 0;
    if (scan >> 8)
        flags |= KeyExtended;
    if (pEvent->type == XKeyPress)
        flags |= KeyPressed;

    /* Remove the extended flag */
    scan &= 0x7F;

    switch (ks)
    {
        case XK_Print:
            flags |= KeyPrint;
            break;
        case XK_Pause:
            flags |= KeyPause;
            break;
    }

    return keyEvent(ks, scan, flags);
}

#elif defined(Q_WS_MAC)

void UIMachineView::darwinGrabKeyboardEvents(bool fGrab)
{
    m_fKeyboardGrabbed = fGrab;
    if (fGrab)
    {
        /* Disable mouse and keyboard event compression/delaying to make sure we *really* get all of the events. */
        ::CGSetLocalEventsSuppressionInterval(0.0);
        machineLogic()->mouseHandler()->setMouseCoalescingEnabled(false);

        /* Register the event callback/hook and grab the keyboard. */
        UICocoaApplication::instance()->registerForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                UIMachineView::darwinEventHandlerProc, this);

        ::DarwinGrabKeyboard (false);
    }
    else
    {
        ::DarwinReleaseKeyboard();
        UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                  UIMachineView::darwinEventHandlerProc, this);
    }
}

bool UIMachineView::darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    UIMachineView *view = (UIMachineView*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 eventClass = ::GetEventClass(inEvent);

    /* Check if this is an application key combo. In that case we will not pass
     * the event to the guest, but let the host process it. */
    if (::darwinIsApplicationCommand(pvCocoaEvent))
        return false;

    /* All keyboard class events needs to be handled. */
    if (eventClass == kEventClassKeyboard)
    {
        if (view->darwinKeyboardEvent (pvCocoaEvent, inEvent))
            return true;
    }
    /* Pass the event along. */
    return false;
}

bool UIMachineView::darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent)
{
    bool ret = false;
    UInt32 EventKind = ::GetEventKind(inEvent);
    if (EventKind != kEventRawKeyModifiersChanged)
    {
        /* convert keycode to set 1 scan code. */
        UInt32 keyCode = ~0U;
        ::GetEventParameter(inEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof (keyCode), NULL, &keyCode);
        unsigned scanCode = ::DarwinKeycodeToSet1Scancode(keyCode);
        if (scanCode)
        {
            /* calc flags. */
            int flags = 0;
            if (EventKind != kEventRawKeyUp)
                flags |= KeyPressed;
            if (scanCode & VBOXKEY_EXTENDED)
                flags |= KeyExtended;
            /** @todo KeyPause, KeyPrint. */
            scanCode &= VBOXKEY_SCANCODE_MASK;

            /* get the unicode string (if present). */
            AssertCompileSize(wchar_t, 2);
            AssertCompileSize(UniChar, 2);
            ByteCount cbWritten = 0;
            wchar_t ucs[8];
            if (::GetEventParameter(inEvent, kEventParamKeyUnicodes, typeUnicodeText, NULL,
                                    sizeof(ucs), &cbWritten, &ucs[0]) != 0)
                cbWritten = 0;
            ucs[cbWritten / sizeof(wchar_t)] = 0; /* The api doesn't terminate it. */

            ret = keyEvent(keyCode, scanCode, flags, ucs[0] ? ucs : NULL);
        }
    }
    else
    {
        /* May contain multiple modifier changes, kind of annoying. */
        UInt32 newMask = 0;
        ::GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                            sizeof(newMask), NULL, &newMask);
        newMask = ::DarwinAdjustModifierMask(newMask, pvCocoaEvent);
        UInt32 changed = newMask ^ m_darwinKeyModifiers;
        if (changed)
        {
            for (UInt32 bit = 0; bit < 32; bit++)
            {
                if (!(changed & (1 << bit)))
                    continue;
                unsigned scanCode = ::DarwinModifierMaskToSet1Scancode(1 << bit);
                if (!scanCode)
                    continue;
                unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode(1 << bit);
                Assert(keyCode);

                if (!(scanCode & VBOXKEY_LOCK))
                {
                    unsigned flags = (newMask & (1 << bit)) ? KeyPressed : 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    ret |= keyEvent(keyCode, scanCode & 0xff, flags);
                }
                else
                {
                    unsigned flags = 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    keyEvent(keyCode, scanCode, flags | KeyPressed);
                    keyEvent(keyCode, scanCode, flags);
                }
            }
        }

        m_darwinKeyModifiers = newMask;

        /* Always return true here because we'll otherwise getting a Qt event
           we don't want and that will only cause the Pause warning to pop up. */
        ret = true;
    }

    return ret;
}

#endif

