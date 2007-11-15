/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleView class implementation
 */

/*
 * Copyright (C) 22006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxConsoleView.h"
#include "VBoxConsoleWnd.h"

#include "VBoxFrameBuffer.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#ifdef Q_WS_PM
#include "QIHotKeyEdit.h"
#endif

#include <qapplication.h>
#include <qstatusbar.h>
#include <qlabel.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qbitmap.h>
#include <qcursor.h>
#include <qthread.h>

#include <qmenudata.h>
#include <qmenubar.h>
#include <qwidgetlist.h>
#include <qtimer.h>

#ifdef Q_WS_WIN
// VBox/cdefs.h defines these:
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <windows.h>
#endif

#ifdef Q_WS_X11
// We need to capture some X11 events directly which
// requires the XEvent structure to be defined. However,
// including the Xlib header file will cause some nasty
// conflicts with Qt. Therefore we use the following hack
// to redefine those conflicting identifiers.
#define XK_XKB_KEYS
#define XK_MISCELLANY
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyRelease
#undef KeyPress
#undef FocusOut
#undef FocusIn
#endif
#include "XKeyboard.h"
#ifndef VBOX_WITHOUT_XCURSOR
# include <X11/Xcursor/Xcursor.h>
#endif
#endif // Q_WS_X11

#if defined (Q_WS_MAC)
# include "DarwinKeyboard.h"
# include "DarwinCursor.h"
# ifdef VBOX_WITH_HACKED_QT
#  include "QIApplication.h"
# endif
# include <VBox/err.h>
#endif /* defined (Q_WS_MAC) */

#if defined (VBOX_GUI_USE_REFRESH_TIMER)
enum { UPDATE_FREQ = 1000 / 60 }; // a-la 60Hz
#endif

#if defined (Q_WS_WIN32)

static HHOOK g_kbdhook = NULL;
static VBoxConsoleView *g_view = 0;

LRESULT CALLBACK VBoxConsoleView::lowLevelKeyboardProc (int nCode,
                                                        WPARAM wParam, LPARAM lParam)
{
    Assert (g_view);
    if (g_view && nCode == HC_ACTION &&
            g_view->winLowKeyboardEvent (wParam, *(KBDLLHOOKSTRUCT *) lParam))
        return 1;

    return CallNextHookEx (NULL, nCode, wParam, lParam);
}

#endif

#if defined (Q_WS_MAC)

# ifndef VBOX_WITH_HACKED_QT
/**
 *  Event handler callback for Mac OS X.
 */
/* static */
pascal OSStatus VBoxConsoleView::darwinEventHandlerProc (EventHandlerCallRef inHandlerCallRef,
                                                         EventRef inEvent, void *inUserData)
{
    VBoxConsoleView *view = (VBoxConsoleView *)inUserData;
    UInt32 EventClass = ::GetEventClass (inEvent);
    if (EventClass == kEventClassKeyboard)
    {
        if (view->darwinKeyboardEvent (inEvent))
            return 0;
    }
    /*
     * Command-H and Command-Q aren't properly disabled yet, and it's still
     * possible to use the left command key to invoke them when the keyboard
     * is captured. We discard the events these if the keyboard is captured
     * as a half measure to prevent unexpected behaviour. However, we don't
     * get any key down/up events, so these combinations are dead to the guest...
     */
    else if (EventClass == kEventClassCommand)
    {
        if (view->kbd_captured)
            return 0;
    }
    return ::CallNextEventHandler (inHandlerCallRef, inEvent);
}

# else /* VBOX_WITH_HACKED_QT */

/**
 *  Event handler callback for Mac OS X.
 */
/* static */
bool VBoxConsoleView::macEventFilter (EventRef inEvent, void *inUserData)
{
    VBoxConsoleView *view = (VBoxConsoleView *)inUserData;
    UInt32 EventClass = ::GetEventClass (inEvent);
    if (EventClass == kEventClassKeyboard)
    {
        if (view->darwinKeyboardEvent (inEvent))
            return true;
    }
    return false;
}
# endif /* VBOX_WITH_HACKED_QT */

#endif /* Q_WS_MAC */

/** Guest mouse pointer shape change event. */
class MousePointerChangeEvent : public QEvent
{
public:
    MousePointerChangeEvent (bool visible, bool alpha, uint xhot, uint yhot,
                             uint width, uint height,
                             const uchar *shape) :
        QEvent ((QEvent::Type) VBoxDefs::MousePointerChangeEventType),
        vis (visible), alph (alpha), xh (xhot), yh (yhot), w (width), h (height),
        data (NULL)
    {
        // make a copy of shape
        uint dataSize = ((((width + 7) / 8 * height) + 3) & ~3) + width * 4 * height;

        if (shape) {
            data = new uchar [dataSize];
            memcpy ((void *) data, (void *) shape, dataSize);
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
    MouseCapabilityEvent (bool supportsAbsolute, bool needsHostCursor) :
        QEvent ((QEvent::Type) VBoxDefs::MouseCapabilityEventType),
        can_abs (supportsAbsolute),
        needs_host_cursor (needsHostCursor) {}
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
    StateChangeEvent (CEnums::MachineState state) :
        QEvent ((QEvent::Type) VBoxDefs::MachineStateChangeEventType),
        s (state) {}
    CEnums::MachineState machineState() const { return s; }
private:
    CEnums::MachineState s;
};

/** Guest Additions property changes. */
class GuestAdditionsEvent : public QEvent
{
public:
    GuestAdditionsEvent (const QString &aOsTypeId,
                         const QString &aAddVersion,
                         bool aAddActive,
                         bool aSupportsSeamless) :
        QEvent ((QEvent::Type) VBoxDefs::AdditionsStateChangeEventType),
        mOsTypeId (aOsTypeId), mAddVersion (aAddVersion),
        mAddActive (aAddActive), mSupportsSeamless (aSupportsSeamless) {}
    const QString &osTypeId() const { return mOsTypeId; }
    const QString &additionVersion() const { return mAddVersion; }
    bool additionActive() const { return mAddActive; }
    bool supportsSeamless() const { return mSupportsSeamless; }
private:
    QString mOsTypeId;
    QString mAddVersion;
    bool mAddActive;
    bool mSupportsSeamless;
};

/** DVD/FD change event */
class MediaChangeEvent : public QEvent
{
public:
    MediaChangeEvent (VBoxDefs::DiskType aType)
        : QEvent ((QEvent::Type) VBoxDefs::MediaChangeEventType)
        , mType (aType) {}
    VBoxDefs::DiskType diskType() const { return mType; }
private:
    VBoxDefs::DiskType mType;
};

/** Menu activation event */
class ActivateMenuEvent : public QEvent
{
public:
    ActivateMenuEvent (QMenuData *menuData, uint index) :
        QEvent ((QEvent::Type) VBoxDefs::ActivateMenuEventType),
        md (menuData), i (index) {}
    QMenuData *menuData() const { return md; }
    uint index() const { return i; }
private:
    QMenuData *md;
    uint i;
};

/** VM Runtime error event */
class RuntimeErrorEvent : public QEvent
{
public:
    RuntimeErrorEvent (bool aFatal, const QString &aErrorID,
                       const QString &aMessage) :
        QEvent ((QEvent::Type) VBoxDefs::RuntimeErrorEventType),
        mFatal (aFatal), mErrorID (aErrorID), mMessage (aMessage) {}
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
    ModifierKeyChangeEvent (bool fNumLock, bool fCapsLock, bool fScrollLock) :
        QEvent ((QEvent::Type) VBoxDefs::ModifierKeyChangeEventType),
        mNumLock (fNumLock), mCapsLock (fCapsLock), mScrollLock (fScrollLock) {}
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
    NetworkAdapterChangeEvent (INetworkAdapter *aAdapter) :
        QEvent ((QEvent::Type) VBoxDefs::NetworkAdapterChangeEventType),
        mAdapter (aAdapter) {}
    INetworkAdapter* networkAdapter() { return mAdapter; }
private:
    INetworkAdapter *mAdapter;
};

/** USB controller state change event */
class USBControllerStateChangeEvent : public QEvent
{
public:
    USBControllerStateChangeEvent()
        : QEvent ((QEvent::Type) VBoxDefs::USBCtlStateChangeEventType) {}
};

/** USB device state change event */
class USBDeviceStateChangeEvent : public QEvent
{
public:
    USBDeviceStateChangeEvent (const CUSBDevice &aDevice, bool aAttached,
                               const CVirtualBoxErrorInfo &aError) :
        QEvent ((QEvent::Type) VBoxDefs::USBDeviceStateChangeEventType),
        mDevice (aDevice), mAttached (aAttached), mError (aError) {}
    CUSBDevice device() const { return mDevice; }
    bool attached() const { return mAttached; }
    CVirtualBoxErrorInfo error() const { return mError; }
private:
    CUSBDevice mDevice;
    bool mAttached;
    CVirtualBoxErrorInfo mError;
};

//
// VBoxConsoleCallback class
/////////////////////////////////////////////////////////////////////////////

class VBoxConsoleCallback : public IConsoleCallback
{
public:

    VBoxConsoleCallback (VBoxConsoleView *v) {
#if defined (Q_WS_WIN)
        mRefCnt = 0;
#endif
        mView = v;
    }

    virtual ~VBoxConsoleCallback() {}

    NS_DECL_ISUPPORTS

#if defined (Q_WS_WIN)
    STDMETHOD_(ULONG, AddRef)() {
        return ::InterlockedIncrement (&mRefCnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&mRefCnt);
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
        if (riid == IID_IConsoleCallback) {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    STDMETHOD(OnMousePointerShapeChange) (BOOL visible, BOOL alpha,
                                          ULONG xhot, ULONG yhot,
                                          ULONG width, ULONG height,
                                          BYTE *shape)
    {
        QApplication::postEvent (mView,
                                 new MousePointerChangeEvent (visible, alpha,
                                                              xhot, yhot,
                                                              width, height, shape));
        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange)(BOOL supportsAbsolute, BOOL needsHostCursor)
    {
        QApplication::postEvent (mView,
                                 new MouseCapabilityEvent (supportsAbsolute,
                                                           needsHostCursor));
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        QApplication::postEvent (mView,
                                 new ModifierKeyChangeEvent (fNumLock, fCapsLock,
                                                             fScrollLock));
        return S_OK;
    }

    STDMETHOD(OnStateChange)(MachineState_T machineState)
    {
        LogFlowFunc (("machineState=%d\n", machineState));
        QApplication::postEvent (mView,
                                 new StateChangeEvent (
                                     (CEnums::MachineState) machineState));
        return S_OK;
    }

    STDMETHOD(OnAdditionsStateChange)()
    {
        CGuest guest = mView->console().GetGuest();
        LogFlowFunc (("ver=%s, active=%d\n",
                      guest.GetAdditionsVersion().latin1(),
                      guest.GetAdditionsActive()));
        QApplication::postEvent (mView,
                                 new GuestAdditionsEvent (
                                     guest.GetOSTypeId(),
                                     guest.GetAdditionsVersion(),
                                     guest.GetAdditionsActive(),
                                     guest.GetSupportsSeamless()));
        return S_OK;
    }

    STDMETHOD(OnDVDDriveChange)()
    {
        LogFlowFunc (("DVD Drive changed\n"));
        QApplication::postEvent (mView, new MediaChangeEvent (VBoxDefs::CD));
        return S_OK;
    }

    STDMETHOD(OnFloppyDriveChange)()
    {
        LogFlowFunc (("Floppy Drive changed\n"));
        QApplication::postEvent (mView, new MediaChangeEvent (VBoxDefs::FD));
        return S_OK;
    }

    STDMETHOD(OnNetworkAdapterChange) (INetworkAdapter *aNetworkAdapter)
    {
        QApplication::postEvent (mView,
            new NetworkAdapterChangeEvent (aNetworkAdapter));
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

    STDMETHOD(OnUSBControllerChange)()
    {
        QApplication::postEvent (mView,
                                 new USBControllerStateChangeEvent());
        return S_OK;
    }

    STDMETHOD(OnUSBDeviceStateChange)(IUSBDevice *aDevice, BOOL aAttached,
                                      IVirtualBoxErrorInfo *aError)
    {
        QApplication::postEvent (mView,
                                 new USBDeviceStateChangeEvent (
                                     CUSBDevice (aDevice),
                                     bool (aAttached),
                                     CVirtualBoxErrorInfo (aError)));
        return S_OK;
    }

    STDMETHOD(OnSharedFolderChange) (Scope_T aScope)
    {
        NOREF(aScope);
        QApplication::postEvent (mView,
                                 new QEvent ((QEvent::Type)
                                             VBoxDefs::SharedFolderChangeEventType));
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, IN_BSTRPARAM id, IN_BSTRPARAM message)
    {
        QApplication::postEvent (mView,
                                 new RuntimeErrorEvent (!!fatal,
                                                        QString::fromUcs2 (id),
                                                        QString::fromUcs2 (message)));
        return S_OK;
    }

    STDMETHOD(OnCanShowWindow) (BOOL *canShow)
    {
        if (!canShow)
            return E_POINTER;

        /* as long as there is VBoxConsoleView (which creates/destroys us), it
         * can be shown */
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
            QApplication::postEvent (mView, new QEvent ((QEvent::Type)VBoxDefs::ShowWindowEventType));
        else
        {
            /*
             * It failed for some reason, send the other process our PSN so it can try.
             * (This is just a precaution should Mac OS X start imposing the same sensible
             * focus stealing restrictions that other window managers implement.)
             */
            AssertMsgFailed(("SetFrontProcess -> %#x\n", rc));
            if (::GetCurrentProcess (&psn))
                *winId = RT_MAKE_U64 (psn.lowLongOfPSN, psn.highLongOfPSN);
        }

#else
        /* Return the ID of the top-level console window. */
        *winId = (ULONG64) mView->topLevelWidget()->winId();
#endif

        return S_OK;
    }

protected:

    VBoxConsoleView *mView;

#if defined (Q_WS_WIN)
private:
    long mRefCnt;
#endif
};

#if !defined (Q_WS_WIN)
NS_DECL_CLASSINFO (VBoxConsoleCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (VBoxConsoleCallback, IConsoleCallback)
#endif

//
// VBoxConsoleView class
/////////////////////////////////////////////////////////////////////////////

/** @class VBoxConsoleView
 *
 *  The VBoxConsoleView class is a widget that implements a console
 *  for the running virtual machine.
 */

VBoxConsoleView::VBoxConsoleView (VBoxConsoleWnd *mainWnd,
                                  const CConsole &console,
                                  VBoxDefs::RenderMode rm,
                                  QWidget *parent, const char *name, WFlags f)
    : QScrollView (parent, name, f | WStaticContents | WNoAutoErase)
    , mainwnd (mainWnd)
    , cconsole (console)
    , gs (vboxGlobal().settings())
    , attached (false)
    , kbd_captured (false)
    , mouse_captured (false)
    , mouse_absolute (false)
    , mouse_integration (true)
    , mIsHostkeyPressed (false)
    , mIsHostkeyAlone (false)
    , mIgnoreMainwndResize (true)
    , mAutoresizeGuest (false)
    , mIsAdditionsActive (false)
    , mNumLock (false)
    , mScrollLock (false)
    , mCapsLock (false)
    , muNumLockAdaptionCnt (2)
    , muCapsLockAdaptionCnt (2)
    , mode (rm)
#if defined(Q_WS_WIN)
    , mAlphaCursor (NULL)
#endif
#if defined(Q_WS_MAC)
# ifndef VBOX_WITH_HACKED_QT
    , mDarwinEventHandlerRef (NULL)
# endif
    , mDarwinKeyModifiers (0)
#endif
{
    Assert (!cconsole.isNull() &&
            !cconsole.GetDisplay().isNull() &&
            !cconsole.GetKeyboard().isNull() &&
            !cconsole.GetMouse().isNull());

    /* enable MouseMove events */
    viewport()->setMouseTracking (true);

    /*
     *  QScrollView does the below on its own, but let's do it anyway
     *  for the case it will not do it in the future.
     */
    viewport()->installEventFilter (this);

    /* to fix some focus issues */
    mainwnd->menuBar()->installEventFilter (this);

    /* we want to be notified on some parent's events */
    mainwnd->installEventFilter (this);

#ifdef Q_WS_X11
    /* initialize the X keyboard subsystem */
    initXKeyboard (this->x11Display());
#endif

    ::memset (mPressedKeys, 0, SIZEOF_ARRAY (mPressedKeys));

    resize_hint_timer = new QTimer (this);
    connect (resize_hint_timer, SIGNAL (timeout()),
             this, SLOT (doResizeHint()));

    mToggleFSModeTimer = new QTimer (this);
    connect (mToggleFSModeTimer, SIGNAL (timeout()),
             this, SIGNAL (resizeHintDone()));

    /* setup rendering */

    CDisplay display = cconsole.GetDisplay();
    Assert (!display.isNull());

#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    tid = 0;
#endif
    mFrameBuf = 0;

    LogFlowFunc (("Rendering mode: %d\n", mode));

    switch (mode)
    {
#if defined (VBOX_GUI_USE_REFRESH_TIMER)
        case VBoxDefs::TimerMode:
            display.SetupInternalFramebuffer (32);
            tid = startTimer (UPDATE_FREQ);
            break;
#endif
#if defined (VBOX_GUI_USE_QIMAGE)
        case VBoxDefs::QImageMode:
            mFrameBuf = new VBoxQImageFrameBuffer (this);
            break;
#endif
#if defined (VBOX_GUI_USE_SDL)
        case VBoxDefs::SDLMode:
            mFrameBuf = new VBoxSDLFrameBuffer (this);
            /*
             *  disable scrollbars because we cannot correctly draw in a
             *  scrolled window using SDL
             */
            horizontalScrollBar()->setEnabled (false);
            verticalScrollBar()->setEnabled (false);
            break;
#endif
#if defined (VBOX_GUI_USE_DDRAW)
        case VBoxDefs::DDRAWMode:
            mFrameBuf = new VBoxDDRAWFrameBuffer (this);
            break;
#endif
        default:
            AssertReleaseMsgFailed (("Render mode must be valid: %d\n", mode));
            LogRel (("Invalid render mode: %d\n", mode));
            qApp->exit (1);
            break;
    }

#if defined (VBOX_GUI_USE_DDRAW)
    if (!mFrameBuf || mFrameBuf->address () == NULL)
    {
        if (mFrameBuf)
            delete mFrameBuf;
        mode = VBoxDefs::QImageMode;
        mFrameBuf = new VBoxQImageFrameBuffer (this);
    }
#endif

    if (mFrameBuf)
    {
        mFrameBuf->AddRef();
        display.RegisterExternalFramebuffer (CFramebuffer (mFrameBuf));
    }

    /* setup the callback */
    mCallback = CConsoleCallback (new VBoxConsoleCallback (this));
    cconsole.RegisterCallback (mCallback);
    AssertWrapperOk (cconsole);

    viewport()->setEraseColor (black);

    setSizePolicy (QSizePolicy (QSizePolicy::Maximum, QSizePolicy::Maximum));
    setMaximumSize (sizeHint());

    setFocusPolicy (WheelFocus);

#if defined (VBOX_GUI_DEBUG) && defined (VBOX_GUI_FRAMEBUF_STAT)
    VMCPUTimer::calibrate (200);
#endif

#if defined (Q_WS_WIN)
    g_view = this;
#endif

#ifdef Q_WS_MAC
    DarwinCursorClearHandle (&mDarwinCursor);
#endif
}

VBoxConsoleView::~VBoxConsoleView()
{
#if defined (Q_WS_WIN)
    if (g_kbdhook)
        UnhookWindowsHookEx (g_kbdhook);
    g_view = 0;
    if (mAlphaCursor)
        DestroyIcon (mAlphaCursor);
#endif

#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    if (tid)
        killTimer (tid);
#endif
    if (mFrameBuf)
    {
        /* detach our framebuffer from Display */
        CDisplay display = cconsole.GetDisplay();
        Assert (!display.isNull());
        display.SetupInternalFramebuffer (0);
        /* release the reference */
        mFrameBuf->Release();
    }

    cconsole.UnregisterCallback (mCallback);
}

//
// Public members
/////////////////////////////////////////////////////////////////////////////

QSize VBoxConsoleView::sizeHint() const
{
#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    if (mode == VBoxDefs::TimerMode)
    {
        CDisplay display = cconsole.GetDisplay();
        return QSize (display.GetWidth() + frameWidth() * 2,
                      display.GetHeight() + frameWidth() * 2);
    }
    else
#endif
    {
        return QSize (mFrameBuf->width() + frameWidth() * 2,
                      mFrameBuf->height() + frameWidth() * 2);
    }
}

/**
 *  Attaches this console view to the managed virtual machine.
 *
 *  @note This method is not really necessary these days -- the only place where
 *        it gets called is VBoxConsole::openView(), right after powering the
 *        VM up. We leave it as is just in case attaching/detaching will become
 *        necessary some day (there are useful attached checks everywhere in the
 *        code).
 */
void VBoxConsoleView::attach()
{
    if (!attached)
    {
        attached = true;
    }
}

/**
 *  Detaches this console view from the VM. Must be called to indicate
 *  that the virtual machine managed by this instance will be no more valid
 *  after this call.
 *
 *  @note This method is not really necessary these days -- the only place where
 *        it gets called is VBoxConsole::closeView(), when the VM is powered
 *        down, before deleting VBoxConsoleView. We leave it as is just in case
 *        attaching/detaching will become necessary some day (there are useful
 *        attached checks everywhere in the code).
 */
void VBoxConsoleView::detach()
{
    if (attached)
    {
        /* reuse the focus event handler to uncapture everything */
        focusEvent (false);
        attached = false;
    }
}

/**
 *  Resizes the toplevel widget to fit the console view w/o scrollbars.
 *  If adjustPosition is true and such resize is not possible (because the
 *  console view size is lagrer then the available screen space) the toplevel
 *  widget is resized and moved to become as large as possible while staying
 *  fully visible.
 */
void VBoxConsoleView::normalizeGeometry (bool adjustPosition /* = false */)
{
    /* Make no normalizeGeometry in case we are in manual resize
     * mode or main window is maximized */
    if (mainwnd->isMaximized() || mainwnd->isFullScreen())
        return;

    QWidget *tlw = topLevelWidget();

    /* calculate client window offsets */
    QRect fr = tlw->frameGeometry();
    QRect r = tlw->geometry();
    int dl = r.left() - fr.left();
    int dt = r.top() - fr.top();
    int dr = fr.right() - r.right();
    int db = fr.bottom() - r.bottom();

    /* get the best size w/o scroll bars */
    QSize s = tlw->sizeHint();

    /* resize the frame to fit the contents */
    s -= tlw->size();
    fr.rRight() += s.width();
    fr.rBottom() += s.height();

    if (adjustPosition)
    {
        QRect ar = QApplication::desktop()->availableGeometry (tlw->pos());
        fr = VBoxGlobal::normalizeGeometry (
            fr, ar, mode != VBoxDefs::SDLMode /* canResize */);
    }

#if 0
    /* center the frame on the desktop */
    fr.moveCenter (ar.center());
#endif

    /* finally, set the frame geometry */
    tlw->setGeometry (fr.left() + dl, fr.top() + dt,
                      fr.width() - dl - dr, fr.height() - dt - db);
}

/**
 *  Pauses or resumes the VM execution.
 */
bool VBoxConsoleView::pause (bool on)
{
    /* QAction::setOn() emits the toggled() signal, so avoid recursion when
     * QAction::setOn() is called from VBoxConsoleWnd::updateMachineState() */
    if (isPaused() == on)
        return true;

    if (on)
        cconsole.Pause();
    else
        cconsole.Resume();

    bool ok = cconsole.isOk();
    if (!ok)
    {
        if (on)
            vboxProblem().cannotPauseMachine (cconsole);
        else
            vboxProblem().cannotResumeMachine (cconsole);
    }

    return ok;
}

/**
 *  Temporarily disables the mouse integration (or enables it back).
 */
void VBoxConsoleView::setMouseIntegrationEnabled (bool enabled)
{
    if (mouse_integration == enabled)
        return;

    if (mouse_absolute)
        captureMouse (!enabled, false);

    /* Hiding host cursor in case we are entering mouse integration
     * mode until it's shape is set to the guest cursor shape in
     * OnMousePointerShapeChange event handler.
     *
     * This is necessary to avoid double-cursor issue when both the
     * guest and the host cursors are displayed in one place one-above-one.
     *
     * This is a workaround because the correct decision is to notify
     * the Guest Additions about we are entering the mouse integration
     * mode. The GuestOS should hide it's cursor to allow using of
     * host cursor for the guest's manipulation.
     *
     * This notification is not possible right now due to there is
     * no the required API. */
    if (enabled)
        viewport()->setCursor (QCursor (BlankCursor));

    mouse_integration = enabled;

    emitMouseStateChanged();
}

void VBoxConsoleView::setAutoresizeGuest (bool on)
{
    if (mAutoresizeGuest != on)
    {
        mAutoresizeGuest = on;

        maybeRestrictMinimumSize();

        if (mIsAdditionsActive && mAutoresizeGuest)
            doResizeHint();
    }
}

/**
 *  This method is called by VBoxConsoleWnd after it does everything necessary
 *  on its side to go to or from fullscreen, but before it is shown.
 */
void VBoxConsoleView::onFullscreenChange (bool /* on */)
{
    /* Nothing to do here so far */
}

/**
 *  Notify the console scroll-view about the console-window is opened.
 */
void VBoxConsoleView::onViewOpened()
{
    /* Variable mIgnoreMainwndResize was initially "true" to ignore QT
     * initial resize event in case of auto-resize feature is on.
     * Currently, initial resize event is already processed, so we set
     * mIgnoreMainwndResize to "false" to process all further resize
     * events as user-initiated window resize events. */
    mIgnoreMainwndResize = false;
}

//
// Protected Events
/////////////////////////////////////////////////////////////////////////////

bool VBoxConsoleView::event (QEvent *e)
{
    if (attached)
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
                break;
            }

            case VBoxDefs::ResizeEventType:
            {
                bool oldIgnoreMainwndResize = mIgnoreMainwndResize;
                mIgnoreMainwndResize = true;

                VBoxResizeEvent *re = (VBoxResizeEvent *) e;
                LogFlow (("VBoxDefs::ResizeEventType: %d x %d x %d bpp\n",
                          re->width(), re->height(), re->bitsPerPixel()));

                if (mToggleFSModeTimer->isActive())
                    mToggleFSModeTimer->stop();

                /* do frame buffer dependent resize */
                mFrameBuf->resizeEvent (re);
                viewport()->unsetCursor();

                /* This event appears in case of guest video was changed
                 * for somehow even without video resolution change.
                 * In this last case the host VM window will not be resized
                 * according this event and the host mouse cursor which was
                 * unset to default here will not be hidden in capture state.
                 * So it is necessary to perform updateMouseClipping() for
                 * the guest resize event if the mouse cursor was captured. */
                if (mouse_captured)
                    updateMouseClipping();

                /* apply maximum size restriction */
                setMaximumSize (sizeHint());

                maybeRestrictMinimumSize();

                /* resize the guest canvas */
                resizeContents (re->width(), re->height());
                /* let our toplevel widget calculate its sizeHint properly */
                QApplication::sendPostedEvents (0, QEvent::LayoutHint);

                normalizeGeometry (true /* adjustPosition */);

                /* report to the VM thread that we finished resizing */
                cconsole.GetDisplay().ResizeCompleted (0);

                mIgnoreMainwndResize = oldIgnoreMainwndResize;

                /* emit a signal about guest was resized */
                emit resizeHintDone();

                return true;
            }

#if !defined (Q_WS_WIN) && !defined (Q_WS_PM)
            /* see VBox[QImage|SDL]FrameBuffer::NotifyUpdate(). */
            case VBoxDefs::RepaintEventType:
            {
                VBoxRepaintEvent *re = (VBoxRepaintEvent *) e;
                viewport()->repaint (re->x() - contentsX(),
                                     re->y() - contentsY(),
                                     re->width(), re->height(), false);
                /*cconsole.GetDisplay().UpdateCompleted(); - the event was acked already */
                return true;
            }
#endif

            case VBoxDefs::SetRegionEventType:
            {
                VBoxSetRegionEvent *sre = (VBoxSetRegionEvent*) e;
                if (mainwnd->isTrueSeamless() &&
                    sre->region() != mLastVisibleRegion)
                {
                    mLastVisibleRegion = sre->region();
                    mainwnd->setMask (sre->region());
                }
                else if (!mLastVisibleRegion.isNull() &&
                         !mainwnd->isTrueSeamless())
                    mLastVisibleRegion = QRegion();
                return true;
            }

            case VBoxDefs::MousePointerChangeEventType:
            {
                MousePointerChangeEvent *me = (MousePointerChangeEvent *) e;
                /* change cursor shape only when mouse integration is
                 * supported (change mouse shape type event may arrive after
                 * mouse capability change that disables integration */
                if (mouse_absolute)
                    setPointerShape (me);
                return true;
            }
            case VBoxDefs::MouseCapabilityEventType:
            {
                MouseCapabilityEvent *me = (MouseCapabilityEvent *) e;
                if (mouse_absolute != me->supportsAbsolute())
                {
                    mouse_absolute = me->supportsAbsolute();
                    /* correct the mouse capture state and reset the cursor
                     * to the default shape if necessary */
                    if (mouse_absolute)
                    {
                        CMouse mouse = cconsole.GetMouse();
                        mouse.PutMouseEventAbsolute (-1, -1, 0, 0);
                        captureMouse (false, false);
                    }
                    else
                        viewport()->unsetCursor();
                    emitMouseStateChanged();
                    vboxProblem().remindAboutMouseIntegration (mouse_absolute);
                }
                if (me->needsHostCursor())
                    mainwnd->setMouseIntegrationLocked (false);
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

                mIsAdditionsActive = ge->additionActive();

                maybeRestrictMinimumSize();

                emit additionsStateChanged (ge->additionVersion(),
                                            ge->additionActive(),
                                            ge->supportsSeamless());
                return true;
            }

            case VBoxDefs::MediaChangeEventType:
            {
                MediaChangeEvent *mce = (MediaChangeEvent *) e;
                LogFlowFunc (("MediaChangeEvent\n"));

                emit mediaChanged (mce->diskType());
                return true;
            }

            case VBoxDefs::ActivateMenuEventType:
            {
                ActivateMenuEvent *ame = (ActivateMenuEvent *) e;
                ame->menuData()->activateItemAt (ame->index());

                /*
                 *  The main window and its children can be destroyed at this
                 *  point (if, for example, the activated menu item closes the
                 *  main window). Detect this situation to prevent calls to
                 *  destroyed widgets.
                 */
                QWidgetList *list = QApplication::topLevelWidgets ();
                bool destroyed = list->find (mainwnd) < 0;
                delete list;
                if (!destroyed && mainwnd->statusBar())
                    mainwnd->statusBar()->clear();

                return true;
            }

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
                            cconsole,
                            vboxGlobal().details (ue->device()), ue->error());
                    else
                        vboxProblem().cannotDetachUSBDevice (
                            cconsole,
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
                vboxProblem().showRuntimeError (cconsole, ee->fatal(),
                                                ee->errorID(), ee->message());
                return true;
            }

            case QEvent::KeyPress:
            {
                QKeyEvent *ke = (QKeyEvent *) e;
                if (mIsHostkeyPressed)
                {
                    if (ke->key() >= Key_F1 && ke->key() <= Key_F12)
                    {
                        LONG combo [6];
                        combo [0] = 0x1d; /* Ctrl down */
                        combo [1] = 0x38; /* Alt  down */
                        combo [4] = 0xb8; /* Alt  up   */
                        combo [5] = 0x9d; /* Ctrl up   */
                        if (ke->key() >= Key_F1 && ke->key() <= Key_F10)
                        {
                            combo [2] = 0x3b + (ke->key() - Key_F1); /* F1-F10 down */
                            combo [3] = 0xbb + (ke->key() - Key_F1); /* F1-F10 up   */
                        }
                        /* some scan slice */
                        else if (ke->key() >= Key_F11 && ke->key() <= Key_F12)
                        {
                            combo [2] = 0x57 + (ke->key() - Key_F11); /* F11-F12 down */
                            combo [3] = 0xd7 + (ke->key() - Key_F11); /* F11-F12 up   */
                        }
                        else
                            Assert (0);

                        CKeyboard keyboard = cconsole.GetKeyboard();
                        Assert (!keyboard.isNull());
                        keyboard.PutScancodes (combo, 6);
                    }
                    else if (ke->key() == Key_Home)
                    {
                        /* activate the main menu */
                        if (mainwnd->isTrueSeamless() || mainwnd->isTrueFullscreen())
                            mainwnd->popupMainMenu (mouse_captured);
                        else
                            mainwnd->menuBar()->setFocus();
                    }
                    else
                    {
                        /* process hot keys not processed in keyEvent()
                         * (as in case of non-alphanumeric keys) */
                        processHotKey (QKeySequence (ke->key()),
                                       mainwnd->menuBar());
                    }
                }
                else
                {
                    if (isPaused())
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
                topLevelWidget()->show();
                topLevelWidget()->setActiveWindow();
                return true;
            }
#endif
            default:
                break;
        }
    }

    return QScrollView::event (e);
}

bool VBoxConsoleView::eventFilter (QObject *watched, QEvent *e)
{
    if (attached && watched == viewport())
    {
        switch (e->type())
        {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonRelease:
            {
                QMouseEvent *me = (QMouseEvent *) e;
                if (mouseEvent (me->type(), me->pos(), me->globalPos(),
                                me->button(), me->state(), me->stateAfter(),
                                0, Horizontal))
                    return true; /* stop further event handling */
                break;
            }
            case QEvent::Wheel:
            {
                QWheelEvent *we = (QWheelEvent *) e;
                if (mouseEvent (we->type(), we->pos(), we->globalPos(),
                                NoButton, we->state(), we->state(),
                                we->delta(), we->orientation()))
                    return true; /* stop further event handling */
                break;
            }
            case QEvent::Resize:
            {
                if (mouse_captured)
                    updateMouseClipping();
            }
            default:
                break;
        }
    }
    else if (watched == mainwnd)
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
                g_kbdhook = SetWindowsHookEx (WH_KEYBOARD_LL, lowLevelKeyboardProc,
                                              GetModuleHandle (NULL), 0);
                AssertMsg (g_kbdhook, ("SetWindowsHookEx(): err=%d", GetLastError()));
                break;
            }
            case QEvent::WindowDeactivate:
            {
                if (g_kbdhook)
                {
                    UnhookWindowsHookEx (g_kbdhook);
                    g_kbdhook = NULL;
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
            case QEvent::Resize:
            {
                if (!mIgnoreMainwndResize &&
                    mIsAdditionsActive && mAutoresizeGuest)
                    resize_hint_timer->start (300, TRUE);
                break;
            }

            default:
                break;
        }
    }
    else if (watched == mainwnd->menuBar())
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
                if (ke->key() == Key_Escape && !(ke->state() & KeyButtonMask))
                    if (mainwnd->menuBar()->hasFocus())
                        setFocus();
                break;
            }
            default:
                break;
        }
    }

    return QScrollView::eventFilter (watched, e);
}

#if defined(Q_WS_WIN32)

/**
 *  Low-level keyboard event handler,
 *  @return
 *      true to indicate that the message is processed and false otherwise
 */
bool VBoxConsoleView::winLowKeyboardEvent (UINT msg, const KBDLLHOOKSTRUCT &event)
{
#if 0
    LogFlow (("### vkCode=%08X, scanCode=%08X, flags=%08X, dwExtraInfo=%08X (kbd_captured=%d)\n",
              event.vkCode, event.scanCode, event.flags, event.dwExtraInfo, kbd_captured));
    char buf [256];
    sprintf (buf, "### vkCode=%08X, scanCode=%08X, flags=%08X, dwExtraInfo=%08X",
             event.vkCode, event.scanCode, event.flags, event.dwExtraInfo);
    mainwnd->statusBar()->message (buf);
#endif

    /* Sometimes it happens that Win inserts additional events on some key
     * press/release. For example, it prepends ALT_GR in German layout with
     * the VK_LCONTROL vkey with curious 0x21D scan code (seems to be necessary
     * to specially treat ALT_GR to enter additional chars to regular apps).
     * These events are definitely unwanted in VM, so filter them out. */
    if (hasFocus() && (event.scanCode & ~0xFF))
        return true;

    if (!kbd_captured)
        return false;

    /* it's possible that a key has been pressed while the keyboard was not
     * captured, but is being released under the capture. Detect this situation
     * and return false to let Windows process the message normally and update
     * its key state table (to avoid the stuck key effect). */
    uint8_t what_pressed = (event.flags & 0x01) && (event.vkCode != VK_RSHIFT)
                           ? IsExtKeyPressed
                           : IsKeyPressed;
    if ((event.flags & 0x80) /* released */ &&
        ((event.vkCode == gs.hostKey() && !hostkey_in_capture) ||
         (mPressedKeys [event.scanCode] & (IsKbdCaptured | what_pressed)) == what_pressed))
        return false;

    MSG message;
    message.hwnd = winId();
    message.message = msg;
    message.wParam = event.vkCode;
    message.lParam =
        1 |
        (event.scanCode & 0xFF) << 16 |
        (event.flags & 0xFF) << 24;

    /* Windows sets here the extended bit when the Right Shift key is pressed,
     * which is totally wrong. Undo it. */
    if (event.vkCode == VK_RSHIFT)
        message.lParam &= ~0x1000000;

    /* we suppose here that this hook is always called on the main GUI thread */
    return winEvent (&message);
}

/**
 * Get Win32 messages before they are passed to Qt. This allows us to get
 * the keyboard events directly and bypass the harmful Qt translation. A
 * return value of @c true indicates to Qt that the event has been handled.
 */
bool VBoxConsoleView::winEvent (MSG *msg)
{
    if (!attached || ! (
        msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN ||
        msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP
    ))
        return false;

    /* check for the special flag possibly set at the end of this function */
    if ((msg->lParam >> 25) & 0x1)
        return false;

#if 0
    char buf [256];
    sprintf (buf, "WM_%04X: vk=%04X rep=%05d scan=%02X ext=%01d rzv=%01X ctx=%01d prev=%01d tran=%01d",
             msg->message, msg->wParam,
             (msg->lParam & 0xFFFF),
             ((msg->lParam >> 16) & 0xFF),
             ((msg->lParam >> 24) & 0x1),
             ((msg->lParam >> 25) & 0xF),
             ((msg->lParam >> 29) & 0x1),
             ((msg->lParam >> 30) & 0x1),
             ((msg->lParam >> 31) & 0x1));
    mainwnd->statusBar()->message (buf);
    LogFlow (("%s\n", buf));
#endif

    int scan = (msg->lParam >> 16) & 0x7F;
    /* scancodes 0x80 and 0x00 are ignored */
    if (!scan)
        return true;

    int vkey = msg->wParam;

    /* When one of the SHIFT keys is held and one of the cursor movement
     * keys is pressed, Windows duplicates SHIFT press/release messages,
     * but with the virtual key code set to 0xFF. These virtual keys are also
     * sent in some other situations (Pause, PrtScn, etc.). Ignore such
     * messages. */
    if (vkey == 0xFF)
        return true;

    int flags = 0;
    if (msg->lParam & 0x1000000)
        flags |= KeyExtended;
    if (!(msg->lParam & 0x80000000))
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

    bool result = keyEvent (vkey, scan, flags);
    if (!result && kbd_captured)
    {
        /* keyEvent() returned that it didn't process the message, but since the
         * keyboard is captured, we don't want to pass it to Windows. We just want
         * to let Qt process the message (to handle non-alphanumeric <HOST>+key
         * shortcuts for example). So send it direcltly to the window with the
         * special flag in the reserved area of lParam (to avoid recursion). */
        ::SendMessage (msg->hwnd, msg->message,
                       msg->wParam, msg->lParam | (0x1 << 25));
        return true;
    }
    return result;
}

#elif defined (Q_WS_PM)

/**
 *  Get PM messages before they are passed to Qt. This allows us to get
 *  the keyboard events directly and bypass the harmful Qt translation. A
 *  return value of @c true indicates to Qt that the event has been handled.
 */
bool VBoxConsoleView::pmEvent (QMSG *aMsg)
{
    if (!attached || aMsg->msg != WM_CHAR)
        return false;

    /* check for the special flag possibly set at the end of this function */
    if (SHORT2FROMMP (aMsg->mp2) & 0x8000)
        return false;

#if 0
    {
        char buf [256];
        sprintf (buf, "*** WM_CHAR: f=%04X rep=%03d scan=%02X ch=%04X vk=%04X",
                 SHORT1FROMMP (aMsg->mp1), CHAR3FROMMP (aMsg->mp1),
                 CHAR4FROMMP (aMsg->mp1), SHORT1FROMMP (aMsg->mp2),
                 SHORT2FROMMP (aMsg->mp2));
        mainwnd->statusBar()->message (buf);
        LogFlow (("%s\n", buf));
    }
#endif

    USHORT ch = SHORT1FROMMP (aMsg->mp2);
    USHORT f = SHORT1FROMMP (aMsg->mp1);

    int scan = (unsigned int) CHAR4FROMMP (aMsg->mp1);
    if (!scan || scan > 0x7F)
        return true;

    int vkey = QIHotKeyEdit::virtualKey (aMsg);

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
    if (!result && kbd_captured)
    {
        /* keyEvent() returned that it didn't process the message, but since the
         * keyboard is captured, we don't want to pass it to PM. We just want
         * to let Qt process the message (to handle non-alphanumeric <HOST>+key
         * shortcuts for example). So send it direcltly to the window with the
         * special flag in the reserved area of lParam (to avoid recursion). */
        ::WinSendMsg (aMsg->hwnd, aMsg->msg,
                      aMsg->mp1,
                      MPFROM2SHORT (SHORT1FROMMP (aMsg->mp2),
                                    SHORT2FROMMP (aMsg->mp2) | 0x8000));
        return true;
    }
    return result;
}

#elif defined(Q_WS_X11)

/**
 *  This routine gets X11 events before they are processed by Qt. This is
 *  used for our platform specific keyboard implementation. A return value
 *  of TRUE indicates that the event has been processed by us.
 */
bool VBoxConsoleView::x11Event (XEvent *event)
{
    static WINEKEYBOARDINFO wineKeyboardInfo;

    switch (event->type)
    {
        case XKeyPress:
        case XKeyRelease:
            if (attached)
                break;
            /*  else fall through */
        /// @todo (AH) later, we might want to handle these as well
        case KeymapNotify:
        case MappingNotify:
        default:
            return false; /* pass the event to Qt */
    }

    /* perform the mega-complex translation using the wine algorithms */
    handleXKeyEvent (this->x11Display(), event, &wineKeyboardInfo);

#if 0
    char buf [256];
    sprintf (buf, "pr=%d kc=%08X st=%08X fl=%08lX scan=%04X",
             event->type == XKeyPress ? 1 : 0, event->xkey.keycode,
             event->xkey.state, wineKeyboardInfo.dwFlags, wineKeyboardInfo.wScan);
    mainwnd->statusBar()->message (buf);
    LogFlow (("### %s\n", buf));
#endif

    int scan = wineKeyboardInfo.wScan & 0x7F;
    // scancodes 0x00 (no valid translation) and 0x80 are ignored
    if (!scan)
        return true;

    KeySym ks = ::XKeycodeToKeysym (event->xkey.display, event->xkey.keycode, 0);

    int flags = 0;
    if (wineKeyboardInfo.dwFlags & 0x0001)
        flags |= KeyExtended;
    if (event->type == XKeyPress)
        flags |= KeyPressed;

    switch (ks)
    {
        case XK_Num_Lock:
            // Wine sets the extended bit for the NumLock key. Reset it.
            flags &= ~KeyExtended;
            break;
        case XK_Print:
            flags |= KeyPrint;
            break;
        case XK_Pause:
            flags |= KeyPause;
            break;
    }

    return keyEvent (ks, scan, flags);
}

#elif defined (Q_WS_MAC)

/**
 *  Invoked by VBoxConsoleView::darwinEventHandlerProc / VBoxConsoleView::macEventFilter when
 *  it receives a raw keyboard event.
 *
 *  @param inEvent      The keyboard event.
 *
 *  @return true if the key was processed, false if it wasn't processed and should be passed on.
 */
bool VBoxConsoleView::darwinKeyboardEvent (EventRef inEvent)
{
    bool ret = false;
    UInt32 EventKind = ::GetEventKind (inEvent);
    if (EventKind != kEventRawKeyModifiersChanged)
    {
        /* convert keycode to set 1 scan code. */
        UInt32 keyCode = ~0U;
        ::GetEventParameter (inEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof (keyCode), NULL, &keyCode);
        unsigned scanCode = ::DarwinKeycodeToSet1Scancode (keyCode);
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
            AssertCompileSize (wchar_t, 2);
            AssertCompileSize (UniChar, 2);
            UInt32 cbWritten = 0;
            wchar_t ucs[8];
            if (::GetEventParameter (inEvent, kEventParamKeyUnicodes, typeUnicodeText, NULL,
                                     sizeof (ucs), &cbWritten, &ucs[0]) != 0)
                cbWritten = 0;
            ucs[cbWritten / sizeof(wchar_t)] = 0; /* The api doesn't terminate it. */

            ret = keyEvent (keyCode, scanCode, flags, ucs[0] ? ucs : NULL);
        }
    }
    else
    {
        /* May contain multiple modifier changes, kind of annoying. */
        UInt32 newMask = 0;
        ::GetEventParameter (inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                             sizeof (newMask), NULL, &newMask);
        newMask = ::DarwinAdjustModifierMask (newMask);
        UInt32 changed = newMask ^ mDarwinKeyModifiers;
        if (changed)
        {
            for (UInt32 bit = 0; bit < 32; bit++)
            {
                if (!(changed & (1 << bit)))
                    continue;
                unsigned scanCode = ::DarwinModifierMaskToSet1Scancode (1 << bit);
                if (!scanCode)
                    continue;
                unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode (1 << bit);
                Assert (keyCode);

                if (!(scanCode & VBOXKEY_LOCK))
                {
                    unsigned flags = (newMask & (1 << bit)) ? KeyPressed : 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    ret |= keyEvent (keyCode, scanCode & 0xff, flags);
                }
                else
                {
                    unsigned flags = 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    keyEvent (keyCode, scanCode, flags | KeyPressed);
                    keyEvent (keyCode, scanCode, flags);
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


/**
 * Installs or removes the keyboard event handler.
 *
 * @param   fGrab    True if we're to grab the events, false if we're not to.
 */
void VBoxConsoleView::darwinGrabKeyboardEvents (bool fGrab)
{
    if (fGrab)
    {
        ::SetMouseCoalescingEnabled (false, NULL);      //??
        ::CGSetLocalEventsSuppressionInterval (0.0);    //??

#ifndef VBOX_WITH_HACKED_QT

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

        EventHandlerUPP eventHandler = ::NewEventHandlerUPP (VBoxConsoleView::darwinEventHandlerProc);

        mDarwinEventHandlerRef = NULL;
        ::InstallApplicationEventHandler (eventHandler, RT_ELEMENTS (eventTypes), &eventTypes[0],
                                          this, &mDarwinEventHandlerRef);
        ::DisposeEventHandlerUPP (eventHandler);

#else /* VBOX_WITH_HACKED_QT */
        ((QIApplication *)qApp)->setEventFilter (VBoxConsoleView::macEventFilter, this);
#endif /* VBOX_WITH_HACKED_QT */

        ::DarwinGrabKeyboard (false);
    }
    else
    {
        ::DarwinReleaseKeyboard ();
#ifndef VBOX_WITH_HACKED_QT
        if (mDarwinEventHandlerRef)
        {
            ::RemoveEventHandler (mDarwinEventHandlerRef);
            mDarwinEventHandlerRef = NULL;
        }
#else
        ((QIApplication *)qApp)->setEventFilter (NULL, NULL);
#endif
    }
}

#endif // defined (Q_WS_WIN)

//
// Private members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Called on every focus change
 *  and also to forcibly capture/uncapture the input in situations similar
 *  to gaining or losing focus.
 *
 *  @param aHasFocus True if the window got focus and false otherwise.
 */
void VBoxConsoleView::focusEvent (bool aHasFocus)
{
    if (aHasFocus)
    {
#ifdef RT_OS_WINDOWS
        if (   gs.autoCapture()
            && GetAncestor (winId(), GA_ROOT) == GetForegroundWindow())
#else
        if (gs.autoCapture())
#endif /* RT_OS_WINDOWS */
        {
            captureKbd (true);
/// @todo (dmik)
//      the below is for the mouse auto-capture. disabled for now. in order to
//      properly support it, we need to know when *all* mouse buttons are
//      released after we got focus, and grab the mouse only after then.
//      btw, the similar would be good the for keyboard auto-capture, too.
//            if (!(mouse_absolute && mouse_integration))
//                captureMouse (true);
        }
    }
    else
    {
        captureMouse (false);
        captureKbd (false, false);
        releaseAllPressedKeys (!aHasFocus);
    }
}

/**
 *  Synchronize the views of the host and the guest to the modifier keys.
 *  This function will add up to 6 additional keycodes to codes.
 *
 *  @param  codes  pointer to keycodes which are sent to the keyboard
 *  @param  count  pointer to the keycodes counter
 */
void VBoxConsoleView::fixModifierState(LONG *codes, uint *count)
{
#if defined(Q_WS_X11)

    Window   wDummy1, wDummy2;
    int      iDummy3, iDummy4, iDummy5, iDummy6;
    unsigned uMask;
    unsigned uKeyMaskNum = 0, uKeyMaskCaps = 0, uKeyMaskScroll = 0;

    uKeyMaskCaps          = LockMask;
    XModifierKeymap* map  = XGetModifierMapping(qt_xdisplay());
    KeyCode keyCodeNum    = XKeysymToKeycode(qt_xdisplay(), XK_Num_Lock);
    KeyCode keyCodeScroll = XKeysymToKeycode(qt_xdisplay(), XK_Scroll_Lock);

    for (int i = 0; i < 8; i++)
    {
        if (   keyCodeNum != NoSymbol
            && map->modifiermap[map->max_keypermod * i] == keyCodeNum)
            uKeyMaskNum    = 1 << i;
        else if (   keyCodeScroll != NoSymbol
                 && map->modifiermap[map->max_keypermod * i] == keyCodeScroll)
            uKeyMaskScroll = 1 << i;
    }
    XQueryPointer(qt_xdisplay(), DefaultRootWindow(qt_xdisplay()), &wDummy1, &wDummy2,
                  &iDummy3, &iDummy4, &iDummy5, &iDummy6, &uMask);
    XFreeModifiermap(map);

    if (muNumLockAdaptionCnt && (mNumLock ^ !!(uMask & uKeyMaskNum)))
    {
        muNumLockAdaptionCnt--;
        codes[(*count)++] = 0x45;
        codes[(*count)++] = 0x45 | 0x80;
    }
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(uMask & uKeyMaskCaps)))
    {
        muCapsLockAdaptionCnt--;
        codes[(*count)++] = 0x3a;
        codes[(*count)++] = 0x3a | 0x80;
    }

#elif defined(Q_WS_WIN32)

    if (muNumLockAdaptionCnt && (mNumLock ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        muNumLockAdaptionCnt--;
        codes[(*count)++] = 0x45;
        codes[(*count)++] = 0x45 | 0x80;
    }
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(GetKeyState(VK_CAPITAL))))
    {
        muCapsLockAdaptionCnt--;
        codes[(*count)++] = 0x3a;
        codes[(*count)++] = 0x3a | 0x80;
    }

#elif defined(Q_WS_MAC)

    /* if (muNumLockAdaptionCnt) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (muCapsLockAdaptionCnt && (mCapsLock ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        muCapsLockAdaptionCnt--;
        codes[(*count)++] = 0x3a;
        codes[(*count)++] = 0x3a | 0x80;
    }

#else

#warning Adapt VBoxConsoleView::fixModifierState

#endif


}

/**
 *  Called on enter/exit seamless/fullscreen mode.
 */
void VBoxConsoleView::toggleFSMode()
{
    if (mIsAdditionsActive && mAutoresizeGuest)
    {
        QSize newSize = QSize();
        if (mainwnd->isTrueFullscreen() || mainwnd->isTrueSeamless())
        {
            mNormalSize = frameSize();
            newSize = maximumSize();
        }
        else
            newSize = mNormalSize;
        doResizeHint (newSize);
    }
    mToggleFSModeTimer->start (400, true);
}

/**
 *  Called on every key press and release (while in focus).
 *
 *  @key        virtual scan code (virtual key on Win32 and KeySym on X11)
 *  @scan       hardware scan code
 *  @flags      flags, a combination of Key* constants
 *  @aUniKey    Unicode translation of the key. Optional.
 *
 *  @return     true to consume the event and false to pass it to Qt
 */
bool VBoxConsoleView::keyEvent (int aKey, uint8_t aScan, int aFlags,
                                wchar_t *aUniKey/* = NULL*/)
{
#if 0
    {
        char buf [256];
        sprintf (buf, "aKey=%08X aScan=%02X aFlags=%08X",
                 aKey, aScan, aFlags);
        mainwnd->statusBar()->message (buf);
    }
#endif

    const bool isHostKey = aKey == gs.hostKey();

    LONG buf [16];
    LONG *codes = buf;
    uint count = 0;
    uint8_t whatPressed = 0;

    if (!isHostKey)
    {
        if (aFlags & KeyPrint)
        {
            static LONG PrintMake[] = { 0xE0, 0x2A, 0xE0, 0x37 };
            static LONG PrintBreak[] = { 0xE0, 0xB7, 0xE0, 0xAA };
            if (aFlags & KeyPressed)
            {
                codes = PrintMake;
                count = SIZEOF_ARRAY (PrintMake);
            }
            else
            {
                codes = PrintBreak;
                count = SIZEOF_ARRAY (PrintBreak);
            }
        }
        else if (aFlags & KeyPause)
        {
            if (aFlags & KeyPressed)
            {
                static LONG Pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
                codes = Pause;
                count = SIZEOF_ARRAY (Pause);
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
                fixModifierState (codes, &count);
            }

            /* process the scancode and update the table of pressed keys */
            whatPressed = IsKeyPressed;

            if (aFlags & KeyExtended)
            {
                codes [count++] = 0xE0;
                whatPressed = IsExtKeyPressed;
            }

            if (aFlags & KeyPressed)
            {
                codes [count++] = aScan;
                mPressedKeys [aScan] |= whatPressed;
            }
            else
            {
                /* if we haven't got this key's press message, we ignore its
                 * release */
                if (!(mPressedKeys [aScan] & whatPressed))
                    return true;
                codes [count++] = aScan | 0x80;
                mPressedKeys [aScan] &= ~whatPressed;
            }

            if (kbd_captured)
                mPressedKeys [aScan] |= IsKbdCaptured;
            else
                mPressedKeys [aScan] &= ~IsKbdCaptured;
        }
    }
    else
    {
        /* currently this is used in winLowKeyboardEvent() only */
        hostkey_in_capture = kbd_captured;
    }

    bool emitSignal = false;
    int hotkey = 0;

    /* process the host key */
    if (aFlags & KeyPressed)
    {
        if (isHostKey)
        {
            if (!mIsHostkeyPressed)
            {
                mIsHostkeyPressed = mIsHostkeyAlone = true;
                if (isRunning())
                    saveKeyStates();
                emitSignal = true;
            }
        }
        else
        {
            if (mIsHostkeyPressed)
            {
                if (mIsHostkeyAlone)
                {
                    hotkey = aKey;
                    mIsHostkeyAlone = false;
                }
            }
        }
    }
    else
    {
        if (isHostKey)
        {
            if (mIsHostkeyPressed)
            {
                mIsHostkeyPressed = false;
                if (mIsHostkeyAlone)
                {
                    if (isPaused())
                    {
                        vboxProblem().remindAboutPausedVMInput();
                    }
                    else
                    if (isRunning())
                    {
                        bool captured = kbd_captured;
                        if (!captured)
                            vboxProblem().remindAboutInputCapture();
                        captureKbd (!captured, false);
                        if (!(mouse_absolute && mouse_integration))
                            captureMouse (kbd_captured);
                    }
                }
                if (isRunning())
                    sendChangedKeyStates();
                emitSignal = true;
            }
        }
        else
        {
            if (mIsHostkeyPressed)
                mIsHostkeyAlone = false;
        }
    }

    /* emit the keyboard state change signal */
    if (emitSignal)
        emitKeyboardStateChanged();

    /* process HOST+<key> shortcuts. currently, <key> is limited to
     * alphanumeric chars. */
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
                processed = processHotKey (QKeySequence (UNICODE_ACCEL +
                                                QChar (ch).upper().unicode()),
                                           mainwnd->menuBar());
        }
        delete[] list;
#elif defined (Q_WS_X11)
        NOREF(aUniKey);
        Display *display = x11Display();
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
                processed = processHotKey (QKeySequence (UNICODE_ACCEL +
                                                c.upper().unicode()),
                                           mainwnd->menuBar());
            }
        }
#elif defined (Q_WS_MAC)
        if (aUniKey && aUniKey [0] && !aUniKey [1])
            processed = processHotKey (QKeySequence (UNICODE_ACCEL +
                                                     QChar (aUniKey [0]).upper().unicode()),
                                       mainwnd->menuBar());

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
    if (mIsHostkeyPressed || isHostKey || isPaused())
    {
        /* grab the key from Qt and from VM if it's a host key,
         * otherwise just pass it to Qt */
        return isHostKey;
    }

    CKeyboard keyboard = cconsole.GetKeyboard();
    Assert (!keyboard.isNull());

#if defined (Q_WS_WIN32)
    /* send pending WM_PAINT events */
    ::UpdateWindow (viewport()->winId());
#endif

#if 0
    {
        char buf [256];
        sprintf (buf, "*** SCANS: ");
        for (uint i = 0; i < count; ++ i)
            sprintf (buf + strlen (buf), "%02X ", codes [i]);
        mainwnd->statusBar()->message (buf);
        LogFlow (("%s\n", buf));
    }
#endif

    keyboard.PutScancodes (codes, count);

    /* grab the key from Qt */
    return true;
}

/**
 *  Called on every mouse/wheel move and button press/release.
 *
 *  @return     true to consume the event and false to pass it to Qt
 */
bool VBoxConsoleView::mouseEvent (int aType, const QPoint &aPos,
                                  const QPoint &aGlobalPos, ButtonState aButton,
                                  ButtonState aState, ButtonState aStateAfter,
                                  int aWheelDelta, Orientation aWheelDir)
{
#if 0
    char buf [256];
    sprintf (buf,
             "MOUSE: type=%03d x=%03d y=%03d btn=%03d st=%08X stAfter=%08X "
             "wdelta=%03d wdir=%03d",
             aType, aPos.x(), aPos.y(), aButton, aState, aStateAfter,
             aWheelDelta, aWheelDir);
    mainwnd->statusBar()->message (buf);
#else
    Q_UNUSED (aButton);
    Q_UNUSED (aState);
#endif

    int state = 0;
    if (aStateAfter & LeftButton)
        state |= CEnums::LeftButton;
    if (aStateAfter & RightButton)
        state |= CEnums::RightButton;
    if (aStateAfter & MidButton)
        state |= CEnums::MiddleButton;

    int wheel = 0;
    if (aWheelDir == Vertical)
    {
        /* the absolute value of wheel delta is 120 units per every wheel
         * move; positive deltas correspond to counterclockwize rotations
         * (usually up), negative -- to clockwize (usually down). */
        wheel = - (aWheelDelta / 120);
    }

    if (mouse_captured)
    {
#ifdef Q_WS_WIN32
        /* send pending WM_PAINT events */
        ::UpdateWindow (viewport()->winId());
#endif

        CMouse mouse = cconsole.GetMouse();
        mouse.PutMouseEvent (aGlobalPos.x() - last_pos.x(),
                             aGlobalPos.y() - last_pos.y(),
                             wheel, state);

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
        QRect rect = viewport()->visibleRect();
        QPoint pw = viewport()->mapToGlobal (viewport()->pos());
        rect.moveBy (pw.x(), pw.y());

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
            last_pos = aGlobalPos;
        else
        {
            last_pos = rect.center();
            QCursor::setPos (last_pos);
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
            last_pos = viewport()->mapToGlobal (p);
            QCursor::setPos (last_pos);
        }
        else
        {
            last_pos = aGlobalPos;
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
            last_pos =  p;
            QCursor::setPos (last_pos);
        }
        else
        {
            last_pos = aGlobalPos;
        }
#endif
#endif /* !Q_WS_MAC */
        return true; /* stop further event handling */
    }
    else /* !mouse_captured */
    {
#ifdef Q_WS_MAC
        /* Update the mouse cursor; this is a bit excessive really... */
        if (!DarwinCursorIsNull (&mDarwinCursor))
            DarwinCursorSet (&mDarwinCursor);
#endif
        if (mainwnd->isTrueFullscreen())
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
                    if (scrGeo.rLeft() == aGlobalPos.x()) dx = -1;
                    if (scrGeo.rRight() == aGlobalPos.x()) dx = +1;
                }
                if (scrGeo.height() < contentsHeight())
                {
                    if (scrGeo.rTop() == aGlobalPos.y()) dy = -1;
                    if (scrGeo.rBottom() == aGlobalPos.y()) dy = +1;
                }
                if (dx || dy)
                    scrollBy (dx, dy);
            }
        }

        if (mouse_absolute && mouse_integration)
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
            else if (cpnt.x() >= cw) cpnt.setX (cw - 1);
            if (cpnt.y() < 0) cpnt.setY (0);
            else if (cpnt.y() >= ch) cpnt.setY (ch - 1);

            CMouse mouse = cconsole.GetMouse();
            mouse.PutMouseEventAbsolute (cpnt.x() + 1, cpnt.y() + 1,
                                         wheel, state);
            return true; /* stop further event handling */
        }
        else
        {
            if (hasFocus() &&
                (aType == QEvent::MouseButtonRelease &&
                 !aStateAfter))
            {
                if (isPaused())
                {
                    vboxProblem().remindAboutPausedVMInput();
                }
                else if (isRunning())
                {
                    vboxProblem().remindAboutInputCapture();
                    captureKbd (true);
                    captureMouse (true);
                }
            }
        }
    }

    return false;
}

void VBoxConsoleView::onStateChange (CEnums::MachineState state)
{
    switch (state)
    {
        case CEnums::Paused:
        {
            if (mode != VBoxDefs::TimerMode && mFrameBuf)
            {
                /*
                 *  Take a screen snapshot. Note that TakeScreenShot() always
                 *  needs a 32bpp image
                 */
                QImage shot = QImage (mFrameBuf->width(), mFrameBuf->height(), 32, 0);
                CDisplay dsp = cconsole.GetDisplay();
                dsp.TakeScreenShot (shot.bits(), shot.width(), shot.height());
                /*
                 *  TakeScreenShot() may fail if, e.g. the Paused notification
                 *  was delivered after the machine execution was resumed. It's
                 *  not fatal.
                 */
                if (dsp.isOk())
                {
                    dimImage (shot);
                    mPausedShot = shot;
                    /* fully repaint to pick up mPausedShot */
                    viewport()->repaint();
                }
            }
            /* fall through */
        }
        case CEnums::Stuck:
        {
            /* reuse the focus event handler to uncapture everything */
            if (hasFocus())
                focusEvent (false);
            break;
        }
        case CEnums::Running:
        {
            if (last_state == CEnums::Paused)
            {
                if (mode != VBoxDefs::TimerMode && mFrameBuf)
                {
                    /* reset the pixmap to free memory */
                    mPausedShot.resize (0, 0);
                    /*
                     *  ask for full guest display update (it will also update
                     *  the viewport through IFramebuffer::NotifyUpdate)
                     */
                    CDisplay dsp = cconsole.GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
            /* reuse the focus event handler to capture input */
            if (hasFocus())
                focusEvent (true);
            break;
        }
        default:
            break;
    }

    last_state = state;
}

void VBoxConsoleView::doRefresh()
{
#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    if ( mode == VBoxDefs::TimerMode ) {
        FRAMEBUF_DEBUG_START( xxx );
        QSize last_sz = pm.size();
        bool rc = display_to_pixmap( cconsole, pm );
        if ( rc ) {
            if ( pm.size() != last_sz ) {
                int pw = pm.width(), ph = pm.height();
                resizeContents( pw, ph );
                updateGeometry();
                setMaximumSize( sizeHint() );
                // let our toplevel widget calculate its sizeHint properly
                QApplication::sendPostedEvents( 0, QEvent::LayoutHint );
                normalizeGeometry();
            } else {
                // the alternative is to update, so we will be repainted
                // on the next event loop iteration. currently disabled.
                //updateContents();
                repaintContents( false );
            }
        }
        if ( rc )
            FRAMEBUF_DEBUG_STOP( xxx, pm.width(), pm.height() );
    }
    else
#endif
        repaintContents( false );
}

void VBoxConsoleView::viewportPaintEvent (QPaintEvent *pe)
{
#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    if (mode == VBoxDefs::TimerMode)
    {
        if (!pm.isNull())
        {
            /* draw a part of vbuf */
            const QRect &r = pe->rect();
            ::bitBlt (viewport(), r.x(), r.y(),
                      &pm, r.x() + contentsX(), r.y() + contentsY(),
                      r.width(), r.height(),
                      CopyROP, TRUE);
        }
        else
        {
            viewport()->erase (pe->rect());
        }
    }
    else
#endif
    {
        if (mPausedShot.isNull())
        {
            /* delegate the paint function to the VBoxFrameBuffer interface */
            mFrameBuf->paintEvent (pe);
            return;
        }

        /* we have a snapshot for the paused state */
        QRect r = pe->rect().intersect (viewport()->rect());
        QPainter pnt (viewport());
        pnt.drawPixmap (r.x(), r.y(), mPausedShot,
                        r.x() + contentsX(), r.y() + contentsY(),
                        r.width(), r.height());
    }
}

#ifdef VBOX_GUI_USE_REFRESH_TIMER
void VBoxConsoleView::timerEvent( QTimerEvent * )
{
    doRefresh();
}
#endif

/**
 *  Captures the keyboard. When captured, no keyboard input reaches the host
 *  system (including most system combinations like Alt-Tab).
 *
 *  @capture        true to capture, false to uncapture
 *  @emit_signal    whether to emit keyboardStateChanged() or not
 */
void VBoxConsoleView::captureKbd (bool capture, bool emit_signal)
{
    AssertMsg (attached, ("Console must be attached"));

    if (kbd_captured == capture)
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
	if (capture)
		XGrabKey (x11Display(), AnyKey, AnyModifier,
                  topLevelWidget()->winId(), False,
                  GrabModeAsync, GrabModeAsync);
	else
		XUngrabKey (x11Display(),  AnyKey, AnyModifier,
                    topLevelWidget()->winId());
#elif defined (Q_WS_MAC)
    if (capture)
    {
        ::DarwinDisableGlobalHotKeys (true);
        grabKeyboard();
    }
    else
    {
        ::DarwinDisableGlobalHotKeys (false);
        releaseKeyboard();
    }
#else
    if (capture)
        grabKeyboard();
    else
        releaseKeyboard();
#endif

    kbd_captured = capture;

    if (emit_signal)
        emitKeyboardStateChanged ();
}

/**
 *  Captures the host mouse pointer. When captured, the mouse pointer is
 *  unavailable to the host applications.
 *
 *  @capture    true to capture, false to uncapture
 */
void VBoxConsoleView::captureMouse (bool capture, bool emit_signal)
{
    AssertMsg (attached, ("Console must be attached"));

    if (mouse_captured == capture)
        return;

    if (capture)
    {
        /* memorize the host position where the cursor was captured */
        captured_pos = QCursor::pos();
#ifdef Q_WS_WIN32
        viewport()->setCursor (QCursor (BlankCursor));
        /* move the mouse to the center of the visible area */
        QCursor::setPos (mapToGlobal (visibleRect().center()));
        last_pos = QCursor::pos();
#elif defined (Q_WS_MAC)
        /* move the mouse to the center of the visible area */
        last_pos = mapToGlobal (visibleRect().center());
        QCursor::setPos (last_pos);
        /* grab all mouse events. */
        viewport()->grabMouse();
#else
        viewport()->grabMouse();
        last_pos = QCursor::pos();
#endif
    }
    else
    {
#ifndef Q_WS_WIN32
        viewport()->releaseMouse();
#endif
        /* release mouse buttons */
        CMouse mouse = cconsole.GetMouse();
        mouse.PutMouseEvent (0, 0, 0, 0);
    }

    mouse_captured = capture;

    updateMouseClipping();

    if (emit_signal)
        emitMouseStateChanged ();
}

/**
 *  Searches for a menu item with a given hot key (shortcut). If the item
 *  is found, activates it and returns true. Otherwise returns false.
 */
bool VBoxConsoleView::processHotKey (const QKeySequence &key,  QMenuData *data)
{
    if (!data) return false;

    /*
     *  Note: below, we use the internal class QMenuItem, that is subject
     *  to change w/o notice... (the alternative would be to explicitly assign
     *  specific IDs to all popup submenus in VBoxConsoleWnd and then
     *  look through its children in order to find a popup with a given ID,
     *  which is unconvenient).
     */

    for (uint i = 0; i < data->count(); i++)
    {
        int id = data->idAt (i);
        QMenuItem *item = data->findItem (id);
        if (item->popup())
        {
            if (processHotKey (key, item->popup()))
                return true;
        }
        else
        {
            QStringList list = QStringList::split ("\tHost+", data->text (id));
            if (list.count() == 2)
            {
                if (key.matches (QKeySequence (list[1])) == Identical)
                {
                    /*
                     *  we asynchronously post a special event instead of calling
                     *  data->activateItemAt (i) directly, to let key presses
                     *  and releases be processed correctly by Qt first.
                     *  Note: we assume that nobody will delete the menu item
                     *  corresponding to the key sequence, so that the pointer to
                     *  menu data posted along with the event will remain valid in
                     *  the event handler, at least until the main window is closed.
                     */

                    QApplication::postEvent (this,
                                             new ActivateMenuEvent (data, i));
                    return true;
                }
            }
        }
    }

    return false;
}

void VBoxConsoleView::releaseAllPressedKeys (bool aReleaseHostkey)
{
    AssertMsg (attached, ("Console must be attached"));

    CKeyboard keyboard = cconsole.GetKeyboard();
    bool fSentRESEND = false;

    // send a dummy scan code (RESEND) to prevent the guest OS from recognizing
    // a single key click (for ex., Alt) and performing an unwanted action
    // (for ex., activating the menu) when we release all pressed keys below.
    // Note, that it's just a guess that sending RESEND will give the desired
    // effect :), but at least it works with NT and W2k guests.

    // @TODO Sending 0xFE is responsible for the warning
    //
    //         ``atkbd.c: Spurious NAK on isa0060/serio0. Some program might
    //           be trying access hardware directly''
    //
    //       on Linux guests (#1944). It might also be responsible for #1949. Don't
    //       send this command unless we really have to release any key modifier.
    //                                                                    --frank
    for (uint i = 0; i < SIZEOF_ARRAY (mPressedKeys); i++)
    {
        if (mPressedKeys [i] & IsKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode (0xFE);
                fSentRESEND = true;
            }
            keyboard.PutScancode (i | 0x80);
        }
        else if (mPressedKeys [i] & IsExtKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode (0xFE);
                fSentRESEND = true;
            }
            LONG codes [2];
            codes[0] = 0xE0;
            codes[1] = i | 0x80;
            keyboard.PutScancodes (codes, 2);
        }
        mPressedKeys [i] = 0;
    }

    if (aReleaseHostkey)
        mIsHostkeyPressed = false;

#ifdef Q_WS_MAC
    /* clear most of the modifiers. */
    mDarwinKeyModifiers &=
        alphaLock | kEventKeyModifierNumLockMask |
        (aReleaseHostkey ? 0 : ::DarwinKeyCodeToDarwinModifierMask (gs.hostKey()));
#endif

    emitKeyboardStateChanged ();
}

void VBoxConsoleView::saveKeyStates()
{
    ::memcpy (mPressedKeysCopy, mPressedKeys,
              SIZEOF_ARRAY (mPressedKeys));
}

void VBoxConsoleView::sendChangedKeyStates()
{
    AssertMsg (attached, ("Console must be attached"));

    LONG codes [2];
    CKeyboard keyboard = cconsole.GetKeyboard();
    for (uint i = 0; i < SIZEOF_ARRAY (mPressedKeys); ++ i)
    {
        uint8_t os = mPressedKeysCopy [i];
        uint8_t ns = mPressedKeys [i];
        if ((os & IsKeyPressed) != (ns & IsKeyPressed))
        {
            codes [0] = i;
            if (!(ns & IsKeyPressed))
                codes[0] |= 0x80;
            keyboard.PutScancode (codes[0]);
        }
        else if ((os & IsExtKeyPressed) != (ns & IsExtKeyPressed))
        {
            codes [0] = 0xE0;
            codes [1] = i;
            if (!(ns & IsExtKeyPressed))
                codes [1] |= 0x80;
            keyboard.PutScancodes (codes, 2);
        }
    }
}

void VBoxConsoleView::updateMouseClipping()
{
    AssertMsg (attached, ("Console must be attached"));

    if ( mouse_captured ) {
        viewport()->setCursor( QCursor( BlankCursor ) );
#ifdef Q_WS_WIN32
        QRect r = viewport()->rect();
        r.moveTopLeft( viewport()->mapToGlobal( QPoint( 0, 0 ) ) );
        RECT rect = { r.left(), r.top(), r.right() + 1, r.bottom() + 1 };
        ::ClipCursor( &rect );
#endif
    } else {
#ifdef Q_WS_WIN32
        ::ClipCursor( NULL );
#endif
        /* return the cursor to where it was when we captured it and show it */
        QCursor::setPos( captured_pos );
        viewport()->unsetCursor();
    }
}

void VBoxConsoleView::setPointerShape (MousePointerChangeEvent *me)
{
    if (me->shapeData () != NULL)
    {
        bool ok = false;

        const uchar *srcAndMaskPtr = me->shapeData();
        uint andMaskSize = (me->width() + 7) / 8 * me->height();
        const uchar *srcShapePtr = me->shapeData() + ((andMaskSize + 3) & ~3);
        uint srcShapePtrScan = me->width() * 4;

#if defined (Q_WS_WIN)

        BITMAPV5HEADER bi;
        HBITMAP hBitmap;
        void *lpBits;

        ::ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
        bi.bV5Size = sizeof (BITMAPV5HEADER);
        bi.bV5Width = me->width();
        bi.bV5Height = - (LONG) me->height();
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        // specifiy a supported 32 BPP alpha format for Windows XP
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        if (me->hasAlpha())
            bi.bV5AlphaMask = 0xFF000000;
        else
            bi.bV5AlphaMask = 0;

        HDC hdc = GetDC (NULL);

        // create the DIB section with an alpha channel
        hBitmap = CreateDIBSection (hdc, (BITMAPINFO *) &bi, DIB_RGB_COLORS,
                                    (void **) &lpBits, NULL, (DWORD) 0);

        ReleaseDC (NULL, hdc);

        HBITMAP hMonoBitmap = NULL;
        if (me->hasAlpha())
        {
            // create an empty mask bitmap
            hMonoBitmap = CreateBitmap (me->width(), me->height(), 1, 1, NULL);
        }
        else
        {
            /* Word aligned AND mask. Will be allocated and created if necessary. */
            uint8_t *pu8AndMaskWordAligned = NULL;

            /* Width in bytes of the original AND mask scan line. */
            uint32_t cbAndMaskScan = (me->width() + 7) / 8;

            if (cbAndMaskScan & 1)
            {
                /* Original AND mask is not word aligned. */

                /* Allocate memory for aligned AND mask. */
                pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ ((cbAndMaskScan + 1) * me->height());

                Assert(pu8AndMaskWordAligned);

                if (pu8AndMaskWordAligned)
                {
                    /* According to MSDN the padding bits must be 0.
                     * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask.
                     */
                    uint32_t u32PaddingBits = cbAndMaskScan * 8  - me->width();
                    Assert(u32PaddingBits < 8);
                    uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                    Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                          u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, me->width(), cbAndMaskScan));

                    uint8_t *src = (uint8_t *)srcAndMaskPtr;
                    uint8_t *dst = pu8AndMaskWordAligned;

                    unsigned i;
                    for (i = 0; i < me->height(); i++)
                    {
                        memcpy (dst, src, cbAndMaskScan);

                        dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                        src += cbAndMaskScan;
                        dst += cbAndMaskScan + 1;
                    }
                }
            }

            /* create the AND mask bitmap */
            hMonoBitmap = ::CreateBitmap (me->width(), me->height(), 1, 1,
                                          pu8AndMaskWordAligned? pu8AndMaskWordAligned: srcAndMaskPtr);

            if (pu8AndMaskWordAligned)
            {
                RTMemTmpFree (pu8AndMaskWordAligned);
            }
        }

        Assert (hBitmap);
        Assert (hMonoBitmap);
        if (hBitmap && hMonoBitmap)
        {
            DWORD *dstShapePtr = (DWORD *) lpBits;

            for (uint y = 0; y < me->height(); y ++)
            {
                memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);
                srcShapePtr += srcShapePtrScan;
                dstShapePtr += me->width();
            }

            ICONINFO ii;
            ii.fIcon = FALSE;
            ii.xHotspot = me->xHot();
            ii.yHotspot = me->yHot();
            ii.hbmMask = hMonoBitmap;
            ii.hbmColor = hBitmap;

            HCURSOR hAlphaCursor = CreateIconIndirect (&ii);
            Assert (hAlphaCursor);
            if (hAlphaCursor)
            {
                viewport()->setCursor (QCursor (hAlphaCursor));
                ok = true;
                if (mAlphaCursor)
                    DestroyIcon (mAlphaCursor);
                mAlphaCursor = hAlphaCursor;
            }
        }

        if (hMonoBitmap)
            DeleteObject (hMonoBitmap);
        if (hBitmap)
            DeleteObject (hBitmap);

#elif defined (Q_WS_X11) && !defined (VBOX_WITHOUT_XCURSOR)

        XcursorImage *img = XcursorImageCreate (me->width(), me->height());
        Assert (img);
        if (img)
        {
            img->xhot = me->xHot();
            img->yhot = me->yHot();

            XcursorPixel *dstShapePtr = img->pixels;

            for (uint y = 0; y < me->height(); y ++)
            {
                memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);

                if (!me->hasAlpha())
                {
                    /* convert AND mask to the alpha channel */
                    uchar byte = 0;
                    for (uint x = 0; x < me->width(); x ++)
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
                dstShapePtr += me->width();
            }

            Cursor cur = XcursorImageLoadCursor (x11Display(), img);
            Assert (cur);
            if (cur)
            {
                viewport()->setCursor (QCursor (cur));
                ok = true;
            }

            XcursorImageDestroy (img);
        }

#elif defined(Q_WS_MAC)

        /*
         * Qt3/Mac only supports black/white cursors and it offers no way
         * to create your own cursors here unlike on X11 and Windows.
         * Which means we're pretty much forced to do it our own way.
         */
        int rc;

        /* dispose of the old cursor. */
        if (!DarwinCursorIsNull (&mDarwinCursor))
        {
            rc = DarwinCursorDestroy (&mDarwinCursor);
            AssertRC (rc);
        }

        /* create the new cursor */
        rc = DarwinCursorCreate (me->width(), me->height(), me->xHot(), me->yHot(), me->hasAlpha(),
                                 srcAndMaskPtr, srcShapePtr, &mDarwinCursor);
        AssertRC (rc);
        if (VBOX_SUCCESS (rc))
        {
            /** @todo check current mouse coordinates. */
            rc = DarwinCursorSet (&mDarwinCursor);
            AssertRC (rc);
        }
        ok = VBOX_SUCCESS (rc);
        NOREF (srcShapePtrScan);

#else

# warning "port me"

#endif
        if (!ok)
            viewport()->unsetCursor();
    }
    else
    {
        /*
         * We did not get any shape data
         */
        if (me->isVisible ())
        {
            /*
             * We're supposed to make the last shape we got visible.
             * We don't support that for now...
             */
            /// @todo viewport()->setCursor (QCursor());
        }
        else
        {
            viewport()->setCursor (QCursor::BlankCursor);
        }
    }
}

inline QRgb qRgbIntensity (QRgb rgb, int mul, int div)
{
    int r = qRed (rgb);
    int g = qGreen (rgb);
    int b = qBlue (rgb);
    return qRgb (mul * r / div, mul * g / div, mul * b / div);
}

/* static */
void VBoxConsoleView::dimImage (QImage &img)
{
    for (int y = 0; y < img.height(); y ++) {
        if (y % 2) {
            if (img.depth() == 32) {
                for (int x = 0; x < img.width(); x ++) {
                    int gray = qGray (img.pixel (x, y)) / 2;
                    img.setPixel (x, y, qRgb (gray, gray, gray));
//                    img.setPixel (x, y, qRgbIntensity (img.pixel (x, y), 1, 2));
                }
            } else {
                ::memset (img.scanLine (y), 0, img.bytesPerLine());
            }
        } else {
            if (img.depth() == 32) {
                for (int x = 0; x < img.width(); x ++) {
                    int gray = (2 * qGray (img.pixel (x, y))) / 3;
                    img.setPixel (x, y, qRgb (gray, gray, gray));
//                    img.setPixel (x, y, qRgbIntensity (img.pixel(x, y), 2, 3));
                }
            }
        }
    }
}

void VBoxConsoleView::doResizeHint (const QSize &aToSize)
{
    if (mIsAdditionsActive && mAutoresizeGuest)
    {
        /* If this slot is invoked directly then use the passed size
         * otherwise get the available size for the guest display.
         * We assume here that the centralWidget() contains this view only
         * and gives it all available space. */
        QSize sz (aToSize.isValid() ? aToSize : mainwnd->centralWidget()->size());
        if (!aToSize.isValid())
            sz -= QSize (frameWidth() * 2, frameWidth() * 2);
        LogFlowFunc (("Will suggest %d x %d\n", sz.width(), sz.height()));

        cconsole.GetDisplay().SetVideoModeHint (sz.width(), sz.height(), 0, 0);
    }
}

/**
 *  Sets the the minimum size restriction depending on the auto-resize feature
 *  state and the current rendering mode.
 *
 *  Currently, the restriction is set only in SDL mode and only when the
 *  auto-resize feature is inactive. We need to do that because we cannot
 *  correctly draw in a scrolled window in SDL mode.
 *
 *  In all other modes, or when auto-resize is in force, this function does
 *  nothing.
 */
void VBoxConsoleView::maybeRestrictMinimumSize()
{
    if (mode == VBoxDefs::SDLMode)
    {
        if (!mIsAdditionsActive || !mAutoresizeGuest)
            setMinimumSize (sizeHint());
        else
            setMinimumSize (0, 0);
    }
}
