/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISession stuff implementation
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

/* Global inclues */
#include <QApplication>
#include <QWidget>
#include <QTimer>

/* Local includes */
#include "UISession.h"
#include "UIMachine.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineMenuBar.h"
#include "VBoxProblemReporter.h"
#include "UIFirstRunWzd.h"
#ifdef VBOX_WITH_VIDEOHWACCEL
# include "VBoxFBOverlay.h"
# include "UIFrameBuffer.h"
#endif

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# ifndef VBOX_WITHOUT_XCURSOR
#  include <X11/Xcursor/Xcursor.h>
# endif
#endif

#if defined (Q_WS_MAC)
# include "VBoxUtils.h"
#endif

/* Guest mouse pointer shape change event: */
class UIMousePointerShapeChangeEvent : public QEvent
{
public:

    UIMousePointerShapeChangeEvent(bool bIsVisible, bool bIsAlpha, uint uXHot, uint uYHot, uint uWidth, uint uHeight, const uchar *pShape)
        : QEvent((QEvent::Type)UIConsoleEventType_MousePointerShapeChange)
        , m_bIsVisible(bIsVisible), m_bIsAlpha(bIsAlpha), m_uXHot(uXHot), m_uYHot(uYHot), m_uWidth(uWidth), m_uHeight(uHeight), m_pData(0)
    {
        uint dataSize = ((((m_uWidth + 7) / 8 * m_uHeight) + 3) & ~3) + m_uWidth * 4 * m_uHeight;
        if (pShape)
        {
            m_pData = new uchar[dataSize];
            memcpy((void*)m_pData, (void*)pShape, dataSize);
        }
    }

    virtual ~UIMousePointerShapeChangeEvent()
    {
        if (m_pData) delete[] m_pData;
    }

    bool isVisible() const { return m_bIsVisible; }
    bool hasAlpha() const { return m_bIsAlpha; }
    uint xHot() const { return m_uXHot; }
    uint yHot() const { return m_uYHot; }
    uint width() const { return m_uWidth; }
    uint height() const { return m_uHeight; }
    const uchar *shapeData() const { return m_pData; }

private:

    bool m_bIsVisible, m_bIsAlpha;
    uint m_uXHot, m_uYHot, m_uWidth, m_uHeight;
    const uchar *m_pData;
};

/* Guest mouse absolute positioning capability change event: */
class UIMouseCapabilityChangeEvent : public QEvent
{
public:

    UIMouseCapabilityChangeEvent(bool bSupportsAbsolute, bool bSupportsRelative, bool bNeedsHostCursor)
        : QEvent((QEvent::Type)UIConsoleEventType_MouseCapabilityChange)
        , m_bSupportsAbsolute(bSupportsAbsolute), m_bSupportsRelative(bSupportsRelative), m_bNeedsHostCursor(bNeedsHostCursor) {}

    bool supportsAbsolute() const { return m_bSupportsAbsolute; }
    bool supportsRelative() const { return m_bSupportsRelative; }
    bool needsHostCursor() const { return m_bNeedsHostCursor; }

private:

    bool m_bSupportsAbsolute;
    bool m_bSupportsRelative;
    bool m_bNeedsHostCursor;
};

/* Keyboard LEDs change event: */
class UIKeyboardLedsChangeEvent : public QEvent
{
public:

    UIKeyboardLedsChangeEvent(bool bNumLock, bool bCapsLock, bool bScrollLock)
        : QEvent((QEvent::Type)UIConsoleEventType_KeyboardLedsChange)
        , m_bNumLock(bNumLock), m_bCapsLock(bCapsLock), m_bScrollLock(bScrollLock) {}

    bool numLock() const { return m_bNumLock; }
    bool capsLock() const { return m_bCapsLock; }
    bool scrollLock() const { return m_bScrollLock; }

private:

    bool m_bNumLock;
    bool m_bCapsLock;
    bool m_bScrollLock;
};

/* Machine state change event: */
class UIStateChangeEvent : public QEvent
{
public:

    UIStateChangeEvent(KMachineState machineState)
        : QEvent((QEvent::Type)UIConsoleEventType_StateChange)
        , m_machineState(machineState) {}

    KMachineState machineState() const { return m_machineState; }

private:

    KMachineState m_machineState;
};

/* Guest Additions state change event: */
class UIAdditionsStateChangeEvent : public QEvent
{
public:

    UIAdditionsStateChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_AdditionsStateChange) {}
};

/* Network adapter change event: */
class UINetworkAdapterChangeEvent : public QEvent
{
public:

    UINetworkAdapterChangeEvent(const CNetworkAdapter &networkAdapter)
        : QEvent((QEvent::Type)UIConsoleEventType_NetworkAdapterChange)
        , m_networkAdapter(networkAdapter) {}

    const CNetworkAdapter& networkAdapter() { return m_networkAdapter; }

private:

    const CNetworkAdapter m_networkAdapter;
};

/* Serial port change event: */
/* Not used:
class UISerialPortChangeEvent : public QEvent
{
public:

    UISerialPortChangeEvent(const CSerialPort &serialPort)
        : QEvent((QEvent::Type)UIConsoleEventType_SerialPortChange)
        , m_serialPort(serialPort) {}

    const CSerialPort& serialPort() { return m_serialPort; }

private:

    const CSerialPort m_serialPort;
};
*/

/* Parallel port change event: */
/* Not used:
class UIParallelPortChangeEvent : public QEvent
{
public:

    UIParallelPortChangeEvent(const CParallelPort &parallelPort)
        : QEvent((QEvent::Type)UIConsoleEventType_ParallelPortChange)
        , m_parallelPort(parallelPort) {}

    const CParallelPort& parallelPort() { return m_parallelPort; }

private:

    const CParallelPort m_parallelPort;
};
*/

/* Storage controller change event: */
/* Not used:
class UIStorageControllerChangeEvent : public QEvent
{
public:

    UIStorageControllerChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_StorageControllerChange) {}
};
*/

/* Storage medium change event: */
class UIMediumChangeEvent : public QEvent
{
public:

    UIMediumChangeEvent(const CMediumAttachment &mediumAttachment)
        : QEvent((QEvent::Type)UIConsoleEventType_MediumChange)
        , m_mediumAttachment(mediumAttachment) {}
    const CMediumAttachment& mediumAttachment() { return m_mediumAttachment; }

private:

    const CMediumAttachment m_mediumAttachment;
};

/* CPU change event: */
/* Not used:
class UICPUChangeEvent : public QEvent
{
public:

    UICPUChangeEvent(ulong uCPU, bool bRemove)
        : QEvent((QEvent::Type)UIConsoleEventType_CPUChange)
        , m_uCPU(uCPU), m_bRemove(bRemove) {}

    ulong cpu() const { return m_uCPU; }
    bool remove() const { return m_bRemove; }

private:

    ulong m_uCPU;
    bool m_bRemove;
};
*/

/* VRDP server change event: */
/* Not used:
class UIVRDPServerChangeEvent : public QEvent
{
public:

    UIVRDPServerChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_VRDPServerChange) {}
};
*/

/* Remote display info change event: */
/* Not used:
class UIRemoteDisplayInfoChangeEvent : public QEvent
{
public:

    UIRemoteDisplayInfoChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_RemoteDisplayInfoChange) {}
};
*/

/* USB controller change event: */
class UIUSBControllerChangeEvent : public QEvent
{
public:

    UIUSBControllerChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_USBControllerChange) {}
};

/* USB device state change event: */
class UIUSBDeviceUIStateChangeEvent : public QEvent
{
public:

    UIUSBDeviceUIStateChangeEvent(const CUSBDevice &device, bool bAttached, const CVirtualBoxErrorInfo &error)
        : QEvent((QEvent::Type)UIConsoleEventType_USBDeviceStateChange)
        , m_device(device), m_bAttached(bAttached), m_error(error) {}

    const CUSBDevice& device() const { return m_device; }
    bool attached() const { return m_bAttached; }
    const CVirtualBoxErrorInfo& error() const { return m_error; }

private:

    const CUSBDevice m_device;
    bool m_bAttached;
    const CVirtualBoxErrorInfo m_error;
};

/* Shared folder change event: */
class UISharedFolderChangeEvent : public QEvent
{
public:

    UISharedFolderChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_SharedFolderChange) {}
};

/* VM Runtime error event: */
class UIRuntimeErrorEvent : public QEvent
{
public:

    UIRuntimeErrorEvent(bool bFatal, const QString &strErrorID, const QString &strMessage)
        : QEvent((QEvent::Type)UIConsoleEventType_RuntimeError)
        , m_bFatal(bFatal), m_strErrorID(strErrorID), m_strMessage(strMessage) {}

    bool fatal() const { return m_bFatal; }
    QString errorID() const { return m_strErrorID; }
    QString message() const { return m_strMessage; }

private:

    bool m_bFatal;
    QString m_strErrorID;
    QString m_strMessage;
};

/* Can show window event: */
/* Not used:
class UICanUIShowWindowEvent : public QEvent
{
public:

    UICanUIShowWindowEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_CanShowWindow) {}
};
*/

/* Show window event: */
#ifdef Q_WS_MAC
class UIShowWindowEvent : public QEvent
{
public:

    UIShowWindowEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_ShowWindow) {}
};
#endif /* Q_WS_MAC */

class UIConsoleCallback : VBOX_SCRIPTABLE_IMPL(IConsoleCallback)
{
public:

    UIConsoleCallback(UISession *pEventHandler)
        : m_pEventHandler(pEventHandler)
#if defined (Q_WS_WIN)
        , m_iRefCount(0)
#endif
    {
    }

    virtual ~UIConsoleCallback()
    {
    }

    NS_DECL_ISUPPORTS

#if defined (Q_WS_WIN)
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&m_iRefCount);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long iCount = ::InterlockedDecrement(&m_iRefCount);
        if (iCount == 0)
            delete this;
        return iCount;
    }
#endif

    VBOX_SCRIPTABLE_DISPATCH_IMPL(IConsoleCallback)

    STDMETHOD(OnMousePointerShapeChange)(BOOL bIsVisible, BOOL bAlpha, ULONG uXHot, ULONG uYHot, ULONG uWidth, ULONG uHeight, BYTE *pShape)
    {
        QApplication::postEvent(m_pEventHandler, new UIMousePointerShapeChangeEvent(bIsVisible, bAlpha, uXHot, uYHot, uWidth, uHeight, pShape));
        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange)(BOOL bSupportsAbsolute, BOOL bSupportsRelative, BOOL bNeedHostCursor)
    {
        QApplication::postEvent(m_pEventHandler, new UIMouseCapabilityChangeEvent(bSupportsAbsolute, bSupportsRelative, bNeedHostCursor));
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL bNumLock, BOOL bCapsLock, BOOL bScrollLock)
    {
        QApplication::postEvent(m_pEventHandler, new UIKeyboardLedsChangeEvent(bNumLock, bCapsLock, bScrollLock));
        return S_OK;
    }

    STDMETHOD(OnStateChange)(MachineState_T machineState)
    {
        QApplication::postEvent(m_pEventHandler, new UIStateChangeEvent((KMachineState)machineState));
        return S_OK;
    }

    STDMETHOD(OnAdditionsStateChange)()
    {
        QApplication::postEvent(m_pEventHandler, new UIAdditionsStateChangeEvent);
        return S_OK;
    }

    STDMETHOD(OnNetworkAdapterChange)(INetworkAdapter *pNetworkAdapter)
    {
        QApplication::postEvent(m_pEventHandler, new UINetworkAdapterChangeEvent(CNetworkAdapter(pNetworkAdapter)));
        return S_OK;
    }

    STDMETHOD(OnSerialPortChange)(ISerialPort * /* pSerialPort */)
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UISerialPortChangeEvent(CSerialPort(pSerialPort))); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnParallelPortChange)(IParallelPort * /* pParallelPort */)
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UIParallelPortChangeEvent(CParallelPort(pParallelPort))); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnStorageControllerChange)()
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UIStorageControllerChangeEvent); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnMediumChange)(IMediumAttachment *pMediumAttachment)
    {
        QApplication::postEvent(m_pEventHandler, new UIMediumChangeEvent(CMediumAttachment(pMediumAttachment)));
        return S_OK;
    }

    STDMETHOD(OnCPUChange)(ULONG /* uCPU */, BOOL /* bRemove */)
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UICPUChangeEvent(uCPU, bRemove)); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnVRDPServerChange)()
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UIVRDPServerChangeEvent); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnRemoteDisplayInfoChange)()
    {
        /* Not used: QApplication::postEvent(m_pEventHandler, new UIRemoteDisplayInfoChangeEvent); */
        return VBOX_E_DONT_CALL_AGAIN;
    }

    STDMETHOD(OnUSBControllerChange)()
    {
        QApplication::postEvent(m_pEventHandler, new UIUSBControllerChangeEvent);
        return S_OK;
    }

    STDMETHOD(OnUSBDeviceStateChange)(IUSBDevice *pDevice, BOOL bAttached, IVirtualBoxErrorInfo *pError)
    {
        QApplication::postEvent(m_pEventHandler, new UIUSBDeviceUIStateChangeEvent(CUSBDevice(pDevice), bAttached, CVirtualBoxErrorInfo(pError)));
        return S_OK;
    }

    STDMETHOD(OnSharedFolderChange)(Scope_T scope)
    {
        NOREF(scope);
        QApplication::postEvent(m_pEventHandler, new UISharedFolderChangeEvent);
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL bFatal, IN_BSTR strId, IN_BSTR strMessage)
    {
        QApplication::postEvent(m_pEventHandler, new UIRuntimeErrorEvent(bFatal, QString::fromUtf16(strId), QString::fromUtf16(strMessage)));
        return S_OK;
    }

    STDMETHOD(OnCanShowWindow)(BOOL *pbCanShow)
    {
        if (!pbCanShow)
            return E_POINTER;

        *pbCanShow = TRUE;
        return S_OK;
    }

    STDMETHOD(OnShowWindow)(ULONG64 *puWinId)
    {
        if (!puWinId)
            return E_POINTER;

#ifdef Q_WS_MAC
        /* Let's try the simple approach first - grab the focus.
         * Getting a window out of the dock (minimized or whatever it's called)
         * needs to be done on the GUI thread, so post it a note: */
        *puWinId = 0;
        if (!m_pEventHandler)
            return S_OK;

        if (::darwinSetFrontMostProcess())
            QApplication::postEvent(m_pEventHandler, new UIShowWindowEvent);
        else
        {
            /* It failed for some reason, send the other process our PSN so it can try.
             * (This is just a precaution should Mac OS X start imposing the same sensible
             * focus stealing restrictions that other window managers implement). */
            *puWinId = ::darwinGetCurrentProcessId();
        }
#else /* Q_WS_MAC */
        /* Return the ID of the top-level console window. */
        *puWinId = (ULONG64)m_pEventHandler->winId();
#endif /* !Q_WS_MAC */

        return S_OK;
    }

private:

    UISession *m_pEventHandler;

#if defined (Q_WS_WIN)
    long m_iRefCount;
#endif
};

#if !defined (Q_WS_WIN)
NS_DECL_CLASSINFO(UIConsoleCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(UIConsoleCallback, IConsoleCallback)
#endif

UISession::UISession(UIMachine *pMachine, CSession &sessionReference)
    : QObject(pMachine)
    /* Base variables: */
    , m_pMachine(pMachine)
    , m_session(sessionReference)
    , m_callback(CConsoleCallback(new UIConsoleCallback(this)))
    /* Common variables: */
    , m_pMenuPool(0)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_FrameBufferVector(sessionReference.GetMachine().GetMonitorCount())
#endif
    , m_machineState(KMachineState_Null)
#if defined(Q_WS_WIN)
    , m_alphaCursor(0)
#endif
    /* Common flags: */
    , m_fIsFirstTimeStarted(false)
    , m_fIsIgnoreRuntimeMediumsChanging(false)
    , m_fIsGuestResizeIgnored(false)
    , m_fIsSeamlessModeRequested(false)
    /* Guest additions flags: */
    , m_fIsGuestAdditionsActive(false)
    , m_fIsGuestSupportsGraphics(false)
    , m_fIsGuestSupportsSeamless(false)
    /* Mouse flags: */
    , m_fNumLock(false)
    , m_fCapsLock(false)
    , m_fScrollLock(false)
    , m_uNumLockAdaptionCnt(2)
    , m_uCapsLockAdaptionCnt(2)
    /* Mouse flags: */
    , m_fIsMouseSupportsAbsolute(false)
    , m_fIsMouseSupportsRelative(false)
    , m_fIsMouseHostCursorNeeded(false)
    , m_fIsMouseCaptured(false)
    , m_fIsMouseIntegrated(true)
    , m_fIsValidPointerShapePresent(false)
    , m_fIsHidingHostPointer(true)
{
    /* Register console callback: */
    session().GetConsole().RegisterCallback(m_callback);

    /* Prepare main menu: */
    prepareMenuPool();

    /* Load uisession settings: */
    loadSessionSettings();
}

UISession::~UISession()
{
    /* Save uisession settings: */
    saveSessionSettings();

    /* Cleanup main menu: */
    cleanupMenuPool();

    /* Unregister console callback: */
    session().GetConsole().UnregisterCallback(m_callback);

#if defined(Q_WS_WIN)
    /* Destroy alpha cursor: */
    if (m_alphaCursor)
        DestroyIcon(m_alphaCursor);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
    for (int i = m_FrameBufferVector.size() - 1; i >= 0; --i)
    {
        UIFrameBuffer *pFb = m_FrameBufferVector[i];
        if (pFb)
        {
            /* Warn framebuffer about its no more necessary: */
            pFb->setDeleted(true);
            /* Detach framebuffer from Display: */
            CDisplay display = session().GetConsole().GetDisplay();
            display.SetFramebuffer(i, CFramebuffer(NULL));
            /* Release the reference: */
            pFb->Release();
        }
    }
#endif
}

void UISession::powerUp()
{
    /* Do nothing if we had started already: */
    if (isRunning() || isPaused())
        return;

    /* Prepare powerup: */
    preparePowerUp();

    /* Get current machine/console: */
    CMachine machine = session().GetMachine();
    CConsole console = session().GetConsole();

    /* Power UP machine: */
    CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled() ?
                         console.PowerUpPaused() : console.PowerUp();

    /* Check for immediate failure: */
    if (!console.isOk())
    {
        if (vboxGlobal().showStartVMErrors())
            vboxProblem().cannotStartMachine(console);
        QTimer::singleShot(0, this, SLOT(sltCloseVirtualSession()));
        return;
    }

    /* Guard progressbar warnings from auto-closing: */
    if (uimachine()->machineLogic())
        uimachine()->machineLogic()->setPreventAutoClose(true);

    /* Show "Starting/Restoring" progress dialog: */
    if (isSaved())
        vboxProblem().showModalProgressDialog(progress, machine.GetName(), mainMachineWindow(), 0);
    else
        vboxProblem().showModalProgressDialog(progress, machine.GetName(), mainMachineWindow());

    /* Allow further auto-closing: */
    if (uimachine()->machineLogic())
        uimachine()->machineLogic()->setPreventAutoClose(false);

    /* Check for a progress failure: */
    if (progress.GetResultCode() != 0)
    {
        if (vboxGlobal().showStartVMErrors())
            vboxProblem().cannotStartMachine(progress);
        QTimer::singleShot(0, this, SLOT(sltCloseVirtualSession()));
        return;
    }

    /* Check if we missed a really quick termination after successful startup, and process it if we did: */
    if (isTurnedOff())
    {
        QTimer::singleShot(0, this, SLOT(sltCloseVirtualSession()));
        return;
    }

    /* Check if the required virtualization features are active. We get this
     * info only when the session is active. */
    bool fIs64BitsGuest = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetIs64Bit();
    bool fRecommendVirtEx = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetRecommendedVirtEx();
    AssertMsg(!fIs64BitsGuest || fRecommendVirtEx, ("Virtualization support missed for 64bit guest!\n"));
    bool fIsVirtEnabled = console.GetDebugger().GetHWVirtExEnabled();
    if (fRecommendVirtEx && !fIsVirtEnabled)
    {
        bool fShouldWeClose;

        bool fVTxAMDVSupported = vboxGlobal().virtualBox().GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx);

        QApplication::processEvents();
        setPause(true);

        if (fIs64BitsGuest)
            fShouldWeClose = vboxProblem().warnAboutVirtNotEnabled64BitsGuest(fVTxAMDVSupported);
        else
            fShouldWeClose = vboxProblem().warnAboutVirtNotEnabledGuestRequired(fVTxAMDVSupported);

        if (fShouldWeClose)
        {
            /* At this point the console is powered up. So we have to close
             * this session again. */
            CProgress progress = console.PowerDown();
            if (console.isOk())
            {
                /* Guard progressbar warnings from auto-closing: */
                if (uimachine()->machineLogic())
                    uimachine()->machineLogic()->setPreventAutoClose(true);
                /* Show the power down progress dialog */
                vboxProblem().showModalProgressDialog(progress, machine.GetName(), mainMachineWindow());
                if (progress.GetResultCode() != 0)
                    vboxProblem().cannotStopMachine(progress);
                /* Allow further auto-closing: */
                if (uimachine()->machineLogic())
                    uimachine()->machineLogic()->setPreventAutoClose(false);
            }
            else
                vboxProblem().cannotStopMachine(console);
            /* Now signal the destruction of the rest. */
            QTimer::singleShot(0, this, SLOT(sltCloseVirtualSession()));
            return;
        }
        else
            setPause(false);
    }

#if 0 // TODO: Rework debugger logic!
# ifdef VBOX_WITH_DEBUGGER_GUI
    /* Open the debugger in "full screen" mode requested by the user. */
    else if (vboxGlobal().isDebuggerAutoShowEnabled())
    {
        /* console in upper left corner of the desktop. */
        QRect rct (0, 0, 0, 0);
        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop)
            rct = desktop->availableGeometry(pos());
        move (QPoint (rct.x(), rct.y()));

        if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled())
            sltShowDebugStatistics();
        if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled())
            sltShowDebugCommandLine();

        if (!vboxGlobal().isStartPausedEnabled())
            machineWindowWrapper()->machineView()->pause (false);
    }
# endif
#endif

    /* Warn listeners about machine was started: */
    emit sigMachineStarted();
}

UIActionsPool* UISession::actionsPool() const
{
    return m_pMachine->actionsPool();
}

QWidget* UISession::mainMachineWindow() const
{
    return uimachine()->machineLogic()->mainMachineWindow()->machineWindow();
}

UIMachineLogic* UISession::machineLogic() const
{
    return uimachine()->machineLogic();
}

QMenu* UISession::newMenu(UIMainMenuType fOptions /* = UIMainMenuType_ALL */)
{
    /* Create new menu: */
    QMenu *pMenu = m_pMenuPool->createMenu(actionsPool(), fOptions);

    /* Re-init menu pool for the case menu were recreated: */
    reinitMenuPool();

    /* Return newly created menu: */
    return pMenu;
}

QMenuBar* UISession::newMenuBar(UIMainMenuType fOptions /* = UIMainMenuType_ALL */)
{
    /* Create new menubar: */
    QMenuBar *pMenuBar = m_pMenuPool->createMenuBar(actionsPool(), fOptions);

    /* Re-init menu pool for the case menu were recreated: */
    reinitMenuPool();

    /* Return newly created menubar: */
    return pMenuBar;
}

bool UISession::setPause(bool fOn)
{
    /* Commenting it out as isPaused() could reflect
     * quite obsolete state due to synchronization: */
    //if (isPaused() == fOn)
    //    return true;

    CConsole console = session().GetConsole();

    if (fOn)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (fOn)
            vboxProblem().cannotPauseMachine(console);
        else
            vboxProblem().cannotResumeMachine(console);
    }

    return ok;
}

void UISession::sltInstallGuestAdditionsFrom(const QString &strSource)
{
    CMachine machine = session().GetMachine();
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strUuid;

    CMedium image = vbox.FindDVDImage(strSource);
    if (image.isNull())
    {
        image = vbox.OpenDVDImage(strSource, strUuid);
        if (vbox.isOk())
            strUuid = image.GetId();
    }
    else
        strUuid = image.GetId();

    if (!vbox.isOk())
    {
        vboxProblem().cannotOpenMedium(0, vbox, VBoxDefs::MediumType_DVD, strSource);
        return;
    }

    AssertMsg(!strUuid.isNull(), ("Guest Additions image UUID should be valid!\n"));

    QString strCntName;
    LONG iCntPort = -1, iCntDevice = -1;
    /* Searching for the first suitable slot */
    {
        CStorageControllerVector controllers = machine.GetStorageControllers();
        int i = 0;
        while (i < controllers.size() && strCntName.isNull())
        {
            CStorageController controller = controllers[i];
            CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
            int j = 0;
            while (j < attachments.size() && strCntName.isNull())
            {
                CMediumAttachment attachment = attachments[j];
                if (attachment.GetType() == KDeviceType_DVD)
                {
                    strCntName = controller.GetName();
                    iCntPort = attachment.GetPort();
                    iCntDevice = attachment.GetDevice();
                }
                ++ j;
            }
            ++ i;
        }
    }

    if (!strCntName.isNull())
    {
        bool fIsMounted = false;

        /* Mount medium to the predefined port/device */
        machine.MountMedium(strCntName, iCntPort, iCntDevice, strUuid, false /* force */);
        if (machine.isOk())
            fIsMounted = true;
        else
        {
            /* Ask for force mounting */
            if (vboxProblem().cannotRemountMedium(0, machine, VBoxMedium(image, VBoxDefs::MediumType_DVD),
                                                  true /* mount? */, true /* retry? */) == QIMessageBox::Ok)
            {
                /* Force mount medium to the predefined port/device */
                machine.MountMedium(strCntName, iCntPort, iCntDevice, strUuid, true /* force */);
                if (machine.isOk())
                    fIsMounted = true;
                else
                    vboxProblem().cannotRemountMedium(0, machine, VBoxMedium(image, VBoxDefs::MediumType_DVD),
                                                      true /* mount? */, false /* retry? */);
            }
        }
    }
    else
        vboxProblem().cannotMountGuestAdditions(machine.GetName());
}

void UISession::sltCloseVirtualSession()
{
    m_pMachine->closeVirtualMachine();
}

bool UISession::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case UIConsoleEventType_MousePointerShapeChange:
        {
            /* Convert to mouse shape change event: */
            UIMousePointerShapeChangeEvent *pConsoleEvent = static_cast<UIMousePointerShapeChangeEvent*>(pEvent);

            /* In case of shape data is present: */
            if (pConsoleEvent->shapeData())
            {
                /* We are ignoring visibility flag: */
                m_fIsHidingHostPointer = false;

                /* And updating current cursor shape: */
                setPointerShape(pConsoleEvent->shapeData(), pConsoleEvent->hasAlpha(),
                                pConsoleEvent->xHot(), pConsoleEvent->yHot(),
                                pConsoleEvent->width(), pConsoleEvent->height());
            }
            /* In case of shape data is NOT present: */
            else
            {
                /* Remember if we should hide the cursor: */
                m_fIsHidingHostPointer = !pConsoleEvent->isVisible();
            }

            /* Notify listeners about mouse capability changed: */
            emit sigMousePointerShapeChange();

            /* Accept event: */
            pEvent->accept();
            return true;
        }

        case UIConsoleEventType_MouseCapabilityChange:
        {
            /* Convert to mouse capability event: */
            UIMouseCapabilityChangeEvent *pConsoleEvent = static_cast<UIMouseCapabilityChangeEvent*>(pEvent);

            /* Check if something had changed: */
            if (m_fIsMouseSupportsAbsolute != pConsoleEvent->supportsAbsolute() ||
                m_fIsMouseSupportsRelative != pConsoleEvent->supportsRelative() ||
                m_fIsMouseHostCursorNeeded != pConsoleEvent->needsHostCursor())
            {
                /* Store new data: */
                m_fIsMouseSupportsAbsolute = pConsoleEvent->supportsAbsolute();
                m_fIsMouseSupportsRelative = pConsoleEvent->supportsRelative();
                m_fIsMouseHostCursorNeeded = pConsoleEvent->needsHostCursor();

                /* Notify listeners about mouse capability changed: */
                emit sigMouseCapabilityChange();
            }

            /* Accept event: */
            pEvent->accept();
            return true;
        }

        case UIConsoleEventType_KeyboardLedsChange:
        {
            /* Convert to keyboard LEDs change event: */
            UIKeyboardLedsChangeEvent *pConsoleEvent = static_cast<UIKeyboardLedsChangeEvent*>(pEvent);

            /* Check if something had changed: */
            if (m_fNumLock != pConsoleEvent->numLock() ||
                m_fCapsLock != pConsoleEvent->capsLock() ||
                m_fScrollLock != pConsoleEvent->scrollLock())
            {
                /* Store new num lock data: */
                if (m_fNumLock != pConsoleEvent->numLock())
                {
                    m_fNumLock = pConsoleEvent->numLock();
                    m_uNumLockAdaptionCnt = 2;
                }

                /* Store new caps lock data: */
                if (m_fCapsLock != pConsoleEvent->capsLock())
                {
                    m_fCapsLock = pConsoleEvent->capsLock();
                    m_uCapsLockAdaptionCnt = 2;
                }

                /* Store new scroll lock data: */
                if (m_fScrollLock != pConsoleEvent->scrollLock())
                {
                    m_fScrollLock = pConsoleEvent->scrollLock();
                }

                /* Notify listeners about mouse capability changed: */
                emit sigKeyboardLedsChange();
            }

            /* Accept event: */
            pEvent->accept();
            return true;
        }

        case UIConsoleEventType_StateChange:
        {
            /* Convert to machine state event: */
            UIStateChangeEvent *pConsoleEvent = static_cast<UIStateChangeEvent*>(pEvent);

            /* Check if something had changed: */
            if (m_machineState != pConsoleEvent->machineState())
            {
                /* Store new data: */
                m_machineState = pConsoleEvent->machineState();

                /* Notify listeners about machine state changed: */
                emit sigMachineStateChange();
            }

            /* Accept event: */
            pEvent->accept();
            return true;
        }

        case UIConsoleEventType_AdditionsStateChange:
        {
            /* Get our guest: */
            CGuest guest = session().GetConsole().GetGuest();

            /* Variable flags: */
            bool fIsGuestAdditionsActive = guest.GetAdditionsActive();
            bool fIsGuestSupportsGraphics = guest.GetSupportsGraphics();
            bool fIsGuestSupportsSeamless = guest.GetSupportsSeamless();

            /* Check if something had changed: */
            if (m_fIsGuestAdditionsActive != fIsGuestAdditionsActive ||
                m_fIsGuestSupportsGraphics != fIsGuestSupportsGraphics ||
                m_fIsGuestSupportsSeamless != fIsGuestSupportsSeamless)
            {
                /* Store new data: */
                m_fIsGuestAdditionsActive = fIsGuestAdditionsActive;
                m_fIsGuestSupportsGraphics = fIsGuestSupportsGraphics;
                m_fIsGuestSupportsSeamless = fIsGuestSupportsSeamless;

                /* Notify listeners about guest additions state changed: */
                emit sigAdditionsStateChange();
            }

            /* Accept event: */
            pEvent->accept();
            return true;
        }

        case UIConsoleEventType_NetworkAdapterChange:
        {
            UINetworkAdapterChangeEvent *pConsoleEvent = static_cast<UINetworkAdapterChangeEvent*>(pEvent);
            emit sigNetworkAdapterChange(pConsoleEvent->networkAdapter());
            return true;
        }

        /* Not used:
        case UIConsoleEventType_SerialPortChange:
        {
            UISerialPortChangeEvent *pConsoleEvent = static_cast<UISerialPortChangeEvent*>(pEvent);
            emit sigSerialPortChange(pConsoleEvent->serialPort());
            return true;
        }
        */

        /* Not used:
        case UIConsoleEventType_ParallelPortChange:
        {
            UIParallelPortChangeEvent *pConsoleEvent = static_cast<UIParallelPortChangeEvent*>(pEvent);
            emit sigParallelPortChange(pConsoleEvent->parallelPort());
            return true;
        }
        */

        /* Not used:
        case UIConsoleEventType_StorageControllerChange:
        {
            emit sigStorageControllerChange();
            return true;
        }
        */

        case UIConsoleEventType_MediumChange:
        {
            UIMediumChangeEvent *pConsoleEvent = static_cast<UIMediumChangeEvent*>(pEvent);
            emit sigMediumChange(pConsoleEvent->mediumAttachment());
            return true;
        }

        /* Not used:
        case UIConsoleEventType_CPUChange:
        {
            UICPUChangeEvent *pConsoleEvent = static_cast<UICPUChangeEvent*>(pEvent);
            emit sigCPUChange(pConsoleEvent->cpu(), pConsoleEvent->remove());
            return true;
        }
        */

        /* Not used:
        case UIConsoleEventType_VRDPServerChange:
        {
            emit sigVRDPServerChange();
            return true;
        }
        */

        /* Not used:
        case UIConsoleEventType_RemoteDisplayInfoChange:
        {
            emit sigRemoteDisplayInfoChange();
            return true;
        }
        */

        case UIConsoleEventType_USBControllerChange:
        {
            emit sigUSBControllerChange();
            return true;
        }

        case UIConsoleEventType_USBDeviceStateChange:
        {
            UIUSBDeviceUIStateChangeEvent *pConsoleEvent = static_cast<UIUSBDeviceUIStateChangeEvent*>(pEvent);
            emit sigUSBDeviceStateChange(pConsoleEvent->device(), pConsoleEvent->attached(), pConsoleEvent->error());
            return true;
        }

        case UIConsoleEventType_SharedFolderChange:
        {
            emit sigSharedFolderChange();
            return true;
        }

        case UIConsoleEventType_RuntimeError:
        {
            UIRuntimeErrorEvent *pConsoleEvent = static_cast<UIRuntimeErrorEvent*>(pEvent);
            emit sigRuntimeError(pConsoleEvent->fatal(), pConsoleEvent->errorID(), pConsoleEvent->message());
            return true;
        }

#ifdef Q_WS_MAC
        case UIConsoleEventType_ShowWindow:
        {
            emit sigShowWindows();
            /* Accept event: */
            pEvent->accept();
            return true;
        }
#endif

        default:
            break;
    }
    return QObject::event(pEvent);
}

void UISession::prepareMenuPool()
{
    m_pMenuPool = new UIMachineMenuBar;
}

void UISession::loadSessionSettings()
{
   /* Get uisession machine: */
    CMachine machine = session().GetConsole().GetMachine();

    /* Availability settings: */
    {
        /* VRDP Stuff: */
        CVRDPServer vrdpServer = machine.GetVRDPServer();
        if (vrdpServer.isNull())
        {
            /* Hide VRDP Action: */
            uimachine()->actionsPool()->action(UIActionIndex_Toggle_VRDP)->setVisible(false);
        }
    }

    /* Load extra-data settings: */
    {
        /* Temporary: */
        QString strSettings;

        /* Is there shoul be First RUN Wizard? */
        strSettings = machine.GetExtraData(VBoxDefs::GUI_FirstRun);
        if (strSettings == "yes")
            m_fIsFirstTimeStarted = true;

        /* Ignore mediums mounted at runtime? */
        strSettings = machine.GetExtraData(VBoxDefs::GUI_SaveMountedAtRuntime);
        if (strSettings == "no")
            m_fIsIgnoreRuntimeMediumsChanging = true;

        /* Should guest autoresize? */
        strSettings = machine.GetExtraData(VBoxDefs::GUI_AutoresizeGuest);
        QAction *pGuestAutoresizeSwitch = uimachine()->actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize);
        pGuestAutoresizeSwitch->blockSignals(true);
        pGuestAutoresizeSwitch->setChecked(strSettings != "off");
        pGuestAutoresizeSwitch->blockSignals(false);
    }
}

void UISession::saveSessionSettings()
{
    /* Get uisession machine: */
    CMachine machine = session().GetConsole().GetMachine();

    /* Save extra-data settings: */
    {
        /* Disable First RUN Wizard for the since now: */
        machine.SetExtraData(VBoxDefs::GUI_FirstRun, QString());

        /* Remember if guest should autoresize: */
        machine.SetExtraData(VBoxDefs::GUI_AutoresizeGuest,
                             uimachine()->actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->isChecked() ?
                             QString() : "off");

        // TODO: Move to fullscreen/seamless logic:
        //machine.SetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide, mMiniToolBar->isAutoHide() ? "on" : "off");
    }
}

void UISession::cleanupMenuPool()
{
    delete m_pMenuPool;
    m_pMenuPool = 0;
}

WId UISession::winId() const
{
    return mainMachineWindow()->winId();
}

void UISession::setPointerShape(const uchar *pShapeData, bool fHasAlpha,
                                uint uXHot, uint uYHot, uint uWidth, uint uHeight)
{
    AssertMsg(pShapeData, ("Shape data must not be NULL!\n"));

    m_fIsValidPointerShapePresent = false;
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
            /* Set the new cursor: */
            m_cursor = QCursor(hAlphaCursor);
            if (m_alphaCursor)
                DestroyIcon(m_alphaCursor);
            m_alphaCursor = hAlphaCursor;
            m_fIsValidPointerShapePresent = true;
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

        /* Set the new cursor: */
        m_cursor = QCursor(XcursorImageLoadCursor(QX11Info::display(), img));
        m_fIsValidPointerShapePresent = true;

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
    m_cursor = QCursor(QPixmap::fromImage(image), uXHot, uYHot);
    m_fIsValidPointerShapePresent = true;
    NOREF(srcShapePtrScan);

#else

# warning "port me"

#endif
}

void UISession::reinitMenuPool()
{
    /* Get uisession machine: */
    const CMachine &machine = session().GetConsole().GetMachine();

    /* Availability settings: */
    {
        /* USB Stuff: */
        const CUSBController &usbController = machine.GetUSBController();
        if (   usbController.isNull()
            || !usbController.GetEnabled()
            || !usbController.GetProxyAvailable())
        {
            /* Hide USB menu if controller is NULL or no USB proxy available: */
            uimachine()->actionsPool()->action(UIActionIndex_Menu_USBDevices)->setVisible(false);
        }
        else
        {
            /* Enable/Disable USB menu depending on USB controller: */
            uimachine()->actionsPool()->action(UIActionIndex_Menu_USBDevices)->setEnabled(usbController.GetEnabled());
        }
    }

    /* Prepare some initial settings: */
    {
        /* Initialize CD/FD menus: */
        int iDevicesCountCD = 0;
        int iDevicesCountFD = 0;
        const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
        foreach (const CMediumAttachment &attachment, attachments)
        {
            if (attachment.GetType() == KDeviceType_DVD)
                ++ iDevicesCountCD;
            if (attachment.GetType() == KDeviceType_Floppy)
                ++ iDevicesCountFD;
        }
        QAction *pOpticalDevicesMenu = uimachine()->actionsPool()->action(UIActionIndex_Menu_OpticalDevices);
        QAction *pFloppyDevicesMenu = uimachine()->actionsPool()->action(UIActionIndex_Menu_FloppyDevices);
        pOpticalDevicesMenu->setData(iDevicesCountCD);
        pOpticalDevicesMenu->setVisible(iDevicesCountCD);
        pFloppyDevicesMenu->setData(iDevicesCountFD);
        pFloppyDevicesMenu->setVisible(iDevicesCountFD);
    }
}

void UISession::preparePowerUp()
{
#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Check for updates if necessary: */
    vboxGlobal().showUpdateDialog(false /* force request? */);
#endif

    /* Notify user about mouse&keyboard auto-capturing: */
    if (vboxGlobal().settings().autoCapture())
        vboxProblem().remindAboutAutoCapture();

    /* Shows first run wizard if necessary: */
    if (isFirstTimeStarted())
    {
        UIFirstRunWzd wzd(mainMachineWindow(), session().GetMachine());
        wzd.exec();
    }
}

#ifdef VBOX_WITH_VIDEOHWACCEL
UIFrameBuffer* UISession::frameBuffer(ulong screenId) const
{
    Assert(screenId < (ulong)m_FrameBufferVector.size());
    return m_FrameBufferVector.value((int)screenId, NULL);
}

int UISession::setFrameBuffer(ulong screenId, UIFrameBuffer* pFrameBuffer)
{
    Assert(screenId < (ulong)m_FrameBufferVector.size());
    if (screenId < (ulong)m_FrameBufferVector.size())
    {
        m_FrameBufferVector[(int)screenId] = pFrameBuffer;
        return VINF_SUCCESS;
    }
    return VERR_INVALID_PARAMETER;
}
#endif
