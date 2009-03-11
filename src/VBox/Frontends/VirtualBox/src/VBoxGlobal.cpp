/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxGlobal class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include "VBoxGlobal.h"
#include <VBox/VBoxHDD.h>

#include "VBoxDefs.h"
#include "VBoxSelectorWnd.h"
#include "VBoxConsoleWnd.h"
#include "VBoxProblemReporter.h"
#include "QIHotKeyEdit.h"
#include "QIMessageBox.h"
#include "QIDialogButtonBox.h"

#ifdef VBOX_WITH_REGISTRATION
#include "VBoxRegistrationDlg.h"
#endif
#include "VBoxUpdateDlg.h"

/* Qt includes */
#include <QLibraryInfo>
#include <QFileDialog>
#include <QToolTip>
#include <QTranslator>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QMutex>
#include <QToolButton>
#include <QProcess>
#include <QThread>
#include <QPainter>
#include <QTimer>

#include <math.h>

#ifdef Q_WS_X11
#ifndef VBOX_OSE
# include "VBoxLicenseViewer.h"
#endif /* VBOX_OSE */

#include <QTextBrowser>
#include <QScrollBar>
#include <QX11Info>
#endif

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

#if defined (Q_WS_WIN)
#include "shlobj.h"
#include <QEventLoop>
#endif

#if defined (Q_WS_X11)
#undef BOOL /* typedef CARD8 BOOL in Xmd.h conflicts with #define BOOL PRBool
             * in COMDefs.h. A better fix would be to isolate X11-specific
             * stuff by placing XX* helpers below to a separate source file. */
#include <X11/X.h>
#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define BOOL PRBool
#endif

#include <VBox/sup.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/system.h>

#ifdef VBOX_GUI_WITH_SYSTRAY
#include <iprt/process.h>

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
#define HOSTSUFF_EXE ".exe"
#else /* !RT_OS_WINDOWS */
#define HOSTSUFF_EXE ""
#endif /* !RT_OS_WINDOWS */
#endif

#if defined (Q_WS_X11)
#include <iprt/mem.h>
#endif

//#warning "port me: check this"
/// @todo bird: Use (U)INT_PTR, (U)LONG_PTR, DWORD_PTR, or (u)intptr_t.
#if defined(Q_OS_WIN64)
typedef __int64 Q_LONG;             /* word up to 64 bit signed */
typedef unsigned __int64 Q_ULONG;   /* word up to 64 bit unsigned */
#else
typedef long Q_LONG;                /* word up to 64 bit signed */
typedef unsigned long Q_ULONG;      /* word up to 64 bit unsigned */
#endif

// VBoxMediaEnumEvent
/////////////////////////////////////////////////////////////////////////////

class VBoxMediaEnumEvent : public QEvent
{
public:

    /** Constructs a regular enum event */
    VBoxMediaEnumEvent (const VBoxMedium &aMedium,
                        VBoxMediaList::iterator &aIterator)
        : QEvent ((QEvent::Type) VBoxDefs::MediaEnumEventType)
        , mMedium (aMedium), mIterator (aIterator), mLast (false)
        {}
    /** Constructs the last enum event */
    VBoxMediaEnumEvent (VBoxMediaList::iterator &aIterator)
        : QEvent ((QEvent::Type) VBoxDefs::MediaEnumEventType)
        , mIterator (aIterator), mLast (true)
        {}

    /** Last enumerated medium (not valid when #last is true) */
    const VBoxMedium mMedium;
    /** Opaque iterator provided by the event sender (guaranteed to be
     *  the same variable for all media in the single enumeration procedure) */
    VBoxMediaList::iterator &mIterator;
    /** Whether this is the last event for the given enumeration or not */
    const bool mLast;
};

// VirtualBox callback class
/////////////////////////////////////////////////////////////////////////////

class VBoxCallback : public IVirtualBoxCallback
{
public:

    VBoxCallback (VBoxGlobal &aGlobal)
        : mGlobal (aGlobal)
        , mIsRegDlgOwner (false)
        , mIsUpdDlgOwner (false)
#ifdef VBOX_GUI_WITH_SYSTRAY
        , mIsTrayIconOwner (false)
#endif
    {
#if defined (Q_OS_WIN32)
        refcnt = 0;
#endif
    }

    virtual ~VBoxCallback() {}

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
        if (riid == IID_IVirtualBoxCallback) {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    // IVirtualBoxCallback methods

    // Note: we need to post custom events to the GUI event queue
    // instead of doing what we need directly from here because on Win32
    // these callback methods are never called on the main GUI thread.
    // Another reason to handle events asynchronously is that internally
    // most callback interface methods are called from under the initiator
    // object's lock, so accessing the initiator object (for example, reading
    // some property) directly from the callback method will definitely cause
    // a deadlock.

    STDMETHOD(OnMachineStateChange) (IN_GUID id, MachineState_T state)
    {
        postEvent (new VBoxMachineStateChangeEvent (COMBase::ToQUuid (id),
                                                    (KMachineState) state));
        return S_OK;
    }

    STDMETHOD(OnMachineDataChange) (IN_GUID id)
    {
        postEvent (new VBoxMachineDataChangeEvent (COMBase::ToQUuid (id)));
        return S_OK;
    }

    STDMETHOD(OnExtraDataCanChange)(IN_GUID id,
                                    IN_BSTR key, IN_BSTR value,
                                    BSTR *error, BOOL *allowChange)
    {
        if (!error || !allowChange)
            return E_INVALIDARG;

        if (COMBase::ToQUuid (id).isNull())
        {
            /* it's a global extra data key someone wants to change */
            QString sKey = QString::fromUtf16 (key);
            QString sVal = QString::fromUtf16 (value);
            if (sKey.startsWith ("GUI/"))
            {
                if (sKey == VBoxDefs::GUI_RegistrationDlgWinID)
                {
                    if (mIsRegDlgOwner)
                    {
                        if (sVal.isEmpty() ||
                            sVal == QString ("%1")
                                .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                            *allowChange = TRUE;
                        else
                            *allowChange = FALSE;
                    }
                    else
                        *allowChange = TRUE;
                    return S_OK;
                }

                if (sKey == VBoxDefs::GUI_UpdateDlgWinID)
                {
                    if (mIsUpdDlgOwner)
                    {
                        if (sVal.isEmpty() ||
                            sVal == QString ("%1")
                                .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                            *allowChange = TRUE;
                        else
                            *allowChange = FALSE;
                    }
                    else
                        *allowChange = TRUE;
                    return S_OK;
                }
#ifdef VBOX_GUI_WITH_SYSTRAY
                if (sKey == VBoxDefs::GUI_TrayIconWinID)
                {
                    if (mIsTrayIconOwner)
                    {
                        if (sVal.isEmpty() ||
                            sVal == QString ("%1")
                                .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                            *allowChange = TRUE;
                        else
                            *allowChange = FALSE;
                    }
                    else
                        *allowChange = TRUE;
                    return S_OK;
                }
#endif
                /* try to set the global setting to check its syntax */
                VBoxGlobalSettings gs (false /* non-null */);
                if (gs.setPublicProperty (sKey, sVal))
                {
                    /* this is a known GUI property key */
                    if (!gs)
                    {
                        /* disallow the change when there is an error*/
                        *error = SysAllocString ((const OLECHAR *)
                            (gs.lastError().isNull() ? 0 : gs.lastError().utf16()));
                        *allowChange = FALSE;
                    }
                    else
                        *allowChange = TRUE;
                    return S_OK;
                }
            }
        }

        /* not interested in this key -- never disagree */
        *allowChange = TRUE;
        return S_OK;
    }

    STDMETHOD(OnExtraDataChange) (IN_GUID id,
                                  IN_BSTR key, IN_BSTR value)
    {
        if (COMBase::ToQUuid (id).isNull())
        {
            QString sKey = QString::fromUtf16 (key);
            QString sVal = QString::fromUtf16 (value);
            if (sKey.startsWith ("GUI/"))
            {
                if (sKey == VBoxDefs::GUI_RegistrationDlgWinID)
                {
                    if (sVal.isEmpty())
                    {
                        mIsRegDlgOwner = false;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowRegDlgEvent (true));
                    }
                    else if (sVal == QString ("%1")
                             .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                    {
                        mIsRegDlgOwner = true;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowRegDlgEvent (true));
                    }
                    else
                        QApplication::postEvent (&mGlobal, new VBoxCanShowRegDlgEvent (false));
                }
                if (sKey == VBoxDefs::GUI_UpdateDlgWinID)
                {
                    if (sVal.isEmpty())
                    {
                        mIsUpdDlgOwner = false;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowUpdDlgEvent (true));
                    }
                    else if (sVal == QString ("%1")
                             .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                    {
                        mIsUpdDlgOwner = true;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowUpdDlgEvent (true));
                    }
                    else
                        QApplication::postEvent (&mGlobal, new VBoxCanShowUpdDlgEvent (false));
                }
                if (sKey == "GUI/LanguageID")
                    QApplication::postEvent (&mGlobal, new VBoxChangeGUILanguageEvent (sVal));
#ifdef VBOX_GUI_WITH_SYSTRAY
                if (sKey == "GUI/MainWindowCount")
                    QApplication::postEvent (&mGlobal, new VBoxMainWindowCountChangeEvent (sVal.toInt()));
                if (sKey == VBoxDefs::GUI_TrayIconWinID)
                {
                    if (sVal.isEmpty())
                    {
                        mIsTrayIconOwner = false;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowTrayIconEvent (true));
                    }
                    else if (sVal == QString ("%1")
                             .arg ((qulonglong) vboxGlobal().mainWindow()->winId()))
                    {
                        mIsTrayIconOwner = true;
                        QApplication::postEvent (&mGlobal, new VBoxCanShowTrayIconEvent (true));
                    }
                    else
                        QApplication::postEvent (&mGlobal, new VBoxCanShowTrayIconEvent (false));
                }
                if (sKey == "GUI/TrayIcon/Enabled")
                    QApplication::postEvent (&mGlobal, new VBoxChangeTrayIconEvent ((sVal.toLower() == "true") ? true : false));
#endif
#ifdef Q_WS_MAC
                if (sKey == VBoxDefs::GUI_RealtimeDockIconUpdateEnabled)
                {
                    /* Default to true if it is an empty value */
                    QString testStr = sVal.toLower();
                    bool f = (testStr.isEmpty() || testStr == "true");
                    QApplication::postEvent (&mGlobal, new VBoxChangeDockIconUpdateEvent (f));
                }
#endif

                mMutex.lock();
                mGlobal.gset.setPublicProperty (sKey, sVal);
                mMutex.unlock();
                Assert (!!mGlobal.gset);
            }
        }
        return S_OK;
    }

    STDMETHOD(OnMediaRegistered) (IN_GUID id, DeviceType_T type,
                                  BOOL registered)
    {
        /** @todo */
        Q_UNUSED (id);
        Q_UNUSED (type);
        Q_UNUSED (registered);
        return S_OK;
    }

    STDMETHOD(OnMachineRegistered) (IN_GUID id, BOOL registered)
    {
        postEvent (new VBoxMachineRegisteredEvent (COMBase::ToQUuid (id),
                                                   registered));
        return S_OK;
    }

    STDMETHOD(OnSessionStateChange) (IN_GUID id, SessionState_T state)
    {
        postEvent (new VBoxSessionStateChangeEvent (COMBase::ToQUuid (id),
                                                    (KSessionState) state));
        return S_OK;
    }

    STDMETHOD(OnSnapshotTaken) (IN_GUID aMachineId, IN_GUID aSnapshotId)
    {
        postEvent (new VBoxSnapshotEvent (COMBase::ToQUuid (aMachineId),
                                          COMBase::ToQUuid (aSnapshotId),
                                          VBoxSnapshotEvent::Taken));
        return S_OK;
    }

    STDMETHOD(OnSnapshotDiscarded) (IN_GUID aMachineId, IN_GUID aSnapshotId)
    {
        postEvent (new VBoxSnapshotEvent (COMBase::ToQUuid (aMachineId),
                                          COMBase::ToQUuid (aSnapshotId),
                                          VBoxSnapshotEvent::Discarded));
        return S_OK;
    }

    STDMETHOD(OnSnapshotChange) (IN_GUID aMachineId, IN_GUID aSnapshotId)
    {
        postEvent (new VBoxSnapshotEvent (COMBase::ToQUuid (aMachineId),
                                          COMBase::ToQUuid (aSnapshotId),
                                          VBoxSnapshotEvent::Changed));
        return S_OK;
    }

    STDMETHOD(OnGuestPropertyChange) (IN_GUID /* id */,
                                      IN_BSTR /* key */,
                                      IN_BSTR /* value */,
                                      IN_BSTR /* flags */)
    {
        return S_OK;
    }

private:

    void postEvent (QEvent *e)
    {
        // currently, we don't post events if we are in the VM execution
        // console mode, to save some CPU ticks (so far, there was no need
        // to handle VirtualBox callback events in the execution console mode)
        if (!mGlobal.isVMConsoleProcess())
            QApplication::postEvent (&mGlobal, e);
    }

    VBoxGlobal &mGlobal;

    /** protects #OnExtraDataChange() */
    QMutex mMutex;

    bool mIsRegDlgOwner;
    bool mIsUpdDlgOwner;
#ifdef VBOX_GUI_WITH_SYSTRAY
    bool mIsTrayIconOwner;
#endif

#if defined (Q_OS_WIN32)
private:
    long refcnt;
#endif
};

#if !defined (Q_OS_WIN32)
NS_DECL_CLASSINFO (VBoxCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (VBoxCallback, IVirtualBoxCallback)
#endif

// Helpers for VBoxGlobal::getOpenFileName() & getExistingDirectory()
/////////////////////////////////////////////////////////////////////////////

#if defined Q_WS_WIN

extern void qt_enter_modal (QWidget*);
extern void qt_leave_modal (QWidget*);

static QString extractFilter (const QString &aRawFilter)
{
    static const char qt_file_dialog_filter_reg_exp[] =
        "([a-zA-Z0-9 ]*)\\(([a-zA-Z0-9_.*? +;#\\[\\]]*)\\)$";

    QString result = aRawFilter;
    QRegExp r (QString::fromLatin1 (qt_file_dialog_filter_reg_exp));
    int index = r.indexIn (result);
    if (index >= 0)
        result = r.cap (2);
    return result.replace (QChar (' '), QChar (';'));
}

/**
 * Converts QFileDialog filter list to Win32 API filter list.
 */
static QString winFilter (const QString &aFilter)
{
    QStringList filterLst;

    if (!aFilter.isEmpty())
    {
        int i = aFilter.indexOf (";;", 0);
        QString sep (";;");
        if (i == -1)
        {
            if (aFilter.indexOf ("\n", 0) != -1)
            {
                sep = "\n";
                i = aFilter.indexOf (sep, 0);
            }
        }

        filterLst = aFilter.split (sep);
    }

    QStringList::Iterator it = filterLst.begin();
    QString winfilters;
    for (; it != filterLst.end(); ++it)
    {
        winfilters += *it;
        winfilters += QChar::Null;
        winfilters += extractFilter (*it);
        winfilters += QChar::Null;
    }
    winfilters += QChar::Null;
    return winfilters;
}

/*
 * Callback function to control the native Win32 API file dialog
 */
UINT_PTR CALLBACK OFNHookProc (HWND aHdlg, UINT aUiMsg, WPARAM aWParam, LPARAM aLParam)
{
    if (aUiMsg == WM_NOTIFY)
    {
        OFNOTIFY *notif = (OFNOTIFY*) aLParam;
        if (notif->hdr.code == CDN_TYPECHANGE)
        {
            /* locate native dialog controls */
            HWND parent = GetParent (aHdlg);
            HWND button = GetDlgItem (parent, IDOK);
            HWND textfield = ::GetDlgItem (parent, cmb13);
            if (textfield == NULL)
                textfield = ::GetDlgItem (parent, edt1);
            if (textfield == NULL)
                return FALSE;
            HWND selector = ::GetDlgItem (parent, cmb1);

            /* simulate filter change by pressing apply-key */
            int    size = 256;
            TCHAR *buffer = (TCHAR*)malloc (size);
            SendMessage (textfield, WM_GETTEXT, size, (LPARAM)buffer);
            SendMessage (textfield, WM_SETTEXT, 0, (LPARAM)"\0");
            SendMessage (button, BM_CLICK, 0, 0);
            SendMessage (textfield, WM_SETTEXT, 0, (LPARAM)buffer);
            free (buffer);

            /* make request for focus moving to filter selector combo-box */
            HWND curFocus = GetFocus();
            PostMessage (curFocus, WM_KILLFOCUS, (WPARAM)selector, 0);
            PostMessage (selector, WM_SETFOCUS, (WPARAM)curFocus, 0);
            WPARAM wParam = MAKEWPARAM (WA_ACTIVE, 0);
            PostMessage (selector, WM_ACTIVATE, wParam, (LPARAM)curFocus);
        }
    }
    return FALSE;
}

/*
 * Callback function to control the native Win32 API folders dialog
 */
static int __stdcall winGetExistDirCallbackProc (HWND hwnd, UINT uMsg,
                                                 LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED && lpData != 0)
    {
        QString *initDir = (QString *)(lpData);
        if (!initDir->isEmpty())
        {
            SendMessage (hwnd, BFFM_SETSELECTION, TRUE, Q_ULONG (
                initDir->isNull() ? 0 : initDir->utf16()));
            //SendMessage (hwnd, BFFM_SETEXPANDED, TRUE, Q_ULONG (initDir->utf16()));
        }
    }
    else if (uMsg == BFFM_SELCHANGED)
    {
        TCHAR path [MAX_PATH];
        SHGetPathFromIDList (LPITEMIDLIST (lParam), path);
        QString tmpStr = QString::fromUtf16 ((ushort*)path);
        if (!tmpStr.isEmpty())
            SendMessage (hwnd, BFFM_ENABLEOK, 1, 1);
        else
            SendMessage (hwnd, BFFM_ENABLEOK, 0, 0);
        SendMessage (hwnd, BFFM_SETSTATUSTEXT, 1, Q_ULONG (path));
    }
    return 0;
}

/**
 *  QEvent class to carry Win32 API native dialog's result information
 */
class OpenNativeDialogEvent : public QEvent
{
public:

    OpenNativeDialogEvent (const QString &aResult, QEvent::Type aType)
        : QEvent (aType), mResult (aResult) {}

    const QString& result() { return mResult; }

private:

    QString mResult;
};

/**
 *  QObject class reimplementation which is the target for OpenNativeDialogEvent
 *  event. It receives OpenNativeDialogEvent event from another thread,
 *  stores result information and exits the given local event loop.
 */
class LoopObject : public QObject
{
public:

    LoopObject (QEvent::Type aType, QEventLoop &aLoop)
        : mType (aType), mLoop (aLoop), mResult (QString::null) {}
    const QString& result() { return mResult; }

private:

    bool event (QEvent *aEvent)
    {
        if (aEvent->type() == mType)
        {
            OpenNativeDialogEvent *ev = (OpenNativeDialogEvent*) aEvent;
            mResult = ev->result();
            mLoop.quit();
            return true;
        }
        return QObject::event (aEvent);
    }

    QEvent::Type mType;
    QEventLoop &mLoop;
    QString mResult;
};

#endif /* Q_WS_WIN */


// VBoxGlobal
////////////////////////////////////////////////////////////////////////////////

static bool sVBoxGlobalInited = false;
static bool sVBoxGlobalInCleanup = false;

/** @internal
 *
 *  Special routine to do VBoxGlobal cleanup when the application is being
 *  terminated. It is called before some essential Qt functionality (for
 *  instance, QThread) becomes unavailable, allowing us to use it from
 *  VBoxGlobal::cleanup() if necessary.
 */
static void vboxGlobalCleanup()
{
    Assert (!sVBoxGlobalInCleanup);
    sVBoxGlobalInCleanup = true;
    vboxGlobal().cleanup();
}

/** @internal
 *
 *  Determines the rendering mode from the argument. Sets the appropriate
 *  default rendering mode if the argumen is NULL.
 */
static VBoxDefs::RenderMode vboxGetRenderMode (const char *aModeStr)
{
    VBoxDefs::RenderMode mode = VBoxDefs::InvalidRenderMode;

#if defined (Q_WS_MAC) && defined (VBOX_GUI_USE_QUARTZ2D)
    mode = VBoxDefs::Quartz2DMode;
#elif (defined (Q_WS_WIN32) || defined (Q_WS_PM) || defined (Q_WS_X11)) && defined (VBOX_GUI_USE_QIMAGE)
    mode = VBoxDefs::QImageMode;
#elif defined (Q_WS_X11) && defined (VBOX_GUI_USE_SDL)
    mode = VBoxDefs::SDLMode;
#elif defined (VBOX_GUI_USE_QIMAGE)
    mode = VBoxDefs::QImageMode;
#else
# error "Cannot determine the default render mode!"
#endif

    if (aModeStr)
    {
        if (0) ;
#if defined (VBOX_GUI_USE_QIMAGE)
        else if (::strcmp (aModeStr, "image") == 0)
            mode = VBoxDefs::QImageMode;
#endif
#if defined (VBOX_GUI_USE_SDL)
        else if (::strcmp (aModeStr, "sdl") == 0)
            mode = VBoxDefs::SDLMode;
#endif
#if defined (VBOX_GUI_USE_DDRAW)
        else if (::strcmp (aModeStr, "ddraw") == 0)
            mode = VBoxDefs::DDRAWMode;
#endif
#if defined (VBOX_GUI_USE_QUARTZ2D)
        else if (::strcmp (aModeStr, "quartz2d") == 0)
            mode = VBoxDefs::Quartz2DMode;
#endif
    }

    return mode;
}

/** @class VBoxGlobal
 *
 *  The VBoxGlobal class incapsulates the global VirtualBox data.
 *
 *  There is only one instance of this class per VirtualBox application,
 *  the reference to it is returned by the static instance() method, or by
 *  the global vboxGlobal() function, that is just an inlined shortcut.
 */

VBoxGlobal::VBoxGlobal()
    : mValid (false)
    , mSelectorWnd (NULL), mConsoleWnd (NULL)
    , mMainWindow (NULL)
#ifdef VBOX_WITH_REGISTRATION
    , mRegDlg (NULL)
#endif
    , mUpdDlg (NULL)
#ifdef VBOX_GUI_WITH_SYSTRAY
    , mIsTrayMenu (false)
    , mIncreasedWindowCounter (false)
#endif
    , mMediaEnumThread (NULL)
    , mVerString ("1.0")
    , mDetailReportTemplatesReady (false)
{
}

//
// Public members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Returns a reference to the global VirtualBox data, managed by this class.
 *
 *  The main() function of the VBox GUI must call this function soon after
 *  creating a QApplication instance but before opening any of the main windows
 *  (to let the VBoxGlobal initialization procedure use various Qt facilities),
 *  and continue execution only when the isValid() method of the returned
 *  instancereturns true, i.e. do something like:
 *
 *  @code
 *  if ( !VBoxGlobal::instance().isValid() )
 *      return 1;
 *  @endcode
 *  or
 *  @code
 *  if ( !vboxGlobal().isValid() )
 *      return 1;
 *  @endcode
 *
 *  @note Some VBoxGlobal methods can be used on a partially constructed
 *  VBoxGlobal instance, i.e. from constructors and methods called
 *  from the VBoxGlobal::init() method, which obtain the instance
 *  using this instance() call or the ::vboxGlobal() function. Currently, such
 *  methods are:
 *      #vmStateText, #vmTypeIcon, #vmTypeText, #vmTypeTextList, #vmTypeFromText.
 *
 *  @see ::vboxGlobal
 */
VBoxGlobal &VBoxGlobal::instance()
{
    static VBoxGlobal vboxGlobal_instance;

    if (!sVBoxGlobalInited)
    {
        /* check that a QApplication instance is created */
        if (qApp)
        {
            sVBoxGlobalInited = true;
            vboxGlobal_instance.init();
            /* add our cleanup handler to the list of Qt post routines */
            qAddPostRoutine (vboxGlobalCleanup);
        }
        else
            AssertMsgFailed (("Must construct a QApplication first!"));
    }
    return vboxGlobal_instance;
}

VBoxGlobal::~VBoxGlobal()
{
    qDeleteAll (mOsTypeIcons);
    qDeleteAll (mVMStateIcons);
    qDeleteAll (mVMStateColors);
}

/**
 *  Sets the new global settings and saves them to the VirtualBox server.
 */
bool VBoxGlobal::setSettings (const VBoxGlobalSettings &gs)
{
    gs.save (mVBox);

    if (!mVBox.isOk())
    {
        vboxProblem().cannotSaveGlobalConfig (mVBox);
        return false;
    }

    /* We don't assign gs to our gset member here, because VBoxCallback
     * will update gset as necessary when new settings are successfullly
     * sent to the VirtualBox server by gs.save(). */

    return true;
}

/**
 *  Returns a reference to the main VBox VM Selector window.
 *  The reference is valid until application termination.
 *
 *  There is only one such a window per VirtualBox application.
 */
VBoxSelectorWnd &VBoxGlobal::selectorWnd()
{
#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)
    AssertMsg (!vboxGlobal().isVMConsoleProcess(),
               ("Must NOT be a VM console process"));
#endif

    Assert (mValid);

    if (!mSelectorWnd)
    {
        /*
         *  We pass the address of mSelectorWnd to the constructor to let it be
         *  initialized right after the constructor is called. It is necessary
         *  to avoid recursion, since this method may be (and will be) called
         *  from the below constructor or from constructors/methods it calls.
         */
        VBoxSelectorWnd *w = new VBoxSelectorWnd (&mSelectorWnd, 0);
        Assert (w == mSelectorWnd);
        NOREF(w);
    }

    return *mSelectorWnd;
}

/**
 *  Returns a reference to the main VBox VM Console window.
 *  The reference is valid until application termination.
 *
 *  There is only one such a window per VirtualBox application.
 */
VBoxConsoleWnd &VBoxGlobal::consoleWnd()
{
#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)
    AssertMsg (vboxGlobal().isVMConsoleProcess(),
               ("Must be a VM console process"));
#endif

    Assert (mValid);

    if (!mConsoleWnd)
    {
        /*
         *  We pass the address of mConsoleWnd to the constructor to let it be
         *  initialized right after the constructor is called. It is necessary
         *  to avoid recursion, since this method may be (and will be) called
         *  from the below constructor or from constructors/methods it calls.
         */
        VBoxConsoleWnd *w = new VBoxConsoleWnd (&mConsoleWnd, 0);
        Assert (w == mConsoleWnd);
        NOREF(w);
    }

    return *mConsoleWnd;
}

#ifdef VBOX_GUI_WITH_SYSTRAY

/**
 *  Returns true if the current instance a systray menu only (started with
 *  "-systray" parameter).
 */
bool VBoxGlobal::isTrayMenu() const
{
    return mIsTrayMenu;
}

void VBoxGlobal::setTrayMenu(bool aIsTrayMenu)
{
    mIsTrayMenu = aIsTrayMenu;
}

/**
 *  Spawns a new selector window (process).
 */
void VBoxGlobal::trayIconShowSelector()
{
    /* Get the path to the executable. */
    char path [RTPATH_MAX];
    RTPathAppPrivateArch (path, RTPATH_MAX);
    size_t sz = strlen (path);
    path [sz++] = RTPATH_DELIMITER;
    path [sz] = 0;
    char *cmd = path + sz;
    sz = RTPATH_MAX - sz;

    int rc = 0;
    RTPROCESS pid = NIL_RTPROCESS;
    RTENV env = RTENV_DEFAULT;

    const char VirtualBox_exe[] = "VirtualBox" HOSTSUFF_EXE;
    Assert (sz >= sizeof (VirtualBox_exe));
    strcpy (cmd, VirtualBox_exe);
    const char * args[] = {path, 0 };
# ifdef RT_OS_WINDOWS
    rc = RTProcCreate (path, args, env, 0, &pid);
# else
    rc = RTProcCreate (path, args, env, RTPROC_FLAGS_DAEMONIZE, &pid);
# endif
    if (RT_FAILURE (rc))
        LogRel(("Systray: Failed to start new selector window! Path=%s, rc=%Rrc\n", path, rc));
}

/**
 *  Tries to install the tray icon using the current instance (singleton).
 *  Returns true if this instance is the tray icon, false if not.
 */
bool VBoxGlobal::trayIconInstall()
{
    int rc = 0;
    QString strTrayWinID = mVBox.GetExtraData (VBoxDefs::GUI_TrayIconWinID);
    if (false == strTrayWinID.isEmpty())
    {
        /* Check if current tray icon is alive by writing some bogus value. */
        mVBox.SetExtraData (VBoxDefs::GUI_TrayIconWinID, "0");
        if (mVBox.isOk())
        {
            /* Current tray icon died - clean up. */
            mVBox.SetExtraData (VBoxDefs::GUI_TrayIconWinID, NULL);
            strTrayWinID.clear();
        }
    }

    /* Is there already a tray icon or is tray icon not active? */
    if (   (mIsTrayMenu == false)
        && (vboxGlobal().settings().trayIconEnabled())
        && (QSystemTrayIcon::isSystemTrayAvailable())
        && (strTrayWinID.isEmpty()))
    {
        /* Get the path to the executable. */
        char path [RTPATH_MAX];
        RTPathAppPrivateArch (path, RTPATH_MAX);
        size_t sz = strlen (path);
        path [sz++] = RTPATH_DELIMITER;
        path [sz] = 0;
        char *cmd = path + sz;
        sz = RTPATH_MAX - sz;

        RTPROCESS pid = NIL_RTPROCESS;
        RTENV env = RTENV_DEFAULT;

        const char VirtualBox_exe[] = "VirtualBox" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VirtualBox_exe));
        strcpy (cmd, VirtualBox_exe);
        const char * args[] = {path, "-systray", 0 };
    # ifdef RT_OS_WINDOWS /** @todo drop this once the RTProcCreate bug has been fixed */
        rc = RTProcCreate (path, args, env, 0, &pid);
    # else
        rc = RTProcCreate (path, args, env, RTPROC_FLAGS_DAEMONIZE, &pid);
    # endif

        if (RT_FAILURE (rc))
        {
            LogRel(("Systray: Failed to start systray window! Path=%s, rc=%Rrc\n", path, rc));
            return false;
        }
    }

    if (mIsTrayMenu)
    {
        // Use this selector for displaying the tray icon
        mVBox.SetExtraData (VBoxDefs::GUI_TrayIconWinID,
                            QString ("%1").arg ((qulonglong) vboxGlobal().mainWindow()->winId()));

        /* The first process which can grab this "mutex" will win ->
         * It will be the tray icon menu then. */
        if (mVBox.isOk())
        {
            emit trayIconShow (*(new VBoxShowTrayIconEvent (true)));
            return true;
        }
    }

    return false;
}

#endif

/**
 *  Returns the list of few guest OS types, queried from
 *  IVirtualBox corresponding to every family id.
 */
QList <CGuestOSType> VBoxGlobal::vmGuestOSFamilyList() const
{
    QList <CGuestOSType> result;
    for (int i = 0 ; i < mFamilyIDs.size(); ++ i)
        result << mTypes [i][0];
    return result;
}

/**
 *  Returns the list of all guest OS types, queried from
 *  IVirtualBox corresponding to passed family id.
 */
QList <CGuestOSType> VBoxGlobal::vmGuestOSTypeList (const QString &aFamilyId) const
{
    AssertMsg (mFamilyIDs.contains (aFamilyId), ("Family ID incorrect: '%s'.", aFamilyId.toLatin1().constData()));
    return mFamilyIDs.contains (aFamilyId) ?
           mTypes [mFamilyIDs.indexOf (aFamilyId)] : QList <CGuestOSType>();
}

/**
 *  Returns the icon corresponding to the given guest OS type id.
 */
QPixmap VBoxGlobal::vmGuestOSTypeIcon (const QString &aTypeId) const
{
    static const QPixmap none;
    QPixmap *p = mOsTypeIcons.value (aTypeId);
    AssertMsg (p, ("Icon for type '%s' must be defined.", aTypeId.toLatin1().constData()));
    return p ? *p : none;
}

/**
 *  Returns the guest OS type object corresponding to the given type id of list
 *  containing OS types related to OS family determined by family id attribute.
 *  If the index is invalid a null object is returned.
 */
CGuestOSType VBoxGlobal::vmGuestOSType (const QString &aTypeId,
             const QString &aFamilyId /* = QString::null */) const
{
    QList <CGuestOSType> list;
    if (mFamilyIDs.contains (aFamilyId))
    {
        list = mTypes [mFamilyIDs.indexOf (aFamilyId)];
    }
    else
    {
        for (int i = 0; i < mFamilyIDs.size(); ++ i)
            list += mTypes [i];
    }
    for (int j = 0; j < list.size(); ++ j)
        if (!list [j].GetId().compare (aTypeId))
            return list [j];
    AssertMsgFailed (("Type ID incorrect: '%s'.", aTypeId.toLatin1().constData()));
    return CGuestOSType();
}

/**
 *  Returns the description corresponding to the given guest OS type id.
 */
QString VBoxGlobal::vmGuestOSTypeDescription (const QString &aTypeId) const
{
    for (int i = 0; i < mFamilyIDs.size(); ++ i)
    {
        QList <CGuestOSType> list (mTypes [i]);
        for ( int j = 0; j < list.size(); ++ j)
            if (!list [j].GetId().compare (aTypeId))
                return list [j].GetDescription();
    }
    return QString::null;
}

/**
 * Returns a string representation of the given channel number on the given
 * storage bus. Complementary to #toStorageChannel (KStorageBus, const
 * QString &) const.
 */
QString VBoxGlobal::toString (KStorageBus aBus, LONG aChannel) const
{
    QString channel;

    switch (aBus)
    {
        case KStorageBus_IDE:
        {
            if (aChannel == 0 || aChannel == 1)
            {
                channel = mStorageBusChannels [aChannel];
                break;
            }

            AssertMsgFailedBreak (("Invalid channel %d\n", aChannel));
        }
        case KStorageBus_SATA:
        {
            channel = mStorageBusChannels [2].arg (aChannel);
            break;
        }
        case KStorageBus_SCSI:
        {
            channel = mStorageBusChannels [2].arg (aChannel);
            break;
        }
        default:
            AssertFailedBreak();
    }

    Assert (!channel.isNull());
    return channel;
}

/**
 * Returns a channel number on the given storage bus corresponding to the given
 * string representation. Complementary to #toString (KStorageBus, LONG) const.
 */
LONG VBoxGlobal::toStorageChannel (KStorageBus aBus, const QString &aChannel) const
{
    LONG channel = 0;

    switch (aBus)
    {
        case KStorageBus_IDE:
        {
            QLongStringHash::const_iterator it =
                qFind (mStorageBusChannels.begin(), mStorageBusChannels.end(),
                       aChannel);
            AssertMsgBreak (it != mStorageBusChannels.end(),
                            ("No value for {%s}\n", aChannel.toLatin1().constData()));
            channel = it.key();
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            /// @todo use regexp to properly extract the %1 text
            QString tpl = mStorageBusChannels [2].arg ("");
            if (aChannel.startsWith (tpl))
            {
                channel = aChannel.right (aChannel.length() - tpl.length()).toLong();
                break;
            }

            AssertMsgFailedBreak (("Invalid channel {%s}\n", aChannel.toLatin1().constData()));
            break;
        }
        default:
            AssertFailedBreak();
    }

    return channel;
}

/**
 * Returns a string representation of the given device number of the given
 * channel on the given storage bus. Complementary to #toStorageDevice
 * (KStorageBus, LONG, const QString &) const.
 */
QString VBoxGlobal::toString (KStorageBus aBus, LONG aChannel, LONG aDevice) const
{
    NOREF (aChannel);

    QString device;

    switch (aBus)
    {
        case KStorageBus_IDE:
        {
            if (aDevice == 0 || aDevice == 1)
            {
                device = mStorageBusDevices [aDevice];
                break;
            }

            AssertMsgFailedBreak (("Invalid device %d\n", aDevice));
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            AssertMsgBreak (aDevice == 0, ("Invalid device %d\n", aDevice));
            /* always empty so far for SATA */
            device = "";
            break;
        }
        default:
            AssertFailedBreak();
    }

    Assert (!device.isNull());
    return device;
}

/**
 * Returns a device number of the given channel on the given storage bus
 * corresponding to the given string representation. Complementary to #toString
 * (KStorageBus, LONG, LONG) const.
 */
LONG VBoxGlobal::toStorageDevice (KStorageBus aBus, LONG aChannel,
                                  const QString &aDevice) const
{
    NOREF (aChannel);

    LONG device = 0;

    switch (aBus)
    {
        case KStorageBus_IDE:
        {
            QLongStringHash::const_iterator it =
                qFind (mStorageBusDevices.begin(), mStorageBusDevices.end(),
                       aDevice);
            AssertMsg (it != mStorageBusDevices.end(),
                       ("No value for {%s}", aDevice.toLatin1().constData()));
            device = it.key();
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            AssertMsgBreak(aDevice.isEmpty(), ("Invalid device {%s}\n", aDevice.toLatin1().constData()));
            /* always zero for SATA so far. */
            break;
        }
        default:
            AssertFailedBreak();
    }

    return device;
}

/**
 * Returns a full string representation of the given device of the given channel
 * on the given storage bus. Complementary to #toStorageParams (KStorageBus,
 * LONG, LONG) const.
 */
QString VBoxGlobal::toFullString (KStorageBus aBus, LONG aChannel,
                                  LONG aDevice) const
{
    QString device;

    switch (aBus)
    {
        case KStorageBus_IDE:
        {
            device = QString ("%1 %2 %3")
                .arg (vboxGlobal().toString (aBus))
                .arg (vboxGlobal().toString (aBus, aChannel))
                .arg (vboxGlobal().toString (aBus, aChannel, aDevice));
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            /* we only have one SATA device so far which is always zero */
            device = QString ("%1 %2")
                .arg (vboxGlobal().toString (aBus))
                .arg (vboxGlobal().toString (aBus, aChannel));
            break;
        }
        default:
            AssertFailedBreak();
    }

    return device;
}

/**
 * Returns the list of all device types (VirtualBox::DeviceType COM enum).
 */
QStringList VBoxGlobal::deviceTypeStrings() const
{
    static QStringList list;
    if (list.empty())
        for (QULongStringHash::const_iterator it = mDeviceTypes.begin();
             it != mDeviceTypes.end(); ++ it)
            list += it.value();
    return list;
}

struct PortConfig
{
    const char *name;
    const ulong IRQ;
    const ulong IOBase;
};

static const PortConfig kComKnownPorts[] =
{
    { "COM1", 4, 0x3F8 },
    { "COM2", 3, 0x2F8 },
    { "COM3", 4, 0x3E8 },
    { "COM4", 3, 0x2E8 },
    /* must not contain an element with IRQ=0 and IOBase=0 used to cause
     * toCOMPortName() to return the "User-defined" string for these values. */
};

static const PortConfig kLptKnownPorts[] =
{
    { "LPT1", 7, 0x3BC },
    { "LPT2", 5, 0x378 },
    { "LPT3", 5, 0x278 },
    /* must not contain an element with IRQ=0 and IOBase=0 used to cause
     * toLPTPortName() to return the "User-defined" string for these values. */
};

/**
 *  Returns the list of the standard COM port names (i.e. "COMx").
 */
QStringList VBoxGlobal::COMPortNames() const
{
    QStringList list;
    for (size_t i = 0; i < RT_ELEMENTS (kComKnownPorts); ++ i)
        list << kComKnownPorts [i].name;

    return list;
}

/**
 *  Returns the list of the standard LPT port names (i.e. "LPTx").
 */
QStringList VBoxGlobal::LPTPortNames() const
{
    QStringList list;
    for (size_t i = 0; i < RT_ELEMENTS (kLptKnownPorts); ++ i)
        list << kLptKnownPorts [i].name;

    return list;
}

/**
 *  Returns the name of the standard COM port corresponding to the given
 *  parameters, or "User-defined" (which is also returned when both
 *  @a aIRQ and @a aIOBase are 0).
 */
QString VBoxGlobal::toCOMPortName (ulong aIRQ, ulong aIOBase) const
{
    for (size_t i = 0; i < RT_ELEMENTS (kComKnownPorts); ++ i)
        if (kComKnownPorts [i].IRQ == aIRQ &&
            kComKnownPorts [i].IOBase == aIOBase)
            return kComKnownPorts [i].name;

    return mUserDefinedPortName;
}

/**
 *  Returns the name of the standard LPT port corresponding to the given
 *  parameters, or "User-defined" (which is also returned when both
 *  @a aIRQ and @a aIOBase are 0).
 */
QString VBoxGlobal::toLPTPortName (ulong aIRQ, ulong aIOBase) const
{
    for (size_t i = 0; i < RT_ELEMENTS (kLptKnownPorts); ++ i)
        if (kLptKnownPorts [i].IRQ == aIRQ &&
            kLptKnownPorts [i].IOBase == aIOBase)
            return kLptKnownPorts [i].name;

    return mUserDefinedPortName;
}

/**
 *  Returns port parameters corresponding to the given standard COM name.
 *  Returns @c true on success, or @c false if the given port name is not one
 *  of the standard names (i.e. "COMx").
 */
bool VBoxGlobal::toCOMPortNumbers (const QString &aName, ulong &aIRQ,
                                   ulong &aIOBase) const
{
    for (size_t i = 0; i < RT_ELEMENTS (kComKnownPorts); ++ i)
        if (strcmp (kComKnownPorts [i].name, aName.toUtf8().data()) == 0)
        {
            aIRQ = kComKnownPorts [i].IRQ;
            aIOBase = kComKnownPorts [i].IOBase;
            return true;
        }

    return false;
}

/**
 *  Returns port parameters corresponding to the given standard LPT name.
 *  Returns @c true on success, or @c false if the given port name is not one
 *  of the standard names (i.e. "LPTx").
 */
bool VBoxGlobal::toLPTPortNumbers (const QString &aName, ulong &aIRQ,
                                   ulong &aIOBase) const
{
    for (size_t i = 0; i < RT_ELEMENTS (kLptKnownPorts); ++ i)
        if (strcmp (kLptKnownPorts [i].name, aName.toUtf8().data()) == 0)
        {
            aIRQ = kLptKnownPorts [i].IRQ;
            aIOBase = kLptKnownPorts [i].IOBase;
            return true;
        }

    return false;
}

/**
 * Searches for the given hard disk in the list of known media descriptors and
 * calls VBoxMedium::details() on the found desriptor.
 *
 * If the requeststed hard disk is not found (for example, it's a new hard disk
 * for a new VM created outside our UI), then media enumeration is requested and
 * the search is repeated. We assume that the secont attempt always succeeds and
 * assert otherwise.
 *
 * @note Technically, the second attempt may fail if, for example, the new hard
 *       passed to this method disk gets removed before #startEnumeratingMedia()
 *       succeeds. This (unexpected object uninitialization) is a generic
 *       problem though and needs to be addressed using exceptions (see also the
 *       @todo in VBoxMedium::details()).
 */
QString VBoxGlobal::details (const CHardDisk &aHD,
                             bool aPredictDiff)
{
    CMedium cmedium (aHD);
    VBoxMedium medium;

    if (!findMedium (cmedium, medium))
    {
        /* media may be new and not alredy in the media list, request refresh */
        startEnumeratingMedia();
        if (!findMedium (cmedium, medium))
        {
            /// @todo Still not found. Means that we are trying to get details
            /// of a hard disk that was deleted by a third party before we got a
            /// chance to complete the task. Returning null in this case should
            /// be OK, For more information, see *** in VBoxMedium::etails().
            return QString::null;
        }
    }

    return medium.detailsHTML (true /* aNoDiffs */, aPredictDiff);
}

/**
 *  Returns the details of the given USB device as a single-line string.
 */
QString VBoxGlobal::details (const CUSBDevice &aDevice) const
{
    QString details;
    QString m = aDevice.GetManufacturer().trimmed();
    QString p = aDevice.GetProduct().trimmed();
    if (m.isEmpty() && p.isEmpty())
    {
        details =
            tr ("Unknown device %1:%2", "USB device details")
            .arg (QString().sprintf ("%04hX", aDevice.GetVendorId()))
            .arg (QString().sprintf ("%04hX", aDevice.GetProductId()));
    }
    else
    {
        if (p.toUpper().startsWith (m.toUpper()))
            details = p;
        else
            details = m + " " + p;
    }
    ushort r = aDevice.GetRevision();
    if (r != 0)
        details += QString().sprintf (" [%04hX]", r);

    return details.trimmed();
}

/**
 *  Returns the multi-line description of the given USB device.
 */
QString VBoxGlobal::toolTip (const CUSBDevice &aDevice) const
{
    QString tip =
        tr ("<nobr>Vendor ID: %1</nobr><br>"
            "<nobr>Product ID: %2</nobr><br>"
            "<nobr>Revision: %3</nobr>", "USB device tooltip")
        .arg (QString().sprintf ("%04hX", aDevice.GetVendorId()))
        .arg (QString().sprintf ("%04hX", aDevice.GetProductId()))
        .arg (QString().sprintf ("%04hX", aDevice.GetRevision()));

    QString ser = aDevice.GetSerialNumber();
    if (!ser.isEmpty())
        tip += QString (tr ("<br><nobr>Serial No. %1</nobr>", "USB device tooltip"))
                        .arg (ser);

    /* add the state field if it's a host USB device */
    CHostUSBDevice hostDev (aDevice);
    if (!hostDev.isNull())
    {
        tip += QString (tr ("<br><nobr>State: %1</nobr>", "USB device tooltip"))
                        .arg (vboxGlobal().toString (hostDev.GetState()));
    }

    return tip;
}

/**
 *  Returns the multi-line description of the given USB filter
 */
QString VBoxGlobal::toolTip (const CUSBDeviceFilter &aFilter) const
{
    QString tip;

    QString vendorId = aFilter.GetVendorId();
    if (!vendorId.isEmpty())
        tip += tr ("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip")
                   .arg (vendorId);

    QString productId = aFilter.GetProductId();
    if (!productId.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Product ID: %2</nobr>", "USB filter tooltip")
                                                .arg (productId);

    QString revision = aFilter.GetRevision();
    if (!revision.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Revision: %3</nobr>", "USB filter tooltip")
                                                .arg (revision);

    QString product = aFilter.GetProduct();
    if (!product.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Product: %4</nobr>", "USB filter tooltip")
                                                .arg (product);

    QString manufacturer = aFilter.GetManufacturer();
    if (!manufacturer.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip")
                                                .arg (manufacturer);

    QString serial = aFilter.GetSerialNumber();
    if (!serial.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Serial No.: %1</nobr>", "USB filter tooltip")
                                                .arg (serial);

    QString port = aFilter.GetPort();
    if (!port.isEmpty())
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>Port: %1</nobr>", "USB filter tooltip")
                                                .arg (port);

    /* add the state field if it's a host USB device */
    CHostUSBDevice hostDev (aFilter);
    if (!hostDev.isNull())
    {
        tip += tip.isEmpty() ? "":"<br/>" + tr ("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                .arg (vboxGlobal().toString (hostDev.GetState()));
    }

    return tip;
}

/**
 * Returns a details report on a given VM represented as a HTML table.
 *
 * @param aMachine      Machine to create a report for.
 * @param aIsNewVM      @c true when called by the New VM Wizard.
 * @param aWithLinks    @c true if section titles should be hypertext links.
 */
QString VBoxGlobal::detailsReport (const CMachine &aMachine, bool aIsNewVM,
                                   bool aWithLinks)
{
    static const char *sTableTpl =
        "<table border=0 cellspacing=1 cellpadding=0>%1</table>";
    static const char *sSectionHrefTpl =
        "<tr><td width=22 rowspan=%1 align=left><img src='%2'></td>"
            "<td colspan=3><b><a href='%3'><nobr>%4</nobr></a></b></td></tr>"
            "%5"
        "<tr><td colspan=3><font size=1>&nbsp;</font></td></tr>";
    static const char *sSectionBoldTpl =
        "<tr><td width=22 rowspan=%1 align=left><img src='%2'></td>"
            "<td colspan=3><!-- %3 --><b><nobr>%4</nobr></b></td></tr>"
            "%5"
        "<tr><td colspan=3><font size=1>&nbsp;</font></td></tr>";
    static const char *sSectionItemTpl1 =
        "<tr><td width=40%><nobr><i>%1</i></nobr></td><td/><td/></tr>";
    static const char *sSectionItemTpl2 =
        "<tr><td width=40%><nobr>%1:</nobr></td><td/><td>%2</td></tr>";

    static QString sGeneralBasicHrefTpl, sGeneralBasicBoldTpl;
    static QString sGeneralFullHrefTpl, sGeneralFullBoldTpl;

    /* generate templates after every language change */

    if (!mDetailReportTemplatesReady)
    {
        mDetailReportTemplatesReady = true;

        QString generalItems
            = QString (sSectionItemTpl2).arg (tr ("Name", "details report"), "%1")
            + QString (sSectionItemTpl2).arg (tr ("OS Type", "details report"), "%2")
            + QString (sSectionItemTpl2).arg (tr ("Base Memory", "details report"),
                                              tr ("<nobr>%3 MB</nobr>", "details report"));
        sGeneralBasicHrefTpl = QString (sSectionHrefTpl)
                .arg (2 + 3) /* rows */
                .arg (":/machine_16px.png", /* icon */
                      "#general", /* link */
                      tr ("General", "details report"), /* title */
                      generalItems); /* items */
        sGeneralBasicBoldTpl = QString (sSectionBoldTpl)
                .arg (2 + 3) /* rows */
                .arg (":/machine_16px.png", /* icon */
                      "#general", /* link */
                      tr ("General", "details report"), /* title */
                      generalItems); /* items */

        generalItems
           += QString (sSectionItemTpl2).arg (tr ("Video Memory", "details report"),
                                              tr ("<nobr>%4 MB</nobr>", "details report"))
            + QString (sSectionItemTpl2).arg (tr ("Boot Order", "details report"), "%5")
            + QString (sSectionItemTpl2).arg (tr ("ACPI", "details report"), "%6")
            + QString (sSectionItemTpl2).arg (tr ("IO APIC", "details report"), "%7")
            + QString (sSectionItemTpl2).arg (tr ("VT-x/AMD-V", "details report"), "%8")
            + QString (sSectionItemTpl2).arg (tr ("Nested Paging", "details report"), "%9")
            + QString (sSectionItemTpl2).arg (tr ("PAE/NX", "details report"), "%10")
            + QString (sSectionItemTpl2).arg (tr ("3D Acceleration", "details report"), "%11");

        sGeneralFullHrefTpl = QString (sSectionHrefTpl)
            .arg (2 + 11) /* rows */
            .arg (":/machine_16px.png", /* icon */
                  "#general", /* link */
                  tr ("General", "details report"), /* title */
                  generalItems); /* items */
        sGeneralFullBoldTpl = QString (sSectionBoldTpl)
            .arg (2 + 11) /* rows */
            .arg (":/machine_16px.png", /* icon */
                  "#general", /* link */
                  tr ("General", "details report"), /* title */
                  generalItems); /* items */
    }

    /* common generated content */

    const QString &sectionTpl = aWithLinks
        ? sSectionHrefTpl
        : sSectionBoldTpl;

    QString hardDisks;
    {
        int rows = 2; /* including section header and footer */

        CHardDiskAttachmentVector vec = aMachine.GetHardDiskAttachments();
        for (size_t i = 0; i < (size_t) vec.size(); ++ i)
        {
            CHardDiskAttachment hda = vec [i];
            CHardDisk hd = hda.GetHardDisk();

            /// @todo for the explaination of the below isOk() checks, see ***
            /// in VBoxMedium::details().
            if (hda.isOk())
            {
                const QString controller = hda.GetController();
                KStorageBus bus;

                CStorageController ctrl = aMachine.GetStorageControllerByName(controller);
                bus = ctrl.GetBus();

                LONG port   = hda.GetPort();
                LONG device = hda.GetDevice();
                hardDisks += QString (sSectionItemTpl2)
                    .arg (toFullString (bus, port, device))
                    .arg (details (hd, aIsNewVM));
                ++ rows;
            }
        }

        if (hardDisks.isNull())
        {
            hardDisks = QString (sSectionItemTpl1)
                .arg (tr ("Not Attached", "details report (HDDs)"));
            ++ rows;
        }

        hardDisks = sectionTpl
            .arg (rows) /* rows */
            .arg (":/hd_16px.png", /* icon */
                  "#hdds", /* link */
                  tr ("Hard Disks", "details report"), /* title */
                  hardDisks); /* items */
    }

    /* compose details report */

    const QString &generalBasicTpl = aWithLinks
        ? sGeneralBasicHrefTpl
        : sGeneralBasicBoldTpl;

    const QString &generalFullTpl = aWithLinks
        ? sGeneralFullHrefTpl
        : sGeneralFullBoldTpl;

    QString detailsReport;

    if (aIsNewVM)
    {
        detailsReport
            = generalBasicTpl
                .arg (aMachine.GetName())
                .arg (vmGuestOSTypeDescription (aMachine.GetOSTypeId()))
                .arg (aMachine.GetMemorySize())
            + hardDisks;
    }
    else
    {
        /* boot order */
        QString bootOrder;
        for (ulong i = 1; i <= mVBox.GetSystemProperties().GetMaxBootPosition(); i++)
        {
            KDeviceType device = aMachine.GetBootOrder (i);
            if (device == KDeviceType_Null)
                continue;
            if (!bootOrder.isEmpty())
                bootOrder += ", ";
            bootOrder += toString (device);
        }
        if (bootOrder.isEmpty())
            bootOrder = toString (KDeviceType_Null);

        CBIOSSettings biosSettings = aMachine.GetBIOSSettings();

        /* ACPI */
        QString acpi = biosSettings.GetACPIEnabled()
            ? tr ("Enabled", "details report (ACPI)")
            : tr ("Disabled", "details report (ACPI)");

        /* IO APIC */
        QString ioapic = biosSettings.GetIOAPICEnabled()
            ? tr ("Enabled", "details report (IO APIC)")
            : tr ("Disabled", "details report (IO APIC)");

        /* VT-x/AMD-V */
        QString virt = aMachine.GetHWVirtExEnabled() == KTSBool_True ?
                       tr ("Enabled", "details report (VT-x/AMD-V)") :
                       tr ("Disabled", "details report (VT-x/AMD-V)");

        /* Nested Paging */
        QString nested = aMachine.GetHWVirtExNestedPagingEnabled()
            ? tr ("Enabled", "details report (Nested Paging)")
            : tr ("Disabled", "details report (Nested Paging)");

        /* PAE/NX */
        QString pae = aMachine.GetPAEEnabled()
            ? tr ("Enabled", "details report (PAE/NX)")
            : tr ("Disabled", "details report (PAE/NX)");

        /* 3D Acceleration */
        QString acc3d = aMachine.GetAccelerate3DEnabled()
            ? tr ("Enabled", "details report (3D Acceleration)")
            : tr ("Disabled", "details report (3D Acceleration)");

        /* General + Hard Disks */
        detailsReport
            = generalFullTpl
                .arg (aMachine.GetName())
                .arg (vmGuestOSTypeDescription (aMachine.GetOSTypeId()))
                .arg (aMachine.GetMemorySize())
                .arg (aMachine.GetVRAMSize())
                .arg (bootOrder)
                .arg (acpi)
                .arg (ioapic)
                .arg (virt)
                .arg (nested)
                .arg (pae)
                .arg (acc3d)
            + hardDisks;

        QString item;

        /* DVD */
        CDVDDrive dvd = aMachine.GetDVDDrive();
        switch (dvd.GetState())
        {
            case KDriveState_NotMounted:
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Not mounted", "details report (DVD)"));
                break;
            case KDriveState_ImageMounted:
            {
                CDVDImage img = dvd.GetImage();
                item = QString (sSectionItemTpl2)
                    .arg (tr ("Image", "details report (DVD)"),
                          locationForHTML (img.GetName()));
                break;
            }
            case KDriveState_HostDriveCaptured:
            {
                CHostDVDDrive drv = dvd.GetHostDrive();
                QString drvName = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    drvName :
                    QString ("%1 (%2)").arg (description, drvName);
                item = QString (sSectionItemTpl2)
                    .arg (tr ("Host Drive", "details report (DVD)"),
                          fullName);
                break;
            }
            default:
                AssertMsgFailed (("Invalid DVD state: %d", dvd.GetState()));
        }
        detailsReport += sectionTpl
            .arg (2 + 1) /* rows */
            .arg (":/cd_16px.png", /* icon */
                  "#dvd", /* link */
                  tr ("CD/DVD-ROM", "details report"), /* title */
                  item); // items

        /* Floppy */
        CFloppyDrive floppy = aMachine.GetFloppyDrive();
        switch (floppy.GetState())
        {
            case KDriveState_NotMounted:
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Not mounted", "details report (floppy)"));
                break;
            case KDriveState_ImageMounted:
            {
                CFloppyImage img = floppy.GetImage();
                item = QString (sSectionItemTpl2)
                    .arg (tr ("Image", "details report (floppy)"),
                          locationForHTML (img.GetName()));
                break;
            }
            case KDriveState_HostDriveCaptured:
            {
                CHostFloppyDrive drv = floppy.GetHostDrive();
                QString drvName = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    drvName :
                    QString ("%1 (%2)").arg (description, drvName);
                item = QString (sSectionItemTpl2)
                    .arg (tr ("Host Drive", "details report (floppy)"),
                          fullName);
                break;
            }
            default:
                AssertMsgFailed (("Invalid floppy state: %d", floppy.GetState()));
        }
        detailsReport += sectionTpl
            .arg (2 + 1) /* rows */
            .arg (":/fd_16px.png", /* icon */
                  "#floppy", /* link */
                  tr ("Floppy", "details report"), /* title */
                  item); /* items */

        /* audio */
        {
            CAudioAdapter audio = aMachine.GetAudioAdapter();
            int rows = audio.GetEnabled() ? 3 : 2;
            if (audio.GetEnabled())
                item = QString (sSectionItemTpl2)
                       .arg (tr ("Host Driver", "details report (audio)"),
                             toString (audio.GetAudioDriver())) +
                       QString (sSectionItemTpl2)
                       .arg (tr ("Controller", "details report (audio)"),
                             toString (audio.GetAudioController()));
            else
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Disabled", "details report (audio)"));

            detailsReport += sectionTpl
                .arg (rows + 1) /* rows */
                .arg (":/sound_16px.png", /* icon */
                      "#audio", /* link */
                      tr ("Audio", "details report"), /* title */
                      item); /* items */
        }
        /* network */
        {
            item = QString::null;
            ulong count = mVBox.GetSystemProperties().GetNetworkAdapterCount();
            int rows = 2; /* including section header and footer */
            for (ulong slot = 0; slot < count; slot ++)
            {
                CNetworkAdapter adapter = aMachine.GetNetworkAdapter (slot);
                if (adapter.GetEnabled())
                {
                    KNetworkAttachmentType type = adapter.GetAttachmentType();
                    QString attType = toString (adapter.GetAdapterType())
                                      .replace (QRegExp ("\\s\\(.+\\)"), " (%1)");
                    /* don't use the adapter type string for types that have
                     * an additional symbolic network/interface name field, use
                     * this name instead */
                    if (type == KNetworkAttachmentType_Bridged)
                        attType = attType.arg (tr ("host interface, %1",
                            "details report (network)").arg (adapter.GetHostInterface()));
                    else if (type == KNetworkAttachmentType_Internal)
                        attType = attType.arg (tr ("internal network, '%1'",
                            "details report (network)").arg (adapter.GetInternalNetwork()));
                    else
                        attType = attType.arg (vboxGlobal().toString (type));

                    item += QString (sSectionItemTpl2)
                        .arg (tr ("Adapter %1", "details report (network)")
                              .arg (adapter.GetSlot() + 1))
                        .arg (attType);
                    ++ rows;
                }
            }
            if (item.isNull())
            {
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Disabled", "details report (network)"));
                ++ rows;
            }

            detailsReport += sectionTpl
                .arg (rows) /* rows */
                .arg (":/nw_16px.png", /* icon */
                      "#network", /* link */
                      tr ("Network", "details report"), /* title */
                      item); /* items */
        }
        /* serial ports */
        {
            item = QString::null;
            ulong count = mVBox.GetSystemProperties().GetSerialPortCount();
            int rows = 2; /* including section header and footer */
            for (ulong slot = 0; slot < count; slot ++)
            {
                CSerialPort port = aMachine.GetSerialPort (slot);
                if (port.GetEnabled())
                {
                    KPortMode mode = port.GetHostMode();
                    QString data =
                        toCOMPortName (port.GetIRQ(), port.GetIOBase()) + ", ";
                    if (mode == KPortMode_HostPipe ||
                        mode == KPortMode_HostDevice)
                        data += QString ("%1 (<nobr>%2</nobr>)")
                            .arg (vboxGlobal().toString (mode))
                            .arg (QDir::toNativeSeparators (port.GetPath()));
                    else
                        data += toString (mode);

                    item += QString (sSectionItemTpl2)
                        .arg (tr ("Port %1", "details report (serial ports)")
                              .arg (port.GetSlot() + 1))
                        .arg (data);
                    ++ rows;
                }
            }
            if (item.isNull())
            {
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Disabled", "details report (serial ports)"));
                ++ rows;
            }

            detailsReport += sectionTpl
                .arg (rows) /* rows */
                .arg (":/serial_port_16px.png", /* icon */
                      "#serialPorts", /* link */
                      tr ("Serial Ports", "details report"), /* title */
                      item); /* items */
        }
        /* parallel ports */
        {
            item = QString::null;
            ulong count = mVBox.GetSystemProperties().GetParallelPortCount();
            int rows = 2; /* including section header and footer */
            for (ulong slot = 0; slot < count; slot ++)
            {
                CParallelPort port = aMachine.GetParallelPort (slot);
                if (port.GetEnabled())
                {
                    QString data =
                        toLPTPortName (port.GetIRQ(), port.GetIOBase()) +
                        QString (" (<nobr>%1</nobr>)")
                        .arg (QDir::toNativeSeparators (port.GetPath()));

                    item += QString (sSectionItemTpl2)
                        .arg (tr ("Port %1", "details report (parallel ports)")
                              .arg (port.GetSlot() + 1))
                        .arg (data);
                    ++ rows;
                }
            }
            if (item.isNull())
            {
                item = QString (sSectionItemTpl1)
                    .arg (tr ("Disabled", "details report (parallel ports)"));
                ++ rows;
            }

            /* Temporary disabled */
            QString dummy = sectionTpl /* detailsReport += sectionTpl */
                .arg (rows) /* rows */
                .arg (":/parallel_port_16px.png", /* icon */
                      "#parallelPorts", /* link */
                      tr ("Parallel Ports", "details report"), /* title */
                      item); /* items */
        }
        /* USB */
        {
            CUSBController ctl = aMachine.GetUSBController();
            if (!ctl.isNull())
            {
                /* the USB controller may be unavailable (i.e. in VirtualBox OSE) */

                if (ctl.GetEnabled())
                {
                    CUSBDeviceFilterVector coll = ctl.GetDeviceFilters();
                    uint active = 0;
                    for (int i = 0; i < coll.size(); ++i)
                        if (coll[i].GetActive())
                            active ++;

                    item = QString (sSectionItemTpl2)
                        .arg (tr ("Device Filters", "details report (USB)"),
                              tr ("%1 (%2 active)", "details report (USB)")
                                  .arg (coll.size()).arg (active));
                }
                else
                    item = QString (sSectionItemTpl1)
                        .arg (tr ("Disabled", "details report (USB)"));

                detailsReport += sectionTpl
                    .arg (2 + 1) /* rows */
                    .arg (":/usb_16px.png", /* icon */
                          "#usb", /* link */
                          tr ("USB", "details report"), /* title */
                          item); /* items */
            }
        }
        /* Shared folders */
        {
            ulong count = aMachine.GetSharedFolders().size();
            if (count > 0)
            {
                item = QString (sSectionItemTpl2)
                    .arg (tr ("Shared Folders", "details report (shared folders)"))
                    .arg (count);
            }
            else
                item = QString (sSectionItemTpl1)
                    .arg (tr ("None", "details report (shared folders)"));

            detailsReport += sectionTpl
                .arg (2 + 1) /* rows */
                .arg (":/shared_folder_16px.png", /* icon */
                      "#sfolders", /* link */
                      tr ("Shared Folders", "details report"), /* title */
                      item); /* items */
        }
        /* VRDP */
        {
            CVRDPServer srv = aMachine.GetVRDPServer();
            if (!srv.isNull())
            {
                /* the VRDP server may be unavailable (i.e. in VirtualBox OSE) */

                if (srv.GetEnabled())
                    item = QString (sSectionItemTpl2)
                        .arg (tr ("VRDP Server Port", "details report (VRDP)"))
                        .arg (srv.GetPort());
                else
                    item = QString (sSectionItemTpl1)
                        .arg (tr ("Disabled", "details report (VRDP)"));

                detailsReport += sectionTpl
                    .arg (2 + 1) /* rows */
                    .arg (":/vrdp_16px.png", /* icon */
                          "#vrdp", /* link */
                          tr ("Remote Display", "details report"), /* title */
                          item); /* items */
            }
        }
    }

    return QString (sTableTpl). arg (detailsReport);
}

QString VBoxGlobal::platformInfo()
{
    QString platform;

#if defined (Q_OS_WIN)
    platform = "win";
#elif defined (Q_OS_LINUX)
    platform = "linux";
#elif defined (Q_OS_MACX)
    platform = "macosx";
#elif defined (Q_OS_OS2)
    platform = "os2";
#elif defined (Q_OS_FREEBSD)
    platform = "freebsd";
#elif defined (Q_OS_SOLARIS)
    platform = "solaris";
#else
    platform = "unknown";
#endif

    /* The format is <system>.<bitness> */
    platform += QString (".%1").arg (ARCH_BITS);

    /* Add more system information */
#if defined (Q_OS_WIN)
    OSVERSIONINFO versionInfo;
    ZeroMemory (&versionInfo, sizeof (OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    GetVersionEx (&versionInfo);
    int major = versionInfo.dwMajorVersion;
    int minor = versionInfo.dwMinorVersion;
    int build = versionInfo.dwBuildNumber;
    QString sp = QString::fromUtf16 ((ushort*)versionInfo.szCSDVersion);

    QString distrib;
    if (major == 6)
        distrib = QString ("Windows Vista %1");
    else if (major == 5)
    {
        if (minor == 2)
            distrib = QString ("Windows Server 2003 %1");
        else if (minor == 1)
            distrib = QString ("Windows XP %1");
        else if (minor == 0)
            distrib = QString ("Windows 2000 %1");
        else
            distrib = QString ("Unknown %1");
    }
    else if (major == 4)
    {
        if (minor == 90)
            distrib = QString ("Windows Me %1");
        else if (minor == 10)
            distrib = QString ("Windows 98 %1");
        else if (minor == 0)
            distrib = QString ("Windows 95 %1");
        else
            distrib = QString ("Unknown %1");
    }
    else /** @todo Windows Server 2008 == vista? Probably not... */
        distrib = QString ("Unknown %1");
    distrib = distrib.arg (sp);
    QString version = QString ("%1.%2").arg (major).arg (minor);
    QString kernel = QString ("%1").arg (build);
    platform += QString (" [Distribution: %1 | Version: %2 | Build: %3]")
        .arg (distrib).arg (version).arg (kernel);
#elif defined (Q_OS_LINUX)
    /* Get script path */
    char szAppPrivPath [RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch (szAppPrivPath, sizeof (szAppPrivPath)); NOREF(rc);
    AssertRC (rc);
    /* Run script */
    QByteArray result =
        Process::singleShot (QString (szAppPrivPath) + "/VBoxSysInfo.sh");
    if (!result.isNull())
        platform += QString (" [%1]").arg (QString (result).trimmed());
#else
    /* Use RTSystemQueryOSInfo. */
    char szTmp[256];
    QStringList components;
    int vrc = RTSystemQueryOSInfo (RTSYSOSINFO_PRODUCT, szTmp, sizeof (szTmp));
    if ((RT_SUCCESS (vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
        components << QString ("Product: %1").arg (szTmp);
    vrc = RTSystemQueryOSInfo (RTSYSOSINFO_RELEASE, szTmp, sizeof (szTmp));
    if ((RT_SUCCESS (vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
        components << QString ("Release: %1").arg (szTmp);
    vrc = RTSystemQueryOSInfo (RTSYSOSINFO_VERSION, szTmp, sizeof (szTmp));
    if ((RT_SUCCESS (vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
        components << QString ("Version: %1").arg (szTmp);
    vrc = RTSystemQueryOSInfo (RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof (szTmp));
    if ((RT_SUCCESS (vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
        components << QString ("SP: %1").arg (szTmp);
    if (!components.isEmpty())
        platform += QString (" [%1]").arg (components.join (" | "));
#endif

    return platform;
}

#if defined(Q_WS_X11) && !defined(VBOX_OSE)
double VBoxGlobal::findLicenseFile (const QStringList &aFilesList, QRegExp aPattern, QString &aLicenseFile) const
{
    double maxVersionNumber = 0;
    aLicenseFile = "";
    for (int index = 0; index < aFilesList.count(); ++ index)
    {
        aPattern.indexIn (aFilesList [index]);
        QString version = aPattern.cap (1);
        if (maxVersionNumber < version.toDouble())
        {
            maxVersionNumber = version.toDouble();
            aLicenseFile = aFilesList [index];
        }
    }
    return maxVersionNumber;
}

bool VBoxGlobal::showVirtualBoxLicense()
{
    /* get the apps doc path */
    int size = 256;
    char *buffer = (char*) RTMemTmpAlloc (size);
    RTPathAppDocs (buffer, size);
    QString path (buffer);
    RTMemTmpFree (buffer);
    QDir docDir (path);
    docDir.setFilter (QDir::Files);
    docDir.setNameFilters (QStringList ("License-*.html"));

    /* Make sure that the language is in two letter code.
     * Note: if languageId() returns an empty string lang.name() will
     * return "C" which is an valid language code. */
    QLocale lang (VBoxGlobal::languageId());

    QStringList filesList = docDir.entryList();
    QString licenseFile;
    /* First try to find a localized version of the license file. */
    double versionNumber = findLicenseFile (filesList, QRegExp (QString ("License-([\\d\\.]+)-%1.html").arg (lang.name())), licenseFile);
    /* If there wasn't a localized version of the currently selected language,
     * search for the generic one. */
    if (versionNumber == 0)
        versionNumber = findLicenseFile (filesList, QRegExp ("License-([\\d\\.]+).html"), licenseFile);
    /* Check the version again. */
    if (!versionNumber)
    {
        vboxProblem().cannotFindLicenseFiles (path);
        return false;
    }

    /* compose the latest license file full path */
    QString latestVersion = QString::number (versionNumber);
    QString latestFilePath = docDir.absoluteFilePath (licenseFile);

    /* check for the agreed license version */
    QString licenseAgreed = virtualBox().GetExtraData (VBoxDefs::GUI_LicenseKey);
    if (licenseAgreed == latestVersion)
        return true;

    VBoxLicenseViewer licenseDialog (latestFilePath);
    bool result = licenseDialog.exec() == QDialog::Accepted;
    if (result)
        virtualBox().SetExtraData (VBoxDefs::GUI_LicenseKey, latestVersion);
    return result;
}
#endif /* defined(Q_WS_X11) && !defined(VBOX_OSE) */

/**
 * Checks if any of the settings files were auto-converted and informs the user
 * if so. Returns @c false if the user select to exit the application.
 *
 * @param aAfterRefresh @c true to suppress the first simple dialog aExit
 *                      button. Used when calling after the VM refresh.
 */
bool VBoxGlobal::checkForAutoConvertedSettings (bool aAfterRefresh /*= false*/)
{
    QString formatVersion = mVBox.GetSettingsFormatVersion();

    bool isGlobalConverted = false;
    QList <CMachine> machines;
    QString fileList;
    QString version;

    CMachineVector vec = mVBox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
    {
        if (!m->GetAccessible())
            continue;

        version = m->GetSettingsFileVersion();
        if (version != formatVersion)
        {
            machines.append (*m);
            fileList += QString ("<tr><td><nobr>%1</nobr></td><td>&nbsp;&nbsp;</td>"
                                 "</td><td><nobr><i>%2</i></nobr></td></tr>")
                .arg (m->GetSettingsFilePath())
                .arg (version);
        }
    }

    if (!aAfterRefresh)
    {
        version = mVBox.GetSettingsFileVersion();
        if (version != formatVersion)
        {
            isGlobalConverted = true;
            fileList += QString ("<tr><td><nobr>%1</nobr></td><td>&nbsp;&nbsp;</td>"
                                 "</td><td><nobr><i>%2</i></nobr></td></tr>")
                .arg (mVBox.GetSettingsFilePath())
                .arg (version);
        }
    }

    if (!fileList.isNull())
    {
        fileList = QString ("<table cellspacing=0 cellpadding=0>%1</table>")
                            .arg (fileList);

        int rc = vboxProblem()
            .warnAboutAutoConvertedSettings (formatVersion, fileList,
                                             aAfterRefresh);

        if (rc == QIMessageBox::Cancel)
            return false;

        Assert (rc == QIMessageBox::No || rc == QIMessageBox::Yes);

        /* backup (optionally) and save all settings files
         * (QIMessageBox::No = Backup, QIMessageBox::Yes = Save) */

        foreach (CMachine m, machines)
        {
            CSession session = openSession (m.GetId());
            if (!session.isNull())
            {
                CMachine sm = session.GetMachine();
                if (rc == QIMessageBox::No)
                    sm.SaveSettingsWithBackup();
                else
                    sm.SaveSettings();

                if (!sm.isOk())
                    vboxProblem().cannotSaveMachineSettings (sm);
                session.Close();
            }
        }

        if (isGlobalConverted)
        {
            if (rc == QIMessageBox::No)
                mVBox.SaveSettingsWithBackup();
            else
                mVBox.SaveSettings();

            if (!mVBox.isOk())
                vboxProblem().cannotSaveGlobalSettings (mVBox);
        }
    }

    return true;
}

/**
 *  Opens a direct session for a machine with the given ID.
 *  This method does user-friendly error handling (display error messages, etc.).
 *  and returns a null CSession object in case of any error.
 *  If this method succeeds, don't forget to close the returned session when
 *  it is no more necessary.
 *
 *  @param aId          Machine ID.
 *  @param aExisting    @c true to open an existing session with the machine
 *                      which is already running, @c false to open a new direct
 *                      session.
 */
CSession VBoxGlobal::openSession (const QUuid &aId, bool aExisting /* = false */)
{
    CSession session;
    session.createInstance (CLSID_Session);
    if (session.isNull())
    {
        vboxProblem().cannotOpenSession (session);
        return session;
    }

    if (aExisting)
        mVBox.OpenExistingSession (session, aId);
    else
    {
        mVBox.OpenSession (session, aId);
        CMachine machine = session.GetMachine ();
        /* Make sure that the language is in two letter code.
         * Note: if languageId() returns an empty string lang.name() will
         * return "C" which is an valid language code. */
        QLocale lang (VBoxGlobal::languageId());
        machine.SetGuestPropertyValue ("/VirtualBox/HostInfo/GUI/LanguageID", lang.name());
    }

    if (!mVBox.isOk())
    {
        CMachine machine = CVirtualBox (mVBox).GetMachine (aId);
        vboxProblem().cannotOpenSession (mVBox, machine);
        session.detach();
    }

    return session;
}

/**
 *  Starts a machine with the given ID.
 */
bool VBoxGlobal::startMachine (const QUuid &id)
{
    AssertReturn (mValid, false);

    CSession session = vboxGlobal().openSession (id);
    if (session.isNull())
        return false;

    return consoleWnd().openView (session);
}

/**
 * Appends the given list of hard disks and all their children to the media
 * list. To be called only from VBoxGlobal::startEnumeratingMedia().
 */
static
void AddHardDisksToList (const CHardDiskVector &aVector,
                         VBoxMediaList &aList,
                         VBoxMediaList::iterator aWhere,
                         VBoxMedium *aParent = 0)
{
    VBoxMediaList::iterator first = aWhere;

    /* First pass: Add siblings sorted */
    for (CHardDiskVector::ConstIterator it = aVector.begin();
         it != aVector.end(); ++ it)
    {
        CMedium cmedium (*it);
        VBoxMedium medium (cmedium, VBoxDefs::MediaType_HardDisk, aParent);

        /* Search for a proper alphabetic position */
        VBoxMediaList::iterator jt = first;
        for (; jt != aWhere; ++ jt)
            if ((*jt).name().localeAwareCompare (medium.name()) > 0)
                break;

        aList.insert (jt, medium);

        /* Adjust the first item if inserted before it */
        if (jt == first)
            -- first;
    }

    /* Second pass: Add children */
    for (VBoxMediaList::iterator it = first; it != aWhere;)
    {
        CHardDiskVector children = (*it).hardDisk().GetChildren();
        VBoxMedium *parent = &(*it);

        ++ it; /* go to the next sibling before inserting children */
        AddHardDisksToList (children, aList, it, parent);
    }
}

/**
 * Starts a thread that asynchronously enumerates all currently registered
 * media.
 *
 * Before the enumeration is started, the current media list (a list returned by
 * #currentMediaList()) is populated with all registered media and the
 * #mediumEnumStarted() signal is emitted. The enumeration thread then walks this
 * list, checks for media acessiblity and emits #mediumEnumerated() signals of
 * each checked medium. When all media are checked, the enumeration thread is
 * stopped and the #mediumEnumFinished() signal is emitted.
 *
 * If the enumeration is already in progress, no new thread is started.
 *
 * The media list returned by #currentMediaList() is always sorted
 * alphabetically by the location attribute and comes in the following order:
 * <ol>
 *  <li>All hard disks. If a hard disk has children, these children
 *      (alphabetically sorted) immediately follow their parent and terefore
 *      appear before its next sibling hard disk.</li>
 *  <li>All CD/DVD images.</li>
 *  <li>All Floppy images.</li>
 * </ol>
 *
 * Note that #mediumEnumerated() signals are emitted in the same order as
 * described above.
 *
 * @sa #currentMediaList()
 * @sa #isMediaEnumerationStarted()
 */
void VBoxGlobal::startEnumeratingMedia()
{
    AssertReturnVoid (mValid);

    /* check if already started but not yet finished */
    if (mMediaEnumThread != NULL)
        return;

    /* ignore the request during application termination */
    if (sVBoxGlobalInCleanup)
        return;

    /* composes a list of all currently known media & their children */
    mMediaList.clear();
    {
        AddHardDisksToList (mVBox.GetHardDisks(), mMediaList, mMediaList.end());
    }
    {
        VBoxMediaList::iterator first = mMediaList.end();

        CDVDImageVector vec = mVBox.GetDVDImages();
        for (CDVDImageVector::ConstIterator it = vec.begin();
             it != vec.end(); ++ it)
        {
            CMedium cmedium (*it);
            VBoxMedium medium (cmedium, VBoxDefs::MediaType_DVD);

            /* Search for a proper alphabetic position */
            VBoxMediaList::iterator jt = first;
            for (; jt != mMediaList.end(); ++ jt)
                if ((*jt).name().localeAwareCompare (medium.name()) > 0)
                    break;

            mMediaList.insert (jt, medium);

            /* Adjust the first item if inserted before it */
            if (jt == first)
                -- first;
        }
    }
    {
        VBoxMediaList::iterator first = mMediaList.end();

        CFloppyImageVector vec = mVBox.GetFloppyImages();
        for (CFloppyImageVector::ConstIterator it = vec.begin();
             it != vec.end(); ++ it)
        {
            CMedium cmedium (*it);
            VBoxMedium medium (cmedium, VBoxDefs::MediaType_Floppy);

            /* Search for a proper alphabetic position */
            VBoxMediaList::iterator jt = first;
            for (; jt != mMediaList.end(); ++ jt)
                if ((*jt).name().localeAwareCompare (medium.name()) > 0)
                    break;

            mMediaList.insert (jt, medium);

            /* Adjust the first item if inserted before it */
            if (jt == first)
                -- first;
        }
    }

    /* enumeration thread class */
    class MediaEnumThread : public QThread
    {
    public:

        MediaEnumThread (VBoxMediaList &aList)
            : mVector (aList.size())
            , mSavedIt (aList.begin())
        {
            int i = 0;
            for (VBoxMediaList::const_iterator it = aList.begin();
                 it != aList.end(); ++ it)
                mVector [i ++] = *it;
        }

        virtual void run()
        {
            LogFlow (("MediaEnumThread started.\n"));
            COMBase::InitializeCOM();

            CVirtualBox mVBox = vboxGlobal().virtualBox();
            QObject *self = &vboxGlobal();

            /* Enumerate the list */
            for (int i = 0; i < mVector.size() && !sVBoxGlobalInCleanup; ++ i)
            {
                mVector [i].blockAndQueryState();
                QApplication::
                    postEvent (self,
                               new VBoxMediaEnumEvent (mVector [i], mSavedIt));
            }

            /* Post the end-of-enumeration event */
            if (!sVBoxGlobalInCleanup)
                QApplication::postEvent (self, new VBoxMediaEnumEvent (mSavedIt));

            COMBase::CleanupCOM();
            LogFlow (("MediaEnumThread finished.\n"));
        }

    private:

        QVector <VBoxMedium> mVector;
        VBoxMediaList::iterator mSavedIt;
    };

    mMediaEnumThread = new MediaEnumThread (mMediaList);
    AssertReturnVoid (mMediaEnumThread);

    /* emit mediumEnumStarted() after we set mMediaEnumThread to != NULL
     * to cause isMediaEnumerationStarted() to return TRUE from slots */
    emit mediumEnumStarted();

    mMediaEnumThread->start();
}

/**
 * Adds a new medium to the current media list and emits the #mediumAdded()
 * signal.
 *
 * @sa #currentMediaList()
 */
void VBoxGlobal::addMedium (const VBoxMedium &aMedium)
{
    /* Note that we maitain the same order here as #startEnumeratingMedia() */

    VBoxMediaList::iterator it = mMediaList.begin();

    if (aMedium.type() == VBoxDefs::MediaType_HardDisk)
    {
        VBoxMediaList::iterator parent = mMediaList.end();

        for (; it != mMediaList.end(); ++ it)
        {
            if ((*it).type() != VBoxDefs::MediaType_HardDisk)
                break;

            if (aMedium.parent() != NULL && parent == mMediaList.end())
            {
                if (&*it == aMedium.parent())
                    parent = it;
            }
            else
            {
                /* break if met a parent's sibling (will insert before it) */
                if (aMedium.parent() != NULL &&
                    (*it).parent() == (*parent).parent())
                    break;

                /* compare to aMedium's siblings */
                if ((*it).parent() == aMedium.parent() &&
                    (*it).name().localeAwareCompare (aMedium.name()) > 0)
                    break;
            }
        }

        AssertReturnVoid (aMedium.parent() == NULL || parent != mMediaList.end());
    }
    else
    {
        for (; it != mMediaList.end(); ++ it)
        {
            /* skip HardDisks that come first */
            if ((*it).type() == VBoxDefs::MediaType_HardDisk)
                continue;

            /* skip DVD when inserting Floppy */
            if (aMedium.type() == VBoxDefs::MediaType_Floppy &&
                (*it).type() == VBoxDefs::MediaType_DVD)
                continue;

            if ((*it).name().localeAwareCompare (aMedium.name()) > 0 ||
                (aMedium.type() == VBoxDefs::MediaType_DVD &&
                 (*it).type() == VBoxDefs::MediaType_Floppy))
                break;
        }
    }

    it = mMediaList.insert (it, aMedium);

    emit mediumAdded (*it);
}

/**
 * Updates the medium in the current media list and emits the #mediumUpdated()
 * signal.
 *
 * @sa #currentMediaList()
 */
void VBoxGlobal::updateMedium (const VBoxMedium &aMedium)
{
    VBoxMediaList::Iterator it;
    for (it = mMediaList.begin(); it != mMediaList.end(); ++ it)
        if ((*it).id() == aMedium.id())
            break;

    AssertReturnVoid (it != mMediaList.end());

    if (&*it != &aMedium)
        *it = aMedium;

    emit mediumUpdated (*it);
}

/**
 * Removes the medium from the current media list and emits the #mediumRemoved()
 * signal.
 *
 * @sa #currentMediaList()
 */
void VBoxGlobal::removeMedium (VBoxDefs::MediaType aType, const QUuid &aId)
{
    VBoxMediaList::Iterator it;
    for (it = mMediaList.begin(); it != mMediaList.end(); ++ it)
        if ((*it).id() == aId)
            break;

    AssertReturnVoid (it != mMediaList.end());

#if DEBUG
    /* sanity: must be no children */
    {
        VBoxMediaList::Iterator jt = it;
        ++ jt;
        AssertReturnVoid (jt == mMediaList.end() || (*jt).parent() != &*it);
    }
#endif

    VBoxMedium *parent = (*it).parent();

    /* remove the medium from the list to keep it in sync with the server "for
     * free" when the medium is deleted from one of our UIs */
    mMediaList.erase (it);

    emit mediumRemoved (aType, aId);

    /* also emit the parent update signal because some attributes like
     * isReadOnly() may have been changed after child removal */
    if (parent != NULL)
    {
        parent->refresh();
        emit mediumUpdated (*parent);
    }
}

/**
 *  Searches for a VBoxMedum object representing the given COM medium object.
 *
 *  @return true if found and false otherwise.
 */
bool VBoxGlobal::findMedium (const CMedium &aObj, VBoxMedium &aMedium) const
{
    for (VBoxMediaList::ConstIterator it = mMediaList.begin();
         it != mMediaList.end(); ++ it)
    {
        if ((*it).medium() == aObj)
        {
            aMedium = (*it);
            return true;
        }
    }

    return false;
}

#ifdef VBOX_GUI_WITH_SYSTRAY
/**
 *  Returns the number of current running Fe/Qt4 main windows.
 *
 *  @return Number of running main windows.
 */
int VBoxGlobal::mainWindowCount ()
{
    return mVBox.GetExtraData (VBoxDefs::GUI_MainWindowCount).toInt();
}
#endif

/**
 *  Native language name of the currently installed translation.
 *  Returns "English" if no translation is installed
 *  or if the translation file is invalid.
 */
QString VBoxGlobal::languageName() const
{

    return qApp->translate ("@@@", "English",
                            "Native language name");
}

/**
 *  Native language country name of the currently installed translation.
 *  Returns "--" if no translation is installed or if the translation file is
 *  invalid, or if the language is independent on the country.
 */
QString VBoxGlobal::languageCountry() const
{
    return qApp->translate ("@@@", "--",
                            "Native language country name "
                            "(empty if this language is for all countries)");
}

/**
 *  Language name of the currently installed translation, in English.
 *  Returns "English" if no translation is installed
 *  or if the translation file is invalid.
 */
QString VBoxGlobal::languageNameEnglish() const
{

    return qApp->translate ("@@@", "English",
                            "Language name, in English");
}

/**
 *  Language country name of the currently installed translation, in English.
 *  Returns "--" if no translation is installed or if the translation file is
 *  invalid, or if the language is independent on the country.
 */
QString VBoxGlobal::languageCountryEnglish() const
{
    return qApp->translate ("@@@", "--",
                            "Language country name, in English "
                            "(empty if native country name is empty)");
}

/**
 *  Comma-separated list of authors of the currently installed translation.
 *  Returns "Sun Microsystems, Inc." if no translation is installed or if the
 *  translation file is invalid, or if the translation is supplied by Sun
 *  Microsystems, inc.
 */
QString VBoxGlobal::languageTranslators() const
{
    return qApp->translate ("@@@", "Sun Microsystems, Inc.",
                            "Comma-separated list of translators");
}

/**
 *  Changes the language of all global string constants according to the
 *  currently installed translations tables.
 */
void VBoxGlobal::retranslateUi()
{
    mMachineStates [KMachineState_PoweredOff] = tr ("Powered Off", "MachineState");
    mMachineStates [KMachineState_Saved] =      tr ("Saved", "MachineState");
    mMachineStates [KMachineState_Aborted] =    tr ("Aborted", "MachineState");
    mMachineStates [KMachineState_Running] =    tr ("Running", "MachineState");
    mMachineStates [KMachineState_Paused] =     tr ("Paused", "MachineState");
    mMachineStates [KMachineState_Stuck] =      tr ("Stuck", "MachineState");
    mMachineStates [KMachineState_Starting] =   tr ("Starting", "MachineState");
    mMachineStates [KMachineState_Stopping] =   tr ("Stopping", "MachineState");
    mMachineStates [KMachineState_Saving] =     tr ("Saving", "MachineState");
    mMachineStates [KMachineState_Restoring] =  tr ("Restoring", "MachineState");
    mMachineStates [KMachineState_Discarding] = tr ("Discarding", "MachineState");
    mMachineStates [KMachineState_SettingUp] =  tr ("Setting Up", "MachineState");

    mSessionStates [KSessionState_Closed] =     tr ("Closed", "SessionState");
    mSessionStates [KSessionState_Open] =       tr ("Open", "SessionState");
    mSessionStates [KSessionState_Spawning] =   tr ("Spawning", "SessionState");
    mSessionStates [KSessionState_Closing] =    tr ("Closing", "SessionState");

    mDeviceTypes [KDeviceType_Null] =           tr ("None", "DeviceType");
    mDeviceTypes [KDeviceType_Floppy] =         tr ("Floppy", "DeviceType");
    mDeviceTypes [KDeviceType_DVD] =            tr ("CD/DVD-ROM", "DeviceType");
    mDeviceTypes [KDeviceType_HardDisk] =       tr ("Hard Disk", "DeviceType");
    mDeviceTypes [KDeviceType_Network] =        tr ("Network", "DeviceType");
    mDeviceTypes [KDeviceType_USB] =            tr ("USB", "DeviceType");
    mDeviceTypes [KDeviceType_SharedFolder] =   tr ("Shared Folder", "DeviceType");

    mStorageBuses [KStorageBus_IDE] =   tr ("IDE", "StorageBus");
    mStorageBuses [KStorageBus_SATA] =  tr ("SATA", "StorageBus");
    mStorageBuses [KStorageBus_SCSI] =  tr ("SCSI", "StorageBus");

    mStorageBusChannels [0] =   tr ("Primary", "StorageBusChannel");
    mStorageBusChannels [1] =   tr ("Secondary", "StorageBusChannel");
    mStorageBusChannels [2] =   tr ("Port %1", "StorageBusChannel");

    mStorageBusDevices [0] =    tr ("Master", "StorageBusDevice");
    mStorageBusDevices [1] =    tr ("Slave", "StorageBusDevice");

    mDiskTypes [KHardDiskType_Normal] =         tr ("Normal", "DiskType");
    mDiskTypes [KHardDiskType_Immutable] =      tr ("Immutable", "DiskType");
    mDiskTypes [KHardDiskType_Writethrough] =   tr ("Writethrough", "DiskType");
    mDiskTypes_Differencing =                   tr ("Differencing", "DiskType");

    mVRDPAuthTypes [KVRDPAuthType_Null] =       tr ("Null", "VRDPAuthType");
    mVRDPAuthTypes [KVRDPAuthType_External] =   tr ("External", "VRDPAuthType");
    mVRDPAuthTypes [KVRDPAuthType_Guest] =      tr ("Guest", "VRDPAuthType");

    mPortModeTypes [KPortMode_Disconnected] =   tr ("Disconnected", "PortMode");
    mPortModeTypes [KPortMode_HostPipe] =       tr ("Host Pipe", "PortMode");
    mPortModeTypes [KPortMode_HostDevice] =     tr ("Host Device", "PortMode");

    mUSBFilterActionTypes [KUSBDeviceFilterAction_Ignore] =
        tr ("Ignore", "USBFilterActionType");
    mUSBFilterActionTypes [KUSBDeviceFilterAction_Hold] =
        tr ("Hold", "USBFilterActionType");

    mAudioDriverTypes [KAudioDriverType_Null] =
        tr ("Null Audio Driver", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_WinMM] =
        tr ("Windows Multimedia", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_SolAudio] =
        tr ("Solaris Audio", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_OSS] =
        tr ("OSS Audio Driver", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_ALSA] =
        tr ("ALSA Audio Driver", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_DirectSound] =
        tr ("Windows DirectSound", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_CoreAudio] =
        tr ("CoreAudio", "AudioDriverType");
    mAudioDriverTypes [KAudioDriverType_Pulse] =
        tr ("PulseAudio", "AudioDriverType");

    mAudioControllerTypes [KAudioControllerType_AC97] =
        tr ("ICH AC97", "AudioControllerType");
    mAudioControllerTypes [KAudioControllerType_SB16] =
        tr ("SoundBlaster 16", "AudioControllerType");

    mNetworkAdapterTypes [KNetworkAdapterType_Am79C970A] =
        tr ("PCnet-PCI II (Am79C970A)", "NetworkAdapterType");
    mNetworkAdapterTypes [KNetworkAdapterType_Am79C973] =
        tr ("PCnet-FAST III (Am79C973)", "NetworkAdapterType");
    mNetworkAdapterTypes [KNetworkAdapterType_I82540EM] =
        tr ("Intel PRO/1000 MT Desktop (82540EM)", "NetworkAdapterType");
    mNetworkAdapterTypes [KNetworkAdapterType_I82543GC] =
        tr ("Intel PRO/1000 T Server (82543GC)", "NetworkAdapterType");

    mNetworkAttachmentTypes [KNetworkAttachmentType_Null] =
        tr ("Not attached", "NetworkAttachmentType");
    mNetworkAttachmentTypes [KNetworkAttachmentType_NAT] =
        tr ("NAT", "NetworkAttachmentType");
    mNetworkAttachmentTypes [KNetworkAttachmentType_Bridged] =
        tr ("Bridged Interface", "NetworkAttachmentType");
    mNetworkAttachmentTypes [KNetworkAttachmentType_Internal] =
        tr ("Internal Network", "NetworkAttachmentType");
    mNetworkAttachmentTypes [KNetworkAttachmentType_HostOnly] =
        tr ("Host-only Network", "NetworkAttachmentType");

    mClipboardTypes [KClipboardMode_Disabled] =
        tr ("Disabled", "ClipboardType");
    mClipboardTypes [KClipboardMode_HostToGuest] =
        tr ("Host To Guest", "ClipboardType");
    mClipboardTypes [KClipboardMode_GuestToHost] =
        tr ("Guest To Host", "ClipboardType");
    mClipboardTypes [KClipboardMode_Bidirectional] =
        tr ("Bidirectional", "ClipboardType");

    mStorageControllerTypes [KStorageControllerType_PIIX3] =
        tr ("PIIX3", "StorageControllerType");
    mStorageControllerTypes [KStorageControllerType_PIIX4] =
        tr ("PIIX4", "StorageControllerType");
    mStorageControllerTypes [KStorageControllerType_ICH6] =
        tr ("ICH6", "StorageControllerType");
    /* Leave them out for now because this is used for the IDE controller
     * setting in the general page and we do not want that the other controllers
     * show up there.
     */
#if 0
    mStorageControllerTypes [KStorageControllerType_IntelAhci] =
        tr ("AHCI", "StorageControllerType");
    mStorageControllerTypes [KStorageControllerType_LsiLogic] =
        tr ("Lsilogic", "StorageControllerType");
    mStorageControllerTypes [KStorageControllerType_BusLogic] =
        tr ("BusLogic", "StorageControllerType");
#endif

    mUSBDeviceStates [KUSBDeviceState_NotSupported] =
        tr ("Not supported", "USBDeviceState");
    mUSBDeviceStates [KUSBDeviceState_Unavailable] =
        tr ("Unavailable", "USBDeviceState");
    mUSBDeviceStates [KUSBDeviceState_Busy] =
        tr ("Busy", "USBDeviceState");
    mUSBDeviceStates [KUSBDeviceState_Available] =
        tr ("Available", "USBDeviceState");
    mUSBDeviceStates [KUSBDeviceState_Held] =
        tr ("Held", "USBDeviceState");
    mUSBDeviceStates [KUSBDeviceState_Captured] =
        tr ("Captured", "USBDeviceState");

    mUserDefinedPortName = tr ("User-defined", "serial port");

    mWarningIcon = standardIcon (QStyle::SP_MessageBoxWarning, 0).pixmap (16, 16);
    Assert (!mWarningIcon.isNull());

    mErrorIcon = standardIcon (QStyle::SP_MessageBoxCritical, 0).pixmap (16, 16);
    Assert (!mErrorIcon.isNull());

    mDetailReportTemplatesReady = false;

    /* refresh media properties since they contain some translations too  */
    for (VBoxMediaList::iterator it = mMediaList.begin();
         it != mMediaList.end(); ++ it)
        it->refresh();

#if defined (Q_WS_PM) || defined (Q_WS_X11)
    /* As PM and X11 do not (to my knowledge) have functionality for providing
     * human readable key names, we keep a table of them, which must be
     * updated when the language is changed. */
    QIHotKeyEdit::retranslateUi();
#endif
}

// public static stuff
////////////////////////////////////////////////////////////////////////////////

/* static */
bool VBoxGlobal::isDOSType (const QString &aOSTypeId)
{
    if (aOSTypeId.left (3) == "dos" ||
        aOSTypeId.left (3) == "win" ||
        aOSTypeId.left (3) == "os2")
        return true;

    return false;
}

/**
 *  Sets the QLabel background and frame colors according tho the pixmap
 *  contents. The bottom right pixel of the label pixmap defines the
 *  background color of the label, the top right pixel defines the color of
 *  the one-pixel frame around it. This function also sets the alignment of
 *  the pixmap to AlignVTop (to correspond to the color choosing logic).
 *
 *  This method is useful to provide nice scaling of pixmal labels without
 *  scaling pixmaps themselves. To see th eeffect, the size policy of the
 *  label in the corresponding direction (vertical, for now) should be set to
 *  something like MinimumExpanding.
 *
 *  @todo Parametrize corners to select pixels from and set the alignment
 *  accordingly.
 */
/* static */
void VBoxGlobal::adoptLabelPixmap (QLabel *aLabel)
{
    AssertReturnVoid (aLabel);

    aLabel->setAlignment (Qt::AlignTop);
    aLabel->setFrameShape (QFrame::Box);
    aLabel->setFrameShadow (QFrame::Plain);

    const QPixmap *pix = aLabel->pixmap();
    QImage img = pix->toImage();
    QRgb rgbBack = img.pixel (img.width() - 1, img.height() - 1);
    QRgb rgbFrame = img.pixel (img.width() - 1, 0);

    QPalette pal = aLabel->palette();
    pal.setColor (QPalette::Window, rgbBack);
    pal.setColor (QPalette::WindowText, rgbFrame);
    aLabel->setPalette (pal);
}

extern const char *gVBoxLangSubDir = "/nls";
extern const char *gVBoxLangFileBase = "VirtualBox_";
extern const char *gVBoxLangFileExt = ".qm";
extern const char *gVBoxLangIDRegExp = "(([a-z]{2})(?:_([A-Z]{2}))?)|(C)";
extern const char *gVBoxBuiltInLangName   = "C";

class VBoxTranslator : public QTranslator
{
public:

    VBoxTranslator (QObject *aParent = 0)
        : QTranslator (aParent) {}

    bool loadFile (const QString &aFileName)
    {
        QFile file (aFileName);
        if (!file.open (QIODevice::ReadOnly))
            return false;
        mData = file.readAll();
        return load ((uchar*) mData.data(), mData.size());
    }

private:

    QByteArray mData;
};

static VBoxTranslator *sTranslator = 0;
static QString sLoadedLangId = gVBoxBuiltInLangName;

/**
 *  Returns the loaded (active) language ID.
 *  Note that it may not match with VBoxGlobalSettings::languageId() if the
 *  specified language cannot be loaded.
 *  If the built-in language is active, this method returns "C".
 *
 *  @note "C" is treated as the built-in language for simplicity -- the C
 *  locale is used in unix environments as a fallback when the requested
 *  locale is invalid. This way we don't need to process both the "built_in"
 *  language and the "C" language (which is a valid environment setting)
 *  separately.
 */
/* static */
QString VBoxGlobal::languageId()
{
    return sLoadedLangId;
}

/**
 *  Loads the language by language ID.
 *
 *  @param aLangId Language ID in in form of xx_YY. QString::null means the
 *                 system default language.
 */
/* static */
void VBoxGlobal::loadLanguage (const QString &aLangId)
{
    QString langId = aLangId.isNull() ?
        VBoxGlobal::systemLanguageId() : aLangId;
    QString languageFileName;
    QString selectedLangId = gVBoxBuiltInLangName;

    char szNlsPath[RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateNoArch(szNlsPath, sizeof(szNlsPath));
    AssertRC (rc);

    QString nlsPath = QString(szNlsPath) + gVBoxLangSubDir;
    QDir nlsDir (nlsPath);

    Assert (!langId.isEmpty());
    if (!langId.isEmpty() && langId != gVBoxBuiltInLangName)
    {
        QRegExp regExp (gVBoxLangIDRegExp);
        int pos = regExp.indexIn (langId);
        /* the language ID should match the regexp completely */
        AssertReturnVoid (pos == 0);

        QString lang = regExp.cap (2);

        if (nlsDir.exists (gVBoxLangFileBase + langId + gVBoxLangFileExt))
        {
            languageFileName = nlsDir.absoluteFilePath (gVBoxLangFileBase + langId +
                                                        gVBoxLangFileExt);
            selectedLangId = langId;
        }
        else if (nlsDir.exists (gVBoxLangFileBase + lang + gVBoxLangFileExt))
        {
            languageFileName = nlsDir.absoluteFilePath (gVBoxLangFileBase + lang +
                                                        gVBoxLangFileExt);
            selectedLangId = lang;
        }
        else
        {
            /* Never complain when the default language is requested.  In any
             * case, if no explicit language file exists, we will simply
             * fall-back to English (built-in). */
            if (!aLangId.isNull())
                vboxProblem().cannotFindLanguage (langId, nlsPath);
            /* selectedLangId remains built-in here */
            AssertReturnVoid (selectedLangId == gVBoxBuiltInLangName);
        }
    }

    /* delete the old translator if there is one */
    if (sTranslator)
    {
        /* QTranslator destructor will call qApp->removeTranslator() for
         * us. It will also delete all its child translations we attach to it
         * below, so we don't have to care about them specially. */
        delete sTranslator;
    }

    /* load new language files */
    sTranslator = new VBoxTranslator (qApp);
    Assert (sTranslator);
    bool loadOk = true;
    if (sTranslator)
    {
        if (selectedLangId != gVBoxBuiltInLangName)
        {
            Assert (!languageFileName.isNull());
            loadOk = sTranslator->loadFile (languageFileName);
        }
        /* we install the translator in any case: on failure, this will
         * activate an empty translator that will give us English
         * (built-in) */
        qApp->installTranslator (sTranslator);
    }
    else
        loadOk = false;

    if (loadOk)
        sLoadedLangId = selectedLangId;
    else
    {
        vboxProblem().cannotLoadLanguage (languageFileName);
        sLoadedLangId = gVBoxBuiltInLangName;
    }

    /* Try to load the corresponding Qt translation */
    if (sLoadedLangId != gVBoxBuiltInLangName)
    {
#ifdef Q_OS_UNIX
        /* We use system installations of Qt on Linux systems, so first, try
         * to load the Qt translation from the system location. */
        languageFileName = QLibraryInfo::location(QLibraryInfo::TranslationsPath) + "/qt_" +
                           sLoadedLangId + gVBoxLangFileExt;
        QTranslator *qtSysTr = new QTranslator (sTranslator);
        Assert (qtSysTr);
        if (qtSysTr && qtSysTr->load (languageFileName))
            qApp->installTranslator (qtSysTr);
        /* Note that the Qt translation supplied by Sun is always loaded
         * afterwards to make sure it will take precedence over the system
         * translation (it may contain more decent variants of translation
         * that better correspond to VirtualBox UI). We need to load both
         * because a newer version of Qt may be installed on the user computer
         * and the Sun version may not fully support it. We don't do it on
         * Win32 because we supply a Qt library there and therefore the
         * Sun translation is always the best one. */
#endif
        languageFileName =  nlsDir.absoluteFilePath (QString ("qt_") +
                                                     sLoadedLangId +
                                                     gVBoxLangFileExt);
        QTranslator *qtTr = new QTranslator (sTranslator);
        Assert (qtTr);
        if (qtTr && (loadOk = qtTr->load (languageFileName)))
            qApp->installTranslator (qtTr);
        /* The below message doesn't fit 100% (because it's an additonal
         * language and the main one won't be reset to built-in on failure)
         * but the load failure is so rare here that it's not worth a separate
         * message (but still, having something is better than having none) */
        if (!loadOk && !aLangId.isNull())
            vboxProblem().cannotLoadLanguage (languageFileName);
    }
}

QString VBoxGlobal::helpFile() const
{
#if defined (Q_WS_WIN32) || defined (Q_WS_X11)
    const QString name = "VirtualBox";
    const QString suffix = "chm";
#elif defined (Q_WS_MAC)
    const QString name = "UserManual";
    const QString suffix = "pdf";
#endif
    /* Where are the docs located? */
    char szDocsPath[RTPATH_MAX];
    int rc = RTPathAppDocs (szDocsPath, sizeof (szDocsPath));
    AssertRC (rc);
    /* Make sure that the language is in two letter code.
     * Note: if languageId() returns an empty string lang.name() will
     * return "C" which is an valid language code. */
    QLocale lang (VBoxGlobal::languageId());

    /* Construct the path and the filename */
    QString manual = QString ("%1/%2_%3.%4").arg (szDocsPath)
                                            .arg (name)
                                            .arg (lang.name())
                                            .arg (suffix);
    /* Check if a help file with that name exists */
    QFileInfo fi (manual);
    if (fi.exists())
        return manual;

    /* Fall back to the standard */
    manual = QString ("%1/%2.%4").arg (szDocsPath)
                                 .arg (name)
                                 .arg (suffix);
    return manual;
}

/* static */
QIcon VBoxGlobal::iconSet (const char *aNormal,
                           const char *aDisabled /* = NULL */,
                           const char *aActive /* = NULL */)
{
    QIcon iconSet;

    Assert (aNormal != NULL);
    iconSet.addFile (aNormal, QSize(),
                     QIcon::Normal);
    if (aDisabled != NULL)
        iconSet.addFile (aDisabled, QSize(),
                         QIcon::Disabled);
    if (aActive != NULL)
        iconSet.addFile (aActive, QSize(),
                         QIcon::Active);
    return iconSet;
}

/* static */
QIcon VBoxGlobal::iconSetOnOff (const char *aNormal, const char *aNormalOff,
                                const char *aDisabled /* = NULL */,
                                const char *aDisabledOff /* = NULL */,
                                const char *aActive /* = NULL */,
                                const char *aActiveOff /* = NULL */)
{
    QIcon iconSet;

    Assert (aNormal != NULL);
    iconSet.addFile (aNormal, QSize(), QIcon::Normal, QIcon::On);
    if (aNormalOff != NULL)
        iconSet.addFile (aNormalOff, QSize(), QIcon::Normal, QIcon::Off);

    if (aDisabled != NULL)
        iconSet.addFile (aDisabled, QSize(), QIcon::Disabled, QIcon::On);
    if (aDisabledOff != NULL)
        iconSet.addFile (aDisabledOff, QSize(), QIcon::Disabled, QIcon::Off);

    if (aActive != NULL)
        iconSet.addFile (aActive, QSize(), QIcon::Active, QIcon::On);
    if (aActiveOff != NULL)
        iconSet.addFile (aActive, QSize(), QIcon::Active, QIcon::Off);

    return iconSet;
}

/* static */
QIcon VBoxGlobal::iconSetFull (const QSize &aNormalSize, const QSize &aSmallSize,
                               const char *aNormal, const char *aSmallNormal,
                               const char *aDisabled /* = NULL */,
                               const char *aSmallDisabled /* = NULL */,
                               const char *aActive /* = NULL */,
                               const char *aSmallActive /* = NULL */)
{
    QIcon iconSet;

    Assert (aNormal != NULL);
    Assert (aSmallNormal != NULL);
    iconSet.addFile (aNormal, aNormalSize, QIcon::Normal);
    iconSet.addFile (aSmallNormal, aSmallSize, QIcon::Normal);

    if (aSmallDisabled != NULL)
    {
        iconSet.addFile (aDisabled, aNormalSize, QIcon::Disabled);
        iconSet.addFile (aSmallDisabled, aSmallSize, QIcon::Disabled);
    }

    if (aSmallActive != NULL)
    {
        iconSet.addFile (aActive, aNormalSize, QIcon::Active);
        iconSet.addFile (aSmallActive, aSmallSize, QIcon::Active);
    }

    return iconSet;
}

QIcon VBoxGlobal::standardIcon (QStyle::StandardPixmap aStandard, QWidget *aWidget /* = NULL */)
{
    QStyle *style = aWidget ? aWidget->style(): QApplication::style();
#ifdef Q_WS_MAC
    /* At least in Qt 4.3.4/4.4 RC1 SP_MessageBoxWarning is the application
     * icon. So change this to the critical icon. (Maybe this would be
     * fixed in a later Qt version) */
    if (aStandard == QStyle::SP_MessageBoxWarning)
        return style->standardIcon (QStyle::SP_MessageBoxCritical, 0, aWidget);
#endif /* Q_WS_MAC */
    return style->standardIcon (aStandard, 0, aWidget);
}

/**
 *  Replacement for QToolButton::setTextLabel() that handles the shortcut
 *  letter (if it is present in the argument string) as if it were a setText()
 *  call: the shortcut letter is used to automatically assign an "Alt+<letter>"
 *  accelerator key sequence to the given tool button.
 *
 *  @note This method preserves the icon set if it was assigned before. Only
 *  the text label and the accelerator are changed.
 *
 *  @param aToolButton  Tool button to set the text label on.
 *  @param aTextLabel   Text label to set.
 */
/* static */
void VBoxGlobal::setTextLabel (QToolButton *aToolButton,
                               const QString &aTextLabel)
{
    AssertReturnVoid (aToolButton != NULL);

    /* remember the icon set as setText() will kill it */
    QIcon iset = aToolButton->icon();
    /* re-use the setText() method to detect and set the accelerator */
    aToolButton->setText (aTextLabel);
    QKeySequence accel = aToolButton->shortcut();
    aToolButton->setText (aTextLabel);
    aToolButton->setIcon (iset);
    /* set the accel last as setIconSet() would kill it */
    aToolButton->setShortcut (accel);
}

/**
 *  Performs direct and flipped search of position for \a aRectangle to make sure
 *  it is fully contained inside \a aBoundRegion region by moving & resizing
 *  \a aRectangle if necessary. Selects the minimum shifted result between direct
 *  and flipped variants.
 */
/* static */
QRect VBoxGlobal::normalizeGeometry (const QRect &aRectangle, const QRegion &aBoundRegion,
                                     bool aCanResize /* = true */)
{
    /* Direct search for normalized rectangle */
    QRect var1 (getNormalized (aRectangle, aBoundRegion, aCanResize));

    /* Flipped search for normalized rectangle */
    QRect var2 (flip (getNormalized (flip (aRectangle).boundingRect(),
                                     flip (aBoundRegion), aCanResize)).boundingRect());

    /* Calculate shift from starting position for both variants */
    double length1 = sqrt (pow ((double) (var1.x() - aRectangle.x()), (double) 2) +
                           pow ((double) (var1.y() - aRectangle.y()), (double) 2));
    double length2 = sqrt (pow ((double) (var2.x() - aRectangle.x()), (double) 2) +
                           pow ((double) (var2.y() - aRectangle.y()), (double) 2));

    /* Return minimum shifted variant */
    return length1 > length2 ? var2 : var1;
}

/**
 *  Ensures that the given rectangle \a aRectangle is fully contained within the
 *  region \a aBoundRegion by moving \a aRectangle if necessary. If \a aRectangle is
 *  larger than \a aBoundRegion, top left corner of \a aRectangle is aligned with the
 *  top left corner of maximum available rectangle and, if \a aCanResize is true,
 *  \a aRectangle is shrinked to become fully visible.
 */
/* static */
QRect VBoxGlobal::getNormalized (const QRect &aRectangle, const QRegion &aBoundRegion,
                                 bool /* aCanResize = true */)
{
    /* Storing available horizontal sub-rectangles & vertical shifts */
    int windowVertical = aRectangle.center().y();
    QVector <QRect> rectanglesVector (aBoundRegion.rects());
    QList <QRect> rectanglesList;
    QList <int> shiftsList;
    foreach (QRect currentItem, rectanglesVector)
    {
        int currentDelta = qAbs (windowVertical - currentItem.center().y());
        int shift2Top = currentItem.top() - aRectangle.top();
        int shift2Bot = currentItem.bottom() - aRectangle.bottom();

        int itemPosition = 0;
        foreach (QRect item, rectanglesList)
        {
            int delta = qAbs (windowVertical - item.center().y());
            if (delta > currentDelta) break; else ++ itemPosition;
        }
        rectanglesList.insert (itemPosition, currentItem);

        int shift2TopPos = 0;
        foreach (int shift, shiftsList)
            if (qAbs (shift) > qAbs (shift2Top)) break; else ++ shift2TopPos;
        shiftsList.insert (shift2TopPos, shift2Top);

        int shift2BotPos = 0;
        foreach (int shift, shiftsList)
            if (qAbs (shift) > qAbs (shift2Bot)) break; else ++ shift2BotPos;
        shiftsList.insert (shift2BotPos, shift2Bot);
    }

    /* Trying to find the appropriate place for window */
    QRect result;
    for (int i = -1; i < shiftsList.size(); ++ i)
    {
        /* Move to appropriate vertical */
        QRect rectangle (aRectangle);
        if (i >= 0) rectangle.translate (0, shiftsList [i]);

        /* Search horizontal shift */
        int maxShift = 0;
        foreach (QRect item, rectanglesList)
        {
            QRect trectangle (rectangle.translated (item.left() - rectangle.left(), 0));
            if (!item.intersects (trectangle))
                continue;

            if (rectangle.left() < item.left())
            {
                int shift = item.left() - rectangle.left();
                maxShift = qAbs (shift) > qAbs (maxShift) ? shift : maxShift;
            }
            else if (rectangle.right() > item.right())
            {
                int shift = item.right() - rectangle.right();
                maxShift = qAbs (shift) > qAbs (maxShift) ? shift : maxShift;
            }
        }

        /* Shift across the horizontal direction */
        rectangle.translate (maxShift, 0);

        /* Check the translated rectangle to feat the rules */
        if (aBoundRegion.united (rectangle) == aBoundRegion)
            result = rectangle;

        if (!result.isNull()) break;
    }

    if (result.isNull())
    {
        /* Resize window to feat desirable size
         * using max of available rectangles */
        QRect maxRectangle;
        quint64 maxSquare = 0;
        foreach (QRect item, rectanglesList)
        {
            quint64 square = item.width() * item.height();
            if (square > maxSquare)
            {
                maxSquare = square;
                maxRectangle = item;
            }
        }

        result = aRectangle;
        result.moveTo (maxRectangle.x(), maxRectangle.y());
        if (maxRectangle.right() < result.right())
            result.setRight (maxRectangle.right());
        if (maxRectangle.bottom() < result.bottom())
            result.setBottom (maxRectangle.bottom());
    }

    return result;
}

/**
 *  Returns the flipped (transposed) region.
 */
/* static */
QRegion VBoxGlobal::flip (const QRegion &aRegion)
{
    QRegion result;
    QVector <QRect> rectangles (aRegion.rects());
    foreach (QRect rectangle, rectangles)
        result += QRect (rectangle.y(), rectangle.x(),
                         rectangle.height(), rectangle.width());
    return result;
}

/**
 *  Aligns the center of \a aWidget with the center of \a aRelative.
 *
 *  If necessary, \a aWidget's position is adjusted to make it fully visible
 *  within the available desktop area. If \a aWidget is bigger then this area,
 *  it will also be resized unless \a aCanResize is false or there is an
 *  inappropriate minimum size limit (in which case the top left corner will be
 *  simply aligned with the top left corner of the available desktop area).
 *
 *  \a aWidget must be a top-level widget. \a aRelative may be any widget, but
 *  if it's not top-level itself, its top-level widget will be used for
 *  calculations. \a aRelative can also be NULL, in which case \a aWidget will
 *  be centered relative to the available desktop area.
 */
/* static */
void VBoxGlobal::centerWidget (QWidget *aWidget, QWidget *aRelative,
                               bool aCanResize /* = true */)
{
    AssertReturnVoid (aWidget);
    AssertReturnVoid (aWidget->isTopLevel());

    QRect deskGeo, parentGeo;
    QWidget *w = aRelative;
    if (w)
    {
        w = w->window();
        deskGeo = QApplication::desktop()->availableGeometry (w);
        parentGeo = w->frameGeometry();
        /* On X11/Gnome, geo/frameGeo.x() and y() are always 0 for top level
         * widgets with parents, what a shame. Use mapToGlobal() to workaround. */
        QPoint d = w->mapToGlobal (QPoint (0, 0));
        d.rx() -= w->geometry().x() - w->x();
        d.ry() -= w->geometry().y() - w->y();
        parentGeo.moveTopLeft (d);
    }
    else
    {
        deskGeo = QApplication::desktop()->availableGeometry();
        parentGeo = deskGeo;
    }

    /* On X11, there is no way to determine frame geometry (including WM
     * decorations) before the widget is shown for the first time. Stupidly
     * enumerate other top level widgets to find the thickest frame. The code
     * is based on the idea taken from QDialog::adjustPositionInternal(). */

    int extraw = 0, extrah = 0;

    QWidgetList list = QApplication::topLevelWidgets();
    QListIterator<QWidget*> it (list);
    while ((extraw == 0 || extrah == 0) && it.hasNext())
    {
        int framew, frameh;
        QWidget *current = it.next();
        if (!current->isVisible())
            continue;

        framew = current->frameGeometry().width() - current->width();
        frameh = current->frameGeometry().height() - current->height();

        extraw = qMax (extraw, framew);
        extrah = qMax (extrah, frameh);
    }

    /// @todo (r=dmik) not sure if we really need this
#if 0
    /* sanity check for decoration frames. With embedding, we
     * might get extraordinary values */
    if (extraw == 0 || extrah == 0 || extraw > 20 || extrah > 50)
    {
        extrah = 50;
        extraw = 20;
    }
#endif

    /* On non-X11 platforms, the following would be enough instead of the
     * above workaround: */
    // QRect geo = frameGeometry();
    QRect geo = QRect (0, 0, aWidget->width() + extraw,
                             aWidget->height() + extrah);

    geo.moveCenter (QPoint (parentGeo.x() + (parentGeo.width() - 1) / 2,
                            parentGeo.y() + (parentGeo.height() - 1) / 2));

    /* ensure the widget is within the available desktop area */
    QRect newGeo = normalizeGeometry (geo, deskGeo, aCanResize);

    aWidget->move (newGeo.topLeft());

    if (aCanResize &&
        (geo.width() != newGeo.width() || geo.height() != newGeo.height()))
        aWidget->resize (newGeo.width() - extraw, newGeo.height() - extrah);
}

/**
 *  Returns the decimal separator for the current locale.
 */
/* static */
QChar VBoxGlobal::decimalSep()
{
    return QLocale::system().decimalPoint();
}

/**
 *  Returns the regexp string that defines the format of the human-readable
 *  size representation, <tt>####[.##] B|KB|MB|GB|TB|PB</tt>.
 *
 *  This regexp will capture 5 groups of text:
 *  - cap(1): integer number in case when no decimal point is present
 *            (if empty, it means that decimal point is present)
 *  - cap(2): size suffix in case when no decimal point is present (may be empty)
 *  - cap(3): integer number in case when decimal point is present (may be empty)
 *  - cap(4): fraction number (hundredth) in case when decimal point is present
 *  - cap(5): size suffix in case when decimal point is present (note that
 *            B cannot appear there)
 */
/* static */
QString VBoxGlobal::sizeRegexp()
{
    QString regexp =
        QString ("^(?:(?:(\\d+)(?:\\s?([KMGTP]?B))?)|(?:(\\d*)%1(\\d{1,2})(?:\\s?([KMGTP]B))))$")
                 .arg (decimalSep());
    return regexp;
}

/**
 *  Parses the given size string that should be in form of
 *  <tt>####[.##] B|KB|MB|GB|TB|PB</tt> and returns the size value
 *  in bytes. Zero is returned on error.
 */
/* static */
quint64 VBoxGlobal::parseSize (const QString &aText)
{
    QRegExp regexp (sizeRegexp());
    int pos = regexp.indexIn (aText);
    if (pos != -1)
    {
        QString intgS = regexp.cap (1);
        QString hundS;
        QString suff = regexp.cap (2);
        if (intgS.isEmpty())
        {
            intgS = regexp.cap (3);
            hundS = regexp.cap (4);
            suff = regexp.cap (5);
        }

        quint64 denom = 0;
        if (suff.isEmpty() || suff == "B")
            denom = 1;
        else if (suff == "KB")
            denom = _1K;
        else if (suff == "MB")
            denom = _1M;
        else if (suff == "GB")
            denom = _1G;
        else if (suff == "TB")
            denom = _1T;
        else if (suff == "PB")
            denom = _1P;

        quint64 intg = intgS.toULongLong();
        if (denom == 1)
            return intg;

        quint64 hund = hundS.leftJustified (2, '0').toULongLong();
        hund = hund * denom / 100;
        intg = intg * denom + hund;
        return intg;
    }
    else
        return 0;
}

/**
 * Formats the given @a aSize value in bytes to a human readable string
 * in form of <tt>####[.##] B|KB|MB|GB|TB|PB</tt>.
 *
 * The @a aMode and @a aDecimal parameters are used for rounding the resulting
 * number when converting the size value to KB, MB, etc gives a fractional part:
 * <ul>
 * <li>When \a aMode is FormatSize_Round, the result is rounded to the
 *     closest number containing \a aDecimal decimal digits.
 * </li>
 * <li>When \a aMode is FormatSize_RoundDown, the result is rounded to the
 *     largest number with \a aDecimal decimal digits that is not greater than
 *     the result. This guarantees that converting the resulting string back to
 *     the integer value in bytes will not produce a value greater that the
 *     initial size parameter.
 * </li>
 * <li>When \a aMode is FormatSize_RoundUp, the result is rounded to the
 *     smallest number with \a aDecimal decimal digits that is not less than the
 *     result. This guarantees that converting the resulting string back to the
 *     integer value in bytes will not produce a value less that the initial
 *     size parameter.
 * </li>
 * </ul>
 *
 * @param aSize     Size value in bytes.
 * @param aMode     Conversion mode.
 * @param aDecimal  Number of decimal digits in result.
 * @return          Human-readable size string.
 */
/* static */
QString VBoxGlobal::formatSize (quint64 aSize, uint aDecimal /* = 2 */,
                                VBoxDefs::FormatSize aMode /* = FormatSize_Round */)
{
    static const char *Suffixes [] = { "B", "KB", "MB", "GB", "TB", "PB", NULL };

    quint64 denom = 0;
    int suffix = 0;

    if (aSize < _1K)
    {
        denom = 1;
        suffix = 0;
    }
    else if (aSize < _1M)
    {
        denom = _1K;
        suffix = 1;
    }
    else if (aSize < _1G)
    {
        denom = _1M;
        suffix = 2;
    }
    else if (aSize < _1T)
    {
        denom = _1G;
        suffix = 3;
    }
    else if (aSize < _1P)
    {
        denom = _1T;
        suffix = 4;
    }
    else
    {
        denom = _1P;
        suffix = 5;
    }

    quint64 intg = aSize / denom;
    quint64 decm = aSize % denom;
    quint64 mult = 1;
    for (uint i = 0; i < aDecimal; ++ i) mult *= 10;

    QString number;
    if (denom > 1)
    {
        if (decm)
        {
            decm *= mult;
            /* not greater */
            if (aMode == VBoxDefs::FormatSize_RoundDown)
                decm = decm / denom;
            /* not less */
            else if (aMode == VBoxDefs::FormatSize_RoundUp)
                decm = (decm + denom - 1) / denom;
            /* nearest */
            else decm = (decm + denom / 2) / denom;
        }
        /* check for the fractional part overflow due to rounding */
        if (decm == mult)
        {
            decm = 0;
            ++ intg;
            /* check if we've got 1024 XB after rounding and scale down if so */
            if (intg == 1024 && Suffixes [suffix + 1] != NULL)
            {
                intg /= 1024;
                ++ suffix;
            }
        }
        number = QString::number (intg);
        if (aDecimal) number += QString ("%1%2").arg (decimalSep())
            .arg (QString::number (decm).rightJustified (aDecimal, '0'));
    }
    else
    {
        number = QString::number (intg);
    }

    return QString ("%1 %2").arg (number).arg (Suffixes [suffix]);
}

/**
 *  Returns the required video memory in bytes for the current desktop
 *  resolution at maximum possible screen depth in bpp.
 */
/* static */
quint64 VBoxGlobal::requiredVideoMemory (CMachine *aMachine)
{
    QSize desktopRes = QApplication::desktop()->screenGeometry().size();
    /* Calculate summary required memory amount in bits */
    quint64 needBits = (desktopRes.width() /* display width */ *
                        desktopRes.height() /* display height */ *
                        32 /* we will take the maximum possible bpp for now */ +
                        8 * _1M /* current cache per screen - may be changed in future */) *
                       (!aMachine || aMachine->isNull() ? 1 : aMachine->GetMonitorCount()) +
                       8 * 4096 /* adapter info */;
    /* Translate value into megabytes with rounding to highest side */
    quint64 needMBytes = needBits % (8 * _1M) ? needBits / (8 * _1M) + 1 :
                         needBits / (8 * _1M) /* convert to megabytes */;

    return needMBytes * _1M;
}

/**
 * Puts soft hyphens after every path component in the given file name.
 *
 * @param aFileName File name (must be a full path name).
 */
/* static */
QString VBoxGlobal::locationForHTML (const QString &aFileName)
{
/// @todo (dmik) remove?
//    QString result = QDir::toNativeSeparators (fn);
//#ifdef Q_OS_LINUX
//    result.replace ('/', "/<font color=red>&shy;</font>");
//#else
//    result.replace ('\\', "\\<font color=red>&shy;</font>");
//#endif
//    return result;
    QFileInfo fi (aFileName);
    return fi.fileName();
}

/**
 *  Reformats the input string @a aStr so that:
 *  - strings in single quotes will be put inside <nobr> and marked
 *    with blue color;
 *  - UUIDs be put inside <nobr> and marked
 *    with green color;
 *  - replaces new line chars with </p><p> constructs to form paragraphs
 *    (note that <p> and </p> are not appended to the beginnign and to the
 *     end of the string respectively, to allow the result be appended
 *     or prepended to the existing paragraph).
 *
 *  If @a aToolTip is true, colouring is not applied, only the <nobr> tag
 *  is added. Also, new line chars are replaced with <br> instead of <p>.
 */
/* static */
QString VBoxGlobal::highlight (const QString &aStr, bool aToolTip /* = false */)
{
    QString strFont;
    QString uuidFont;
    QString endFont;
    if (!aToolTip)
    {
        strFont = "<font color=#0000CC>";
        uuidFont = "<font color=#008000>";
        endFont = "</font>";
    }

    QString text = aStr;

    /* replace special entities, '&' -- first! */
    text.replace ('&', "&amp;");
    text.replace ('<', "&lt;");
    text.replace ('>', "&gt;");
    text.replace ('\"', "&quot;");

    /* mark strings in single quotes with color */
    QRegExp rx = QRegExp ("((?:^|\\s)[(]?)'([^']*)'(?=[:.-!);]?(?:\\s|$))");
    rx.setMinimal (true);
    text.replace (rx,
        QString ("\\1%1<nobr>'\\2'</nobr>%2").arg (strFont).arg (endFont));

    /* mark UUIDs with color */
    text.replace (QRegExp (
        "((?:^|\\s)[(]?)"
        "(\\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\\})"
        "(?=[:.-!);]?(?:\\s|$))"),
        QString ("\\1%1<nobr>\\2</nobr>%2").arg (uuidFont).arg (endFont));

    /* split to paragraphs at \n chars */
    if (!aToolTip)
        text.replace ('\n', "</p><p>");
    else
        text.replace ('\n', "<br>");

    return text;
}

/**
 *  Reformats the input string @a aStr so that:
 *  - strings in single quotes will be put inside <nobr> and marked
 *    with bold style;
 *  - UUIDs be put inside <nobr> and marked
 *    with italic style;
 *  - replaces new line chars with </p><p> constructs to form paragraphs
 *    (note that <p> and </p> are not appended to the beginnign and to the
 *     end of the string respectively, to allow the result be appended
 *     or prepended to the existing paragraph).
 */
/* static */
QString VBoxGlobal::emphasize (const QString &aStr)
{
    QString strEmphStart ("<b>");
    QString strEmphEnd ("</b>");
    QString uuidEmphStart ("<i>");
    QString uuidEmphEnd ("</i>");

    QString text = aStr;

    /* replace special entities, '&' -- first! */
    text.replace ('&', "&amp;");
    text.replace ('<', "&lt;");
    text.replace ('>', "&gt;");
    text.replace ('\"', "&quot;");

    /* mark strings in single quotes with bold style */
    QRegExp rx = QRegExp ("((?:^|\\s)[(]?)'([^']*)'(?=[:.-!);]?(?:\\s|$))");
    rx.setMinimal (true);
    text.replace (rx,
        QString ("\\1%1<nobr>'\\2'</nobr>%2").arg (strEmphStart).arg (strEmphEnd));

    /* mark UUIDs with italic style */
    text.replace (QRegExp (
        "((?:^|\\s)[(]?)"
        "(\\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\\})"
        "(?=[:.-!);]?(?:\\s|$))"),
        QString ("\\1%1<nobr>\\2</nobr>%2").arg (uuidEmphStart).arg (uuidEmphEnd));

    /* split to paragraphs at \n chars */
    text.replace ('\n', "</p><p>");

    return text;
}

/**
 *  This does exactly the same as QLocale::system().name() but corrects its
 *  wrong behavior on Linux systems (LC_NUMERIC for some strange reason takes
 *  precedence over any other locale setting in the QLocale::system()
 *  implementation). This implementation first looks at LC_ALL (as defined by
 *  SUS), then looks at LC_MESSAGES which is designed to define a language for
 *  program messages in case if it differs from the language for other locale
 *  categories. Then it looks for LANG and finally falls back to
 *  QLocale::system().name().
 *
 *  The order of precedence is well defined here:
 *  http://opengroup.org/onlinepubs/007908799/xbd/envvar.html
 *
 *  @note This method will return "C" when the requested locale is invalid or
 *  when the "C" locale is set explicitly.
 */
/* static */
QString VBoxGlobal::systemLanguageId()
{
#if defined (Q_WS_MAC)
    /* QLocale return the right id only if the user select the format of the
     * language also. So we use our own implementation */
    return ::darwinSystemLanguage();
#elif defined (Q_OS_UNIX)
    const char *s = RTEnvGet ("LC_ALL");
    if (s == 0)
        s = RTEnvGet ("LC_MESSAGES");
    if (s == 0)
        s = RTEnvGet ("LANG");
    if (s != 0)
        return QLocale (s).name();
#endif
    return  QLocale::system().name();
}

/**
 *  Reimplementation of QFileDialog::getExistingDirectory() that removes some
 *  oddities and limitations.
 *
 *  On Win32, this function makes sure a native dialog is launched in
 *  another thread to avoid dialog visualization errors occuring due to
 *  multi-threaded COM apartment initialization on the main UI thread while
 *  the appropriate native dialog function expects a single-threaded one.
 *
 *  On all other platforms, this function is equivalent to
 *  QFileDialog::getExistingDirectory().
 */
QString VBoxGlobal::getExistingDirectory (const QString &aDir,
                                          QWidget *aParent,
                                          const QString &aCaption,
                                          bool aDirOnly,
                                          bool aResolveSymlinks)
{
#if defined Q_WS_WIN

    /**
     *  QEvent class reimplementation to carry Win32 API native dialog's
     *  result folder information
     */
    class GetExistDirectoryEvent : public OpenNativeDialogEvent
    {
    public:

        enum { TypeId = QEvent::User + 300 };

        GetExistDirectoryEvent (const QString &aResult)
            : OpenNativeDialogEvent (aResult, (QEvent::Type) TypeId) {}
    };

    /**
     *  QThread class reimplementation to open Win32 API native folder's dialog
     */
    class Thread : public QThread
    {
    public:

        Thread (QWidget *aParent, QObject *aTarget,
                const QString &aDir, const QString &aCaption)
            : mParent (aParent), mTarget (aTarget), mDir (aDir), mCaption (aCaption) {}

        virtual void run()
        {
            QString result;

            QWidget *topParent = mParent ? mParent->window() : vboxGlobal().mainWindow();
            QString title = mCaption.isNull() ? tr ("Select a directory") : mCaption;

            TCHAR path[MAX_PATH];
            path [0] = 0;
            TCHAR initPath [MAX_PATH];
            initPath [0] = 0;

            BROWSEINFO bi;
            bi.hwndOwner = topParent ? topParent->winId() : 0;
            bi.pidlRoot = NULL;
            bi.lpszTitle = (TCHAR*)(title.isNull() ? 0 : title.utf16());
            bi.pszDisplayName = initPath;
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT | BIF_NEWDIALOGSTYLE;
            bi.lpfn = winGetExistDirCallbackProc;
            bi.lParam = Q_ULONG (&mDir);

            /* Qt is uncapable to properly handle modal state if the modal
             * window is not a QWidget. For example, if we have the W1->W2->N
             * ownership where Wx are QWidgets (W2 is modal), and N is a
             * native modal window, cliking on the title bar of W1 will still
             * activate W2 and redirect keyboard/mouse to it. The dirty hack
             * to prevent it is to disable the entire widget... */
            if (mParent)
                mParent->setEnabled (false);

            LPITEMIDLIST itemIdList = SHBrowseForFolder (&bi);
            if (itemIdList)
            {
                SHGetPathFromIDList (itemIdList, path);
                IMalloc *pMalloc;
                if (SHGetMalloc (&pMalloc) != NOERROR)
                    result = QString::null;
                else
                {
                    pMalloc->Free (itemIdList);
                    pMalloc->Release();
                    result = QString::fromUtf16 ((ushort*)path);
                }
            }
            else
                result = QString::null;
            QApplication::postEvent (mTarget, new GetExistDirectoryEvent (result));

            /* Enable the parent widget again. */
            if (mParent)
                mParent->setEnabled (true);
        }

    private:

        QWidget *mParent;
        QObject *mTarget;
        QString mDir;
        QString mCaption;
    };

    /* Local event loop to run while waiting for the result from another
     * thread */
    QEventLoop loop;

    QString dir = QDir::toNativeSeparators (aDir);
    LoopObject loopObject ((QEvent::Type) GetExistDirectoryEvent::TypeId, loop);

    Thread openDirThread (aParent, &loopObject, dir, aCaption);
    openDirThread.start();
    loop.exec();
    openDirThread.wait();

    return loopObject.result();

#elif defined (Q_WS_X11) && (QT_VERSION < 0x040400)

    /* Here is workaround for Qt4.3 bug with QFileDialog which crushes when
     * gets initial path as hidden directory if no hidden files are shown.
     * See http://trolltech.com/developer/task-tracker/index_html?method=entry&id=193483
     * for details */
    QFileDialog dlg (aParent);
    dlg.setWindowTitle (aCaption);
    dlg.setDirectory (aDir);
    dlg.setResolveSymlinks (aResolveSymlinks);
    dlg.setFileMode (aDirOnly ? QFileDialog::DirectoryOnly : QFileDialog::Directory);
    QAction *hidden = dlg.findChild <QAction*> ("qt_show_hidden_action");
    if (hidden)
    {
        hidden->trigger();
        hidden->setVisible (false);
    }
    return dlg.exec() ? dlg.selectedFiles() [0] : QString::null;

#else

    QFileDialog::Options o;
    if (aDirOnly)
        o = QFileDialog::ShowDirsOnly;
    if (!aResolveSymlinks)
        o |= QFileDialog::DontResolveSymlinks;
    return QFileDialog::getExistingDirectory (aParent, aCaption, aDir, o);

#endif
}

/**
 *  Reimplementation of QFileDialog::getSaveFileName() that removes some
 *  oddities and limitations.
 *
 *  On Win32, this function makes sure a file filter is applied automatically
 *  right after it is selected from the drop-down list, to conform to common
 *  experience in other applications. Note that currently, @a selectedFilter
 *  is always set to null on return.
 *
 *  On all other platforms, this function is equivalent to
 *  QFileDialog::getSaveFileName().
 */
/* static */
QString VBoxGlobal::getSaveFileName (const QString &aStartWith,
                                     const QString &aFilters,
                                     QWidget       *aParent,
                                     const QString &aCaption,
                                     QString       *aSelectedFilter /* = NULL */,
                                     bool           aResolveSymlinks /* = true */)
{
#if defined Q_WS_WIN

    /**
     *  QEvent class reimplementation to carry Win32 API native dialog's
     *  result folder information
     */
    class GetOpenFileNameEvent : public OpenNativeDialogEvent
    {
    public:

        enum { TypeId = QEvent::User + 301 };

        GetOpenFileNameEvent (const QString &aResult)
            : OpenNativeDialogEvent (aResult, (QEvent::Type) TypeId) {}
    };

    /**
     *  QThread class reimplementation to open Win32 API native file dialog
     */
    class Thread : public QThread
    {
    public:

        Thread (QWidget *aParent, QObject *aTarget,
                const QString &aStartWith, const QString &aFilters,
                const QString &aCaption) :
                mParent (aParent), mTarget (aTarget),
                mStartWith (aStartWith), mFilters (aFilters),
                mCaption (aCaption) {}

        virtual void run()
        {
            QString result;

            QString workDir;
            QString initSel;
            QFileInfo fi (mStartWith);

            if (fi.isDir())
                workDir = mStartWith;
            else
            {
                workDir = fi.absolutePath();
                initSel = fi.fileName();
            }

            workDir = QDir::toNativeSeparators (workDir);
            if (!workDir.endsWith ("\\"))
                workDir += "\\";

            QString title = mCaption.isNull() ? tr ("Select a file") : mCaption;

            QWidget *topParent = mParent ? mParent->window() : vboxGlobal().mainWindow();
            QString winFilters = winFilter (mFilters);
            AssertCompile (sizeof (TCHAR) == sizeof (QChar));
            TCHAR buf [1024];
            if (initSel.length() > 0 && initSel.length() < sizeof (buf))
                memcpy (buf, initSel.isNull() ? 0 : initSel.utf16(),
                        (initSel.length() + 1) * sizeof (TCHAR));
            else
                buf [0] = 0;

            OPENFILENAME ofn;
            memset (&ofn, 0, sizeof (OPENFILENAME));

            ofn.lStructSize = sizeof (OPENFILENAME);
            ofn.hwndOwner = topParent ? topParent->winId() : 0;
            ofn.lpstrFilter = (TCHAR *) winFilters.isNull() ? 0 : winFilters.utf16();
            ofn.lpstrFile = buf;
            ofn.nMaxFile = sizeof (buf) - 1;
            ofn.lpstrInitialDir = (TCHAR *) workDir.isNull() ? 0 : workDir.utf16();
            ofn.lpstrTitle = (TCHAR *) title.isNull() ? 0 : title.utf16();
            ofn.Flags = (OFN_NOCHANGEDIR | OFN_HIDEREADONLY |
                         OFN_EXPLORER | OFN_ENABLEHOOK |
                         OFN_NOTESTFILECREATE);
            ofn.lpfnHook = OFNHookProc;

            if (GetSaveFileName (&ofn))
            {
                result = QString::fromUtf16 ((ushort *) ofn.lpstrFile);
            }

            // qt_win_eatMouseMove();
            MSG msg = {0, 0, 0, 0, 0, 0, 0};
            while (PeekMessage (&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE));
            if (msg.message == WM_MOUSEMOVE)
                PostMessage (msg.hwnd, msg.message, 0, msg.lParam);

            result = result.isEmpty() ? result : QFileInfo (result).absoluteFilePath();

            QApplication::postEvent (mTarget, new GetOpenFileNameEvent (result));
        }

    private:

        QWidget *mParent;
        QObject *mTarget;
        QString mStartWith;
        QString mFilters;
        QString mCaption;
    };

    if (aSelectedFilter)
        *aSelectedFilter = QString::null;

    /* Local event loop to run while waiting for the result from another
     * thread */
    QEventLoop loop;

    QString startWith = QDir::toNativeSeparators (aStartWith);
    LoopObject loopObject ((QEvent::Type) GetOpenFileNameEvent::TypeId, loop);

//#warning check me!
    if (aParent)
        aParent->setWindowModality (Qt::WindowModal);

    Thread openDirThread (aParent, &loopObject, startWith, aFilters, aCaption);
    openDirThread.start();
    loop.exec();
    openDirThread.wait();

//#warning check me!
    if (aParent)
        aParent->setWindowModality (Qt::NonModal);

    return loopObject.result();

#elif defined (Q_WS_X11) && (QT_VERSION < 0x040400)

    /* Here is workaround for Qt4.3 bug with QFileDialog which crushes when
     * gets initial path as hidden directory if no hidden files are shown.
     * See http://trolltech.com/developer/task-tracker/index_html?method=entry&id=193483
     * for details */
    QFileDialog dlg (aParent);
    dlg.setWindowTitle (aCaption);
    dlg.setDirectory (aStartWith);
    dlg.setFilter (aFilters);
    dlg.setFileMode (QFileDialog::QFileDialog::AnyFile);
    dlg.setAcceptMode (QFileDialog::AcceptSave);
    if (aSelectedFilter)
        dlg.selectFilter (*aSelectedFilter);
    dlg.setResolveSymlinks (aResolveSymlinks);
    dlg.setConfirmOverwrite (false);
    QAction *hidden = dlg.findChild <QAction*> ("qt_show_hidden_action");
    if (hidden)
    {
        hidden->trigger();
        hidden->setVisible (false);
    }
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFiles().value (0, "") : QString::null;

#else

    QFileDialog::Options o;
    if (!aResolveSymlinks)
        o |= QFileDialog::DontResolveSymlinks;
    o |= QFileDialog::DontConfirmOverwrite;
    return QFileDialog::getSaveFileName (aParent, aCaption, aStartWith,
                                         aFilters, aSelectedFilter, o);
#endif
}

/**
 *  Reimplementation of QFileDialog::getOpenFileName() that removes some
 *  oddities and limitations.
 *
 *  On Win32, this function makes sure a file filter is applied automatically
 *  right after it is selected from the drop-down list, to conform to common
 *  experience in other applications. Note that currently, @a selectedFilter
 *  is always set to null on return.
 *
 *  On all other platforms, this function is equivalent to
 *  QFileDialog::getOpenFileName().
 */
/* static */
QString VBoxGlobal::getOpenFileName (const QString &aStartWith,
                                     const QString &aFilters,
                                     QWidget       *aParent,
                                     const QString &aCaption,
                                     QString       *aSelectedFilter /* = NULL */,
                                     bool           aResolveSymlinks /* = true */)
{
    return getOpenFileNames (aStartWith,
                             aFilters,
                             aParent,
                             aCaption,
                             aSelectedFilter,
                             aResolveSymlinks,
                             true /* aSingleFile */).value (0, "");
}

/**
 *  Reimplementation of QFileDialog::getOpenFileNames() that removes some
 *  oddities and limitations.
 *
 *  On Win32, this function makes sure a file filter is applied automatically
 *  right after it is selected from the drop-down list, to conform to common
 *  experience in other applications. Note that currently, @a selectedFilter
 *  is always set to null on return.
 *  @todo: implement the multiple file selection on win
 *  @todo: is this extra handling on win still necessary with Qt4?
 *
 *  On all other platforms, this function is equivalent to
 *  QFileDialog::getOpenFileNames().
 */
/* static */
QStringList VBoxGlobal::getOpenFileNames (const QString &aStartWith,
                                          const QString &aFilters,
                                          QWidget       *aParent,
                                          const QString &aCaption,
                                          QString       *aSelectedFilter /* = NULL */,
                                          bool           aResolveSymlinks /* = true */,
                                          bool           aSingleFile /* = false */)
{
#if defined Q_WS_WIN

    /**
     *  QEvent class reimplementation to carry Win32 API native dialog's
     *  result folder information
     */
    class GetOpenFileNameEvent : public OpenNativeDialogEvent
    {
    public:

        enum { TypeId = QEvent::User + 301 };

        GetOpenFileNameEvent (const QString &aResult)
            : OpenNativeDialogEvent (aResult, (QEvent::Type) TypeId) {}
    };

    /**
     *  QThread class reimplementation to open Win32 API native file dialog
     */
    class Thread : public QThread
    {
    public:

        Thread (QWidget *aParent, QObject *aTarget,
                const QString &aStartWith, const QString &aFilters,
                const QString &aCaption) :
                mParent (aParent), mTarget (aTarget),
                mStartWith (aStartWith), mFilters (aFilters),
                mCaption (aCaption) {}

        virtual void run()
        {
            QString result;

            QString workDir;
            QString initSel;
            QFileInfo fi (mStartWith);

            if (fi.isDir())
                workDir = mStartWith;
            else
            {
                workDir = fi.absolutePath();
                initSel = fi.fileName();
            }

            workDir = QDir::toNativeSeparators (workDir);
            if (!workDir.endsWith ("\\"))
                workDir += "\\";

            QString title = mCaption.isNull() ? tr ("Select a file") : mCaption;

            QWidget *topParent = mParent ? mParent->window() : vboxGlobal().mainWindow();
            QString winFilters = winFilter (mFilters);
            AssertCompile (sizeof (TCHAR) == sizeof (QChar));
            TCHAR buf [1024];
            if (initSel.length() > 0 && initSel.length() < sizeof (buf))
                memcpy (buf, initSel.isNull() ? 0 : initSel.utf16(),
                        (initSel.length() + 1) * sizeof (TCHAR));
            else
                buf [0] = 0;

            OPENFILENAME ofn;
            memset (&ofn, 0, sizeof (OPENFILENAME));

            ofn.lStructSize = sizeof (OPENFILENAME);
            ofn.hwndOwner = topParent ? topParent->winId() : 0;
            ofn.lpstrFilter = (TCHAR *) winFilters.isNull() ? 0 : winFilters.utf16();
            ofn.lpstrFile = buf;
            ofn.nMaxFile = sizeof (buf) - 1;
            ofn.lpstrInitialDir = (TCHAR *) workDir.isNull() ? 0 : workDir.utf16();
            ofn.lpstrTitle = (TCHAR *) title.isNull() ? 0 : title.utf16();
            ofn.Flags = (OFN_NOCHANGEDIR | OFN_HIDEREADONLY |
                          OFN_EXPLORER | OFN_ENABLEHOOK |
                          OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
            ofn.lpfnHook = OFNHookProc;

            if (GetOpenFileName (&ofn))
            {
                result = QString::fromUtf16 ((ushort *) ofn.lpstrFile);
            }

            // qt_win_eatMouseMove();
            MSG msg = {0, 0, 0, 0, 0, 0, 0};
            while (PeekMessage (&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE));
            if (msg.message == WM_MOUSEMOVE)
                PostMessage (msg.hwnd, msg.message, 0, msg.lParam);

            result = result.isEmpty() ? result : QFileInfo (result).absoluteFilePath();

            QApplication::postEvent (mTarget, new GetOpenFileNameEvent (result));
        }

    private:

        QWidget *mParent;
        QObject *mTarget;
        QString mStartWith;
        QString mFilters;
        QString mCaption;
    };

    if (aSelectedFilter)
        *aSelectedFilter = QString::null;

    /* Local event loop to run while waiting for the result from another
     * thread */
    QEventLoop loop;

    QString startWith = QDir::toNativeSeparators (aStartWith);
    LoopObject loopObject ((QEvent::Type) GetOpenFileNameEvent::TypeId, loop);

//#warning check me!
    if (aParent)
        aParent->setWindowModality (Qt::WindowModal);

    Thread openDirThread (aParent, &loopObject, startWith, aFilters, aCaption);
    openDirThread.start();
    loop.exec();
    openDirThread.wait();

//#warning check me!
    if (aParent)
        aParent->setWindowModality (Qt::NonModal);

    return QStringList() << loopObject.result();

#elif defined (Q_WS_X11) && (QT_VERSION < 0x040400)

    /* Here is workaround for Qt4.3 bug with QFileDialog which crushes when
     * gets initial path as hidden directory if no hidden files are shown.
     * See http://trolltech.com/developer/task-tracker/index_html?method=entry&id=193483
     * for details */
    QFileDialog dlg (aParent);
    dlg.setWindowTitle (aCaption);
    dlg.setDirectory (aStartWith);
    dlg.setFilter (aFilters);
    if (aSingleFile)
        dlg.setFileMode (QFileDialog::ExistingFile);
    else
        dlg.setFileMode (QFileDialog::ExistingFiles);
    if (aSelectedFilter)
        dlg.selectFilter (*aSelectedFilter);
    dlg.setResolveSymlinks (aResolveSymlinks);
    QAction *hidden = dlg.findChild <QAction*> ("qt_show_hidden_action");
    if (hidden)
    {
        hidden->trigger();
        hidden->setVisible (false);
    }
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFiles() : QStringList() << QString::null;

#else

    QFileDialog::Options o;
    if (!aResolveSymlinks)
        o |= QFileDialog::DontResolveSymlinks;
    if (aSingleFile)
        return QStringList() << QFileDialog::getOpenFileName (aParent, aCaption, aStartWith,
                                                              aFilters, aSelectedFilter, o);
    else
        return QFileDialog::getOpenFileNames (aParent, aCaption, aStartWith,
                                              aFilters, aSelectedFilter, o);
#endif
}

/**
 *  Search for the first directory that exists starting from the passed one
 *  and going up through its parents.  In case if none of the directories
 *  exist (except the root one), the function returns QString::null.
 */
/* static */
QString VBoxGlobal::getFirstExistingDir (const QString &aStartDir)
{
    QString result = QString::null;
    QDir dir (aStartDir);
    while (!dir.exists() && !dir.isRoot())
    {
        QFileInfo dirInfo (dir.absolutePath());
        dir = dirInfo.absolutePath();
    }
    if (dir.exists() && !dir.isRoot())
        result = dir.absolutePath();
    return result;
}

#if defined (Q_WS_X11)

static char *XXGetProperty (Display *aDpy, Window aWnd,
                            Atom aPropType, const char *aPropName)
{
    Atom propNameAtom = XInternAtom (aDpy, aPropName,
                                     True /* only_if_exists */);
    if (propNameAtom == None)
        return NULL;

    Atom actTypeAtom = None;
    int actFmt = 0;
    unsigned long nItems = 0;
    unsigned long nBytesAfter = 0;
    unsigned char *propVal = NULL;
    int rc = XGetWindowProperty (aDpy, aWnd, propNameAtom,
                                 0, LONG_MAX, False /* delete */,
                                 aPropType, &actTypeAtom, &actFmt,
                                 &nItems, &nBytesAfter, &propVal);
    if (rc != Success)
        return NULL;

    return reinterpret_cast <char *> (propVal);
}

static Bool XXSendClientMessage (Display *aDpy, Window aWnd, const char *aMsg,
                                 unsigned long aData0 = 0, unsigned long aData1 = 0,
                                 unsigned long aData2 = 0, unsigned long aData3 = 0,
                                 unsigned long aData4 = 0)
{
    Atom msgAtom = XInternAtom (aDpy, aMsg, True /* only_if_exists */);
    if (msgAtom == None)
        return False;

    XEvent ev;

    ev.xclient.type = ClientMessage;
    ev.xclient.serial = 0;
    ev.xclient.send_event = True;
    ev.xclient.display = aDpy;
    ev.xclient.window = aWnd;
    ev.xclient.message_type = msgAtom;

    /* always send as 32 bit for now */
    ev.xclient.format = 32;
    ev.xclient.data.l [0] = aData0;
    ev.xclient.data.l [1] = aData1;
    ev.xclient.data.l [2] = aData2;
    ev.xclient.data.l [3] = aData3;
    ev.xclient.data.l [4] = aData4;

    return XSendEvent (aDpy, DefaultRootWindow (aDpy), False,
                       SubstructureRedirectMask, &ev) != 0;
}

#endif

/**
 * Activates the specified window. If necessary, the window will be
 * de-iconified activation.
 *
 * @note On X11, it is implied that @a aWid represents a window of the same
 * display the application was started on.
 *
 * @param aWId              Window ID to activate.
 * @param aSwitchDesktop    @c true to switch to the window's desktop before
 *                          activation.
 *
 * @return @c true on success and @c false otherwise.
 */
/* static */
bool VBoxGlobal::activateWindow (WId aWId, bool aSwitchDesktop /* = true */)
{
    bool result = true;

#if defined (Q_WS_WIN32)

    if (IsIconic (aWId))
        result &= !!ShowWindow (aWId, SW_RESTORE);
    else if (!IsWindowVisible (aWId))
        result &= !!ShowWindow (aWId, SW_SHOW);

    result &= !!SetForegroundWindow (aWId);

#elif defined (Q_WS_X11)

    Display *dpy = QX11Info::display();

    if (aSwitchDesktop)
    {
        /* try to find the desktop ID using the NetWM property */
        CARD32 *desktop = (CARD32 *) XXGetProperty (dpy, aWId, XA_CARDINAL,
                                                    "_NET_WM_DESKTOP");
        if (desktop == NULL)
            /* if the NetWM propery is not supported try to find the desktop
             * ID using the GNOME WM property */
            desktop = (CARD32 *) XXGetProperty (dpy, aWId, XA_CARDINAL,
                                                "_WIN_WORKSPACE");

        if (desktop != NULL)
        {
            Bool ok = XXSendClientMessage (dpy, DefaultRootWindow (dpy),
                                           "_NET_CURRENT_DESKTOP",
                                           *desktop);
            if (!ok)
            {
                LogWarningFunc (("Couldn't switch to desktop=%08X\n",
                                 desktop));
                result = false;
            }
            XFree (desktop);
        }
        else
        {
            LogWarningFunc (("Couldn't find a desktop ID for aWId=%08X\n",
                             aWId));
            result = false;
        }
    }

    Bool ok = XXSendClientMessage (dpy, aWId, "_NET_ACTIVE_WINDOW");
    result &= !!ok;

    XRaiseWindow (dpy, aWId);

#else

    NOREF (aSwitchDesktop);
    AssertFailed();
    result = false;

#endif

    if (!result)
        LogWarningFunc (("Couldn't activate aWId=%08X\n", aWId));

    return result;
}

/**
 *  Removes the acceletartor mark (the ampersand symbol) from the given string
 *  and returns the result. The string is supposed to be a menu item's text
 *  that may (or may not) contain the accelerator mark.
 *
 *  In order to support accelerators used in non-alphabet languages
 *  (e.g. Japanese) that has a form of "(&<L>)" (where <L> is a latin letter),
 *  this method first searches for this pattern and, if found, removes it as a
 *  whole. If such a pattern is not found, then the '&' character is simply
 *  removed from the string.
 *
 *  @note This function removes only the first occurense of the accelerator
 *  mark.
 *
 *  @param aText Menu item's text to remove the acceletaror mark from.
 *
 *  @return The resulting string.
 */
/* static */
QString VBoxGlobal::removeAccelMark (const QString &aText)
{
    QString result = aText;

    QRegExp accel ("\\(&[a-zA-Z]\\)");
    int pos = accel.indexIn (result);
    if (pos >= 0)
        result.remove (pos, accel.cap().length());
    else
    {
        pos = result.indexOf ('&');
        if (pos >= 0)
            result.remove (pos, 1);
    }

    return result;
}

/* static */
QString VBoxGlobal::insertKeyToActionText (const QString &aText, const QString &aKey)
{
#ifdef Q_WS_MAC
    QString key ("%1 (Host+%2)");
#else
    QString key ("%1 \tHost+%2");
#endif
    return key.arg (aText).arg (QKeySequence (aKey).toString (QKeySequence::NativeText));
}

/* static */
QString VBoxGlobal::extractKeyFromActionText (const QString &aText)
{
    QString key;
#ifdef Q_WS_MAC
    QRegExp re (".* \\(Host\\+(.+)\\)");
#else
    QRegExp re (".* \\t\\Host\\+(.+)");
#endif
    if (re.exactMatch (aText))
        key = re.cap (1);
    return key;
}

/**
 * Joins two pixmaps horizontally with 2px space between them and returns the
 * result.
 *
 * @param aPM1 Left pixmap.
 * @param aPM2 Right pixmap.
 */
/* static */
QPixmap VBoxGlobal::joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2)
{
    if (aPM1.isNull())
        return aPM2;
    if (aPM2.isNull())
        return aPM1;

    QPixmap result (aPM1.width() + aPM2.width() + 2,
                    qMax (aPM1.height(), aPM2.height()));
    result.fill (Qt::transparent);

    QPainter painter (&result);
    painter.drawPixmap (0, 0, aPM1);
    painter.drawPixmap (aPM1.width() + 2, result.height() - aPM2.height(), aPM2);
    painter.end();

    return result;
}

/**
 *  Searches for a widget that with @a aName (if it is not NULL) which inherits
 *  @a aClassName (if it is not NULL) and among children of @a aParent. If @a
 *  aParent is NULL, all top-level widgets are searched. If @a aRecursive is
 *  true, child widgets are recursively searched as well.
 */
/* static */
QWidget *VBoxGlobal::findWidget (QWidget *aParent, const char *aName,
                                 const char *aClassName /* = NULL */,
                                 bool aRecursive /* = false */)
{
    if (aParent == NULL)
    {
        QWidgetList list = QApplication::topLevelWidgets();
        QWidget* w = NULL;
        foreach(w, list)
        {
            if ((!aName || strcmp (w->objectName().toAscii().constData(), aName) == 0) &&
                (!aClassName || strcmp (w->metaObject()->className(), aClassName) == 0))
                break;
            if (aRecursive)
            {
                w = findWidget (w, aName, aClassName, aRecursive);
                if (w)
                    break;
            }
        }
        return w;
    }

    /* Find the first children of aParent with the appropriate properties.
     * Please note that this call is recursivly. */
    QList<QWidget *> list = qFindChildren<QWidget *> (aParent, aName);
    QWidget *child = NULL;
    foreach(child, list)
    {
        if (!aClassName || strcmp (child->metaObject()->className(), aClassName) == 0)
            break;
    }
    return child;
}

/**
 * Figures out which hard disk formats are currently supported by VirtualBox.
 * Returned is a list of pairs with the form
 *   <tt>{"Backend Name", "*.suffix1 .suffix2 ..."}</tt>.
 */
/* static */
QList <QPair <QString, QString> > VBoxGlobal::HDDBackends()
{
    CSystemProperties systemProperties = vboxGlobal().virtualBox().GetSystemProperties();
    QVector<CHardDiskFormat> hardDiskFormats = systemProperties.GetHardDiskFormats();
    QList< QPair<QString, QString> > backendPropList;
    for (int i = 0; i < hardDiskFormats.size(); ++ i)
    {
        /* File extensions */
        QVector <QString> fileExtensions = hardDiskFormats [i].GetFileExtensions();
        QStringList f;
        for (int a = 0; a < fileExtensions.size(); ++ a)
            f << QString ("*.%1").arg (fileExtensions [a]);
        /* Create a pair out of the backend description and all suffix's. */
        if (!f.isEmpty())
            backendPropList << QPair<QString, QString> (hardDiskFormats [i].GetName(), f.join(" "));
    }
    return backendPropList;
}

// Public slots
////////////////////////////////////////////////////////////////////////////////

/**
 * Opens the specified URL using OS/Desktop capabilities.
 *
 * @param aURL URL to open
 *
 * @return true on success and false otherwise
 */
bool VBoxGlobal::openURL (const QString &aURL)
{
    if (QDesktopServices::openUrl (aURL))
        return true;

    /* If we go here it means we couldn't open the URL */
    vboxProblem().cannotOpenURL (aURL);

    return false;
}

/**
 * Shows the VirtualBox registration dialog.
 *
 * @note that this method is not part of VBoxProblemReporter (like e.g.
 *       VBoxProblemReporter::showHelpAboutDialog()) because it is tied to
 *       VBoxCallback::OnExtraDataChange() handling performed by VBoxGlobal.
 *
 * @param aForce
 */
void VBoxGlobal::showRegistrationDialog (bool aForce)
{
#ifdef VBOX_WITH_REGISTRATION
    if (!aForce && !VBoxRegistrationDlg::hasToBeShown())
        return;

    if (mRegDlg)
    {
        /* Show the already opened registration dialog */
        mRegDlg->setWindowState (mRegDlg->windowState() & ~Qt::WindowMinimized);
        mRegDlg->raise();
        mRegDlg->activateWindow();
    }
    else
    {
        /* Store the ID of the main window to ensure that only one
         * registration dialog is shown at a time. Due to manipulations with
         * OnExtraDataCanChange() and OnExtraDataChange() signals, this extra
         * data item acts like an inter-process mutex, so the first process
         * that attempts to set it will win, the rest will get a failure from
         * the SetExtraData() call. */
        mVBox.SetExtraData (VBoxDefs::GUI_RegistrationDlgWinID,
                            QString ("%1").arg ((qulonglong) mMainWindow->winId()));

        if (mVBox.isOk())
        {
            /* We've got the "mutex", create a new registration dialog */
            VBoxRegistrationDlg *dlg =
                new VBoxRegistrationDlg (&mRegDlg, mMainWindow);
            dlg->setAttribute (Qt::WA_DeleteOnClose);
            Assert (dlg == mRegDlg);
            mRegDlg->show();
        }
    }
#endif
}

/**
 * Shows the VirtualBox version check & update dialog.
 *
 * @note that this method is not part of VBoxProblemReporter (like e.g.
 *       VBoxProblemReporter::showHelpAboutDialog()) because it is tied to
 *       VBoxCallback::OnExtraDataChange() handling performed by VBoxGlobal.
 *
 * @param aForce
 */
void VBoxGlobal::showUpdateDialog (bool aForce)
{
    /* Silently check in one day after current time-stamp */
    QTimer::singleShot (24 /* hours */   * 60   /* minutes */ *
                        60 /* seconds */ * 1000 /* milliseconds */,
                        this, SLOT (perDayNewVersionNotifier()));

    bool isNecessary = VBoxUpdateDlg::isNecessary();

    if (!aForce && !isNecessary)
        return;

    if (mUpdDlg)
    {
        if (!mUpdDlg->isHidden())
        {
            mUpdDlg->setWindowState (mUpdDlg->windowState() & ~Qt::WindowMinimized);
            mUpdDlg->raise();
            mUpdDlg->activateWindow();
        }
    }
    else
    {
        /* Store the ID of the main window to ensure that only one
         * update dialog is shown at a time. Due to manipulations with
         * OnExtraDataCanChange() and OnExtraDataChange() signals, this extra
         * data item acts like an inter-process mutex, so the first process
         * that attempts to set it will win, the rest will get a failure from
         * the SetExtraData() call. */
        mVBox.SetExtraData (VBoxDefs::GUI_UpdateDlgWinID,
                            QString ("%1").arg ((qulonglong) mMainWindow->winId()));

        if (mVBox.isOk())
        {
            /* We've got the "mutex", create a new update dialog */
            VBoxUpdateDlg *dlg = new VBoxUpdateDlg (&mUpdDlg, aForce, 0);
            dlg->setAttribute (Qt::WA_DeleteOnClose);
            Assert (dlg == mUpdDlg);

            /* Update dialog always in background mode for now.
             * if (!aForce && isAutomatic) */
            mUpdDlg->search();
            /* else mUpdDlg->show(); */
        }
    }
}

void VBoxGlobal::perDayNewVersionNotifier()
{
    showUpdateDialog (false /* force show? */);
}

// Protected members
////////////////////////////////////////////////////////////////////////////////

bool VBoxGlobal::event (QEvent *e)
{
    switch (e->type())
    {
        case VBoxDefs::AsyncEventType:
        {
            VBoxAsyncEvent *ev = (VBoxAsyncEvent *) e;
            ev->handle();
            return true;
        }

        case VBoxDefs::MediaEnumEventType:
        {
            VBoxMediaEnumEvent *ev = (VBoxMediaEnumEvent*) e;

            if (!ev->mLast)
            {
                if (ev->mMedium.state() == KMediaState_Inaccessible &&
                    !ev->mMedium.result().isOk())
                    vboxProblem().cannotGetMediaAccessibility (ev->mMedium);
                Assert (ev->mIterator != mMediaList.end());
                *(ev->mIterator) = ev->mMedium;
                emit mediumEnumerated (*ev->mIterator);
                ++ ev->mIterator;
            }
            else
            {
                /* the thread has posted the last message, wait for termination */
                mMediaEnumThread->wait();
                delete mMediaEnumThread;
                mMediaEnumThread = 0;
                emit mediumEnumFinished (mMediaList);
            }

            return true;
        }

        /* VirtualBox callback events */

        case VBoxDefs::MachineStateChangeEventType:
        {
            emit machineStateChanged (*(VBoxMachineStateChangeEvent *) e);
            return true;
        }
        case VBoxDefs::MachineDataChangeEventType:
        {
            emit machineDataChanged (*(VBoxMachineDataChangeEvent *) e);
            return true;
        }
        case VBoxDefs::MachineRegisteredEventType:
        {
            emit machineRegistered (*(VBoxMachineRegisteredEvent *) e);
            return true;
        }
        case VBoxDefs::SessionStateChangeEventType:
        {
            emit sessionStateChanged (*(VBoxSessionStateChangeEvent *) e);
            return true;
        }
        case VBoxDefs::SnapshotEventType:
        {
            emit snapshotChanged (*(VBoxSnapshotEvent *) e);
            return true;
        }
        case VBoxDefs::CanShowRegDlgEventType:
        {
            emit canShowRegDlg (((VBoxCanShowRegDlgEvent *) e)->mCanShow);
            return true;
        }
        case VBoxDefs::CanShowUpdDlgEventType:
        {
            emit canShowUpdDlg (((VBoxCanShowUpdDlgEvent *) e)->mCanShow);
            return true;
        }
        case VBoxDefs::ChangeGUILanguageEventType:
        {
            loadLanguage (static_cast<VBoxChangeGUILanguageEvent*> (e)->mLangId);
            return true;
        }
#ifdef VBOX_GUI_WITH_SYSTRAY
        case VBoxDefs::MainWindowCountChangeEventType:

            emit mainWindowCountChanged (*(VBoxMainWindowCountChangeEvent *) e);
            return true;

        case VBoxDefs::CanShowTrayIconEventType:
        {
            emit trayIconCanShow (*(VBoxCanShowTrayIconEvent *) e);
            return true;
        }
        case VBoxDefs::ShowTrayIconEventType:
        {
            emit trayIconShow (*(VBoxShowTrayIconEvent *) e);
            return true;
        }
        case VBoxDefs::TrayIconChangeEventType:
        {
            emit trayIconChanged (*(VBoxChangeTrayIconEvent *) e);
            return true;
        }
#endif
#if defined(Q_WS_MAC)
        case VBoxDefs::ChangeDockIconUpdateEventType:
        {
            emit dockIconUpdateChanged (*(VBoxChangeDockIconUpdateEvent *) e);
            return true;
        }
#endif
        default:
            break;
    }

    return QObject::event (e);
}

bool VBoxGlobal::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (aEvent->type() == QEvent::LanguageChange &&
        aObject->isWidgetType() &&
        static_cast <QWidget *> (aObject)->isTopLevel())
    {
        /* Catch the language change event before any other widget gets it in
         * order to invalidate cached string resources (like the details view
         * templates) that may be used by other widgets. */
        QWidgetList list = QApplication::topLevelWidgets();
        if (list.first() == aObject)
        {
            /* call this only once per every language change (see
             * QApplication::installTranslator() for details) */
            retranslateUi();
        }
    }

    return QObject::eventFilter (aObject, aEvent);
}

// Private members
////////////////////////////////////////////////////////////////////////////////

void VBoxGlobal::init()
{
#ifdef DEBUG
    mVerString += " [DEBUG]";
#endif

#ifdef Q_WS_WIN
    /* COM for the main thread is initialized in main() */
#else
    HRESULT rc = COMBase::InitializeCOM();
    if (FAILED (rc))
    {
        vboxProblem().cannotInitCOM (rc);
        return;
    }
#endif

    mVBox.createInstance (CLSID_VirtualBox);
    if (!mVBox.isOk())
    {
        vboxProblem().cannotCreateVirtualBox (mVBox);
        return;
    }

    /* create default non-null global settings */
    gset = VBoxGlobalSettings (false);

    /* try to load global settings */
    gset.load (mVBox);
    if (!mVBox.isOk() || !gset)
    {
        vboxProblem().cannotLoadGlobalConfig (mVBox, gset.lastError());
        return;
    }

    /* Load the customized language as early as possible to get possible error
     * messages translated */
    QString languageId = gset.languageId();
    if (!languageId.isNull())
        loadLanguage (languageId);

    retranslateUi();

    /* Note: the settings conversion check must be done before anything else
     * that may unconditionally overwrite settings files in the new format (like
     * SetExtraData()). But after loading the proper the language. */
    if (!checkForAutoConvertedSettings())
        return;

#ifdef VBOX_GUI_WITH_SYSTRAY
    {
        /* Increase open Fe/Qt4 windows reference count. */
        int c = mVBox.GetExtraData (VBoxDefs::GUI_MainWindowCount).toInt() + 1;
        AssertMsgReturnVoid ((c >= 0) || (mVBox.isOk()),
            ("Something went wrong with the window reference count!"));
        mVBox.SetExtraData (VBoxDefs::GUI_MainWindowCount, QString ("%1").arg (c));
        mIncreasedWindowCounter = mVBox.isOk();
        AssertReturnVoid (mIncreasedWindowCounter);
    }
#endif

    /* Initialize guest OS Type list. */
    CGuestOSTypeVector coll = mVBox.GetGuestOSTypes();
    int osTypeCount = coll.size();
    AssertMsg (osTypeCount > 0, ("Number of OS types must not be zero"));
    if (osTypeCount > 0)
    {
        /* Here we assume the 'Other' type is always the first, so we
         * remember it and will append it to the list when finished. */
        CGuestOSType otherType = coll[0];
        QString otherFamilyId (otherType.GetFamilyId());

        /* Fill the lists with all the available OS Types except
         * the 'Other' type, which will be appended. */
        for (int i = 1; i < coll.size(); ++i)
        {
            CGuestOSType os = coll[i];
            QString familyId (os.GetFamilyId());
            if (!mFamilyIDs.contains (familyId))
            {
                mFamilyIDs << familyId;
                mTypes << QList <CGuestOSType> ();
            }
            mTypes [mFamilyIDs.indexOf (familyId)].append (os);
        }

        /* Append the 'Other' OS Type to the end of list. */
        if (!mFamilyIDs.contains (otherFamilyId))
        {
            mFamilyIDs << otherFamilyId;
            mTypes << QList <CGuestOSType> ();
        }
        mTypes [mFamilyIDs.indexOf (otherFamilyId)].append (otherType);
    }

    /* Fill in OS type icon dictionary. */
    static const char *kOSTypeIcons [][2] =
    {
        {"Other",           ":/os_other.png"},
        {"DOS",             ":/os_dos.png"},
        {"Netware",         ":/os_netware.png"},
        {"L4",              ":/os_l4.png"},
        {"Windows31",       ":/os_win31.png"},
        {"Windows95",       ":/os_win95.png"},
        {"Windows98",       ":/os_win98.png"},
        {"WindowsMe",       ":/os_winme.png"},
        {"WindowsNT4",      ":/os_winnt4.png"},
        {"Windows2000",     ":/os_win2k.png"},
        {"WindowsXP",       ":/os_winxp.png"},
        {"WindowsXP_64",    ":/os_winxp_64.png"},
        {"Windows2003",     ":/os_win2k3.png"},
        {"Windows2003_64",  ":/os_win2k3_64.png"},
        {"WindowsVista",    ":/os_winvista.png"},
        {"WindowsVista_64", ":/os_winvista_64.png"},
        {"Windows2008",     ":/os_win2k8.png"},
        {"Windows2008_64",  ":/os_win2k8_64.png"},
        {"Windows7",        ":/os_win7.png"},
        {"Windows7_64",     ":/os_win7_64.png"},
        {"WindowsNT",       ":/os_win_other.png"},
        {"OS2Warp3",        ":/os_os2warp3.png"},
        {"OS2Warp4",        ":/os_os2warp4.png"},
        {"OS2Warp45",       ":/os_os2warp45.png"},
        {"OS2eCS",          ":/os_os2ecs.png"},
        {"OS2",             ":/os_os2_other.png"},
        {"Linux22",         ":/os_linux22.png"},
        {"Linux24",         ":/os_linux24.png"},
        {"Linux24_64",      ":/os_linux24_64.png"},
        {"Linux26",         ":/os_linux26.png"},
        {"Linux26_64",      ":/os_linux26_64.png"},
        {"ArchLinux",       ":/os_archlinux.png"},
        {"ArchLinux_64",    ":/os_archlinux_64.png"},
        {"Debian",          ":/os_debian.png"},
        {"Debian_64",       ":/os_debian_64.png"},
        {"OpenSUSE",        ":/os_opensuse.png"},
        {"OpenSUSE_64",     ":/os_opensuse_64.png"},
        {"Fedora",          ":/os_fedora.png"},
        {"Fedora_64",       ":/os_fedora_64.png"},
        {"Gentoo",          ":/os_gentoo.png"},
        {"Gentoo_64",       ":/os_gentoo_64.png"},
        {"Mandriva",        ":/os_mandriva.png"},
        {"Mandriva_64",     ":/os_mandriva_64.png"},
        {"RedHat",          ":/os_redhat.png"},
        {"RedHat_64",       ":/os_redhat_64.png"},
        {"Ubuntu",          ":/os_ubuntu.png"},
        {"Ubuntu_64",       ":/os_ubuntu_64.png"},
        {"Xandros",         ":/os_xandros.png"},
        {"Xandros_64",      ":/os_xandros_64.png"},
        {"Linux",           ":/os_linux_other.png"},
        {"FreeBSD",         ":/os_freebsd.png"},
        {"FreeBSD_64",      ":/os_freebsd_64.png"},
        {"OpenBSD",         ":/os_openbsd.png"},
        {"OpenBSD_64",      ":/os_openbsd_64.png"},
        {"NetBSD",          ":/os_netbsd.png"},
        {"NetBSD_64",       ":/os_netbsd_64.png"},
        {"Solaris",         ":/os_solaris.png"},
        {"Solaris_64",      ":/os_solaris_64.png"},
        {"OpenSolaris",     ":/os_opensolaris.png"},
        {"OpenSolaris_64",  ":/os_opensolaris_64.png"},
        {"QNX",             ":/os_qnx.png"},
    };
    for (uint n = 0; n < SIZEOF_ARRAY (kOSTypeIcons); ++ n)
    {
        mOsTypeIcons.insert (kOSTypeIcons [n][0],
            new QPixmap (kOSTypeIcons [n][1]));
    }

    /* fill in VM state icon map */
    static const struct
    {
        KMachineState state;
        const char *name;
    }
    kVMStateIcons[] =
    {
        {KMachineState_Null, NULL},
        {KMachineState_PoweredOff, ":/state_powered_off_16px.png"},
        {KMachineState_Saved, ":/state_saved_16px.png"},
        {KMachineState_Aborted, ":/state_aborted_16px.png"},
        {KMachineState_Running, ":/state_running_16px.png"},
        {KMachineState_Paused, ":/state_paused_16px.png"},
        {KMachineState_Stuck, ":/state_stuck_16px.png"},
        {KMachineState_Starting, ":/state_running_16px.png"}, /// @todo (dmik) separate icon?
        {KMachineState_Stopping, ":/state_running_16px.png"}, /// @todo (dmik) separate icon?
        {KMachineState_Saving, ":/state_saving_16px.png"},
        {KMachineState_Restoring, ":/state_restoring_16px.png"},
        {KMachineState_Discarding, ":/state_discarding_16px.png"},
        {KMachineState_SettingUp, ":/settings_16px.png"},
    };
    for (uint n = 0; n < SIZEOF_ARRAY (kVMStateIcons); n ++)
    {
        mVMStateIcons.insert (kVMStateIcons [n].state,
            new QPixmap (kVMStateIcons [n].name));
    }

    /* initialize state colors map */
    mVMStateColors.insert (KMachineState_Null,          new QColor (Qt::red));
    mVMStateColors.insert (KMachineState_PoweredOff,    new QColor (Qt::gray));
    mVMStateColors.insert (KMachineState_Saved,         new QColor (Qt::yellow));
    mVMStateColors.insert (KMachineState_Aborted,       new QColor (Qt::darkRed));
    mVMStateColors.insert (KMachineState_Running,       new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_Paused,        new QColor (Qt::darkGreen));
    mVMStateColors.insert (KMachineState_Stuck,         new QColor (Qt::darkMagenta));
    mVMStateColors.insert (KMachineState_Starting,      new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_Stopping,      new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_Saving,        new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_Restoring,     new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_Discarding,    new QColor (Qt::green));
    mVMStateColors.insert (KMachineState_SettingUp,     new QColor (Qt::green));

    /* online/offline snapshot icons */
    mOfflineSnapshotIcon = QPixmap (":/offline_snapshot_16px.png");
    mOnlineSnapshotIcon = QPixmap (":/online_snapshot_16px.png");

    qApp->installEventFilter (this);

    /* process command line */

    vm_render_mode_str = 0;
#ifdef VBOX_WITH_DEBUGGER_GUI
# ifdef VBOX_WITH_DEBUGGER_GUI_MENU
    mDbgEnabled = true;
# else
    mDbgEnabled = RTEnvExist("VBOX_GUI_DBG_ENABLED");
# endif
    mDbgAutoShow = RTEnvExist("VBOX_GUI_DBG_AUTO_SHOW");
#endif

    int argc = qApp->argc();
    int i = 1;
    while (i < argc)
    {
        const char *arg = qApp->argv() [i];
        if (       !::strcmp (arg, "-startvm"))
        {
            if (++i < argc)
            {
                QString param = QString (qApp->argv() [i]);
                QUuid uuid = QUuid (param);
                if (!uuid.isNull())
                {
                    vmUuid = uuid;
                }
                else
                {
                    CMachine m = mVBox.FindMachine (param);
                    if (m.isNull())
                    {
                        vboxProblem().cannotFindMachineByName (mVBox, param);
                        return;
                    }
                    vmUuid = m.GetId();
                }
            }
        }
#ifdef VBOX_GUI_WITH_SYSTRAY
        else if (!::strcmp (arg, "-systray"))
        {
            mIsTrayMenu = true;
        }
#endif
        else if (!::strcmp (arg, "-comment"))
        {
            ++i;
        }
        else if (!::strcmp (arg, "-rmode"))
        {
            if (++i < argc)
                vm_render_mode_str = qApp->argv() [i];
        }
#ifdef VBOX_WITH_DEBUGGER_GUI
        else if (!::strcmp (arg, "-dbg") || !::strcmp (arg, "--dbg"))
        {
            mDbgEnabled = true;
        }
        else if (!::strcmp( arg, "-debug") || !::strcmp( arg, "--debug"))
        {
            mDbgEnabled = true;
            mDbgAutoShow = true;
        }
        else if (!::strcmp (arg, "-no-debug") || !::strcmp (arg, "--no-debug"))
        {
            mDbgEnabled = false;
            mDbgAutoShow = false;
        }
#endif
        /** @todo add an else { msgbox(syntax error); exit(1); } here, pretty please... */
        i++;
    }

    vm_render_mode = vboxGetRenderMode( vm_render_mode_str );

    /* setup the callback */
    callback = CVirtualBoxCallback (new VBoxCallback (*this));
    mVBox.RegisterCallback (callback);
    AssertWrapperOk (mVBox);
    if (!mVBox.isOk())
        return;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* setup the debugger gui. */
    if (RTEnvExist("VBOX_GUI_NO_DEBUGGER"))
        mDbgEnabled = mDbgAutoShow = false;
    if (mDbgEnabled)
    {
        int rc = SUPR3HardenedLdrLoadAppPriv("VBoxDbg", &mhVBoxDbg);
        if (RT_FAILURE(rc))
        {
            mhVBoxDbg = NIL_RTLDRMOD;
            mDbgAutoShow = false;
            LogRel(("Failed to load VBoxDbg, rc=%Rrc\n", rc));
        }
    }
#endif

    mValid = true;
}

/** @internal
 *
 *  This method should be never called directly. It is called automatically
 *  when the application terminates.
 */
void VBoxGlobal::cleanup()
{
    /* sanity check */
    if (!sVBoxGlobalInCleanup)
    {
        AssertMsgFailed (("Should never be called directly\n"));
        return;
    }

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (mIncreasedWindowCounter)
    {
        /* Decrease open Fe/Qt4 windows reference count. */
        int c = mVBox.GetExtraData (VBoxDefs::GUI_MainWindowCount).toInt() - 1;
        AssertMsg ((c >= 0) || (mVBox.isOk()),
            ("Something went wrong with the window reference count!"));
        if (c < 0)
            c = 0;   /* Clean up the mess. */
        mVBox.SetExtraData (VBoxDefs::GUI_MainWindowCount,
                            (c > 0) ? QString ("%1").arg (c) : NULL);
        AssertWrapperOk (mVBox);
        if (c == 0)
        {
            mVBox.SetExtraData (VBoxDefs::GUI_TrayIconWinID, NULL);
            AssertWrapperOk (mVBox);
        }
    }
#endif

    if (!callback.isNull())
    {
        mVBox.UnregisterCallback (callback);
        AssertWrapperOk (mVBox);
        callback.detach();
    }

    if (mMediaEnumThread)
    {
        /* sVBoxGlobalInCleanup is true here, so just wait for the thread */
        mMediaEnumThread->wait();
        delete mMediaEnumThread;
        mMediaEnumThread = 0;
    }

#ifdef VBOX_WITH_REGISTRATION
    if (mRegDlg)
        mRegDlg->close();
#endif

    if (mConsoleWnd)
        delete mConsoleWnd;
    if (mSelectorWnd)
        delete mSelectorWnd;

    /* ensure CGuestOSType objects are no longer used */
    mFamilyIDs.clear();
    mTypes.clear();

    /* media list contains a lot of CUUnknown, release them */
    mMediaList.clear();
    /* the last step to ensure we don't use COM any more */
    mVBox.detach();

    /* There may be VBoxMediaEnumEvent instances still in the message
     * queue which reference COM objects. Remove them to release those objects
     * before uninitializing the COM subsystem. */
    QApplication::removePostedEvents (this);

#ifdef Q_WS_WIN
    /* COM for the main thread is shutdown in main() */
#else
    COMBase::CleanupCOM();
#endif

    mValid = false;
}

/** @fn vboxGlobal
 *
 *  Shortcut to the static VBoxGlobal::instance() method, for convenience.
 */


/**
 *  USB Popup Menu class methods
 *  This class provides the list of USB devices attached to the host.
 */
VBoxUSBMenu::VBoxUSBMenu (QWidget *aParent) : QMenu (aParent)
{
    connect (this, SIGNAL (aboutToShow()),
             this, SLOT   (processAboutToShow()));
//    connect (this, SIGNAL (hovered (QAction *)),
//             this, SLOT   (processHighlighted (QAction *)));
}

const CUSBDevice& VBoxUSBMenu::getUSB (QAction *aAction)
{
    return mUSBDevicesMap [aAction];
}

void VBoxUSBMenu::setConsole (const CConsole &aConsole)
{
    mConsole = aConsole;
}

void VBoxUSBMenu::processAboutToShow()
{
    clear();
    mUSBDevicesMap.clear();

    CHost host = vboxGlobal().virtualBox().GetHost();

    bool isUSBEmpty = host.GetUSBDevices().GetCount() == 0;
    if (isUSBEmpty)
    {
        QAction *action = addAction (tr ("<no available devices>", "USB devices"));
        action->setEnabled (false);
        action->setToolTip (tr ("No supported devices connected to the host PC",
                                "USB device tooltip"));
    }
    else
    {
        CHostUSBDeviceEnumerator en = host.GetUSBDevices().Enumerate();
        while (en.HasMore())
        {
            CHostUSBDevice dev = en.GetNext();
            CUSBDevice usb (dev);
            QAction *action = addAction (vboxGlobal().details (usb));
            action->setCheckable (true);
            mUSBDevicesMap [action] = usb;
            /* check if created item was alread attached to this session */
            if (!mConsole.isNull())
            {
                CUSBDevice attachedUSB =
                    mConsole.FindUSBDeviceById (usb.GetId());
                action->setChecked (!attachedUSB.isNull());
                action->setEnabled (dev.GetState() !=
                                    KUSBDeviceState_Unavailable);
            }
        }
    }
}

bool VBoxUSBMenu::event(QEvent *aEvent)
{
    /* We provide dynamic tooltips for the usb devices */
    if (aEvent->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *> (aEvent);
        QAction *action = actionAt (helpEvent->pos());
        if (action)
        {
            CUSBDevice usb = mUSBDevicesMap [action];
            if (!usb.isNull())
            {
                QToolTip::showText (helpEvent->globalPos(), vboxGlobal().toolTip (usb));
                return true;
            }
        }
    }
    return QMenu::event (aEvent);
}

/**
 *  Enable/Disable Menu class.
 *  This class provides enable/disable menu items.
 */
VBoxSwitchMenu::VBoxSwitchMenu (QWidget *aParent, QAction *aAction,
                                bool aInverted)
    : QMenu (aParent), mAction (aAction), mInverted (aInverted)
{
    /* this menu works only with toggle action */
    Assert (aAction->isCheckable());
    addAction(aAction);
    connect (this, SIGNAL (aboutToShow()),
             this, SLOT   (processAboutToShow()));
}

void VBoxSwitchMenu::setToolTip (const QString &aTip)
{
    mAction->setToolTip (aTip);
}

void VBoxSwitchMenu::processAboutToShow()
{
    QString text = mAction->isChecked() ^ mInverted ? tr ("Disable") : tr ("Enable");
    mAction->setText (text);
}

