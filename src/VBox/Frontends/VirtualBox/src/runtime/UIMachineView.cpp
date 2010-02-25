/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class implementation
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
#include "UIFrameBufferQuartz2D.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"
#include "UIMachineViewNormal.h"

#ifdef Q_WS_PM
# include "QIHotKeyEdit.h"
#endif

#ifdef Q_WS_WIN
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <windows.h>
#endif

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
# ifndef VBOX_WITHOUT_XCURSOR
#  include <X11/Xcursor/Xcursor.h>
# endif
#endif

#if defined (Q_WS_MAC)
# include "DockIconPreview.h"
# include "DarwinKeyboard.h"
# ifdef QT_MAC_USE_COCOA
#  include "darwin/VBoxCocoaApplication.h"
# else /* QT_MAC_USE_COCOA */
#  include <Carbon/Carbon.h>
# endif /* !QT_MAC_USE_COCOA */
# include <VBox/err.h>
#endif /* defined (Q_WS_MAC) */

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
                                     , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                     , bool bAccelerate2DVideo
#endif
                                     , UIVisualStateType visualStateType)
{
    UIMachineView *view = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            view = new UIMachineViewNormal(  pMachineWindow
                                           , renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                          );
            break;
        case UIVisualStateType_Fullscreen:
            view = new UIMachineViewNormal(  pMachineWindow
                                           , renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                          );
            break;
        case UIVisualStateType_Seamless:
            view = new UIMachineViewNormal(  pMachineWindow
                                           , renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                          );
            break;
        default:
            break;
    }
    return view;
}

int UIMachineView::keyboardState() const
{
    return (m_bIsKeyboardCaptured ? UIViewStateType_KeyboardCaptured : 0) |
           (m_bIsHostkeyPressed ? UIViewStateType_HostKeyPressed : 0);
}

int UIMachineView::mouseState() const
{
    return (m_bIsMouseCaptured ? UIMouseStateType_MouseCaptured : 0) |
           (m_bIsMouseAbsolute ? UIMouseStateType_MouseAbsolute : 0) |
           (m_bIsMouseIntegrated ? 0 : UIMouseStateType_MouseAbsoluteDisabled);
}

void UIMachineView::setMouseIntegrationEnabled(bool bEnabled)
{
    if (m_bIsMouseIntegrated == bEnabled)
        return;

    if (m_bIsMouseAbsolute)
        captureMouse(!bEnabled, false);

    /* Hiding host cursor in case we are entering mouse integration
     * mode until it's shape is set to the guest cursor shape in
     * OnMousePointerShapeChange event handler.
     *
     * This is necessary to avoid double-cursor issues where both the
     * guest and the host cursors are displayed in one place, one above the
     * other.
     *
     * This is a workaround because the correct decision would be to notify
     * the Guest Additions about we are entering the mouse integration
     * mode. The GuestOS should hide it's cursor to allow using of
     * host cursor for the guest's manipulation.
     *
     * This notification is not always possible though, as not all guests
     * support switching to a hardware pointer on demand. */
    if (bEnabled)
        viewport()->setCursor(QCursor(Qt::BlankCursor));

    m_bIsMouseIntegrated = bEnabled;

    emitMouseStateChanged();
}

UIMachineView::UIMachineView(  UIMachineWindow *pMachineWindow
                             , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                             , bool bAccelerate2DVideo
#endif
                            )
    : QAbstractScrollArea(pMachineWindow->machineWindow())
    , m_pMachineWindow(pMachineWindow)
    , m_console(pMachineWindow->machineLogic()->uisession()->session().GetConsole())
    , m_mode(renderMode)
    , m_globalSettings(vboxGlobal().settings())
    , m_machineState(KMachineState_Null)
    , m_pFrameBuffer(0)
#if defined(Q_WS_WIN)
    , m_alphaCursor(0)
#endif
    , m_iLastMouseWheelDelta(0)
    , m_uNumLockAdaptionCnt(2)
    , m_uCapsLockAdaptionCnt(2)
    , m_bIsAutoCaptureDisabled(false)
    , m_bIsKeyboardCaptured(false)
    , m_bIsMouseCaptured(false)
    , m_bIsMouseAbsolute(false)
    , m_bIsMouseIntegrated(true)
    , m_fIsHideHostPointer(true)
    , m_bIsHostkeyPressed(false)
    , m_bIsHostkeyAlone (false)
    , m_bIsHostkeyInCapture(false)
    , m_bIsGuestSupportsGraphics(false)
    , m_bIsMachineWindowResizeIgnored(true)
    , m_bIsFrameBufferResizeIgnored(false)
    , m_bIsGuestResizeIgnored(false)
    , m_fNumLock(false)
    , m_fCapsLock(false)
    , m_fScrollLock(false)
    , m_fPassCAD(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_fAccelerate2DVideo(bAccelerate2DVideo)
#endif
#if 0 // TODO: Do we need this?
    , mDoResize(false)
#endif
#if defined(Q_WS_MAC)
# ifndef QT_MAC_USE_COCOA
    , m_darwinEventHandlerRef(NULL)
# endif /* !QT_MAC_USE_COCOA */
    , m_darwinKeyModifiers(0)
    , m_fKeyboardGrabbed (false)
    , mDockIconEnabled(true)
#endif
    , m_desktopGeometryType(DesktopGeo_Invalid)
      /* Don't show a hardware pointer until we have one to show */
{
}

UIMachineView::~UIMachineView()
{
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
#else
    return QSize(m_pFrameBuffer->width() + frameWidth() * 2, m_pFrameBuffer->height() + frameWidth() * 2);
#endif
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

QRect UIMachineView::desktopGeometry() const
{
    QRect geometry;
    switch (m_desktopGeometryType)
    {
        case DesktopGeo_Fixed:
        case DesktopGeo_Automatic:
            geometry = QRect(0, 0,
                             qMax(m_desktopGeometry.width(), m_storedConsoleSize.width()),
                             qMax(m_desktopGeometry.height(), m_storedConsoleSize.height()));
            break;
        case DesktopGeo_Any:
            geometry = QRect(0, 0, 0, 0);
            break;
        default:
            AssertMsgFailed(("Bad geometry type %d!\n", m_desktopGeometryType));
    }
    return geometry;
}

void UIMachineView::calculateDesktopGeometry()
{
    /* This method should not get called until we have initially set up the m_desktopGeometryType: */
    Assert((m_desktopGeometryType != DesktopGeo_Invalid));
    /* If we are not doing automatic geometry calculation then there is nothing to do: */
    if (DesktopGeo_Automatic == m_desktopGeometryType)
    {
        /* Available geometry of the desktop.  If the desktop is a single
         * screen, this will exclude space taken up by desktop taskbars
         * and things, but this is unfortunately not true for the more
         * complex case of a desktop spanning multiple screens: */
        QRect desktop = availableGeometry();
        /* The area taken up by the machine window on the desktop,
         * including window frame, title and menu bar and whatnot: */
        QRect frame = machineWindowWrapper()->machineWindow()->frameGeometry();
        /* The area taken up by the machine view, so excluding all decorations: */
        QRect window = geometry();
        /* To work out how big we can make the console window while still
         * fitting on the desktop, we calculate desktop - frame + window.
         * This works because the difference between frame and window
         * (or at least its width and height) is a constant. */
        m_desktopGeometry = QRect(0, 0, desktop.width() - frame.width() + window.width(),
                                        desktop.height() - frame.height() + window.height());
    }
}

void UIMachineView::setDesktopGeometry(DesktopGeo geometry, int aWidth, int aHeight)
{
    switch (geometry)
    {
        case DesktopGeo_Fixed:
            m_desktopGeometryType = DesktopGeo_Fixed;
            if (aWidth != 0 && aHeight != 0)
                m_desktopGeometry = QRect(0, 0, aWidth, aHeight);
            else
                m_desktopGeometry = QRect(0, 0, 0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Automatic:
            m_desktopGeometryType = DesktopGeo_Automatic;
            m_desktopGeometry = QRect(0, 0, 0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Any:
            m_desktopGeometryType = DesktopGeo_Any;
            m_desktopGeometry = QRect(0, 0, 0, 0);
            break;
        default:
            AssertMsgFailed(("Invalid desktop geometry type %d\n", geometry));
            m_desktopGeometryType = DesktopGeo_Invalid;
    }
}

void UIMachineView::storeConsoleSize(int aWidth, int aHeight)
{
    LogFlowThisFunc(("aWidth=%d, aHeight=%d\n", aWidth, aHeight));
    m_storedConsoleSize = QRect(0, 0, aWidth, aHeight);
}

QRect UIMachineView::availableGeometry()
{
    return machineWindowWrapper()->machineWindow()->isFullScreen() ?
           QApplication::desktop()->screenGeometry(this) :
           QApplication::desktop()->availableGeometry(this);
}

void UIMachineView::prepareFrameBuffer()
{
    CDisplay display = m_console.GetDisplay();
    Assert(!display.isNull());
    m_pFrameBuffer = NULL;

    switch (mode())
    {
#if defined (VBOX_GUI_USE_QIMAGE)
        case VBoxDefs::QImageMode:
#if 0 // TODO: Enable QImage + Video Acceleration!
# ifdef VBOX_WITH_VIDEOHWACCEL
            m_pFrameBuffer = m_fAccelerate2DVideo ? new VBoxOverlayFrameBuffer<UIFrameBufferQImage>(this, &machineWindowWrapper()->session()) : new UIFrameBufferQImage(this);
# else
            m_pFrameBuffer = new UIFrameBufferQImage(this);
# endif
#endif
            m_pFrameBuffer = new UIFrameBufferQImage(this);
            break;
#endif
#if 0 // TODO: Enable OpenGL frame buffer!
#if defined (VBOX_GUI_USE_QGL)
        case VBoxDefs::QGLMode:
            m_pFrameBuffer = new UIQGLFrameBuffer(this);
            break;
        case VBoxDefs::QGLOverlayMode:
            m_pFrameBuffer = new UIQGLOverlayFrameBuffer(this);
            break;
#endif
#endif
#if 0 // TODO: Enable SDL frame buffer!
#if defined (VBOX_GUI_USE_SDL)
        case VBoxDefs::SDLMode:
            /* Indicate that we are doing all drawing stuff ourself: */
            pViewport->setAttribute(Qt::WA_PaintOnScreen);
# ifdef Q_WS_X11
            /* This is somehow necessary to prevent strange X11 warnings on i386 and segfaults on x86_64: */
            XFlush(QX11Info::display());
# endif
# if defined(VBOX_WITH_VIDEOHWACCEL) && defined(DEBUG_misha) /* not tested yet */
            m_pFrameBuffer = m_fAccelerate2DVideo ? new VBoxOverlayFrameBuffer<UISDLFrameBuffer> (this, &machineWindowWrapper()->session()) : new UISDLFrameBuffer(this);
# else
            m_pFrameBuffer = new UISDLFrameBuffer(this);
# endif
            /* Disable scrollbars because we cannot correctly draw in a scrolled window using SDL: */
            horizontalScrollBar()->setEnabled(false);
            verticalScrollBar()->setEnabled(false);
            break;
#endif
#endif
#if 0 // TODO: Enable DDraw frame buffer!
#if defined (VBOX_GUI_USE_DDRAW)
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
#endif
#endif
#if defined (VBOX_GUI_USE_QUARTZ2D)
        case VBoxDefs::Quartz2DMode:
            /* Indicate that we are doing all drawing stuff ourself: */
//            pViewport->setAttribute(Qt::WA_PaintOnScreen);
//# ifdef VBOX_WITH_VIDEOHWACCEL
//            m_pFrameBuffer = m_fAccelerate2DVideo ? new VBoxOverlayFrameBuffer<VBoxQuartz2DFrameBuffer>(this, &machineWindowWrapper()->session()) : new UIFrameBufferQuartz2D(this);
//# else
            m_pFrameBuffer = new UIFrameBufferQuartz2D(this);
//# endif
            break;
#endif
        default:
            AssertReleaseMsgFailed(("Render mode must be valid: %d\n", mode()));
            LogRel(("Invalid render mode: %d\n", mode()));
            qApp->exit(1);
            break;
    }
    if (m_pFrameBuffer)
    {
        m_pFrameBuffer->AddRef();
        display.SetFramebuffer(VBOX_VIDEO_PRIMARY_SCREEN, CFramebuffer(m_pFrameBuffer));
    }
}

void UIMachineView::prepareCommon()
{
    /* Prepare view frame: */
    //setFrameStyle(QFrame::NoFrame);

    /* Pressed keys: */
    ::memset(m_pressedKeys, 0, sizeof(m_pressedKeys));

    /* Prepare viewport: */
#ifdef VBOX_GUI_USE_QGL
    QWidget *pViewport;
    switch (mode())
    {
#if 0 // TODO: Create Open GL viewport!
        case VBoxDefs::QGLMode:
            pViewport = new VBoxGLWidget(this, this, NULL);
            break;
#endif
        default:
            pViewport = new VBoxViewport(this);
    }
#else
    VBoxViewport *pViewport = new VBoxViewport(this);
#endif
    setViewport(pViewport);

    /* Setup palette: */
    QPalette palette(viewport()->palette());
    palette.setColor(viewport()->backgroundRole(), Qt::black);
    viewport()->setPalette(palette);

    /* Setup size-policy: */
    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    setMaximumSize(sizeHint());

    /* Setup focus policy: */
    setFocusPolicy(Qt::WheelFocus);

#if defined Q_WS_WIN
    gView = this;
#endif

#if defined Q_WS_PM
    bool ok = VBoxHlpInstallKbdHook(0, winId(), UM_PREACCEL_CHAR);
    Assert(ok);
    NOREF(ok);
#endif

#if defined Q_WS_MAC
    /* Dock icon update connection */
    connect(&vboxGlobal(), SIGNAL(dockIconUpdateChanged(const VBoxChangeDockIconUpdateEvent &)),
            this, SLOT(sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &)));
    /* Presentation Mode connection */
    connect(&vboxGlobal(), SIGNAL(presentationModeChanged(const VBoxChangePresentationModeEvent &)),
            this, SLOT(sltChangePresentationMode(const VBoxChangePresentationModeEvent &)));

    /* Overlay logo for the dock icon */
    //mVirtualBoxLogo = ::darwinToCGImageRef("VirtualBox_cube_42px.png");
    QString osTypeId = m_console.GetGuest().GetOSTypeId();

    // TODO_NEW_CORE
//    mDockIconPreview = new VBoxDockIconPreview(machineWindowWrapper(), vboxGlobal().vmGuestOSTypeIcon (osTypeId));

# ifdef QT_MAC_USE_COCOA
    /** @todo Carbon -> Cocoa */
# else /* !QT_MAC_USE_COCOA */
    /* Install the event handler which will proceed external window handling */
    EventHandlerUPP eventHandler = ::NewEventHandlerUPP(::darwinOverlayWindowHandler);
    EventTypeSpec eventTypes[] =
    {
        { kEventClassVBox, kEventVBoxShowWindow },
        { kEventClassVBox, kEventVBoxHideWindow },
        { kEventClassVBox, kEventVBoxMoveWindow },
        { kEventClassVBox, kEventVBoxResizeWindow },
        { kEventClassVBox, kEventVBoxDisposeWindow },
        { kEventClassVBox, kEventVBoxUpdateDock }
    };

    mDarwinWindowOverlayHandlerRef = NULL;
    ::InstallApplicationEventHandler(eventHandler, RT_ELEMENTS (eventTypes), &eventTypes[0], this, &mDarwinWindowOverlayHandlerRef);
    ::DisposeEventHandlerUPP(eventHandler);
# endif /* !QT_MAC_USE_COCOA */
#endif
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
    /* UISession notifier: */
    UISession *sender = machineWindowWrapper()->machineLogic()->uisession();

    /* Machine state-change updater: */
    connect(sender, SIGNAL(sigStateChange(KMachineState)), this, SLOT(sltMachineStateChanged(KMachineState)));

    /* Guest additions state-change updater: */
    connect(sender, SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));

    /* Keyboard LEDs state-change updater: */
    connect(sender, SIGNAL(sigKeyboardLedsChange(bool, bool, bool)), this, SLOT(sltKeyboardLedsChanged(bool, bool, bool)));

    /* Mouse pointer shape state-change updater: */
    connect(sender, SIGNAL(sigMousePointerShapeChange(bool, bool, uint, uint, uint, uint, const uchar *)),
            this, SLOT(sltMousePointerShapeChanged(bool, bool, uint, uint, uint, uint, const uchar *)));

    /* Mouse capability state-change updater: */
    connect(sender, SIGNAL(sigMouseCapabilityChange(bool, bool)), this, SLOT(sltMouseCapabilityChanged(bool, bool)));
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
        connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltDesktopResized()));

#ifdef Q_WS_X11
        /* Initialize the X keyboard subsystem: */
        initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif
    }

    /* Exatra data settings: */
    {
        /* CAD settings: */
        QString passCAD = m_console.GetMachine().GetExtraData(VBoxDefs::GUI_PassCAD);
        if (!passCAD.isEmpty() && ((passCAD != "false") || (passCAD != "no")))
            m_fPassCAD = true;
    }

#if 0
    /* Dynamical property settings: */
    {
        /* Get loader: */
        UISession *loader = machineWindowWrapper()->machineLogic()->uisession();

        /* Load dynamical properties: */
        m_machineState = loader->property("MachineView/MachineState").value<KMachineState>();
        m_uNumLockAdaptionCnt = loader->property("MachineView/NumLockAdaptionCnt").toUInt();
        m_uCapsLockAdaptionCnt = loader->property("MachineView/CapsLockAdaptionCnt").toUInt();
        m_bIsAutoCaptureDisabled = loader->property("MachineView/IsAutoCaptureDisabled").toBool();
        m_bIsKeyboardCaptured = loader->property("MachineView/IsKeyboardCaptured").toBool();
        m_bIsMouseCaptured = loader->property("MachineView/IsMouseCaptured").toBool();
        m_bIsMouseAbsolute = loader->property("MachineView/IsMouseAbsolute").toBool();
        m_bIsMouseIntegrated = loader->property("MachineView/IsMouseIntegrated").toBool();
        m_fIsHideHostPointer = loader->property("MachineView/IsHideHostPointer").toBool();
        m_bIsHostkeyInCapture = loader->property("MachineView/IsHostkeyInCapture").toBool();
        m_bIsGuestSupportsGraphics = loader->property("MachineView/IsGuestSupportsGraphics").toBool();
        m_fNumLock = loader->property("MachineView/IsNumLock").toBool();
        m_fCapsLock = loader->property("MachineView/IsCapsLock").toBool();
        m_fScrollLock = loader->property("MachineView/IsScrollLock").toBool();

        /* Update related things: */
        updateMachineState();
        updateAdditionsState();
        updateMousePointerShape();
        updateMouseCapability();
    }
#endif
}

void UIMachineView::saveMachineViewSettings()
{
#if 0
    /* Dynamical property settings: */
    {
        /* Get saver: */
        UISession *saver = machineWindowWrapper()->machineLogic()->uisession();

        /* Save dynamical properties: */
        saver->setProperty("MachineView/MachineState", QVariant::fromValue(machineState()));
        saver->setProperty("MachineView/NumLockAdaptionCnt", m_uNumLockAdaptionCnt);
        saver->setProperty("MachineView/CapsLockAdaptionCnt", m_uCapsLockAdaptionCnt);
        saver->setProperty("MachineView/IsAutoCaptureDisabled", m_bIsAutoCaptureDisabled);
        saver->setProperty("MachineView/IsKeyboardCaptured", m_bIsKeyboardCaptured);
        saver->setProperty("MachineView/IsMouseCaptured", m_bIsMouseCaptured);
        saver->setProperty("MachineView/IsMouseAbsolute", m_bIsMouseAbsolute);
        saver->setProperty("MachineView/IsMouseIntegrated", m_bIsMouseIntegrated);
        saver->setProperty("MachineView/IsHideHostPointer", m_fIsHideHostPointer);
        saver->setProperty("MachineView/IsHostkeyInCapture", m_bIsHostkeyInCapture);
        saver->setProperty("MachineView/IsGuestSupportsGraphics", m_bIsGuestSupportsGraphics);
        saver->setProperty("MachineView/IsNumLock", m_fNumLock);
        saver->setProperty("MachineView/IsCapsLock", m_fCapsLock);
        saver->setProperty("MachineView/IsScrollLock", m_fScrollLock);
    }
#endif
}

void UIMachineView::cleanupCommon()
{
#if defined (Q_WS_PM)
    bool ok = VBoxHlpUninstallKbdHook(0, winId(), UM_PREACCEL_CHAR);
    Assert(ok);
    NOREF(ok);
#endif

#if defined (Q_WS_WIN)
    if (gKbdHook)
        UnhookWindowsHookEx(gKbdHook);
    gView = 0;
    if (m_alphaCursor)
        DestroyIcon(m_alphaCursor);
#endif

#if defined (Q_WS_MAC)
# if !defined (QT_MAC_USE_COCOA)
    if (mDarwinWindowOverlayHandlerRef)
    {
        ::RemoveEventHandler(mDarwinWindowOverlayHandlerRef);
        mDarwinWindowOverlayHandlerRef = NULL;
    }
# endif
    // TODO_NEW_CORE
//    delete mDockIconPreview;
    mDockIconPreview = NULL;
#endif
}

void UIMachineView::cleanupFrameBuffer()
{
    if (m_pFrameBuffer)
    {
        /* Detach framebuffer from Display: */
        CDisplay display = console().GetDisplay();
        display.SetFramebuffer(VBOX_VIDEO_PRIMARY_SCREEN, CFramebuffer(NULL));
        /* Release the reference: */
        m_pFrameBuffer->Release();
        m_pFrameBuffer = NULL;
    }
}

void UIMachineView::updateMachineState()
{
    switch (machineState())
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            if (mode() != VBoxDefs::TimerMode &&  m_pFrameBuffer &&
                (machineState() != KMachineState_TeleportingPausedVM || machineState() != KMachineState_Teleporting))
            {
                /* Take a screen snapshot. Note that TakeScreenShot() always needs a 32bpp image: */
                QImage shot = QImage(m_pFrameBuffer->width(), m_pFrameBuffer->height(), QImage::Format_RGB32);
                CDisplay dsp = m_console.GetDisplay();
                dsp.TakeScreenShot(shot.bits(), shot.width(), shot.height());
                /* TakeScreenShot() may fail if, e.g. the Paused notification was delivered
                 * after the machine execution was resumed. It's not fatal: */
                if (dsp.isOk())
                {
                    dimImage(shot);
                    m_pauseShot = QPixmap::fromImage(shot);
                    /* Fully repaint to pick up m_pauseShot: */
                    repaint();
                }
            }
        }
        case KMachineState_Stuck:
        {
            /* Reuse the focus event handler to uncapture everything: */
            if (hasFocus())
                focusEvent(false /* aHasFocus*/, false /* aReleaseHostKey */);
            break;
        }
        case KMachineState_Running:
        {
            if (machineState() == KMachineState_Paused || machineState() == KMachineState_TeleportingPausedVM)
            {
                if (mode() != VBoxDefs::TimerMode && m_pFrameBuffer)
                {
                    /* Reset the pixmap to free memory: */
                    m_pauseShot = QPixmap();
                    /* Ask for full guest display update (it will also update
                     * the viewport through IFramebuffer::NotifyUpdate): */
                    CDisplay dsp = m_console.GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
            /* Reuse the focus event handler to capture input: */
            if (hasFocus())
                focusEvent(true /* aHasFocus */);
            break;
        }
        default:
            break;
    }
}

void UIMachineView::updateAdditionsState()
{
    /* Check if we should restrict minimum size: */
    maybeRestrictMinimumSize();

#if 0 // TODO: Do we need this?
    if (!mDoResize && !isGuestSupportsGraphics() && fIsGuestSupportsGraphics &&
        (machineWindowWrapper()->isTrueSeamless() || machineWindowWrapper()->isTrueFullscreen()))
        mDoResize = true;
    /* This will only be acted upon if mDoResize is true: */
    sltPerformGuestResize();
#endif
}

void UIMachineView::updateMousePointerShape()
{
    if (m_bIsMouseAbsolute)
    {
        /* Should we hide/show pointer? */
        if (m_fIsHideHostPointer)
            viewport()->setCursor(Qt::BlankCursor);
        else
            viewport()->setCursor(m_lastCursor);
    }
}

void UIMachineView::updateMouseCapability()
{
    /* Correct the mouse capture state and reset the cursor to the default shape if necessary: */
    if (m_bIsMouseAbsolute)
    {
        CMouse mouse = m_console.GetMouse();
        mouse.PutMouseEventAbsolute(-1, -1, 0, 0 /* Horizontal wheel */, 0);
        captureMouse(false, false);
    }
    else
        viewport()->unsetCursor();

    /* Notify user about mouse integration state: */
    vboxProblem().remindAboutMouseIntegration(m_bIsMouseAbsolute);

    /* Notify all watchers: */
    emitMouseStateChanged();
}

void UIMachineView::sltMachineStateChanged(KMachineState state)
{
    /* Check if something had changed: */
    if (m_machineState != state)
    {
        /* Set new data: */
        m_machineState = state;

        /* Update depending things: */
        updateMachineState();
    }
}

void UIMachineView::sltAdditionsStateChanged()
{
    /* Get new values: */
    CGuest guest = console().GetGuest();
    bool bIsGuestSupportsGraphics = guest.GetSupportsGraphics();

    /* Check if something had changed: */
    if (m_bIsGuestSupportsGraphics != bIsGuestSupportsGraphics)
    {
        /* Get new data: */
        m_bIsGuestSupportsGraphics = bIsGuestSupportsGraphics;

        /* Update depending things: */
        updateAdditionsState();
    }
}

void UIMachineView::sltKeyboardLedsChanged(bool bNumLock, bool bCapsLock, bool bScrollLock)
{
    /* Update num lock status: */
    if (m_fNumLock != bNumLock)
    {
        m_fNumLock = bNumLock;
        m_uNumLockAdaptionCnt = 2;
    }

    /* Update caps lock status: */
    if (m_fCapsLock != bCapsLock)
    {
        m_fCapsLock = bCapsLock;
        m_uCapsLockAdaptionCnt = 2;
    }

    /* Update scroll lock status: */
    if (m_fScrollLock != bScrollLock)
        m_fScrollLock = bScrollLock;
}

void UIMachineView::sltMousePointerShapeChanged(bool fIsVisible, bool fHasAlpha,
                                                uint uXHot, uint uYHot, uint uWidth, uint uHeight,
                                                const uchar *pShapeData)
{
    /* Should we show cursor anyway? */
    m_fIsHideHostPointer = !fIsVisible;

    /* Should we cache shape data? */
    if (pShapeData)
        setPointerShape(pShapeData, fHasAlpha, uXHot, uYHot, uWidth, uHeight);

    /* Perform cursor update: */
    updateMousePointerShape();
}

void UIMachineView::sltMouseCapabilityChanged(bool bIsSupportsAbsolute, bool /* bNeedsHostCursor */)
{
    /* Check if something had changed: */
    if (m_bIsMouseAbsolute != bIsSupportsAbsolute)
    {
        /* Get new data: */
        m_bIsMouseAbsolute = bIsSupportsAbsolute;

        /* Update depending things: */
        updateMouseCapability();
    }
}

/* If the desktop geometry is set automatically, this will update it: */
void UIMachineView::sltDesktopResized()
{
    calculateDesktopGeometry();
}

#ifdef Q_WS_MAC
void UIMachineView::sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event)
{
    setDockIconEnabled(event.mChanged);
    updateDockOverlay();
}

# ifdef QT_MAC_USE_COCOA
void UIMachineView::sltChangePresentationMode(const VBoxChangePresentationModeEvent &event)
{
    // TODO_NEW_CORE
    // this is full screen related
#if 0
    if (mIsFullscreen)
    {
        /* First check if we are on the primary screen, only than the presentation mode have to be changed. */
        QDesktopWidget* pDesktop = QApplication::desktop();
        if (pDesktop->screenNumber(this) == pDesktop->primaryScreen())
        {
            QString testStr = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_PresentationModeEnabled).toLower();
            /* Default to false if it is an empty value */
            if (testStr.isEmpty() || testStr == "false")
                SetSystemUIMode(kUIModeAllHidden, 0);
            else
                SetSystemUIMode(kUIModeAllSuppressed, 0);
        }
    }
    else
        SetSystemUIMode(kUIModeNormal, 0);
#endif
}
# endif /* QT_MAC_USE_COCOA */
#endif

bool UIMachineView::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::FocusIn:
        {
            if (isRunning())
                focusEvent(true);
            break;
        }
        case QEvent::FocusOut:
        {
            if (isRunning())
                focusEvent(false);
            else
            {
                /* Release the host key and all other pressed keys too even when paused (otherwise, we will get stuck
                 * keys in the guest when doing sendChangedKeyStates() on resume because key presses were already
                 * recorded in m_pressedKeys but key releases will most likely not reach us but the new focus window instead): */
                releaseAllPressedKeys(true /* fReleaseHostKey */);
            }
            break;
        }

        case VBoxDefs::ResizeEventType:
        {
            /* Some situations require initial VGA Resize Request
             * to be ignored at all, leaving previous framebuffer,
             * machine view and machine window sizes preserved: */
            if (m_bIsGuestResizeIgnored)
                return true;

            bool oldIgnoreMainwndResize = m_bIsMachineWindowResizeIgnored;
            m_bIsMachineWindowResizeIgnored = true;

            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Store the new size to prevent unwanted resize hints being sent back. */
            storeConsoleSize(pResizeEvent->width(), pResizeEvent->height());

            /* Unfortunately restoreOverrideCursor() is broken in Qt 4.4.0 if WA_PaintOnScreen widgets are present.
             * This is the case on linux with SDL. As workaround we save/restore the arrow cursor manually.
             * See http://trolltech.com/developer/task-tracker/index_html?id=206165&method=entry for details.
             * Moreover the current cursor, which could be set by the guest, should be restored after resize: */
            QCursor cursor;
            if (shouldHideHostPointer())
                cursor = QCursor(Qt::BlankCursor);
            else
                cursor = viewport()->cursor();
            m_pFrameBuffer->resizeEvent(pResizeEvent);
            viewport()->setCursor(cursor);

#ifdef Q_WS_MAC
            // TODO_NEW_CORE
//            mDockIconPreview->setOriginalSize(pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* This event appears in case of guest video was changed for somehow even without video resolution change.
             * In this last case the host VM window will not be resized according this event and the host mouse cursor
             * which was unset to default here will not be hidden in capture state. So it is necessary to perform
             * updateMouseClipping() for the guest resize event if the mouse cursor was captured: */
            if (m_bIsMouseCaptured)
                updateMouseClipping();

            /* Apply maximum size restriction: */
            setMaximumSize(sizeHint());

            /* May be we have to restrict minimum size? */
            maybeRestrictMinimumSize();

            /* Resize the guest canvas: */
            if (!m_bIsFrameBufferResizeIgnored)
                resize(pResizeEvent->width(), pResizeEvent->height());
            updateSliders();

            /* Let our toplevel widget calculate its sizeHint properly. */
#ifdef Q_WS_X11
            /* We use processEvents rather than sendPostedEvents & set the time out value to max cause on X11 otherwise
             * the layout isn't calculated correctly. Dosn't find the bug in Qt, but this could be triggered through
             * the async nature of the X11 window event system. */
            QCoreApplication::processEvents(QEventLoop::AllEvents, INT_MAX);
#else /* Q_WS_X11 */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
#endif /* Q_WS_X11 */

            if (!m_bIsFrameBufferResizeIgnored)
                normalizeGeometry(true /* adjustPosition */);

            /* Report to the VM thread that we finished resizing */
            m_console.GetDisplay().ResizeCompleted(0);

            m_bIsMachineWindowResizeIgnored = oldIgnoreMainwndResize;

            /* Make sure that all posted signals are processed: */
            qApp->processEvents();

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

            /* We also recalculate the desktop geometry if this is determined
             * automatically.  In fact, we only need this on the first resize,
             * but it is done every time to keep the code simpler. */
            calculateDesktopGeometry();

            /* Enable frame-buffer resize watching: */
            if (m_bIsFrameBufferResizeIgnored)
                m_bIsFrameBufferResizeIgnored = false;

            return true;
        }

        /* See VBox[QImage|SDL]FrameBuffer::NotifyUpdate(): */
        case VBoxDefs::RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            viewport()->repaint(pPaintEvent->x() - contentsX(), pPaintEvent->y() - contentsY(),
                                pPaintEvent->width(), pPaintEvent->height());
            /* m_console.GetDisplay().UpdateCompleted(); - the event was acked already */
            return true;
        }

#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBoxDefs::VHWACommandProcessType:
        {
            m_pFrameBuffer->doProcessVHWACommand(pEvent);
            return true;
        }
#endif

#if 0 // TODO: Move that to seamless mode event hadler!
        case VBoxDefs::SetRegionEventType:
        {
            VBoxSetRegionEvent *sre = (VBoxSetRegionEvent*) pEvent;
            if (machineWindowWrapper()->isTrueSeamless() && sre->region() != mLastVisibleRegion)
            {
                mLastVisibleRegion = sre->region();
                machineWindowWrapper()->setMask (sre->region());
            }
            else if (!mLastVisibleRegion.isEmpty() && !machineWindowWrapper()->isTrueSeamless())
                mLastVisibleRegion = QRegion();
            return true;
        }
#endif

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

#ifdef Q_WS_PM
            // TODO: that a temporary solution to send Alt+Tab and friends to the guest.
            // The proper solution is to write a keyboard driver that will steal these combos from the host
            // (it's impossible to do so using hooks on OS/2):

            if (m_bIsHostkeyPressed)
            {
                bool pressed = pEvent->type() == QEvent::KeyPress;
                CKeyboard keyboard = m_console.GetKeyboard();

                /* Whether the host key is Shift so that it will modify the hot key values?
                 * Note that we don't distinguish between left and right shift here (too much hassle): */
                const bool kShift = (m_globalSettings.hostKey() == VK_SHIFT ||
                                    m_globalSettings.hostKey() == VK_LSHIFT) &&
                                    (pKeyEvent->state() & Qt::ShiftModifier);
                /* define hot keys according to the Shift state */
                const int kAltTab      = kShift ? Qt::Key_Exclam     : Qt::Key_1;
                const int kAltShiftTab = kShift ? Qt::Key_At         : Qt::Key_2;
                const int kCtrlEsc     = kShift ? Qt::Key_AsciiTilde : Qt::Key_QuoteLeft;

                /* Simulate Alt+Tab on Host+1 and Alt+Shift+Tab on Host+2 */
                if (pKeyEvent->key() == kAltTab || pKeyEvent->key() == kAltShiftTab)
                {
                    if (pressed)
                    {
                        /* Send the Alt press to the guest */
                        if (!(m_pressedKeysCopy[0x38] & IsKeyPressed))
                        {
                            /* Store the press in *Copy to have it automatically
                             * released when the Host key is released: */
                            m_pressedKeysCopy[0x38] |= IsKeyPressed;
                            keyboard.PutScancode(0x38);
                        }

                        /* Make sure Shift is pressed if it's Key_2 and released if it's Key_1: */
                        if (pKeyEvent->key() == kAltTab &&
                            (m_pressedKeysCopy[0x2A] & IsKeyPressed))
                        {
                            m_pressedKeysCopy[0x2A] &= ~IsKeyPressed;
                            keyboard.PutScancode(0xAA);
                        }
                        else
                        if (pKeyEvent->key() == kAltShiftTab &&
                            !(m_pressedKeysCopy[0x2A] & IsKeyPressed))
                        {
                            m_pressedKeysCopy[0x2A] |= IsKeyPressed;
                            keyboard.PutScancode(0x2A);
                        }
                    }

                    keyboard.PutScancode(pressed ? 0x0F : 0x8F);

                    pKeyEvent->accept();
                    return true;
                }

                /* Simulate Ctrl+Esc on Host+Tilde */
                if (pKeyEvent->key() == kCtrlEsc)
                {
                    /* Send the Ctrl press to the guest */
                    if (pressed && !(m_pressedKeysCopy[0x1d] & IsKeyPressed))
                    {
                        /* store the press in *Copy to have it automatically
                         * released when the Host key is released */
                        m_pressedKeysCopy[0x1d] |= IsKeyPressed;
                        keyboard.PutScancode(0x1d);
                    }

                    keyboard.PutScancode(pressed ? 0x01 : 0x81);

                    pKeyEvent->accept();
                    return true;
                }
            }
#endif /* Q_WS_PM */

            if (m_bIsHostkeyPressed && pEvent->type() == QEvent::KeyPress)
            {
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

                    CKeyboard keyboard = m_console.GetKeyboard();
                    keyboard.PutScancodes(combo);
                }

#if 0 // TODO: Divide tha code to specific parts and move it there:
                if (pKeyEvent->key() == Qt::Key_Home)
                {
                    /* Activate the main menu */
                    if (machineWindowWrapper()->isTrueSeamless() || machineWindowWrapper()->isTrueFullscreen())
                        machineWindowWrapper()->popupMainMenu (m_bIsMouseCaptured);
                    else
                    {
                        /* In Qt4 it is not enough to just set the focus to menu-bar.
                         * So to get the menu-bar we have to send Qt::Key_Alt press/release events directly: */
                        QKeyEvent e1(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
                        QKeyEvent e2(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
                        QApplication::sendEvent(machineWindowWrapper()->menuBar(), &e1);
                        QApplication::sendEvent(machineWindowWrapper()->menuBar(), &e2);
                    }
                }
                else
                {
                    /* Process hot keys not processed in keyEvent() (as in case of non-alphanumeric keys): */
                    processed = machineWindowWrapper()->machineLogic()->actionsPool()->processHotKey(QKeySequence (pKeyEvent->key()));
                }
#endif
            }
            else if (!m_bIsHostkeyPressed && pEvent->type() == QEvent::KeyRelease)
            {
                /* Show a possible warning on key release which seems to be more expected by the end user: */
                if (machineWindowWrapper()->machineLogic()->isPaused())
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
#endif

        default:
            break;
    }

    return QAbstractScrollArea::event(pEvent);
}

bool UIMachineView::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == viewport())
    {
        switch (pEvent->type())
        {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonRelease:
            {
                QMouseEvent *pMouseEvent = static_cast<QMouseEvent*>(pEvent);
                m_iLastMouseWheelDelta = 0;
                if (mouseEvent(pMouseEvent->type(), pMouseEvent->pos(), pMouseEvent->globalPos(),
                               pMouseEvent->buttons(), pMouseEvent->modifiers(), 0, Qt::Horizontal))
                    return true;
                break;
            }
            case QEvent::Wheel:
            {
                QWheelEvent *pWheelEvent = static_cast<QWheelEvent*>(pEvent);
                /* There are pointing devices which send smaller values for the delta than 120.
                 * Here we sum them up until we are greater than 120. This allows to have finer control
                 * over the speed acceleration & enables such devices to send a valid wheel event to our
                 * guest mouse device at all: */
                int iDelta = 0;
                m_iLastMouseWheelDelta += pWheelEvent->delta();
                if (qAbs(m_iLastMouseWheelDelta) >= 120)
                {
                    iDelta = m_iLastMouseWheelDelta;
                    m_iLastMouseWheelDelta = m_iLastMouseWheelDelta % 120;
                }
                if (mouseEvent(pWheelEvent->type(), pWheelEvent->pos(), pWheelEvent->globalPos(),
#ifdef QT_MAC_USE_COCOA
                                /* Qt Cocoa is buggy. It always reports a left button pressed when the
                                 * mouse wheel event occurs. A workaround is to ask the application which
                                 * buttons are pressed currently: */
                                QApplication::mouseButtons(),
#else /* QT_MAC_USE_COCOA */
                                pWheelEvent->buttons(),
#endif /* QT_MAC_USE_COCOA */
                                pWheelEvent->modifiers(),
                                iDelta, pWheelEvent->orientation()))
                    return true;
                break;
            }
#ifdef Q_WS_MAC
            case QEvent::Leave:
            {
                /* Enable mouse event compression if we leave the VM view. This is necessary for
                 * having smooth resizing of the VM/other windows: */
                setMouseCoalescingEnabled(true);
                break;
            }
            case QEvent::Enter:
            {
                /* Disable mouse event compression if we enter the VM view. So all mouse events are
                 * registered in the VM. Only do this if the keyboard/mouse is grabbed (this is when
                 * we have a valid event handler): */
                setMouseCoalescingEnabled (false);
                break;
            }
#endif /* Q_WS_MAC */
            case QEvent::Resize:
            {
                if (m_bIsMouseCaptured)
                    updateMouseClipping();
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (m_pFrameBuffer)
                {
                    m_pFrameBuffer->viewportResized(static_cast<QResizeEvent*>(pEvent));
                }
#endif
                break;
            }
            default:
                break;
        }
    }
    else if (pWatched == machineWindowWrapper()->machineWindow())
    {
        switch (pEvent->type())
        {
#if 0 // TODO Move to normal specific event handler:
            case QEvent::Resize:
            {
                /* Set the "guest needs to resize" hint. This hint is acted upon when (and only when)
                 * the autoresize property is "true": */
                mDoResize = isGuestSupportsGraphics() || machineWindowWrapper()->isTrueFullscreen();
                if (!m_bIsMachineWindowResizeIgnored && isGuestSupportsGraphics() && m_bIsGuestAutoresizeEnabled)
                    QTimer::singleShot(300, this, SLOT(sltPerformGuestResize()));
                break;
            }
#endif
            case QEvent::WindowStateChange:
            {
                /* During minimizing and state restoring machineWindowWrapper() gets the focus
                 * which belongs to console view window, so returning it properly. */
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
#if defined (Q_WS_WIN32)
#if defined (VBOX_GUI_USE_DDRAW)
            case QEvent::Move:
            {
                /* Notification from our parent that it has moved. We need this in order
                 * to possibly adjust the direct screen blitting: */
                if (m_pFrameBuffer)
                    m_pFrameBuffer->moveEvent(static_cast<QMoveEvent*>(pEvent));
                break;
            }
#endif
            /* Install/uninstall low-level kbd hook on every activation/deactivation to:
             * a) avoid excess hook calls when we're not active and
             * b) be always in front of any other possible hooks */
            case QEvent::WindowActivate:
            {
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
                }
                break;
            }
#endif /* defined (Q_WS_WIN32) */
#if defined (Q_WS_MAC)
            /* Install/remove the keyboard event handler: */
            case QEvent::WindowActivate:
                darwinGrabKeyboardEvents(true);
                break;
            case QEvent::WindowDeactivate:
                darwinGrabKeyboardEvents(false);
                break;
#endif /* defined (Q_WS_MAC) */
            default:
                break;
        }
    }
#if 0 // TODO Move to normal specific event handler:
    else if (pWatched == machineWindowWrapper()->menuBar())
    {
        /* Sometimes when we press ESC in the menu it brings the focus away (Qt bug?)
         * causing no widget to have a focus, or holds the focus itself, instead of
         * returning the focus to the console window. Here we fix this: */
        switch (pEvent->type())
        {
            case QEvent::FocusOut:
            {
                if (qApp->focusWidget() == 0)
                    setFocus();
                break;
            }
            case QEvent::KeyPress:
            {
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);
                if (pKeyEvent->key() == Qt::Key_Escape && (pKeyEvent->modifiers() == Qt::NoModifier))
                    if (machineWindowWrapper()->menuBar()->hasFocus())
                        setFocus();
                break;
            }
            default:
                break;
        }
    }
#endif

    return QAbstractScrollArea::eventFilter (pWatched, pEvent);
}

void UIMachineView::focusEvent(bool fHasFocus, bool fReleaseHostKey /* = true */)
{
    if (fHasFocus)
    {
#ifdef Q_WS_WIN32
        if (!m_bIsAutoCaptureDisabled && m_globalSettings.autoCapture() && GetAncestor(winId(), GA_ROOT) == GetForegroundWindow())
#else
        if (!m_bIsAutoCaptureDisabled && m_globalSettings.autoCapture())
#endif
        {
            captureKbd(true);
        }

        /* Reset the single-time disable capture flag: */
        if (m_bIsAutoCaptureDisabled)
            m_bIsAutoCaptureDisabled = false;
    }
    else
    {
        captureMouse(false);
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
                if (isRunning() && m_bIsKeyboardCaptured)
                {
                    captureKbd (false);
                    if (!(m_bIsMouseAbsolute && m_bIsMouseIntegrated))
                        captureMouse (false);
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
                if (isRunning())
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
                    if (machineWindowWrapper()->machineLogic()->isPaused())
                    {
                        vboxProblem().remindAboutPausedVMInput();
                    }
                    else if (isRunning())
                    {
                        bool captured = m_bIsKeyboardCaptured;
                        bool ok = true;
                        if (!captured)
                        {
                            /* temporarily disable auto capture that will take
                             * place after this dialog is dismissed because
                             * the capture state is to be defined by the
                             * dialog result itself */
                            m_bIsAutoCaptureDisabled = true;
                            bool autoConfirmed = false;
                            ok = vboxProblem().confirmInputCapture (&autoConfirmed);
                            if (autoConfirmed)
                                m_bIsAutoCaptureDisabled = false;
                            /* otherwise, the disable flag will be reset in
                             * the next console view's foucs in event (since
                             * may happen asynchronously on some platforms,
                             * after we return from this code) */
                        }

                        if (ok)
                        {
                            captureKbd (!captured, false);
                            if (!(m_bIsMouseAbsolute && m_bIsMouseIntegrated))
                            {
#ifdef Q_WS_X11
                                /* make sure that pending FocusOut events from the
                                 * previous message box are handled, otherwise the
                                 * mouse is immediately ungrabbed. */
                                qApp->processEvents();
#endif
                                captureMouse (m_bIsKeyboardCaptured);
                            }
                        }
                    }
                }

                if (isRunning())
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
#if defined (Q_WS_WIN32)
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
        // TODO_NEW_CORE
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
    if (m_bIsHostkeyPressed || isHostKey || machineWindowWrapper()->machineLogic()->isPaused())
    {
        /* Grab the key from Qt and from VM if it's a host key,
         * otherwise just pass it to Qt */
        return isHostKey;
    }

    CKeyboard keyboard = m_console.GetKeyboard();
    Assert(!keyboard.isNull());

#if defined (Q_WS_WIN32)
    /* send pending WM_PAINT events */
    ::UpdateWindow(viewport()->winId());
#endif

    std::vector <LONG> scancodes(codes, &codes[count]);
    keyboard.PutScancodes(QVector<LONG>::fromStdVector(scancodes));

    /* Grab the key from Qt: */
    return true;
}

bool UIMachineView::mouseEvent(int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                               Qt::MouseButtons aButtons, Qt::KeyboardModifiers aModifiers,
                               int aWheelDelta, Qt::Orientation aWheelDir)
{
#if 0
    LogRel3(("%s: type=%03d x=%03d y=%03d btns=%08X wdelta=%03d wdir=%s\n",
             __PRETTY_FUNCTION__ , aType, aPos.x(), aPos.y(),
               (aButtons & Qt::LeftButton ? 1 : 0)
             | (aButtons & Qt::RightButton ? 2 : 0)
             | (aButtons & Qt::MidButton ? 4 : 0)
             | (aButtons & Qt::XButton1 ? 8 : 0)
             | (aButtons & Qt::XButton2 ? 16 : 0),
             aWheelDelta,
               aWheelDir == Qt::Horizontal ? "Horizontal"
             : aWheelDir == Qt::Vertical ? "Vertical" : "Unknown"));
    Q_UNUSED (aModifiers);
#else
    Q_UNUSED (aModifiers);
#endif

    int state = 0;
    if (aButtons & Qt::LeftButton)
        state |= KMouseButtonState_LeftButton;
    if (aButtons & Qt::RightButton)
        state |= KMouseButtonState_RightButton;
    if (aButtons & Qt::MidButton)
        state |= KMouseButtonState_MiddleButton;
    if (aButtons & Qt::XButton1)
        state |= KMouseButtonState_XButton1;
    if (aButtons & Qt::XButton2)
        state |= KMouseButtonState_XButton2;

#ifdef Q_WS_MAC
    /* Simulate the right click on
     * Host+Left Mouse */
    if (m_bIsHostkeyPressed &&
        m_bIsHostkeyAlone &&
        state == KMouseButtonState_LeftButton)
        state = KMouseButtonState_RightButton;
#endif /* Q_WS_MAC */

    int wheelVertical = 0;
    int wheelHorizontal = 0;
    if (aWheelDir == Qt::Vertical)
    {
        /* the absolute value of wheel delta is 120 units per every wheel
         * move; positive deltas correspond to counterclockwize rotations
         * (usually up), negative -- to clockwize (usually down). */
        wheelVertical = - (aWheelDelta / 120);
    }
    else if (aWheelDir == Qt::Horizontal)
        wheelHorizontal = aWheelDelta / 120;

    if (m_bIsMouseCaptured)
    {
#ifdef Q_WS_WIN32
        /* send pending WM_PAINT events */
        ::UpdateWindow (viewport()->winId());
#endif

        CMouse mouse = m_console.GetMouse();
        mouse.PutMouseEvent(aGlobalPos.x() - m_lastMousePos.x(),
                            aGlobalPos.y() - m_lastMousePos.y(),
                            wheelVertical, wheelHorizontal, state);

#if defined (Q_WS_MAC)
        /*
         * Keep the mouse from leaving the widget.
         *
         * This is a bit tricky to get right because if it escapes we won't necessarily
         * get mouse events any longer and can warp it back. So, we keep safety zone
         * of up to 300 pixels around the borders of the widget to prevent this from
         * happening. Also, the mouse is warped back to the center of the widget.
         *
         * (Note, aPos seems to be unreliable, it caused endless recursion here at one points...)
         * (Note, synergy and other remote clients might not like this cursor warping.)
         */
        QRect rect = viewport()->visibleRegion().boundingRect();
        QPoint pw = viewport()->mapToGlobal (viewport()->pos());
        rect.translate (pw.x(), pw.y());

        QRect dpRect = QApplication::desktop()->screenGeometry (viewport());
        if (rect.intersects (dpRect))
            rect = rect.intersect (dpRect);

        int wsafe = rect.width() / 6;
        rect.setWidth (rect.width() - wsafe * 2);
        rect.setLeft (rect.left() + wsafe);

        int hsafe = rect.height() / 6;
        rect.setWidth (rect.height() - hsafe * 2);
        rect.setTop (rect.top() + hsafe);

        if (rect.contains (aGlobalPos, true))
            m_lastMousePos = aGlobalPos;
        else
        {
            m_lastMousePos = rect.center();
            QCursor::setPos (m_lastMousePos);
        }
#else /* !Q_WS_MAC */

        /* "jerk" the mouse by bringing it to the opposite side
         * to simulate the endless moving */

# ifdef Q_WS_WIN32
        int we = viewport()->width() - 1;
        int he = viewport()->height() - 1;
        QPoint p = aPos;
        if (aPos.x() == 0)
            p.setX (we - 1);
        else if (aPos.x() == we)
            p.setX (1);
        if (aPos.y() == 0 )
            p.setY (he - 1);
        else if (aPos.y() == he)
            p.setY (1);

        if (p != aPos)
        {
            m_lastMousePos = viewport()->mapToGlobal (p);
            QCursor::setPos (m_lastMousePos);
        }
        else
        {
            m_lastMousePos = aGlobalPos;
        }
# else
        int we = QApplication::desktop()->width() - 1;
        int he = QApplication::desktop()->height() - 1;
        QPoint p = aGlobalPos;
        if (aGlobalPos.x() == 0)
            p.setX (we - 1);
        else if (aGlobalPos.x() == we)
            p.setX( 1 );
        if (aGlobalPos.y() == 0)
            p.setY (he - 1);
        else if (aGlobalPos.y() == he)
            p.setY (1);

        if (p != aGlobalPos)
        {
            m_lastMousePos =  p;
            QCursor::setPos (m_lastMousePos);
        }
        else
        {
            m_lastMousePos = aGlobalPos;
        }
# endif
#endif /* !Q_WS_MAC */
        return true; /* stop further event handling */
    }
    else /* !m_bIsMouseCaptured */
    {
#if 0 // TODO: Move that to fullscreen event-hjadler:
        if (mode() != VBoxDefs::SDLMode)
        {
            /* try to automatically scroll the guest canvas if the
             * mouse is on the screen border */
            /// @todo (r=dmik) better use a timer for autoscroll
            QRect scrGeo = QApplication::desktop()->screenGeometry (this);
            int dx = 0, dy = 0;
            if (scrGeo.width() < contentsWidth())
            {
                if (scrGeo.left() == aGlobalPos.x()) dx = -1;
                if (scrGeo.right() == aGlobalPos.x()) dx = +1;
            }
            if (scrGeo.height() < contentsHeight())
            {
                if (scrGeo.top() == aGlobalPos.y()) dy = -1;
                if (scrGeo.bottom() == aGlobalPos.y()) dy = +1;
            }
            if (dx || dy)
                scrollBy(dx, dy);
        }
#endif

        if (m_bIsMouseAbsolute && m_bIsMouseIntegrated)
        {
            int cw = contentsWidth(), ch = contentsHeight();
            int vw = visibleWidth(), vh = visibleHeight();

            if (mode() != VBoxDefs::SDLMode)
            {
                /* Try to automatically scroll the guest canvas if the
                 * mouse goes outside its visible part: */

                int dx = 0;
                if (aPos.x() > vw) dx = aPos.x() - vw;
                else if (aPos.x() < 0) dx = aPos.x();
                int dy = 0;
                if (aPos.y() > vh) dy = aPos.y() - vh;
                else if (aPos.y() < 0) dy = aPos.y();
                if (dx != 0 || dy != 0) scrollBy (dx, dy);
            }

            QPoint cpnt = viewportToContents (aPos);
            if (cpnt.x() < 0) cpnt.setX (0);
            else if (cpnt.x() > cw) cpnt.setX (cw);
            if (cpnt.y() < 0) cpnt.setY (0);
            else if (cpnt.y() > ch) cpnt.setY (ch);

            CMouse mouse = m_console.GetMouse();
            mouse.PutMouseEventAbsolute (cpnt.x(), cpnt.y(), wheelVertical,
                                         wheelHorizontal, state);
            return true;
        }
        else
        {
            if (hasFocus() && (aType == QEvent::MouseButtonRelease && aButtons == Qt::NoButton))
            {
                if (machineWindowWrapper()->machineLogic()->isPaused())
                {
                    vboxProblem().remindAboutPausedVMInput();
                }
                else if (isRunning())
                {
                    /* Temporarily disable auto capture that will take place after this dialog is dismissed because
                     * the capture state is to be defined by the dialog result itself: */
                    m_bIsAutoCaptureDisabled = true;
                    bool autoConfirmed = false;
                    bool ok = vboxProblem().confirmInputCapture (&autoConfirmed);
                    if (autoConfirmed)
                        m_bIsAutoCaptureDisabled = false;
                    /* Otherwise, the disable flag will be reset in the next console view's foucs in event (since
                     * may happen asynchronously on some platforms, after we return from this code): */
                    if (ok)
                    {
#ifdef Q_WS_X11
                        /* Make sure that pending FocusOut events from the previous message box are handled,
                         * otherwise the mouse is immediately ungrabbed again: */
                        qApp->processEvents();
#endif
                        captureKbd(true);
                        captureMouse(true);
                    }
                }
            }
        }
    }

    return false;
}

void UIMachineView::resizeEvent(QResizeEvent *pEvent)
{
    updateSliders();
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
    QRect rectangle = viewport()->geometry();
    PostBoundsChanged(rectangle);
#endif /* Q_WS_MAC */
    return QAbstractScrollArea::resizeEvent(pEvent);
}

void UIMachineView::moveEvent(QMoveEvent *pEvent)
{
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
    QRect r = viewport()->geometry();
    PostBoundsChanged (r);
#endif /* Q_WS_MAC */
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
            // TODO_NEW_CORE
//        if (isRunning())
//            updateDockIcon();
#endif
        return;
    }

#ifdef VBOX_GUI_USE_QUARTZ2D
            // TODO_NEW_CORE
//    if (mode() == VBoxDefs::Quartz2DMode && m_pFrameBuffer)
//    {
//        m_pFrameBuffer->paintEvent(pPaintEvent);
//        updateDockIcon();
//    }
//    else
#endif
//    {
//        /* We have a snapshot for the paused state: */
//        QRect r = pPaintEvent->rect().intersect (viewport()->rect());
//        /* We have to disable paint on screen if we are using the regular painter */
//        bool paintOnScreen = viewport()->testAttribute(Qt::WA_PaintOnScreen);
//        viewport()->setAttribute(Qt::WA_PaintOnScreen, false);
//        QPainter pnt(viewport());
//        pnt.drawPixmap(r.x(), r.y(), m_pauseShot, r.x() + contentsX(), r.y() + contentsY(), r.width(), r.height());
//        /* Restore the attribute to its previous state */
//        viewport()->setAttribute(Qt::WA_PaintOnScreen, paintOnScreen);
#ifdef Q_WS_MAC
//        updateDockIcon();
#endif
//    }
}

#if defined(Q_WS_WIN32)

static HHOOK gKbdHook = NULL;
static UIMachineView *gView = 0;
LRESULT CALLBACK UIMachineView::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Assert(gView);
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

#elif defined(Q_WS_PM)

bool UIMachineView::pmEvent(QMSG *aMsg)
{
    if (aMsg->msg == UM_PREACCEL_CHAR)
    {
        /* We are inside the input hook
         * let the message go through the normal system pipeline. */
        if (!m_bIsKeyboardCaptured)
            return false;
    }

    if (aMsg->msg != WM_CHAR && aMsg->msg != UM_PREACCEL_CHAR)
        return false;

    /* check for the special flag possibly set at the end of this function */
    if (SHORT2FROMMP(aMsg->mp2) & 0x8000)
    {
        aMsg->mp2 = MPFROM2SHORT(SHORT1FROMMP(aMsg->mp2), SHORT2FROMMP(aMsg->mp2) & ~0x8000);
        return false;
    }

    USHORT ch = SHORT1FROMMP(aMsg->mp2);
    USHORT f = SHORT1FROMMP(aMsg->mp1);

    int scan = (unsigned int)CHAR4FROMMP(aMsg->mp1);
    if (!scan || scan > 0x7F)
        return true;

    int vkey = QIHotKeyEdit::virtualKey(aMsg);

    int flags = 0;

    if ((ch & 0xFF) == 0xE0)
    {
        flags |= KeyExtended;
        scan = ch >> 8;
    }
    else if (scan == 0x5C && (ch & 0xFF) == '/')
    {
        /* this is the '/' key on the keypad */
        scan = 0x35;
        flags |= KeyExtended;
    }
    else
    {
        /* For some keys, the scan code passed in QMSG is a pseudo scan
         * code. We replace it with a real hardware scan code, according to
         * http://www.computer-engineering.org/ps2keyboard/scancodes1.html.
         * Also detect Pause and PrtScn and set flags. */
        switch (vkey)
        {
            case VK_ENTER:     scan = 0x1C; flags |= KeyExtended; break;
            case VK_CTRL:      scan = 0x1D; flags |= KeyExtended; break;
            case VK_ALTGRAF:   scan = 0x38; flags |= KeyExtended; break;
            case VK_LWIN:      scan = 0x5B; flags |= KeyExtended; break;
            case VK_RWIN:      scan = 0x5C; flags |= KeyExtended; break;
            case VK_WINMENU:   scan = 0x5D; flags |= KeyExtended; break;
            case VK_FORWARD:   scan = 0x69; flags |= KeyExtended; break;
            case VK_BACKWARD:  scan = 0x6A; flags |= KeyExtended; break;
#if 0
            /// @todo this would send 0xE0 0x46 0xE0 0xC6. It's not fully
            // clear what is more correct
            case VK_BREAK:     scan = 0x46; flags |= KeyExtended; break;
#else
            case VK_BREAK:     scan = 0;    flags |= KeyPause; break;
#endif
            case VK_PAUSE:     scan = 0;    flags |= KeyPause;    break;
            case VK_PRINTSCRN: scan = 0;    flags |= KeyPrint;    break;
            default:;
        }
    }

    if (!(f & KC_KEYUP))
        flags |= KeyPressed;

    bool result = keyEvent (vkey, scan, flags);
    if (!result && m_bIsKeyboardCaptured)
    {
        /* keyEvent() returned that it didn't process the message, but since the
         * keyboard is captured, we don't want to pass it to PM. We just want
         * to let Qt process the message (to handle non-alphanumeric <HOST>+key
         * shortcuts for example). So send it direcltly to the window with the
         * special flag in the reserved area of lParam (to avoid recursion). */
        ::WinSendMsg (aMsg->hwnd, WM_CHAR, aMsg->mp1,
                      MPFROM2SHORT (SHORT1FROMMP (aMsg->mp2), SHORT2FROMMP (aMsg->mp2) | 0x8000));
        return true;
    }
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
         * releasing the keyboard before the screen saver gets active. */
        case XFocusOut:
        case XFocusIn:
            if (isRunning())
                focusEvent(pEvent->type == XFocusIn);
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

void UIMachineView::darwinGrabKeyboardEvents(bool fGrab)
{
    m_fKeyboardGrabbed = fGrab;
    if (fGrab)
    {
        /* Disable mouse and keyboard event compression/delaying to make sure we *really* get all of the events. */
        ::CGSetLocalEventsSuppressionInterval(0.0);
        setMouseCoalescingEnabled(false);

        /* Register the event callback/hook and grab the keyboard. */
# ifdef QT_MAC_USE_COCOA
        ::VBoxCocoaApplication_setCallback (UINT32_MAX, /** @todo fix mask */
                                            UIMachineView::darwinEventHandlerProc, this);

# else /* QT_MAC_USE_COCOA */
        EventTypeSpec eventTypes[6];
        eventTypes[0].eventClass = kEventClassKeyboard;
        eventTypes[0].eventKind  = kEventRawKeyDown;
        eventTypes[1].eventClass = kEventClassKeyboard;
        eventTypes[1].eventKind  = kEventRawKeyUp;
        eventTypes[2].eventClass = kEventClassKeyboard;
        eventTypes[2].eventKind  = kEventRawKeyRepeat;
        eventTypes[3].eventClass = kEventClassKeyboard;
        eventTypes[3].eventKind  = kEventRawKeyModifiersChanged;
        /* For ignorning Command-H and Command-Q which aren't affected by the
         * global hotkey stuff (doesn't work well): */
        eventTypes[4].eventClass = kEventClassCommand;
        eventTypes[4].eventKind  = kEventCommandProcess;
        eventTypes[5].eventClass = kEventClassCommand;
        eventTypes[5].eventKind  = kEventCommandUpdateStatus;

        EventHandlerUPP eventHandler = ::NewEventHandlerUPP(UIMachineView::darwinEventHandlerProc);

        m_darwinEventHandlerRef = NULL;
        ::InstallApplicationEventHandler(eventHandler, RT_ELEMENTS (eventTypes), &eventTypes[0],
                                         this, &m_darwinEventHandlerRef);
        ::DisposeEventHandlerUPP(eventHandler);
# endif /* !QT_MAC_USE_COCOA */

        ::DarwinGrabKeyboard (false);
    }
    else
    {
        ::DarwinReleaseKeyboard();
# ifdef QT_MAC_USE_COCOA
        ::VBoxCocoaApplication_unsetCallback(UINT32_MAX, /** @todo fix mask */
                                             UIMachineView::darwinEventHandlerProc, this);
# else /* QT_MAC_USE_COCOA */
        if (m_darwinEventHandlerRef)
        {
            ::RemoveEventHandler(m_darwinEventHandlerRef);
            m_darwinEventHandlerRef = NULL;
        }
# endif /* !QT_MAC_USE_COCOA */
    }
}

# ifdef QT_MAC_USE_COCOA
bool UIMachineView::darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    UIMachineView *view = (UIMachineView*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 eventClass = ::GetEventClass(inEvent);

    /* Check if this is an application key combo. In that case we will not pass
     * the event to the guest, but let the host process it. */
    if (VBoxCocoaApplication_isApplicationCommand(pvCocoaEvent))
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
# else /* QT_MAC_USE_COCOA */

pascal OSStatus UIMachineView::darwinEventHandlerProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
    UIMachineView *view = static_cast<UIMachineView *> (inUserData);
    UInt32 eventClass = ::GetEventClass (inEvent);

    /* Not sure but this seems an triggered event if the spotlight searchbar is
     * displayed. So flag that the host key isn't pressed alone. */
    if (eventClass == 'cgs ' && view->m_bIsHostkeyPressed && ::GetEventKind (inEvent) == 0x15)
        view->m_bIsHostkeyAlone = false;

    if (eventClass == kEventClassKeyboard)
    {
        if (view->darwinKeyboardEvent (NULL, inEvent))
            return 0;
    }

    /*
     * Command-H and Command-Q aren't properly disabled yet, and it's still
     * possible to use the left command key to invoke them when the keyboard
     * is captured. We discard the events these if the keyboard is captured
     * as a half measure to prevent unexpected behaviour. However, we don't
     * get any key down/up events, so these combinations are dead to the guest...
     */
    else if (eventClass == kEventClassCommand)
    {
        if (view->m_bIsKeyboardCaptured)
            return 0;
    }
    return ::CallNextEventHandler(inHandlerCallRef, inEvent);
}
# endif /* !QT_MAC_USE_COCOA */

#endif

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

    if (m_uNumLockAdaptionCnt && (m_fNumLock ^ !!(uMask & uKeyMaskNum)))
    {
        -- m_uNumLockAdaptionCnt;
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (m_uCapsLockAdaptionCnt && (m_fCapsLock ^ !!(uMask & uKeyMaskCaps)))
    {
        m_uCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (m_fCapsLock && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(Q_WS_WIN32)

    if (m_uNumLockAdaptionCnt && (m_fNumLock ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        m_uNumLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (m_uCapsLockAdaptionCnt && (m_fCapsLock ^ !!(GetKeyState(VK_CAPITAL))))
    {
        m_uCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (m_fCapsLock && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(Q_WS_MAC)

    /* if (m_uNumLockAdaptionCnt) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (m_uCapsLockAdaptionCnt && (m_fCapsLock ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        m_uCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (m_fCapsLock && !(m_pressedKeys[0x2a] & IsKeyPressed))
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
    return QPoint (vp.x() + contentsX(), vp.y() + contentsY());
}

void UIMachineView::updateSliders()
{
    QSize p = viewport()->size();
    QSize m = maximumViewportSize();

    QSize v = QSize(m_pFrameBuffer->width(), m_pFrameBuffer->height());
    /* no scroll bars needed */
    if (m.expandedTo(v) == m)
        p = m;

    horizontalScrollBar()->setRange(0, v.width() - p.width());
    verticalScrollBar()->setRange(0, v.height() - p.height());
    horizontalScrollBar()->setPageStep(p.width());
    verticalScrollBar()->setPageStep(p.height());
}

void UIMachineView::scrollBy(int dx, int dy)
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
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
#endif

void UIMachineView::emitKeyboardStateChanged()
{
    emit keyboardStateChanged(keyboardState());
}

void UIMachineView::emitMouseStateChanged()
{
    emit mouseStateChanged(mouseState());
}

void UIMachineView::captureKbd(bool fCapture, bool fEmitSignal /* = true */)
{
    if (m_bIsKeyboardCaptured == fCapture)
        return;

    /* On Win32, keyboard grabbing is ineffective, a low-level keyboard hook is used instead.
     * On X11, we use XGrabKey instead of XGrabKeyboard (called by QWidget::grabKeyboard())
     * because the latter causes problems under metacity 2.16 (in particular, due to a bug,
     * a window cannot be moved using the mouse if it is currently grabing the keyboard).
     * On Mac OS X, we use the Qt methods + disabling global hot keys + watching modifiers
     * (for right/left separation). */
#if defined(Q_WS_WIN32)
    /**/
#elif defined(Q_WS_X11)
        if (fCapture)
                XGrabKey(QX11Info::display(), AnyKey, AnyModifier, window()->winId(), False, GrabModeAsync, GrabModeAsync);
        else
                XUngrabKey(QX11Info::display(), AnyKey, AnyModifier, window()->winId());
#elif defined(Q_WS_MAC)
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

void UIMachineView::captureMouse(bool fCapture, bool fEmitSignal /* = true */)
{
    if (m_bIsMouseCaptured == fCapture)
        return;

    if (fCapture)
    {
        /* Memorize the host position where the cursor was captured: */
        m_capturedMousePos = QCursor::pos();
#ifdef Q_WS_WIN32
        viewport()->setCursor (QCursor (Qt::BlankCursor));
        /* Move the mouse to the center of the visible area: */
        QCursor::setPos (mapToGlobal (visibleRegion().boundingRect().center()));
        m_lastMousePos = QCursor::pos();
#elif defined (Q_WS_MAC)
        /* Move the mouse to the center of the visible area: */
        m_lastMousePos = mapToGlobal (visibleRegion().boundingRect().center());
        QCursor::setPos (m_lastMousePos);
        /* Grab all mouse events: */
        viewport()->grabMouse();
#else
        viewport()->grabMouse();
        m_lastMousePos = QCursor::pos();
#endif
    }
    else
    {
#ifndef Q_WS_WIN32
        viewport()->releaseMouse();
#endif
        /* Release mouse buttons: */
        CMouse mouse = m_console.GetMouse();
        mouse.PutMouseEvent (0, 0, 0, 0 /* Horizontal wheel */, 0);
    }

    m_bIsMouseCaptured = fCapture;

    updateMouseClipping();

    if (fEmitSignal)
        emitMouseStateChanged();
}

void UIMachineView::saveKeyStates()
{
    ::memcpy(m_pressedKeysCopy, m_pressedKeys, sizeof(m_pressedKeys));
}

void UIMachineView::releaseAllPressedKeys(bool aReleaseHostKey /* = true */)
{
    CKeyboard keyboard = m_console.GetKeyboard();
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
#endif

    emitKeyboardStateChanged();
}

void UIMachineView::sendChangedKeyStates()
{
    QVector <LONG> codes(2);
    CKeyboard keyboard = m_console.GetKeyboard();
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

void UIMachineView::updateMouseClipping()
{
    if (m_bIsMouseCaptured)
    {
        viewport()->setCursor(QCursor(Qt::BlankCursor));
#ifdef Q_WS_WIN32
        QRect r = viewport()->rect();
        r.moveTopLeft(viewport()->mapToGlobal(QPoint (0, 0)));
        RECT rect = { r.left(), r.top(), r.right() + 1, r.bottom() + 1 };
        ::ClipCursor(&rect);
#endif
    }
    else
    {
#ifdef Q_WS_WIN32
        ::ClipCursor(NULL);
#endif
        /* return the cursor to where it was when we captured it and show it */
        QCursor::setPos(m_capturedMousePos);
        viewport()->unsetCursor();
    }
}

void UIMachineView::setPointerShape(const uchar *pShapeData, bool fHasAlpha,
                                    uint uXHot, uint uYHot, uint uWidth, uint uHeight)
{
    AssertMsg(pShapeData, ("Shape data must not be NULL!\n"));

    bool ok = false;

    const uchar *srcAndMaskPtr = pShapeData;
    uint andMaskSize = (uWidth + 7) / 8 * uHeight;
    const uchar *srcShapePtr = pShapeData + ((andMaskSize + 3) & ~3);
    uint srcShapePtrScan = uWidth * 4;

#if defined (Q_WS_WIN)

    BITMAPV5HEADER bi;
    HBITMAP hBitmap;
    void *lpBits;

    ::ZeroMemory(&bi, sizeof (BITMAPV5HEADER));
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = uWidth;
    bi.bV5Height = - (LONG)uHeight;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    if (fHasAlpha)
        bi.bV5AlphaMask = 0xFF000000;
    else
        bi.bV5AlphaMask = 0;

    HDC hdc = GetDC(NULL);

    /* Create the DIB section with an alpha channel: */
    hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&lpBits, NULL, (DWORD) 0);

    ReleaseDC(NULL, hdc);

    HBITMAP hMonoBitmap = NULL;
    if (fHasAlpha)
    {
        /* Create an empty mask bitmap: */
        hMonoBitmap = CreateBitmap(uWidth, uHeight, 1, 1, NULL);
    }
    else
    {
        /* Word aligned AND mask. Will be allocated and created if necessary. */
        uint8_t *pu8AndMaskWordAligned = NULL;

        /* Width in bytes of the original AND mask scan line. */
        uint32_t cbAndMaskScan = (uWidth + 7) / 8;

        if (cbAndMaskScan & 1)
        {
            /* Original AND mask is not word aligned. */

            /* Allocate memory for aligned AND mask. */
            pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ((cbAndMaskScan + 1) * uHeight);

            Assert(pu8AndMaskWordAligned);

            if (pu8AndMaskWordAligned)
            {
                /* According to MSDN the padding bits must be 0.
                 * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask. */
                uint32_t u32PaddingBits = cbAndMaskScan * 8  - uWidth;
                Assert(u32PaddingBits < 8);
                uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                      u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, uWidth, cbAndMaskScan));

                uint8_t *src = (uint8_t *)srcAndMaskPtr;
                uint8_t *dst = pu8AndMaskWordAligned;

                unsigned i;
                for (i = 0; i < uHeight; i++)
                {
                    memcpy(dst, src, cbAndMaskScan);

                    dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                    src += cbAndMaskScan;
                    dst += cbAndMaskScan + 1;
                }
            }
        }

        /* Create the AND mask bitmap: */
        hMonoBitmap = ::CreateBitmap(uWidth, uHeight, 1, 1,
                                     pu8AndMaskWordAligned? pu8AndMaskWordAligned: srcAndMaskPtr);

        if (pu8AndMaskWordAligned)
        {
            RTMemTmpFree(pu8AndMaskWordAligned);
        }
    }

    Assert(hBitmap);
    Assert(hMonoBitmap);
    if (hBitmap && hMonoBitmap)
    {
        DWORD *dstShapePtr = (DWORD *) lpBits;

        for (uint y = 0; y < uHeight; y ++)
        {
            memcpy(dstShapePtr, srcShapePtr, srcShapePtrScan);
            srcShapePtr += srcShapePtrScan;
            dstShapePtr += uWidth;
        }

        ICONINFO ii;
        ii.fIcon = FALSE;
        ii.xHotspot = uXHot;
        ii.yHotspot = uYHot;
        ii.hbmMask = hMonoBitmap;
        ii.hbmColor = hBitmap;

        HCURSOR hAlphaCursor = CreateIconIndirect(&ii);
        Assert(hAlphaCursor);
        if (hAlphaCursor)
        {
            viewport()->setCursor(QCursor(hAlphaCursor));
            ok = true;
            if (m_alphaCursor)
                DestroyIcon(m_alphaCursor);
            m_alphaCursor = hAlphaCursor;
        }
    }

    if (hMonoBitmap)
        DeleteObject(hMonoBitmap);
    if (hBitmap)
        DeleteObject(hBitmap);

#elif defined (Q_WS_X11) && !defined (VBOX_WITHOUT_XCURSOR)

    XcursorImage *img = XcursorImageCreate(uWidth, uHeight);
    Assert(img);
    if (img)
    {
        img->xhot = uXHot;
        img->yhot = uYHot;

        XcursorPixel *dstShapePtr = img->pixels;

        for (uint y = 0; y < uHeight; y ++)
        {
            memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);

            if (!fHasAlpha)
            {
                /* Convert AND mask to the alpha channel: */
                uchar byte = 0;
                for (uint x = 0; x < uWidth; x ++)
                {
                    if (!(x % 8))
                        byte = *(srcAndMaskPtr ++);
                    else
                        byte <<= 1;

                    if (byte & 0x80)
                    {
                        /* Linux doesn't support inverted pixels (XOR ops,
                         * to be exact) in cursor shapes, so we detect such
                         * pixels and always replace them with black ones to
                         * make them visible at least over light colors */
                        if (dstShapePtr [x] & 0x00FFFFFF)
                            dstShapePtr [x] = 0xFF000000;
                        else
                            dstShapePtr [x] = 0x00000000;
                    }
                    else
                        dstShapePtr [x] |= 0xFF000000;
                }
            }

            srcShapePtr += srcShapePtrScan;
            dstShapePtr += uWidth;
        }

        Cursor cur = XcursorImageLoadCursor(QX11Info::display(), img);
        Assert (cur);
        if (cur)
        {
            viewport()->setCursor(QCursor(cur));
            ok = true;
        }

        XcursorImageDestroy(img);
    }

#elif defined(Q_WS_MAC)

    /* Create a ARGB image out of the shape data. */
    QImage image  (uWidth, uHeight, QImage::Format_ARGB32);
    const uint8_t* pbSrcMask = static_cast<const uint8_t*> (srcAndMaskPtr);
    unsigned cbSrcMaskLine = RT_ALIGN (uWidth, 8) / 8;
    for (unsigned int y = 0; y < uHeight; ++y)
    {
        for (unsigned int x = 0; x < uWidth; ++x)
        {
           unsigned int color = ((unsigned int*)srcShapePtr)[y*uWidth+x];
           /* If the alpha channel isn't in the shape data, we have to
            * create them from the and-mask. This is a bit field where 1
            * represent transparency & 0 opaque respectively. */
           if (!fHasAlpha)
           {
               if (!(pbSrcMask[x / 8] & (1 << (7 - (x % 8)))))
                   color  |= 0xff000000;
               else
               {
                   /* This isn't quite right, but it's the best we can do I think... */
                   if (color & 0x00ffffff)
                       color = 0xff000000;
                   else
                       color = 0x00000000;
               }
           }
           image.setPixel (x, y, color);
        }
        /* Move one scanline forward. */
        pbSrcMask += cbSrcMaskLine;
    }
    /* Set the new cursor: */
    QCursor cursor(QPixmap::fromImage(image), uXHot, uYHot);
    viewport()->setCursor(cursor);
    ok = true;
    NOREF(srcShapePtrScan);

#else

# warning "port me"

#endif

    if (ok)
        m_lastCursor = viewport()->cursor();
    else
        viewport()->unsetCursor();
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

#if defined(Q_WS_MAC)
void UIMachineView::updateDockIcon()
{
    // TODO_NEW_CORE
//    if (mDockIconEnabled)
//    {
//        if (!m_pauseShot.isNull())
//        {
//            CGImageRef pauseImg = ::darwinToCGImageRef (&m_pauseShot);
//            /* Use the pause image as background */
//            mDockIconPreview->updateDockPreview (pauseImg);
//            CGImageRelease (pauseImg);
//        }
//        else
//        {
//# if defined (VBOX_GUI_USE_QUARTZ2D)
//            if (mode() == VBoxDefs::Quartz2DMode)
//            {
//                /* If the render mode is Quartz2D we could use the CGImageRef
//                 * of the framebuffer for the dock icon creation. This saves
//                 * some conversion time. */
//                mDockIconPreview->updateDockPreview(static_cast<UIFrameBufferQuartz2D*>(m_pFrameBuffer)->imageRef());
//            }
//            else
//# endif
//                /* In image mode we have to create the image ref out of the
//                 * framebuffer */
//                mDockIconPreview->updateDockPreview(m_pFrameBuffer);
//        }
//    }
}

void UIMachineView::updateDockOverlay()
{
    /* Only to an update to the realtime preview if this is enabled by the user
     * & we are in an state where the framebuffer is likely valid. Otherwise to
     * the overlay stuff only. */
    // TODO_NEW_CORE
//    if (mDockIconEnabled &&
//        (machineState() == KMachineState_Running ||
//         machineState() == KMachineState_Paused ||
//         machineState() == KMachineState_Teleporting ||
//         machineState() == KMachineState_LiveSnapshotting ||
//         machineState() == KMachineState_Restoring ||
//         machineState() == KMachineState_TeleportingPausedVM ||
//         machineState() == KMachineState_TeleportingIn ||
//         machineState() == KMachineState_Saving))
//        updateDockIcon();
//    else
//        mDockIconPreview->updateDockOverlay();
}

void UIMachineView::setMouseCoalescingEnabled (bool aOn)
{
    /* Enable mouse event compression if we leave the VM view. This
       is necessary for having smooth resizing of the VM/other
       windows.
       Disable mouse event compression if we enter the VM view. So
       all mouse events are registered in the VM. Only do this if
       the keyboard/mouse is grabbed (this is when we have a valid
       event handler). */
    if (aOn || m_fKeyboardGrabbed)
        ::darwinSetMouseCoalescingEnabled (aOn);
}
#endif /* Q_WS_MAC */

