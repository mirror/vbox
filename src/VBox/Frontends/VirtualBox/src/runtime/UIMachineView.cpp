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
# elif defined(VBOX_WITH_HACKED_QT)
#  include "QIApplication.h"
# endif
# include <Carbon/Carbon.h>
# include <VBox/err.h>
#endif /* defined (Q_WS_MAC) */

/** Guest mouse pointer shape change event. */
class MousePointerChangeEvent : public QEvent
{
public:

    MousePointerChangeEvent (bool visible, bool alpha, uint xhot, uint yhot, uint width, uint height, const uchar *shape)
        : QEvent((QEvent::Type)VBoxDefs::MousePointerChangeEventType)
        , vis(visible), alph(alpha), xh(xhot), yh(yhot), w(width), h(height), data(0)
    {
        uint dataSize = ((((width + 7) / 8 * height) + 3) & ~3) + width * 4 * height;

        if (shape)
        {
            data = new uchar[dataSize];
            memcpy((void*)data, (void*)shape, dataSize);
        }
    }

    ~MousePointerChangeEvent()
    {
        if (data) delete[] data;
    }

    bool isVisible() const { return vis; }
    bool hasAlpha() const { return alph; }
    uint xHot() const { return xh; }
    uint yHot() const { return yh; }
    uint width() const { return w; }
    uint height() const { return h; }
    const uchar *shapeData() const { return data; }

private:

    bool vis, alph;
    uint xh, yh, w, h;
    const uchar *data;
};

/** Guest mouse absolute positioning capability change event. */
class MouseCapabilityEvent : public QEvent
{
public:

    MouseCapabilityEvent (bool supportsAbsolute, bool needsHostCursor)
        : QEvent((QEvent::Type) VBoxDefs::MouseCapabilityEventType)
        , can_abs(supportsAbsolute), needs_host_cursor(needsHostCursor) {}

    bool supportsAbsolute() const { return can_abs; }
    bool needsHostCursor() const { return needs_host_cursor; }

private:

    bool can_abs;
    bool needs_host_cursor;
};

/** Machine state change. */
class StateChangeEvent : public QEvent
{
public:

    StateChangeEvent (KMachineState state)
        : QEvent((QEvent::Type)VBoxDefs::MachineStateChangeEventType)
        , s (state) {}

    KMachineState machineState() const { return s; }

private:

    KMachineState s;
};

/** Guest Additions property changes. */
class GuestAdditionsEvent : public QEvent
{
public:

    GuestAdditionsEvent (const QString &aOsTypeId,
                         const QString &aAddVersion,
                         bool aAddActive,
                         bool aSupportsSeamless,
                         bool aSupportsGraphics)
        : QEvent((QEvent::Type)VBoxDefs::AdditionsStateChangeEventType)
        , mOsTypeId(aOsTypeId), mAddVersion(aAddVersion)
        , mAddActive(aAddActive), mSupportsSeamless(aSupportsSeamless)
        , mSupportsGraphics (aSupportsGraphics) {}

    const QString &osTypeId() const { return mOsTypeId; }
    const QString &additionVersion() const { return mAddVersion; }
    bool additionActive() const { return mAddActive; }
    bool supportsSeamless() const { return mSupportsSeamless; }
    bool supportsGraphics() const { return mSupportsGraphics; }

private:

    QString mOsTypeId;
    QString mAddVersion;
    bool mAddActive;
    bool mSupportsSeamless;
    bool mSupportsGraphics;
};

/** DVD/Floppy drive change event */
class MediaDriveChangeEvent : public QEvent
{
public:

    MediaDriveChangeEvent(VBoxDefs::MediumType aType)
        : QEvent ((QEvent::Type) VBoxDefs::MediaDriveChangeEventType)
        , mType (aType) {}
    VBoxDefs::MediumType type() const { return mType; }

private:

    VBoxDefs::MediumType mType;
};

/** Menu activation event */
class ActivateMenuEvent : public QEvent
{
public:

    ActivateMenuEvent (QAction *aData)
        : QEvent((QEvent::Type)VBoxDefs::ActivateMenuEventType)
        , mAction(aData) {}
    QAction *action() const { return mAction; }

private:

    QAction *mAction;
};

/** VM Runtime error event */
class RuntimeErrorEvent : public QEvent
{
public:

    RuntimeErrorEvent(bool aFatal, const QString &aErrorID, const QString &aMessage)
        : QEvent((QEvent::Type)VBoxDefs::RuntimeErrorEventType)
        , mFatal(aFatal), mErrorID(aErrorID), mMessage(aMessage) {}

    bool fatal() const { return mFatal; }
    QString errorID() const { return mErrorID; }
    QString message() const { return mMessage; }

private:

    bool mFatal;
    QString mErrorID;
    QString mMessage;
};

/** Modifier key change event */
class ModifierKeyChangeEvent : public QEvent
{
public:

    ModifierKeyChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock)
        : QEvent((QEvent::Type)VBoxDefs::ModifierKeyChangeEventType)
        , mNumLock(fNumLock), mCapsLock(fCapsLock), mScrollLock(fScrollLock) {}

    bool numLock()    const { return mNumLock; }
    bool capsLock()   const { return mCapsLock; }
    bool scrollLock() const { return mScrollLock; }

private:

    bool mNumLock, mCapsLock, mScrollLock;
};

/** Network adapter change event */
class NetworkAdapterChangeEvent : public QEvent
{
public:

    NetworkAdapterChangeEvent(INetworkAdapter *aAdapter)
        : QEvent((QEvent::Type)VBoxDefs::NetworkAdapterChangeEventType)
        , mAdapter(aAdapter) {}

    INetworkAdapter* networkAdapter() { return mAdapter; }

private:

    INetworkAdapter *mAdapter;
};

/** USB controller state change event */
class USBControllerStateChangeEvent : public QEvent
{
public:

    USBControllerStateChangeEvent()
        : QEvent((QEvent::Type)VBoxDefs::USBCtlStateChangeEventType) {}
};

/** USB device state change event */
class USBDeviceStateChangeEvent : public QEvent
{
public:

    USBDeviceStateChangeEvent(const CUSBDevice &aDevice, bool aAttached, const CVirtualBoxErrorInfo &aError)
        : QEvent((QEvent::Type)VBoxDefs::USBDeviceStateChangeEventType)
        , mDevice(aDevice), mAttached(aAttached), mError(aError) {}

    CUSBDevice device() const { return mDevice; }
    bool attached() const { return mAttached; }
    CVirtualBoxErrorInfo error() const { return mError; }

private:

    CUSBDevice mDevice;
    bool mAttached;
    CVirtualBoxErrorInfo mError;
};

class VBoxViewport: public QWidget
{
public:

    VBoxViewport(QWidget *pParent) : QWidget(pParent)
    {
        /* No need for background drawing */
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

class UIConsoleCallback : VBOX_SCRIPTABLE_IMPL(IConsoleCallback)
{
public:

    UIConsoleCallback (UIMachineView *v)
    {
#if defined (Q_WS_WIN)
        mRefCnt = 0;
#endif
        mView = v;
    }

    virtual ~UIConsoleCallback() {}

    NS_DECL_ISUPPORTS

#if defined (Q_WS_WIN)
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&mRefCnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement(&mRefCnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
#endif
    VBOX_SCRIPTABLE_DISPATCH_IMPL(IConsoleCallback)

    STDMETHOD(OnMousePointerShapeChange) (BOOL visible, BOOL alpha,
                                          ULONG xhot, ULONG yhot,
                                          ULONG width, ULONG height,
                                          BYTE *shape)
    {
        QApplication::postEvent(mView, new MousePointerChangeEvent(visible, alpha, xhot, yhot, width, height, shape));
        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange)(BOOL supportsAbsolute, BOOL needsHostCursor)
    {
        QApplication::postEvent(mView, new MouseCapabilityEvent (supportsAbsolute, needsHostCursor));
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        QApplication::postEvent(mView, new ModifierKeyChangeEvent (fNumLock, fCapsLock, fScrollLock));
        return S_OK;
    }

    STDMETHOD(OnStateChange)(MachineState_T machineState)
    {
        QApplication::postEvent(mView, new StateChangeEvent ((KMachineState) machineState));
        return S_OK;
    }

    STDMETHOD(OnAdditionsStateChange)()
    {
        CGuest guest = mView->console().GetGuest();
        QApplication::postEvent (mView, new GuestAdditionsEvent(guest.GetOSTypeId(), guest.GetAdditionsVersion(),
                                                                guest.GetAdditionsActive(), guest.GetSupportsSeamless(),
                                                                guest.GetSupportsGraphics()));
        return S_OK;
    }

    STDMETHOD(OnNetworkAdapterChange) (INetworkAdapter *aNetworkAdapter)
    {
        QApplication::postEvent (mView, new NetworkAdapterChangeEvent (aNetworkAdapter));
        return S_OK;
    }

    STDMETHOD(OnStorageControllerChange) ()
    {
        //QApplication::postEvent(mView, new StorageControllerChangeEvent());
        return S_OK;
    }

    STDMETHOD(OnMediumChange)(IMediumAttachment *aMediumAttachment)
    {
        CMediumAttachment att(aMediumAttachment);
        switch (att.GetType())
        {
            case KDeviceType_Floppy:
                QApplication::postEvent(mView, new MediaDriveChangeEvent(VBoxDefs::MediumType_Floppy));
                break;
            case KDeviceType_DVD:
                QApplication::postEvent(mView, new MediaDriveChangeEvent(VBoxDefs::MediumType_DVD));
                break;
            default:
                break;
        }
        return S_OK;
    }

    STDMETHOD(OnCPUChange)(ULONG aCPU, BOOL aRemove)
    {
        NOREF(aCPU);
        NOREF(aRemove);
        return S_OK;
    }

    STDMETHOD(OnSerialPortChange) (ISerialPort *aSerialPort)
    {
        NOREF(aSerialPort);
        return S_OK;
    }

    STDMETHOD(OnParallelPortChange) (IParallelPort *aParallelPort)
    {
        NOREF(aParallelPort);
        return S_OK;
    }

    STDMETHOD(OnVRDPServerChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnRemoteDisplayInfoChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnUSBControllerChange)()
    {
        QApplication::postEvent (mView, new USBControllerStateChangeEvent());
        return S_OK;
    }

    STDMETHOD(OnUSBDeviceStateChange)(IUSBDevice *aDevice, BOOL aAttached, IVirtualBoxErrorInfo *aError)
    {
        QApplication::postEvent (mView, new USBDeviceStateChangeEvent(CUSBDevice(aDevice), bool(aAttached), CVirtualBoxErrorInfo(aError)));
        return S_OK;
    }

    STDMETHOD(OnSharedFolderChange) (Scope_T aScope)
    {
        NOREF(aScope);
        QApplication::postEvent (mView, new QEvent((QEvent::Type)VBoxDefs::SharedFolderChangeEventType));
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, IN_BSTR id, IN_BSTR message)
    {
        QApplication::postEvent (mView, new RuntimeErrorEvent(!!fatal, QString::fromUtf16(id), QString::fromUtf16(message)));
        return S_OK;
    }

    STDMETHOD(OnCanShowWindow) (BOOL *canShow)
    {
        if (!canShow)
            return E_POINTER;

        *canShow = TRUE;
        return S_OK;
    }

    STDMETHOD(OnShowWindow) (ULONG64 *winId)
    {
        if (!winId)
            return E_POINTER;

#if defined (Q_WS_MAC)
        /*
         * Let's try the simple approach first - grab the focus.
         * Getting a window out of the dock (minimized or whatever it's called)
         * needs to be done on the GUI thread, so post it a note.
         */
        *winId = 0;
        if (!mView)
            return S_OK;

        ProcessSerialNumber psn = { 0, kCurrentProcess };
        OSErr rc = ::SetFrontProcess (&psn);
        if (!rc)
            QApplication::postEvent(mView, new QEvent((QEvent::Type)VBoxDefs::ShowWindowEventType));
        else
        {
            /*
             * It failed for some reason, send the other process our PSN so it can try.
             * (This is just a precaution should Mac OS X start imposing the same sensible
             * focus stealing restrictions that other window managers implement.)
             */
            AssertMsgFailed(("SetFrontProcess -> %#x\n", rc));
            if (::GetCurrentProcess (&psn))
                *winId = RT_MAKE_U64(psn.lowLongOfPSN, psn.highLongOfPSN);
        }

#else
        /* Return the ID of the top-level console window. */
        *winId = (ULONG64)mView->window()->winId();
#endif

        return S_OK;
    }

protected:

    UIMachineView *mView;

#if defined (Q_WS_WIN)
private:
    long mRefCnt;
#endif
};

#if !defined (Q_WS_WIN)
NS_DECL_CLASSINFO(UIConsoleCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(UIConsoleCallback, IConsoleCallback)
#endif

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

void UIMachineView::onViewOpened()
{
    /* Variable m_bIsMachineWindowResizeIgnored was initially "true" to ignore QT
     * initial resize event in case of auto-resize feature is on.
     * Currently, initial resize event is already processed, so we set
     * m_bIsMachineWindowResizeIgnored to "false" to process all further resize
     * events as user-initiated window resize events. */
    m_bIsMachineWindowResizeIgnored = false;
}

int UIMachineView::contentsX() const
{
    return horizontalScrollBar()->value();
}

int UIMachineView::contentsY() const
{
    return verticalScrollBar()->value();
}

QRect UIMachineView::desktopGeometry() const
{
    QRect rc;
    switch (mDesktopGeo)
    {
        case DesktopGeo_Fixed:
        case DesktopGeo_Automatic:
            rc = QRect(0, 0,
                       qMax(mDesktopGeometry.width(), mStoredConsoleSize.width()),
                       qMax(mDesktopGeometry.height(), mStoredConsoleSize.height()));
            break;
        case DesktopGeo_Any:
            rc = QRect(0, 0, 0, 0);
            break;
        default:
            AssertMsgFailed(("Bad geometry type %d\n", mDesktopGeo));
    }
    return rc;
}

void UIMachineView::setIgnoreGuestResize(bool bIgnore)
{
    m_bIsGuestResizeIgnored = bIgnore;
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
    /* Protected members: */
    , mode(renderMode)
    , m_bIsGuestSupportsGraphics(false)
    /* Private members: */
    , m_pMachineWindow(pMachineWindow)
    , m_console(pMachineWindow->machineLogic()->session().GetConsole())
    , m_globalSettings(vboxGlobal().settings())
    , m_iLastMouseWheelDelta(0)
    , muNumLockAdaptionCnt(2)
    , muCapsLockAdaptionCnt(2)
    , m_bIsAutoCaptureDisabled(false)
    , m_bIsKeyboardCaptured(false)
    , m_bIsMouseCaptured(false)
    , m_bIsMouseAbsolute(false)
    , m_bIsMouseIntegrated(true)
    , m_bIsHostkeyPressed(false)
    , m_bIsHostkeyAlone (false)
    , m_bIsMachineWindowResizeIgnored(true)
    , m_bIsFrameBufferResizeIgnored(false)
    , m_bIsGuestResizeIgnored(false)
    , mDoResize(false)
    , mNumLock(false)
    , mScrollLock(false)
    , mCapsLock(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , mAccelerate2DVideo(bAccelerate2DVideo)
#endif
#if defined(Q_WS_WIN)
    , mAlphaCursor (NULL)
#endif
#if defined(Q_WS_MAC)
# if !defined (VBOX_WITH_HACKED_QT) && !defined (QT_MAC_USE_COCOA)
    , mDarwinEventHandlerRef (NULL)
# endif
    , mDarwinKeyModifiers (0)
    , mKeyboardGrabbed (false)
    , mDockIconEnabled (true)
#endif
    , mDesktopGeo (DesktopGeo_Invalid)
    , mPassCAD (false)
      /* Don't show a hardware pointer until we have one to show */
    , mHideHostPointer (true)
{
    Assert (!m_console.isNull() && !m_console.GetDisplay().isNull() && !m_console.GetKeyboard().isNull() && !m_console.GetMouse().isNull());

#ifdef Q_WS_MAC
    /* Overlay logo for the dock icon */
    //mVirtualBoxLogo = ::darwinToCGImageRef("VirtualBox_cube_42px.png");
    QString osTypeId = m_console.GetGuest().GetOSTypeId();
    mDockIconPreview = new VBoxDockIconPreview(machineWindowWrapper(), vboxGlobal().vmGuestOSTypeIcon (osTypeId));

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
#endif /* QT_WS_MAC */

    /* No frame around the view */
    setFrameStyle(QFrame::NoFrame);

#ifdef VBOX_GUI_USE_QGL
    QWidget *pViewport;
    switch (mode)
    {
        // TODO: Fix that!
        //case VBoxDefs::QGLMode:
        //    pViewport = new VBoxGLWidget(this, this, NULL);
        //    break;
        default:
            pViewport = new VBoxViewport(this);
    }
#else
    VBoxViewport *pViewport = new VBoxViewport(this);
#endif
    setViewport(pViewport);
//    pViewport->vboxDoInit();

    /* enable MouseMove events */
    viewport()->setMouseTracking(true);

    /*
     *  QScrollView does the below on its own, but let's do it anyway
     *  for the case it will not do it in the future.
     */
    viewport()->installEventFilter(this);

    /* to fix some focus issues */
    // TODO: Fix that!
    //machineWindowWrapper()->menuBar()->installEventFilter(this);

    /* we want to be notified on some parent's events */
    machineWindowWrapper()->machineWindow()->installEventFilter(this);

#ifdef Q_WS_X11
    /* initialize the X keyboard subsystem */
    initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif

    ::memset(mPressedKeys, 0, sizeof(mPressedKeys));

    /* setup rendering */

    CDisplay display = m_console.GetDisplay();
    Assert(!display.isNull());

    mFrameBuf = NULL;

    LogFlowFunc(("Rendering mode: %d\n", mode));

    switch (mode)
    {
#if defined (VBOX_GUI_USE_QGL)
// TODO: Fix that!
//        case VBoxDefs::QGLMode:
//            mFrameBuf = new VBoxQGLFrameBuffer(this);
//            break;
//        case VBoxDefs::QGLOverlayMode:
//            mFrameBuf = new VBoxQGLOverlayFrameBuffer(this);
//            break;
#endif
#if defined (VBOX_GUI_USE_QIMAGE)
        case VBoxDefs::QImageMode:
            mFrameBuf =
#ifdef VBOX_WITH_VIDEOHWACCEL
//                    mAccelerate2DVideo ? new VBoxOverlayFrameBuffer<UIFrameBufferQImage>(this, &machineWindowWrapper()->session()) :
#endif
                    new UIFrameBufferQImage(this);
            break;
#endif
#if defined (VBOX_GUI_USE_SDL)
        case VBoxDefs::SDLMode:
            /* Indicate that we are doing all
             * drawing stuff ourself */
            pViewport->setAttribute(Qt::WA_PaintOnScreen);
# ifdef Q_WS_X11
            /* This is somehow necessary to prevent strange X11 warnings on
             * i386 and segfaults on x86_64. */
            XFlush(QX11Info::display());
# endif
//            mFrameBuf =
#if defined(VBOX_WITH_VIDEOHWACCEL) && defined(DEBUG_misha) /* not tested yet */
//                mAccelerate2DVideo ? new VBoxOverlayFrameBuffer<VBoxSDLFrameBuffer> (this, &machineWindowWrapper()->session()) :
#endif
//                new VBoxSDLFrameBuffer(this);
            /*
             *  disable scrollbars because we cannot correctly draw in a
             *  scrolled window using SDL
             */
            horizontalScrollBar()->setEnabled(false);
            verticalScrollBar()->setEnabled(false);
            break;
#endif
#if defined (VBOX_GUI_USE_DDRAW)
        case VBoxDefs::DDRAWMode:
            mFrameBuf = new VBoxDDRAWFrameBuffer(this);
            break;
#endif
#if defined (VBOX_GUI_USE_QUARTZ2D)
        case VBoxDefs::Quartz2DMode:
            /* Indicate that we are doing all
             * drawing stuff ourself */
            pViewport->setAttribute(Qt::WA_PaintOnScreen);
            mFrameBuf =
#ifdef VBOX_WITH_VIDEOHWACCEL
                    mAccelerate2DVideo ? new VBoxOverlayFrameBuffer<VBoxQuartz2DFrameBuffer>(this, &machineWindowWrapper()->session()) :
#endif
            	    new VBoxQuartz2DFrameBuffer(this);
            break;
#endif
        default:
            AssertReleaseMsgFailed(("Render mode must be valid: %d\n", mode));
            LogRel (("Invalid render mode: %d\n", mode));
            qApp->exit (1);
            break;
    }

#if defined (VBOX_GUI_USE_DDRAW)
    if (!mFrameBuf || mFrameBuf->address() == NULL)
    {
        if (mFrameBuf)
            delete mFrameBuf;
        mode = VBoxDefs::QImageMode;
        mFrameBuf = new UIFrameBufferQImage(this);
    }
#endif

    if (mFrameBuf)
    {
        mFrameBuf->AddRef();
        display.SetFramebuffer(VBOX_VIDEO_PRIMARY_SCREEN, CFramebuffer(mFrameBuf));
    }

    /* setup the callback */
    mCallback = CConsoleCallback(new UIConsoleCallback(this));
    m_console.RegisterCallback(mCallback);
    AssertWrapperOk(m_console);

    QPalette palette(viewport()->palette());
    palette.setColor(viewport()->backgroundRole(), Qt::black);
    viewport()->setPalette(palette);

    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    setMaximumSize(sizeHint());

    setFocusPolicy(Qt::WheelFocus);

    /* Remember the desktop geometry and register for geometry change
       events for telling the guest about video modes we like. */

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
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(doResizeDesktop(int)));

    QString passCAD = m_console.GetMachine().GetExtraData(VBoxDefs::GUI_PassCAD);
    if (!passCAD.isEmpty() && ((passCAD != "false") || (passCAD != "no")))
        mPassCAD = true;

#if defined (Q_WS_WIN)
    gView = this;
#endif

#if defined (Q_WS_PM)
    bool ok = VBoxHlpInstallKbdHook(0, winId(), UM_PREACCEL_CHAR);
    Assert (ok);
    NOREF (ok);
#endif

#ifdef Q_WS_MAC
    /* Dock icon update connection */
    connect(&vboxGlobal(), SIGNAL(dockIconUpdateChanged(const VBoxChangeDockIconUpdateEvent &)),
            this, SLOT(sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &)));
    /* Presentation Mode connection */
    connect(&vboxGlobal(), SIGNAL(presentationModeChanged(const VBoxChangePresentationModeEvent &)),
            this, SLOT(sltChangePresentationMode(const VBoxChangePresentationModeEvent &)));
#endif
}

UIMachineView::~UIMachineView()
{
#if 0
#if defined (Q_WS_PM)
    bool ok = VBoxHlpUninstallKbdHook(0, winId(), UM_PREACCEL_CHAR);
    Assert (ok);
    NOREF (ok);
#endif

#if defined (Q_WS_WIN)
    if (gKbdHook)
        UnhookWindowsHookEx(gKbdHook);
    gView = 0;
    if (mAlphaCursor)
        DestroyIcon(mAlphaCursor);
#endif

    if (mFrameBuf)
    {
        /* detach our framebuffer from Display */
        CDisplay display = m_console.GetDisplay();
        Assert(!display.isNull());
        display.SetFramebuffer(VBOX_VIDEO_PRIMARY_SCREEN, CFramebuffer(NULL));
        /* release the reference */
        mFrameBuf->Release();
        mFrameBuf = NULL;
    }

    m_console.UnregisterCallback(mCallback);

#if defined (Q_WS_MAC)
# if !defined (QT_MAC_USE_COCOA)
    if (mDarwinWindowOverlayHandlerRef)
    {
        ::RemoveEventHandler(mDarwinWindowOverlayHandlerRef);
        mDarwinWindowOverlayHandlerRef = NULL;
    }
# endif
    delete mDockIconPreview;
    mDockIconPreview = NULL;
#endif
#endif
}

void UIMachineView::calculateDesktopGeometry()
{
    /* This method should not get called until we have initially set up the */
    Assert((mDesktopGeo != DesktopGeo_Invalid));
    /* If we are not doing automatic geometry calculation then there is nothing to do. */
    if (DesktopGeo_Automatic == mDesktopGeo)
    {
        /* Available geometry of the desktop.  If the desktop is a single
         * screen, this will exclude space taken up by desktop taskbars
         * and things, but this is unfortunately not true for the more
         * complex case of a desktop spanning multiple screens. */
        QRect desktop = availableGeometry();
        /* The area taken up by the console window on the desktop,
         * including window frame, title and menu bar and whatnot. */
        QRect frame = machineWindowWrapper()->machineWindow()->frameGeometry();
        /* The area taken up by the console window, minus all decorations. */
        //QRect window = machineWindowWrapper()->centralWidget()->geometry(); // TODO check that!
        /* To work out how big we can make the console window while still
         * fitting on the desktop, we calculate desktop - frame + window.
         * This works because the difference between frame and window
         * (or at least its width and height) is a constant. */
        //mDesktopGeometry = QRect(0, 0, desktop.width() - frame.width() + window.width(),
        //                               desktop.height() - frame.height() + window.height());
    }
}

QSize UIMachineView::sizeHint() const
{
#ifdef VBOX_WITH_DEBUGGER
    /** @todo figure out a more proper fix. */
    /* HACK ALERT! Really ugly workaround for the resizing to 9x1 done
     *             by DevVGA if provoked before power on. */
    QSize fb(mFrameBuf->width(), mFrameBuf->height());
    if ((fb.width() < 16 || fb.height() < 16) && (vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled()))
        fb = QSize(640, 480);
    return QSize(fb.width() + frameWidth() * 2, fb.height() + frameWidth() * 2);
#else
    return QSize(mFrameBuf->width() + frameWidth() * 2, mFrameBuf->height() + frameWidth() * 2);
#endif
}

#ifdef Q_WS_MAC
void UIMachineLogic::sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event)
{
    setDockIconEnabled(event.mChanged);
    updateDockOverlay();
}

# ifdef QT_MAC_USE_COCOA
void UIMachineLogic::sltChangePresentationMode(const VBoxChangePresentationModeEvent &event)
{
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
}
# endif /* QT_MAC_USE_COCOA */
#endif

int UIMachineView::contentsWidth() const
{
    return mFrameBuf->width();
}

int UIMachineView::contentsHeight() const
{
    return mFrameBuf->height();
}

int UIMachineView::visibleWidth() const
{
    return horizontalScrollBar()->pageStep();
}

int UIMachineView::visibleHeight() const
{
    return verticalScrollBar()->pageStep();
}

bool UIMachineView::event(QEvent *e)
{
    //if (m_bIsAttached)
    {
        switch (e->type())
        {
            case QEvent::FocusIn:
            {
                if (isRunning())
                    focusEvent (true);
                break;
            }
            case QEvent::FocusOut:
            {
                if (isRunning())
                    focusEvent (false);
                else
                {
                    /* release the host key and all other pressed keys too even
                     * when paused (otherwise, we will get stuck keys in the
                     * guest when doing sendChangedKeyStates() on resume because
                     * key presses were already recorded in mPressedKeys but key
                     * releases will most likely not reach us but the new focus
                     * window instead). */
                    releaseAllPressedKeys (true /* aReleaseHostKey */);
                }
                break;
            }

            case VBoxDefs::ResizeEventType:
            {
                /* Some situations require initial VGA Resize Request
                 * to be ignored at all, leaving previous framebuffer,
                 * console widget and vm window size preserved. */
                if (m_bIsGuestResizeIgnored)
                    return true;

                bool oldIgnoreMainwndResize = m_bIsMachineWindowResizeIgnored;
                m_bIsMachineWindowResizeIgnored = true;

                UIResizeEvent *re = (UIResizeEvent *) e;
                LogFlow (("VBoxDefs::ResizeEventType: %d x %d x %d bpp\n",
                          re->width(), re->height(), re->bitsPerPixel()));
#ifdef DEBUG_michael
                LogRel (("Resize event from guest: %d x %d x %d bpp\n",
                         re->width(), re->height(), re->bitsPerPixel()));
#endif

                /* Store the new size to prevent unwanted resize hints being
                 * sent back. */
                storeConsoleSize(re->width(), re->height());
                /* do frame buffer dependent resize */

                /* restoreOverrideCursor() is broken in Qt 4.4.0 if WA_PaintOnScreen
                 * widgets are present. This is the case on linux with SDL. As
                 * workaround we save/restore the arrow cursor manually. See
                 * http://trolltech.com/developer/task-tracker/index_html?id=206165&method=entry
                 * for details.
                 *
                 * Moreover the current cursor, which could be set by the guest,
                 * should be restored after resize.
                 */
                QCursor cursor;
                if (shouldHideHostPointer())
                    cursor = QCursor (Qt::BlankCursor);
                else
                    cursor = viewport()->cursor();
                mFrameBuf->resizeEvent (re);
                viewport()->setCursor (cursor);

#ifdef Q_WS_MAC
                mDockIconPreview->setOriginalSize (re->width(), re->height());
#endif /* Q_WS_MAC */

                /* This event appears in case of guest video was changed
                 * for somehow even without video resolution change.
                 * In this last case the host VM window will not be resized
                 * according this event and the host mouse cursor which was
                 * unset to default here will not be hidden in capture state.
                 * So it is necessary to perform updateMouseClipping() for
                 * the guest resize event if the mouse cursor was captured. */
                if (m_bIsMouseCaptured)
                    updateMouseClipping();

                /* apply maximum size restriction */
                setMaximumSize (sizeHint());

                maybeRestrictMinimumSize();

                /* resize the guest canvas */
                if (!m_bIsFrameBufferResizeIgnored)
                    resize (re->width(), re->height());
                updateSliders();
                /* Let our toplevel widget calculate its sizeHint properly. */
#ifdef Q_WS_X11
                /* We use processEvents rather than sendPostedEvents & set the
                 * time out value to max cause on X11 otherwise the layout
                 * isn't calculated correctly. Dosn't find the bug in Qt, but
                 * this could be triggered through the async nature of the X11
                 * window event system. */
                QCoreApplication::processEvents (QEventLoop::AllEvents, INT_MAX);
#else /* Q_WS_X11 */
                QCoreApplication::sendPostedEvents (0, QEvent::LayoutRequest);
#endif /* Q_WS_X11 */

                if (!m_bIsFrameBufferResizeIgnored)
                    normalizeGeometry (true /* adjustPosition */);

                /* report to the VM thread that we finished resizing */
                m_console.GetDisplay().ResizeCompleted (0);

                m_bIsMachineWindowResizeIgnored = oldIgnoreMainwndResize;

                /* update geometry after entering fullscreen | seamless */
                // if (machineWindowWrapper()->isTrueFullscreen() || machineWindowWrapper()->isTrueSeamless()) TODO check that!
                    updateGeometry();

                /* make sure that all posted signals are processed */
                qApp->processEvents();

                /* emit a signal about guest was resized */
                emit resizeHintDone();

                /* We also recalculate the desktop geometry if this is determined
                 * automatically.  In fact, we only need this on the first resize,
                 * but it is done every time to keep the code simpler. */
                calculateDesktopGeometry();

                /* Enable frame-buffer resize watching. */
                if (m_bIsFrameBufferResizeIgnored)
                {
                    m_bIsFrameBufferResizeIgnored = false;
                }

                // machineWindowWrapper()->onDisplayResize (re->width(), re->height()); TODO check that!

                return true;
            }

            /* See VBox[QImage|SDL]FrameBuffer::NotifyUpdate(). */
            case VBoxDefs::RepaintEventType:
            {
                UIRepaintEvent *re = (UIRepaintEvent *) e;
                viewport()->repaint (re->x() - contentsX(),
                                     re->y() - contentsY(),
                                     re->width(), re->height());
                /* m_console.GetDisplay().UpdateCompleted(); - the event was acked already */
                return true;
            }

#ifdef VBOX_WITH_VIDEOHWACCEL
            case VBoxDefs::VHWACommandProcessType:
            {
                mFrameBuf->doProcessVHWACommand(e);
                return true;
            }
#endif

            #if 0
            // TODO check that!
            case VBoxDefs::SetRegionEventType:
            {
                VBoxSetRegionEvent *sre = (VBoxSetRegionEvent*) e;
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

            case VBoxDefs::MousePointerChangeEventType:
            {
                MousePointerChangeEvent *me = (MousePointerChangeEvent *) e;
                /* change cursor shape only when mouse integration is
                 * supported (change mouse shape type event may arrive after
                 * mouse capability change that disables integration */
                if (m_bIsMouseAbsolute)
                    setPointerShape (me);
                else
                    /* Note: actually we should still remember the requested
                     * cursor shape.  If we can't do that, at least remember
                     * the requested visiblilty. */
                    mHideHostPointer = !me->isVisible();
                return true;
            }
            case VBoxDefs::MouseCapabilityEventType:
            {
                MouseCapabilityEvent *me = (MouseCapabilityEvent *) e;
                if (m_bIsMouseAbsolute != me->supportsAbsolute())
                {
                    m_bIsMouseAbsolute = me->supportsAbsolute();
                    /* correct the mouse capture state and reset the cursor
                     * to the default shape if necessary */
                    if (m_bIsMouseAbsolute)
                    {
                        CMouse mouse = m_console.GetMouse();
                        mouse.PutMouseEventAbsolute (-1, -1, 0,
                                                     0 /* Horizontal wheel */,
                                                     0);
                        captureMouse (false, false);
                    }
                    else
                        viewport()->unsetCursor();
                    emitMouseStateChanged();
                    vboxProblem().remindAboutMouseIntegration (m_bIsMouseAbsolute);
                }
                if (me->needsHostCursor())
                    setMouseIntegrationLocked (false);
                else
                    setMouseIntegrationLocked (true);
                return true;
            }

            case VBoxDefs::ModifierKeyChangeEventType:
            {
                ModifierKeyChangeEvent *me = (ModifierKeyChangeEvent* )e;
                if (me->numLock() != mNumLock)
                    muNumLockAdaptionCnt = 2;
                if (me->capsLock() != mCapsLock)
                    muCapsLockAdaptionCnt = 2;
                mNumLock    = me->numLock();
                mCapsLock   = me->capsLock();
                mScrollLock = me->scrollLock();
                return true;
            }

            case VBoxDefs::MachineStateChangeEventType:
            {
                StateChangeEvent *me = (StateChangeEvent *) e;
                LogFlowFunc (("MachineStateChangeEventType: state=%d\n",
                               me->machineState()));
                onStateChange (me->machineState());
                emit machineStateChanged (me->machineState());
                return true;
            }

            case VBoxDefs::AdditionsStateChangeEventType:
            {
                GuestAdditionsEvent *ge = (GuestAdditionsEvent *) e;
                LogFlowFunc (("AdditionsStateChangeEventType\n"));

                /* Always send a size hint if we are in fullscreen or seamless
                 * when the graphics capability is enabled, in case the host
                 * resolution has changed since the VM was last run. */
#if 0
                if (!mDoResize && !m_bIsGuestSupportsGraphics &&
                    ge->supportsGraphics() &&
                    (machineWindowWrapper()->isTrueSeamless() || machineWindowWrapper()->isTrueFullscreen()))
                    mDoResize = true;
#endif

                m_bIsGuestSupportsGraphics = ge->supportsGraphics();

                maybeRestrictMinimumSize();

#if 0
                /* This will only be acted upon if mDoResize is true. */
                doResizeHint();
#endif

                emit additionsStateChanged (ge->additionVersion(),
                                            ge->additionActive(),
                                            ge->supportsSeamless(),
                                            ge->supportsGraphics());
                return true;
            }

            case VBoxDefs::MediaDriveChangeEventType:
            {
                MediaDriveChangeEvent *mce = (MediaDriveChangeEvent *) e;
                LogFlowFunc (("MediaChangeEvent\n"));

                emit mediaDriveChanged (mce->type());
                return true;
            }

            #if 0
            case VBoxDefs::ActivateMenuEventType:
            {
                ActivateMenuEvent *ame = (ActivateMenuEvent *) e;
                ame->action()->trigger();

                /*
                 *  The main window and its children can be destroyed at this
                 *  point (if, for example, the activated menu item closes the
                 *  main window). Detect this situation to prevent calls to
                 *  destroyed widgets.
                 */
                QWidgetList list = QApplication::topLevelWidgets();
                bool destroyed = list.indexOf (machineWindowWrapper()->machineWindow()) < 0;
                if (!destroyed && machineWindowWrapper()->machineWindow()->statusBar())
                    machineWindowWrapper()->machineWindow()->statusBar()->clearMessage();

                return true;
            }
            #endif

            case VBoxDefs::NetworkAdapterChangeEventType:
            {
                /* no specific adapter information stored in this
                 * event is currently used */
                emit networkStateChange();
                return true;
            }

            case VBoxDefs::USBCtlStateChangeEventType:
            {
                emit usbStateChange();
                return true;
            }

            case VBoxDefs::USBDeviceStateChangeEventType:
            {
                USBDeviceStateChangeEvent *ue = (USBDeviceStateChangeEvent *)e;

                bool success = ue->error().isNull();

                if (!success)
                {
                    if (ue->attached())
                        vboxProblem().cannotAttachUSBDevice (
                            m_console,
                            vboxGlobal().details (ue->device()), ue->error());
                    else
                        vboxProblem().cannotDetachUSBDevice (
                            m_console,
                            vboxGlobal().details (ue->device()), ue->error());
                }

                emit usbStateChange();

                return true;
            }

            case VBoxDefs::SharedFolderChangeEventType:
            {
                emit sharedFoldersChanged();
                return true;
            }

            case VBoxDefs::RuntimeErrorEventType:
            {
                RuntimeErrorEvent *ee = (RuntimeErrorEvent *) e;
                vboxProblem().showRuntimeError (m_console, ee->fatal(),
                                                ee->errorID(), ee->message());
                return true;
            }

            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            {
                QKeyEvent *ke = (QKeyEvent *) e;

#ifdef Q_WS_PM
                /// @todo temporary solution to send Alt+Tab and friends to
                //  the guest. The proper solution is to write a keyboard
                //  driver that will steal these combos from the host (it's
                //  impossible to do so using hooks on OS/2).

                if (m_bIsHostkeyPressed)
                {
                    bool pressed = e->type() == QEvent::KeyPress;
                    CKeyboard keyboard = m_console.GetKeyboard();

                    /* whether the host key is Shift so that it will modify
                     * the hot key values? Note that we don't distinguish
                     * between left and right shift here (too much hassle) */
                    const bool kShift = (m_globalSettings.hostKey() == VK_SHIFT ||
                                        m_globalSettings.hostKey() == VK_LSHIFT) &&
                                        (ke->state() & Qt::ShiftModifier);
                    /* define hot keys according to the Shift state */
                    const int kAltTab      = kShift ? Qt::Key_Exclam     : Qt::Key_1;
                    const int kAltShiftTab = kShift ? Qt::Key_At         : Qt::Key_2;
                    const int kCtrlEsc     = kShift ? Qt::Key_AsciiTilde : Qt::Key_QuoteLeft;

                    /* Simulate Alt+Tab on Host+1 and Alt+Shift+Tab on Host+2 */
                    if (ke->key() == kAltTab || ke->key() == kAltShiftTab)
                    {
                        if (pressed)
                        {
                            /* Send the Alt press to the guest */
                            if (!(mPressedKeysCopy [0x38] & IsKeyPressed))
                            {
                                /* store the press in *Copy to have it automatically
                                 * released when the Host key is released */
                                mPressedKeysCopy [0x38] |= IsKeyPressed;
                                keyboard.PutScancode (0x38);
                            }

                            /* Make sure Shift is pressed if it's Key_2 and released
                             * if it's Key_1 */
                            if (ke->key() == kAltTab &&
                                (mPressedKeysCopy [0x2A] & IsKeyPressed))
                            {
                                mPressedKeysCopy [0x2A] &= ~IsKeyPressed;
                                keyboard.PutScancode (0xAA);
                            }
                            else
                            if (ke->key() == kAltShiftTab &&
                                !(mPressedKeysCopy [0x2A] & IsKeyPressed))
                            {
                                mPressedKeysCopy [0x2A] |= IsKeyPressed;
                                keyboard.PutScancode (0x2A);
                            }
                        }

                        keyboard.PutScancode (pressed ? 0x0F : 0x8F);

                        ke->accept();
                        return true;
                    }

                    /* Simulate Ctrl+Esc on Host+Tilde */
                    if (ke->key() == kCtrlEsc)
                    {
                        /* Send the Ctrl press to the guest */
                        if (pressed && !(mPressedKeysCopy [0x1d] & IsKeyPressed))
                        {
                            /* store the press in *Copy to have it automatically
                             * released when the Host key is released */
                            mPressedKeysCopy [0x1d] |= IsKeyPressed;
                            keyboard.PutScancode (0x1d);
                        }

                        keyboard.PutScancode (pressed ? 0x01 : 0x81);

                        ke->accept();
                        return true;
                    }
                }

                /* fall through to normal processing */

#endif /* Q_WS_PM */

                if (m_bIsHostkeyPressed && e->type() == QEvent::KeyPress)
                {
                    if (ke->key() >= Qt::Key_F1 && ke->key() <= Qt::Key_F12)
                    {
                        QVector <LONG> combo (6);
                        combo [0] = 0x1d; /* Ctrl down */
                        combo [1] = 0x38; /* Alt  down */
                        combo [4] = 0xb8; /* Alt  up   */
                        combo [5] = 0x9d; /* Ctrl up   */
                        if (ke->key() >= Qt::Key_F1 && ke->key() <= Qt::Key_F10)
                        {
                            combo [2] = 0x3b + (ke->key() - Qt::Key_F1); /* F1-F10 down */
                            combo [3] = 0xbb + (ke->key() - Qt::Key_F1); /* F1-F10 up   */
                        }
                        /* some scan slice */
                        else if (ke->key() >= Qt::Key_F11 && ke->key() <= Qt::Key_F12)
                        {
                            combo [2] = 0x57 + (ke->key() - Qt::Key_F11); /* F11-F12 down */
                            combo [3] = 0xd7 + (ke->key() - Qt::Key_F11); /* F11-F12 up   */
                        }
                        else
                            Assert (0);

                        CKeyboard keyboard = m_console.GetKeyboard();
                        keyboard.PutScancodes (combo);
                    }
                    #if 0
                    // TODO check that!
                    else if (ke->key() == Qt::Key_Home)
                    {
                        /* Activate the main menu */
                        if (machineWindowWrapper()->isTrueSeamless() || machineWindowWrapper()->isTrueFullscreen())
                            machineWindowWrapper()->popupMainMenu (m_bIsMouseCaptured);
                        else
                        {
                            /* In Qt4 it is not enough to just set the focus to
                             * menu-bar. So to get the menu-bar we have to send
                             * Qt::Key_Alt press/release events directly. */
                            QKeyEvent e1 (QEvent::KeyPress, Qt::Key_Alt,
                                          Qt::NoModifier);
                            QKeyEvent e2 (QEvent::KeyRelease, Qt::Key_Alt,
                                          Qt::NoModifier);
                            QApplication::sendEvent (machineWindowWrapper()->menuBar(), &e1);
                            QApplication::sendEvent (machineWindowWrapper()->menuBar(), &e2);
                        }
                    }
                    else
                    {
                        /* process hot keys not processed in keyEvent()
                         * (as in case of non-alphanumeric keys) */
                        processHotKey (QKeySequence (ke->key()),
                                       machineWindowWrapper()->menuBar()->actions());
                    }
                    #endif
                }
                else if (!m_bIsHostkeyPressed && e->type() == QEvent::KeyRelease)
                {
                    /* Show a possible warning on key release which seems to
                     * be more expected by the end user */

                    if (machineWindowWrapper()->machineLogic()->isPaused())
                    {
                        /* if the reminder is disabled we pass the event to
                         * Qt to enable normal keyboard functionality
                         * (for example, menu access with Alt+Letter) */
                        if (!vboxProblem().remindAboutPausedVMInput())
                            break;
                    }
                }

                ke->accept();
                return true;
            }

#ifdef Q_WS_MAC
            /* posted OnShowWindow */
            case VBoxDefs::ShowWindowEventType:
            {
                /*
                 *  Dunno what Qt3 thinks a window that has minimized to the dock
                 *  should be - it is not hidden, neither is it minimized. OTOH it is
                 *  marked shown and visible, but not activated. This latter isn't of
                 *  much help though, since at this point nothing is marked activated.
                 *  I might have overlooked something, but I'm buggered what if I know
                 *  what. So, I'll just always show & activate the stupid window to
                 *  make it get out of the dock when the user wishes to show a VM.
                 */
                window()->show();
                window()->activateWindow();
                return true;
            }
#endif
            default:
                break;
        }
    }

    return QAbstractScrollArea::event (e);
}

bool UIMachineView::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == viewport())
    {
        switch (e->type())
        {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonRelease:
            {
                QMouseEvent *me = (QMouseEvent *) e;
                m_iLastMouseWheelDelta = 0;
                if (mouseEvent (me->type(), me->pos(), me->globalPos(),
                                me->buttons(), me->modifiers(),
                                0, Qt::Horizontal))
                    return true; /* stop further event handling */
                break;
            }
            case QEvent::Wheel:
            {
                QWheelEvent *we = (QWheelEvent *) e;
                /* There are pointing devices which send smaller values for the
                 * delta than 120. Here we sum them up until we are greater
                 * than 120. This allows to have finer control over the speed
                 * acceleration & enables such devices to send a valid wheel
                 * event to our guest mouse device at all. */
                int iDelta = 0;
                m_iLastMouseWheelDelta += we->delta();
                if (qAbs(m_iLastMouseWheelDelta) >= 120)
                {
                    iDelta = m_iLastMouseWheelDelta;
                    m_iLastMouseWheelDelta = m_iLastMouseWheelDelta % 120;
                }
                if (mouseEvent (we->type(), we->pos(), we->globalPos(),
#ifdef QT_MAC_USE_COCOA
                                /* Qt Cocoa is buggy. It always reports a left
                                 * button pressed when the mouse wheel event
                                 * occurs. A workaround is to ask the
                                 * application which buttons are pressed
                                 * currently. */
                                QApplication::mouseButtons(),
#else /* QT_MAC_USE_COCOA */
                                we->buttons(),
#endif /* QT_MAC_USE_COCOA */
                                we->modifiers(),
                                iDelta, we->orientation()))
                    return true; /* stop further event handling */
                break;
            }
#ifdef Q_WS_MAC
            case QEvent::Leave:
            {
                /* Enable mouse event compression if we leave the VM view. This
                   is necessary for having smooth resizing of the VM/other
                   windows. */
                setMouseCoalescingEnabled (true);
                break;
            }
            case QEvent::Enter:
            {
                /* Disable mouse event compression if we enter the VM view. So
                   all mouse events are registered in the VM. Only do this if
                   the keyboard/mouse is grabbed (this is when we have a valid
                   event handler). */
                setMouseCoalescingEnabled (false);
                break;
            }
#endif /* Q_WS_MAC */
            case QEvent::Resize:
            {
                if (m_bIsMouseCaptured)
                    updateMouseClipping();
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (mFrameBuf)
                {
                    mFrameBuf->viewportResized((QResizeEvent*)e);
                }
#endif
                break;
            }
            default:
                break;
        }
    }
    else if (watched == machineWindowWrapper()->machineWindow())
    {
        switch (e->type())
        {
#if defined (Q_WS_WIN32)
#if defined (VBOX_GUI_USE_DDRAW)
            case QEvent::Move:
            {
                /*
                 *  notification from our parent that it has moved. We need this
                 *  in order to possibly adjust the direct screen blitting.
                 */
                if (mFrameBuf)
                    mFrameBuf->moveEvent ((QMoveEvent *) e);
                break;
            }
#endif
            /*
             *  install/uninstall low-level kbd hook on every
             *  activation/deactivation to:
             *  a) avoid excess hook calls when we're not active and
             *  b) be always in front of any other possible hooks
             */
            case QEvent::WindowActivate:
            {
                gKbdHook = SetWindowsHookEx (WH_KEYBOARD_LL, lowLevelKeyboardProc,
                                              GetModuleHandle (NULL), 0);
                AssertMsg (gKbdHook, ("SetWindowsHookEx(): err=%d", GetLastError()));
                break;
            }
            case QEvent::WindowDeactivate:
            {
                if (gKbdHook)
                {
                    UnhookWindowsHookEx (gKbdHook);
                    gKbdHook = NULL;
                }
                break;
            }
#endif /* defined (Q_WS_WIN32) */
#if defined (Q_WS_MAC)
            /*
             *  Install/remove the keyboard event handler.
             */
            case QEvent::WindowActivate:
                darwinGrabKeyboardEvents (true);
                break;
            case QEvent::WindowDeactivate:
                darwinGrabKeyboardEvents (false);
                break;
#endif /* defined (Q_WS_MAC) */
            #if 0
            // TODO check that!
            case QEvent::Resize:
            {
                /* Set the "guest needs to resize" hint.  This hint is acted upon
                 * when (and only when) the autoresize property is "true". */
                mDoResize = m_bIsGuestSupportsGraphics || machineWindowWrapper()->isTrueFullscreen();
                if (!m_bIsMachineWindowResizeIgnored &&
                    m_bIsGuestSupportsGraphics && m_bIsGuestAutoresizeEnabled)
                    QTimer::singleShot (300, this, SLOT (doResizeHint()));
                break;
            }
            #endif
            case QEvent::WindowStateChange:
            {
                /* During minimizing and state restoring machineWindowWrapper() gets the focus
                 * which belongs to console view window, so returning it properly. */
                QWindowStateChangeEvent *ev = static_cast <QWindowStateChangeEvent*> (e);
                if (ev->oldState() & Qt::WindowMinimized)
                {
                    if (QApplication::focusWidget())
                    {
                        QApplication::focusWidget()->clearFocus();
                        qApp->processEvents();
                    }
                    QTimer::singleShot (0, this, SLOT (setFocus()));
                }
                break;
            }

            default:
                break;
        }
    }
    #if 0 // TODO check that
    else if (watched == machineWindowWrapper()->menuBar())
    {
        /*
         *  sometimes when we press ESC in the menu it brings the
         *  focus away (Qt bug?) causing no widget to have a focus,
         *  or holds the focus itself, instead of returning the focus
         *  to the console window. here we fix this.
         */
        switch (e->type())
        {
            case QEvent::FocusOut:
            {
                if (qApp->focusWidget() == 0)
                    setFocus();
                break;
            }
            case QEvent::KeyPress:
            {
                QKeyEvent *ke = (QKeyEvent *) e;
                if (ke->key() == Qt::Key_Escape && (ke->modifiers() == Qt::NoModifier))
                    if (machineWindowWrapper()->menuBar()->hasFocus())
                        setFocus();
                break;
            }
            default:
                break;
        }
    }
    #endif

    return QAbstractScrollArea::eventFilter (watched, e);
}

#if defined(Q_WS_WIN32)

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
        ((event.vkCode == m_globalSettings.hostKey() && !hostkey_in_capture) ||
         (mPressedKeys [event.scanCode] & (IsKbdCaptured | what_pressed)) == what_pressed))
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

#elif defined (Q_WS_PM)

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
    XEvent *pKeyEvent = (XEvent *) pvArg;
    if ((pEvent->type == XKeyPress) && (pEvent->xkey.keycode == pKeyEvent->xkey.keycode))
        return True;
    else
        return False;
}

bool UIMachineView::x11Event(XEvent *event)
{
    switch (event->type)
    {
        /* We have to handle XFocusOut right here as this event is not passed
         * to UIMachineView::event(). Handling this event is important for
         * releasing the keyboard before the screen saver gets active. */
        case XFocusOut:
        case XFocusIn:
            if (isRunning())
                focusEvent(event->type == XFocusIn);
            return false;
        case XKeyPress:
        case XKeyRelease:
            break;
        default:
            return false; /* pass the event to Qt */
    }

    /* Translate the keycode to a PC scan code. */
    unsigned scan = handleXKeyEvent(event);

    // scancodes 0x00 (no valid translation) and 0x80 are ignored
    if (!scan & 0x7F)
        return true;

    /* Fix for http://www.virtualbox.org/ticket/1296:
     * when X11 sends events for repeated keys, it always inserts an
     * XKeyRelease before the XKeyPress. */
    XEvent returnEvent;
    if ((event->type == XKeyRelease) && (XCheckIfEvent(event->xkey.display, &returnEvent,
        VBoxConsoleViewCompEvent, (XPointer) event) == True))
    {
        XPutBackEvent(event->xkey.display, &returnEvent);
        /* Discard it, don't pass it to Qt. */
        return true;
    }

    KeySym ks = ::XKeycodeToKeysym(event->xkey.display, event->xkey.keycode, 0);

    int flags = 0;
    if (scan >> 8)
        flags |= KeyExtended;
    if (event->type == XKeyPress)
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

#elif defined (Q_WS_MAC)

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
        UInt32 changed = newMask ^ mDarwinKeyModifiers;
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

        mDarwinKeyModifiers = newMask;

        /* Always return true here because we'll otherwise getting a Qt event
           we don't want and that will only cause the Pause warning to pop up. */
        ret = true;
    }

    return ret;
}

void UIMachineView::darwinGrabKeyboardEvents(bool fGrab)
{
    mKeyboardGrabbed = fGrab;
    if (fGrab)
    {
        /* Disable mouse and keyboard event compression/delaying to make sure we *really* get all of the events. */
        ::CGSetLocalEventsSuppressionInterval(0.0);
        setMouseCoalescingEnabled(false);

        /* Register the event callback/hook and grab the keyboard. */
# ifdef QT_MAC_USE_COCOA
        ::VBoxCocoaApplication_setCallback (UINT32_MAX, /** @todo fix mask */
                                            UIMachineView::darwinEventHandlerProc, this);

# elif !defined (VBOX_WITH_HACKED_QT)
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

        mDarwinEventHandlerRef = NULL;
        ::InstallApplicationEventHandler(eventHandler, RT_ELEMENTS (eventTypes), &eventTypes[0],
                                         this, &mDarwinEventHandlerRef);
        ::DisposeEventHandlerUPP(eventHandler);

# else  /* VBOX_WITH_HACKED_QT */
        ((QIApplication *)qApp)->setEventFilter(UIMachineView::macEventFilter, this);
# endif /* VBOX_WITH_HACKED_QT */

        ::DarwinGrabKeyboard (false);
    }
    else
    {
        ::DarwinReleaseKeyboard();
# ifdef QT_MAC_USE_COCOA
        ::VBoxCocoaApplication_unsetCallback(UINT32_MAX, /** @todo fix mask */
                                             UIMachineView::darwinEventHandlerProc, this);
# elif !defined(VBOX_WITH_HACKED_QT)
        if (mDarwinEventHandlerRef)
        {
            ::RemoveEventHandler(mDarwinEventHandlerRef);
            mDarwinEventHandlerRef = NULL;
        }
# else  /* VBOX_WITH_HACKED_QT */
        ((QIApplication *)qApp)->setEventFilter(NULL, NULL);
# endif /* VBOX_WITH_HACKED_QT */
    }
}

#endif

#if defined (Q_WS_WIN32)
static HHOOK gKbdHook = NULL;
static UIMachineView *gView = 0;
LRESULT CALLBACK UIMachineView::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Assert (gView);
    if (gView && nCode == HC_ACTION &&
            gView->winLowKeyboardEvent (wParam, *(KBDLLHOOKSTRUCT *) lParam))
        return 1;

    return CallNextHookEx (NULL, nCode, wParam, lParam);
}
#endif

#if defined (Q_WS_MAC)
# if defined (QT_MAC_USE_COCOA)
bool UIMachineView::darwinEventHandlerProc (const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    UIMachineView *view = (UIMachineView*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 eventClass = ::GetEventClass(inEvent);

    /* Check if this is an application key combo. In that case we will not pass
       the event to the guest, but let the host process it. */
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

# elif !defined (VBOX_WITH_HACKED_QT)

pascal OSStatus UIMachineView::darwinEventHandlerProc (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
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

# else /* VBOX_WITH_HACKED_QT */

bool UIMachineView::macEventFilter(EventRef inEvent, void *inUserData)
{
    UIMachineView *view = static_cast<UIMachineView *>(inUserData);
    UInt32 eventClass = ::GetEventClass(inEvent);
    UInt32 eventKind = ::GetEventKind(inEvent);

    /* Not sure but this seems an triggered event if the spotlight searchbar is
     * displayed. So flag that the host key isn't pressed alone. */
    if (eventClass == 'cgs ' && eventKind == 0x15 &&
        view->m_bIsHostkeyPressed)
        view->m_bIsHostkeyAlone = false;

    if (eventClass == kEventClassKeyboard)
    {
        if (view->darwinKeyboardEvent (NULL, inEvent))
            return true;
    }
    return false;
}
# endif /* VBOX_WITH_HACKED_QT */

#endif /* Q_WS_MAC */

void UIMachineView::focusEvent(bool aHasFocus, bool aReleaseHostKey /* = true */)
{
    if (aHasFocus)
    {
#ifdef RT_OS_WINDOWS
        if (!m_bIsAutoCaptureDisabled && m_globalSettings.autoCapture() && GetAncestor(winId(), GA_ROOT) == GetForegroundWindow())
#else
        if (!m_bIsAutoCaptureDisabled && m_globalSettings.autoCapture())
#endif /* RT_OS_WINDOWS */
        {
            captureKbd(true);
        }

        /* reset the single-time disable capture flag */
        if (m_bIsAutoCaptureDisabled)
            m_bIsAutoCaptureDisabled = false;
    }
    else
    {
        captureMouse(false);
        captureKbd(false, false);
        releaseAllPressedKeys(aReleaseHostKey);
    }
}

bool UIMachineView::keyEvent(int aKey, uint8_t aScan, int aFlags, wchar_t *aUniKey /* = NULL */)
{
    const bool isHostKey = aKey == m_globalSettings.hostKey();

    LONG buf[16];
    LONG *codes = buf;
    uint count = 0;
    uint8_t whatPressed = 0;

    if (!isHostKey && !m_bIsHostkeyPressed)
    {
        if (aFlags & KeyPrint)
        {
            static LONG PrintMake[] = { 0xE0, 0x2A, 0xE0, 0x37 };
            static LONG PrintBreak[] = { 0xE0, 0xB7, 0xE0, 0xAA };
            if (aFlags & KeyPressed)
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
        else if (aFlags & KeyPause)
        {
            if (aFlags & KeyPressed)
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
            if (aFlags & KeyPressed)
            {
                /* Check if the guest has the same view on the modifier keys (NumLock,
                 * CapsLock, ScrollLock) as the X server. If not, send KeyPress events
                 * to synchronize the state. */
                fixModifierState(codes, &count);
            }

            /* Check if it's C-A-D and GUI/PassCAD is not true */
            if (!mPassCAD &&
                aScan == 0x53 /* Del */ &&
                ((mPressedKeys [0x38] & IsKeyPressed) /* Alt */ ||
                 (mPressedKeys [0x38] & IsExtKeyPressed)) &&
                ((mPressedKeys [0x1d] & IsKeyPressed) /* Ctrl */ ||
                 (mPressedKeys [0x1d] & IsExtKeyPressed)))
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

            /* process the scancode and update the table of pressed keys */
            whatPressed = IsKeyPressed;

            if (aFlags & KeyExtended)
            {
                codes[count++] = 0xE0;
                whatPressed = IsExtKeyPressed;
            }

            if (aFlags & KeyPressed)
            {
                codes[count++] = aScan;
                mPressedKeys[aScan] |= whatPressed;
            }
            else
            {
                /* if we haven't got this key's press message, we ignore its
                 * release */
                if (!(mPressedKeys [aScan] & whatPressed))
                    return true;
                codes[count++] = aScan | 0x80;
                mPressedKeys[aScan] &= ~whatPressed;
            }

            if (m_bIsKeyboardCaptured)
                mPressedKeys[aScan] |= IsKbdCaptured;
            else
                mPressedKeys[aScan] &= ~IsKbdCaptured;
        }
    }
    else
    {
        /* currently this is used in winLowKeyboardEvent() only */
        hostkey_in_capture = m_bIsKeyboardCaptured;
    }

    bool emitSignal = false;
    int hotkey = 0;

    /* process the host key */
    if (aFlags & KeyPressed)
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
                    hotkey = aKey;
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
        NOREF(aUniKey);
        int n = GetKeyboardLayoutList (0, NULL);
        Assert (n);
        HKL *list = new HKL [n];
        GetKeyboardLayoutList (n, list);
        for (int i = 0; i < n && !processed; i++)
        {
            wchar_t ch;
            static BYTE keys [256] = {0};
            if (!ToUnicodeEx (hotkey, 0, keys, &ch, 1, 0, list [i]) == 1)
                ch = 0;
            if (ch)
                processed = processHotKey (QKeySequence (Qt::UNICODE_ACCEL +
                                                QChar (ch).toUpper().unicode()),
                                           machineWindowWrapper()->menuBar()->actions());
        }
        delete[] list;
#elif defined (Q_WS_X11)
        NOREF(aUniKey);
        Display *display = QX11Info::display();
        int keysyms_per_keycode = getKeysymsPerKeycode();
        KeyCode kc = XKeysymToKeycode (display, aKey);
        // iterate over the first level (not shifted) keysyms in every group
        for (int i = 0; i < keysyms_per_keycode && !processed; i += 2)
        {
            KeySym ks = XKeycodeToKeysym (display, kc, i);
            char ch = 0;
            if (!XkbTranslateKeySym (display, &ks, 0, &ch, 1, NULL) == 1)
                ch = 0;
            if (ch)
            {
                QChar c = QString::fromLocal8Bit (&ch, 1) [0];
            }
        }
#elif defined (Q_WS_MAC)
        if (aUniKey && aUniKey [0] && !aUniKey [1])
            processed = processHotKey (QKeySequence (Qt::UNICODE_ACCEL +
                                                     QChar (aUniKey [0]).toUpper().unicode()),
                                       machineWindowWrapper()->menuBar()->actions());

        /* Don't consider the hot key as pressed since the guest never saw
         * it. (probably a generic thing) */
        mPressedKeys [aScan] &= ~whatPressed;
#endif

        /* grab the key from Qt if processed, or pass it to Qt otherwise
         * in order to process non-alphanumeric keys in event(), after they are
         * converted to Qt virtual keys. */
        return processed;
    }

    /* no more to do, if the host key is in action or the VM is paused */
    if (m_bIsHostkeyPressed || isHostKey || machineWindowWrapper()->machineLogic()->isPaused())
    {
        /* grab the key from Qt and from VM if it's a host key,
         * otherwise just pass it to Qt */
        return isHostKey;
    }

    CKeyboard keyboard = m_console.GetKeyboard();
    Assert (!keyboard.isNull());

#if defined (Q_WS_WIN32)
    /* send pending WM_PAINT events */
    ::UpdateWindow (viewport()->winId());
#endif

    std::vector <LONG> scancodes(codes, &codes[count]);
    keyboard.PutScancodes (QVector<LONG>::fromStdVector(scancodes));

    /* grab the key from Qt */
    return true;
}

bool UIMachineView::mouseEvent(int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                               Qt::MouseButtons aButtons, Qt::KeyboardModifiers aModifiers,
                               int aWheelDelta, Qt::Orientation aWheelDir)
{
#if 1
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
        mouse.PutMouseEvent (aGlobalPos.x() - mLastPos.x(),
                             aGlobalPos.y() - mLastPos.y(),
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
            mLastPos = aGlobalPos;
        else
        {
            mLastPos = rect.center();
            QCursor::setPos (mLastPos);
        }

#else /* !Q_WS_MAC */

        /* "jerk" the mouse by bringing it to the opposite side
         * to simulate the endless moving */

#ifdef Q_WS_WIN32
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
            mLastPos = viewport()->mapToGlobal (p);
            QCursor::setPos (mLastPos);
        }
        else
        {
            mLastPos = aGlobalPos;
        }
#else
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
            mLastPos =  p;
            QCursor::setPos (mLastPos);
        }
        else
        {
            mLastPos = aGlobalPos;
        }
#endif
#endif /* !Q_WS_MAC */
        return true; /* stop further event handling */
    }
    else /* !m_bIsMouseCaptured */
    {
        //if (machineWindowWrapper()->isTrueFullscreen()) // TODO check that!
        {
            if (mode != VBoxDefs::SDLMode)
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
                    scrollBy (dx, dy);
            }
        }

        if (m_bIsMouseAbsolute && m_bIsMouseIntegrated)
        {
            int cw = contentsWidth(), ch = contentsHeight();
            int vw = visibleWidth(), vh = visibleHeight();

            if (mode != VBoxDefs::SDLMode)
            {
                /* try to automatically scroll the guest canvas if the
                 * mouse goes outside its visible part */

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
            return true; /* stop further event handling */
        }
        else
        {
            if (hasFocus() &&
                (aType == QEvent::MouseButtonRelease &&
                 aButtons == Qt::NoButton))
            {
                if (machineWindowWrapper()->machineLogic()->isPaused())
                {
                    vboxProblem().remindAboutPausedVMInput();
                }
                else if (isRunning())
                {
                    /* temporarily disable auto capture that will take
                     * place after this dialog is dismissed because
                     * the capture state is to be defined by the
                     * dialog result itself */
                    m_bIsAutoCaptureDisabled = true;
                    bool autoConfirmed = false;
                    bool ok = vboxProblem().confirmInputCapture (&autoConfirmed);
                    if (autoConfirmed)
                        m_bIsAutoCaptureDisabled = false;
                    /* otherwise, the disable flag will be reset in
                     * the next console view's foucs in event (since
                     * may happen asynchronously on some platforms,
                     * after we return from this code) */

                    if (ok)
                    {
#ifdef Q_WS_X11
                        /* make sure that pending FocusOut events from the
                         * previous message box are handled, otherwise the
                         * mouse is immediately ungrabbed again */
                        qApp->processEvents();
#endif
                        captureKbd (true);
                        captureMouse (true);
                    }
                }
            }
        }
    }

    return false;
}

void UIMachineView::resizeEvent(QResizeEvent *)
{
    updateSliders();
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
    QRect r = viewport()->geometry();
    PostBoundsChanged(r);
#endif /* Q_WS_MAC */
}

void UIMachineView::moveEvent(QMoveEvent *)
{
#if defined(Q_WS_MAC) && !defined(QT_MAC_USE_COCOA)
    QRect r = viewport()->geometry();
    PostBoundsChanged (r);
#endif /* Q_WS_MAC */
}

void UIMachineView::paintEvent(QPaintEvent *pe)
{
    if (mPausedShot.isNull())
    {
        /* delegate the paint function to the VBoxFrameBuffer interface */
        if (mFrameBuf)
            mFrameBuf->paintEvent(pe);
#ifdef Q_WS_MAC
        /* Update the dock icon if we are in the running state */
        if (isRunning())
            updateDockIcon();
#endif
        return;
    }

#ifdef VBOX_GUI_USE_QUARTZ2D
    if (mode == VBoxDefs::Quartz2DMode && mFrameBuf)
    {
        mFrameBuf->paintEvent(pe);
        updateDockIcon();
    }
    else
#endif
    {
        /* We have a snapshot for the paused state: */
        QRect r = pe->rect().intersect (viewport()->rect());
        /* We have to disable paint on screen if we are using the regular painter */
        bool paintOnScreen = viewport()->testAttribute(Qt::WA_PaintOnScreen);
        viewport()->setAttribute(Qt::WA_PaintOnScreen, false);
        QPainter pnt(viewport());
        pnt.drawPixmap(r.x(), r.y(), mPausedShot,
                       r.x() + contentsX(), r.y() + contentsY(),
                       r.width(), r.height());
        /* Restore the attribute to its previous state */
        viewport()->setAttribute(Qt::WA_PaintOnScreen, paintOnScreen);
#ifdef Q_WS_MAC
        updateDockIcon();
#endif
    }
}

void UIMachineView::fixModifierState(LONG *piCodes, uint *puCount)
{
    /* Synchronize the views of the host and the guest to the modifier keys.
     * This function will add up to 6 additional keycodes to codes. */

#if defined (Q_WS_X11)

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

    if (muNumLockAdaptionCnt && (mNumLock ^ !!(uMask & uKeyMaskNum)))
    {
        -- muNumLockAdaptionCnt;
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(uMask & uKeyMaskCaps)))
    {
        muCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (mCapsLock && !(mPressedKeys [0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined (Q_WS_WIN32)

    if (muNumLockAdaptionCnt && (mNumLock ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        muNumLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(GetKeyState(VK_CAPITAL))))
    {
        muCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (mCapsLock && !(mPressedKeys [0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined (Q_WS_MAC)

    /* if (muNumLockAdaptionCnt) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        muCapsLockAdaptionCnt--;
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (mCapsLock && !(mPressedKeys [0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#else

//#warning Adapt UIMachineView::fixModifierState

#endif
}

void UIMachineView::scrollBy(int dx, int dy)
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
}

QPoint UIMachineView::viewportToContents(const QPoint &vp) const
{
    return QPoint (vp.x() + contentsX(), vp.y() + contentsY());
}

void UIMachineView::updateSliders()
{
    QSize p = viewport()->size();
    QSize m = maximumViewportSize();

    QSize v = QSize(mFrameBuf->width(), mFrameBuf->height());
    /* no scroll bars needed */
    if (m.expandedTo(v) == m)
        p = m;

    horizontalScrollBar()->setRange(0, v.width() - p.width());
    verticalScrollBar()->setRange(0, v.height() - p.height());
    horizontalScrollBar()->setPageStep(p.width());
    verticalScrollBar()->setPageStep(p.height());
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void UIMachineView::scrollContentsBy (int dx, int dy)
{
    if (mFrameBuf)
    {
        mFrameBuf->viewportScrolled(dx, dy);
    }
    QAbstractScrollArea::scrollContentsBy (dx, dy);
}
#endif

#if defined(Q_WS_MAC)
void UIMachineView::updateDockIcon()
{
    if (mDockIconEnabled)
    {
        if (!mPausedShot.isNull())
        {
            CGImageRef pauseImg = ::darwinToCGImageRef (&mPausedShot);
            /* Use the pause image as background */
            mDockIconPreview->updateDockPreview (pauseImg);
            CGImageRelease (pauseImg);
        }
        else
        {
# if defined (VBOX_GUI_USE_QUARTZ2D)
            if (mode == VBoxDefs::Quartz2DMode)
            {
                /* If the render mode is Quartz2D we could use the CGImageRef
                 * of the framebuffer for the dock icon creation. This saves
                 * some conversion time. */
                mDockIconPreview->updateDockPreview (static_cast <VBoxQuartz2DFrameBuffer *> (mFrameBuf)->imageRef());
            }
            else
# endif
                /* In image mode we have to create the image ref out of the
                 * framebuffer */
                mDockIconPreview->updateDockPreview (mFrameBuf);
        }
    }
}

void UIMachineView::updateDockOverlay()
{
    /* Only to an update to the realtime preview if this is enabled by the user
     * & we are in an state where the framebuffer is likely valid. Otherwise to
     * the overlay stuff only. */
    if (mDockIconEnabled &&
        (mLastState == KMachineState_Running ||
         mLastState == KMachineState_Paused ||
         mLastState == KMachineState_Teleporting ||
         mLastState == KMachineState_LiveSnapshotting ||
         mLastState == KMachineState_Restoring ||
         mLastState == KMachineState_TeleportingPausedVM ||
         mLastState == KMachineState_TeleportingIn ||
         mLastState == KMachineState_Saving))
        updateDockIcon();
    else
        mDockIconPreview->updateDockOverlay();
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
    if (aOn || mKeyboardGrabbed)
        ::darwinSetMouseCoalescingEnabled (aOn);
}
#endif /* Q_WS_MAC */

void UIMachineView::onStateChange(KMachineState state)
{
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            if (    mode != VBoxDefs::TimerMode
                &&  mFrameBuf
                &&  (   state      != KMachineState_TeleportingPausedVM
                     || mLastState != KMachineState_Teleporting)
               )
            {
                /*
                 *  Take a screen snapshot. Note that TakeScreenShot() always
                 *  needs a 32bpp image
                 */
                QImage shot = QImage (mFrameBuf->width(), mFrameBuf->height(), QImage::Format_RGB32);
                CDisplay dsp = m_console.GetDisplay();
                dsp.TakeScreenShot (shot.bits(), shot.width(), shot.height());
                /*
                 *  TakeScreenShot() may fail if, e.g. the Paused notification
                 *  was delivered after the machine execution was resumed. It's
                 *  not fatal.
                 */
                if (dsp.isOk())
                {
                    dimImage (shot);
                    mPausedShot = QPixmap::fromImage (shot);
                    /* fully repaint to pick up mPausedShot */
                    repaint();
                }
            }
            /* fall through */
        }
        case KMachineState_Stuck:
        {
            /* reuse the focus event handler to uncapture everything */
            if (hasFocus())
                focusEvent (false /* aHasFocus*/, false /* aReleaseHostKey */);
            break;
        }
        case KMachineState_Running:
        {
            if (   mLastState == KMachineState_Paused
                || mLastState == KMachineState_TeleportingPausedVM
               )
            {
                if (mode != VBoxDefs::TimerMode && mFrameBuf)
                {
                    /* reset the pixmap to free memory */
                    mPausedShot = QPixmap ();
                    /*
                     *  ask for full guest display update (it will also update
                     *  the viewport through IFramebuffer::NotifyUpdate)
                     */
                    CDisplay dsp = m_console.GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
            /* reuse the focus event handler to capture input */
            if (hasFocus())
                focusEvent (true /* aHasFocus */);
            break;
        }
        default:
            break;
    }

    mLastState = state;
}

void UIMachineView::captureKbd(bool aCapture, bool aEmitSignal /* = true */)
{
    //AssertMsg(m_bIsAttached, ("Console must be attached"));

    if (m_bIsKeyboardCaptured == aCapture)
        return;

    /* On Win32, keyboard grabbing is ineffective, a low-level keyboard hook is
     * used instead. On X11, we use XGrabKey instead of XGrabKeyboard (called
     * by QWidget::grabKeyboard()) because the latter causes problems under
     * metacity 2.16 (in particular, due to a bug, a window cannot be moved
     * using the mouse if it is currently grabing the keyboard). On Mac OS X,
     * we use the Qt methods + disabling global hot keys + watching modifiers
     * (for right/left separation). */
#if defined (Q_WS_WIN32)
    /**/
#elif defined (Q_WS_X11)
	if (aCapture)
		XGrabKey(QX11Info::display(), AnyKey, AnyModifier, window()->winId(), False, GrabModeAsync, GrabModeAsync);
	else
		XUngrabKey(QX11Info::display(),  AnyKey, AnyModifier, window()->winId());
#elif defined (Q_WS_MAC)
    if (aCapture)
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
    if (aCapture)
        grabKeyboard();
    else
        releaseKeyboard();
#endif

    m_bIsKeyboardCaptured = aCapture;

    if (aEmitSignal)
        emitKeyboardStateChanged();
}

void UIMachineView::captureMouse(bool aCapture, bool aEmitSignal /* = true */)
{
    //AssertMsg (m_bIsAttached, ("Console must be attached"));

    if (m_bIsMouseCaptured == aCapture)
        return;

    if (aCapture)
    {
        /* memorize the host position where the cursor was captured */
        mCapturedPos = QCursor::pos();
#ifdef Q_WS_WIN32
        viewport()->setCursor (QCursor (Qt::BlankCursor));
        /* move the mouse to the center of the visible area */
        QCursor::setPos (mapToGlobal (visibleRegion().boundingRect().center()));
        mLastPos = QCursor::pos();
#elif defined (Q_WS_MAC)
        /* move the mouse to the center of the visible area */
        mLastPos = mapToGlobal (visibleRegion().boundingRect().center());
        QCursor::setPos (mLastPos);
        /* grab all mouse events. */
        viewport()->grabMouse();
#else
        viewport()->grabMouse();
        mLastPos = QCursor::pos();
#endif
    }
    else
    {
#ifndef Q_WS_WIN32
        viewport()->releaseMouse();
#endif
        /* release mouse buttons */
        CMouse mouse = m_console.GetMouse();
        mouse.PutMouseEvent (0, 0, 0, 0 /* Horizontal wheel */, 0);
    }

    m_bIsMouseCaptured = aCapture;

    updateMouseClipping();

    if (aEmitSignal)
        emitMouseStateChanged();
}

bool UIMachineView::processHotKey(const QKeySequence &aKey, const QList <QAction*> &aData)
{
    foreach (QAction *pAction, aData)
    {
        if (QMenu *menu = pAction->menu())
        {
            /* Process recursively for each sub-menu */
            if (processHotKey(aKey, menu->actions()))
                return true;
        }
        else
        {
            QString hotkey = VBoxGlobal::extractKeyFromActionText(pAction->text());
            if (pAction->isEnabled() && !hotkey.isEmpty())
            {
                if (aKey.matches(QKeySequence (hotkey)) == QKeySequence::ExactMatch)
                {
                    /* We asynchronously post a special event instead of calling
                     * pAction->trigger() directly, to let key presses and
                     * releases be processed correctly by Qt first.
                     * Note: we assume that nobody will delete the menu item
                     * corresponding to the key sequence, so that the pointer to
                     * menu data posted along with the event will remain valid in
                     * the event handler, at least until the main window is closed. */
                    QApplication::postEvent(this, new ActivateMenuEvent(pAction));
                    return true;
                }
            }
        }
    }

    return false;
}

void UIMachineView::releaseAllPressedKeys(bool aReleaseHostKey /* = true */)
{
    //AssertMsg(m_bIsAttached, ("Console must be attached"));

    CKeyboard keyboard = m_console.GetKeyboard();
    bool fSentRESEND = false;

    /* send a dummy scan code (RESEND) to prevent the guest OS from recognizing
     * a single key click (for ex., Alt) and performing an unwanted action
     * (for ex., activating the menu) when we release all pressed keys below.
     * Note, that it's just a guess that sending RESEND will give the desired
     * effect :), but at least it works with NT and W2k guests. */

    for (uint i = 0; i < SIZEOF_ARRAY (mPressedKeys); i++)
    {
        if (mPressedKeys[i] & IsKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode (0xFE);
                fSentRESEND = true;
            }
            keyboard.PutScancode(i | 0x80);
        }
        else if (mPressedKeys[i] & IsExtKeyPressed)
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
        mPressedKeys[i] = 0;
    }

    if (aReleaseHostKey)
        m_bIsHostkeyPressed = false;

#ifdef Q_WS_MAC
    /* clear most of the modifiers. */
    mDarwinKeyModifiers &=
        alphaLock | kEventKeyModifierNumLockMask |
        (aReleaseHostKey ? 0 : ::DarwinKeyCodeToDarwinModifierMask (m_globalSettings.hostKey()));
#endif

    emitKeyboardStateChanged();
}

void UIMachineView::saveKeyStates()
{
    ::memcpy(mPressedKeysCopy, mPressedKeys, sizeof(mPressedKeys));
}

void UIMachineView::sendChangedKeyStates()
{
    //AssertMsg(m_bIsAttached, ("Console must be attached"));

    QVector <LONG> codes(2);
    CKeyboard keyboard = m_console.GetKeyboard();
    for (uint i = 0; i < SIZEOF_ARRAY(mPressedKeys); ++ i)
    {
        uint8_t os = mPressedKeysCopy[i];
        uint8_t ns = mPressedKeys[i];
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
    //AssertMsg(m_bIsAttached, ("Console must be attached"));

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
        QCursor::setPos(mCapturedPos);
        viewport()->unsetCursor();
    }
}

void UIMachineView::setPointerShape(MousePointerChangeEvent *pEvent)
{
    if (pEvent->shapeData() != NULL)
    {
        bool ok = false;

        const uchar *srcAndMaskPtr = pEvent->shapeData();
        uint andMaskSize = (pEvent->width() + 7) / 8 * pEvent->height();
        const uchar *srcShapePtr = pEvent->shapeData() + ((andMaskSize + 3) & ~3);
        uint srcShapePtrScan = pEvent->width() * 4;

#if defined (Q_WS_WIN)

        BITMAPV5HEADER bi;
        HBITMAP hBitmap;
        void *lpBits;

        ::ZeroMemory(&bi, sizeof (BITMAPV5HEADER));
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = pEvent->width();
        bi.bV5Height = - (LONG)pEvent->height();
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        if (pEvent->hasAlpha())
            bi.bV5AlphaMask = 0xFF000000;
        else
            bi.bV5AlphaMask = 0;

        HDC hdc = GetDC(NULL);

        // create the DIB section with an alpha channel
        hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&lpBits, NULL, (DWORD) 0);

        ReleaseDC(NULL, hdc);

        HBITMAP hMonoBitmap = NULL;
        if (pEvent->hasAlpha())
        {
            // create an empty mask bitmap
            hMonoBitmap = CreateBitmap(pEvent->width(), pEvent->height(), 1, 1, NULL);
        }
        else
        {
            /* Word aligned AND mask. Will be allocated and created if necessary. */
            uint8_t *pu8AndMaskWordAligned = NULL;

            /* Width in bytes of the original AND mask scan line. */
            uint32_t cbAndMaskScan = (pEvent->width() + 7) / 8;

            if (cbAndMaskScan & 1)
            {
                /* Original AND mask is not word aligned. */

                /* Allocate memory for aligned AND mask. */
                pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ((cbAndMaskScan + 1) * pEvent->height());

                Assert(pu8AndMaskWordAligned);

                if (pu8AndMaskWordAligned)
                {
                    /* According to MSDN the padding bits must be 0.
                     * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask. */
                    uint32_t u32PaddingBits = cbAndMaskScan * 8  - pEvent->width();
                    Assert(u32PaddingBits < 8);
                    uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                    Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                          u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, pEvent->width(), cbAndMaskScan));

                    uint8_t *src = (uint8_t *)srcAndMaskPtr;
                    uint8_t *dst = pu8AndMaskWordAligned;

                    unsigned i;
                    for (i = 0; i < pEvent->height(); i++)
                    {
                        memcpy(dst, src, cbAndMaskScan);

                        dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                        src += cbAndMaskScan;
                        dst += cbAndMaskScan + 1;
                    }
                }
            }

            /* create the AND mask bitmap */
            hMonoBitmap = ::CreateBitmap(pEvent->width(), pEvent->height(), 1, 1,
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

            for (uint y = 0; y < pEvent->height(); y ++)
            {
                memcpy(dstShapePtr, srcShapePtr, srcShapePtrScan);
                srcShapePtr += srcShapePtrScan;
                dstShapePtr += pEvent->width();
            }

            ICONINFO ii;
            ii.fIcon = FALSE;
            ii.xHotspot = pEvent->xHot();
            ii.yHotspot = pEvent->yHot();
            ii.hbmMask = hMonoBitmap;
            ii.hbmColor = hBitmap;

            HCURSOR hAlphaCursor = CreateIconIndirect(&ii);
            Assert(hAlphaCursor);
            if (hAlphaCursor)
            {
                viewport()->setCursor(QCursor(hAlphaCursor));
                ok = true;
                if (mAlphaCursor)
                    DestroyIcon(mAlphaCursor);
                mAlphaCursor = hAlphaCursor;
            }
        }

        if (hMonoBitmap)
            DeleteObject(hMonoBitmap);
        if (hBitmap)
            DeleteObject(hBitmap);

#elif defined (Q_WS_X11) && !defined (VBOX_WITHOUT_XCURSOR)

        XcursorImage *img = XcursorImageCreate (pEvent->width(), pEvent->height());
        Assert (img);
        if (img)
        {
            img->xhot = pEvent->xHot();
            img->yhot = pEvent->yHot();

            XcursorPixel *dstShapePtr = img->pixels;

            for (uint y = 0; y < pEvent->height(); y ++)
            {
                memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);

                if (!pEvent->hasAlpha())
                {
                    /* convert AND mask to the alpha channel */
                    uchar byte = 0;
                    for (uint x = 0; x < pEvent->width(); x ++)
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
                dstShapePtr += pEvent->width();
            }

            Cursor cur = XcursorImageLoadCursor (QX11Info::display(), img);
            Assert (cur);
            if (cur)
            {
                viewport()->setCursor (QCursor (cur));
                ok = true;
            }

            XcursorImageDestroy (img);
        }

#elif defined(Q_WS_MAC)

        /* Create a ARGB image out of the shape data. */
        QImage image  (pEvent->width(), pEvent->height(), QImage::Format_ARGB32);
        const uint8_t* pbSrcMask = static_cast<const uint8_t*> (srcAndMaskPtr);
        unsigned cbSrcMaskLine = RT_ALIGN (pEvent->width(), 8) / 8;
        for (unsigned int y = 0; y < pEvent->height(); ++y)
        {
            for (unsigned int x = 0; x < pEvent->width(); ++x)
            {
               unsigned int color = ((unsigned int*)srcShapePtr)[y*pEvent->width()+x];
               /* If the alpha channel isn't in the shape data, we have to
                * create them from the and-mask. This is a bit field where 1
                * represent transparency & 0 opaque respectively. */
               if (!pEvent->hasAlpha())
               {
                   if (!(pbSrcMask[x / 8] & (1 << (7 - (x % 8)))))
                       color  |= 0xff000000;
                   else
                   {
                       /* This isn't quite right, but it's the best we can do I
                        * think... */
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
        /* Set the new cursor */
        QCursor cursor (QPixmap::fromImage (image), pEvent->xHot(), pEvent->yHot());
        viewport()->setCursor (cursor);
        ok = true;
        NOREF (srcShapePtrScan);

#else

# warning "port me"

#endif

        if (ok)
            mLastCursor = viewport()->cursor();
        else
            viewport()->unsetCursor();
    }
    else
    {
        if (pEvent->isVisible())
        {
            viewport()->setCursor(mLastCursor);
        }
        else
        {
            viewport()->setCursor(Qt::BlankCursor);
        }
    }
    mHideHostPointer = !pEvent->isVisible();
}

inline QRgb qRgbIntensity(QRgb rgb, int mul, int div)
{
    int r = qRed(rgb);
    int g = qGreen(rgb);
    int b = qBlue(rgb);
    return qRgb(mul * r / div, mul * g / div, mul * b / div);
}

void UIMachineView::storeConsoleSize(int aWidth, int aHeight)
{
    LogFlowThisFunc(("aWidth=%d, aHeight=%d\n", aWidth, aHeight));
    mStoredConsoleSize = QRect(0, 0, aWidth, aHeight);
}

void UIMachineView::setMouseIntegrationLocked(bool bDisabled)
{
    machineWindowWrapper()->machineLogic()->actionsPool()->action(UIActionIndex_Toggle_MouseIntegration)->setChecked(false);
    machineWindowWrapper()->machineLogic()->actionsPool()->action(UIActionIndex_Toggle_MouseIntegration)->setEnabled(bDisabled);
}

void UIMachineView::setDesktopGeometry(DesktopGeo aGeo, int aWidth, int aHeight)
{
    switch (aGeo)
    {
        case DesktopGeo_Fixed:
            mDesktopGeo = DesktopGeo_Fixed;
            if (aWidth != 0 && aHeight != 0)
                mDesktopGeometry = QRect(0, 0, aWidth, aHeight);
            else
                mDesktopGeometry = QRect(0, 0, 0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Automatic:
            mDesktopGeo = DesktopGeo_Automatic;
            mDesktopGeometry = QRect(0, 0, 0, 0);
            storeConsoleSize(0, 0);
            break;
        case DesktopGeo_Any:
            mDesktopGeo = DesktopGeo_Any;
            mDesktopGeometry = QRect(0, 0, 0, 0);
            break;
        default:
            AssertMsgFailed(("Invalid desktop geometry type %d\n", aGeo));
            mDesktopGeo = DesktopGeo_Invalid;
    }
}

QRect UIMachineView::availableGeometry()
{
    return machineWindowWrapper()->machineWindow()->isFullScreen() ?
           QApplication::desktop()->screenGeometry(this) :
           QApplication::desktop()->availableGeometry(this);
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
