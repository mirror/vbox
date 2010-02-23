/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISession stuff implementation
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

/* Global inclues */
#include <QApplication>
#include <QWidget>

/* Local includes */
#include "UISession.h"

#include "UIMachine.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"

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

    UIMouseCapabilityChangeEvent(bool bSupportsAbsolute, bool bNeedsHostCursor)
        : QEvent((QEvent::Type)UIConsoleEventType_MouseCapabilityChange)
        , m_bSupportsAbsolute(bSupportsAbsolute), m_bNeedsHostCursor(bNeedsHostCursor) {}

    bool supportsAbsolute() const { return m_bSupportsAbsolute; }
    bool needsHostCursor() const { return m_bNeedsHostCursor; }

private:

    bool m_bSupportsAbsolute;
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

    const CNetworkAdapter &m_networkAdapter;
};

/* Serial port change event: */
class UISerialPortChangeEvent : public QEvent
{
public:

    UISerialPortChangeEvent(const CSerialPort &serialPort)
        : QEvent((QEvent::Type)UIConsoleEventType_SerialPortChange)
        , m_serialPort(serialPort) {}

    const CSerialPort& serialPort() { return m_serialPort; }

private:

    const CSerialPort &m_serialPort;
};

/* Parallel port change event: */
class UIParallelPortChangeEvent : public QEvent
{
public:

    UIParallelPortChangeEvent(const CParallelPort &parallelPort)
        : QEvent((QEvent::Type)UIConsoleEventType_ParallelPortChange)
        , m_parallelPort(parallelPort) {}

    const CParallelPort& parallelPort() { return m_parallelPort; }

private:

    const CParallelPort &m_parallelPort;
};

/* Storage controller change event: */
class UIStorageControllerChangeEvent : public QEvent
{
public:

    UIStorageControllerChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_StorageControllerChange) {}
};

/* Storage medium change event: */
class UIMediumChangeEvent : public QEvent
{
public:

    UIMediumChangeEvent(const CMediumAttachment &mediumAttachment)
        : QEvent((QEvent::Type)UIConsoleEventType_MediumChange)
        , m_mediumAttachment(mediumAttachment) {}
    const CMediumAttachment& mediumAttahment() { return m_mediumAttachment; }

private:

    const CMediumAttachment &m_mediumAttachment;
};

/* CPU change event: */
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

/* VRDP server change event: */
class UIVRDPServerChangeEvent : public QEvent
{
public:

    UIVRDPServerChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_VRDPServerChange) {}
};

/* Remote display info change event: */
class UIRemoteDisplayInfoChangeEvent : public QEvent
{
public:

    UIRemoteDisplayInfoChangeEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_RemoteDisplayInfoChange) {}
};

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

    const CUSBDevice &m_device;
    bool m_bAttached;
    const CVirtualBoxErrorInfo &m_error;
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
class UICanUIShowWindowEvent : public QEvent
{
public:

    UICanUIShowWindowEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_CanShowWindow) {}
};

/* Show window event: */
class UIShowWindowEvent : public QEvent
{
public:

    UIShowWindowEvent()
        : QEvent((QEvent::Type)UIConsoleEventType_ShowWindow) {}
};

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

    STDMETHOD(OnMouseCapabilityChange)(BOOL bSupportsAbsolute, BOOL bNeedHostCursor)
    {
        QApplication::postEvent(m_pEventHandler, new UIMouseCapabilityChangeEvent(bSupportsAbsolute, bNeedHostCursor));
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

    STDMETHOD(OnSerialPortChange)(ISerialPort *pSerialPort)
    {
        QApplication::postEvent(m_pEventHandler, new UISerialPortChangeEvent(CSerialPort(pSerialPort)));
        return S_OK;
    }

    STDMETHOD(OnParallelPortChange)(IParallelPort *pParallelPort)
    {
        QApplication::postEvent(m_pEventHandler, new UIParallelPortChangeEvent(CParallelPort(pParallelPort)));
        return S_OK;
    }

    STDMETHOD(OnStorageControllerChange)()
    {
        QApplication::postEvent(m_pEventHandler, new UIStorageControllerChangeEvent);
        return S_OK;
    }

    STDMETHOD(OnMediumChange)(IMediumAttachment *pMediumAttachment)
    {
        QApplication::postEvent(m_pEventHandler, new UIMediumChangeEvent(CMediumAttachment(pMediumAttachment)));
        return S_OK;
    }

    STDMETHOD(OnCPUChange)(ULONG uCPU, BOOL bRemove)
    {
        QApplication::postEvent(m_pEventHandler, new UICPUChangeEvent(uCPU, bRemove));
        return S_OK;
    }

    STDMETHOD(OnVRDPServerChange)()
    {
        QApplication::postEvent(m_pEventHandler, new UIVRDPServerChangeEvent);
        return S_OK;
    }

    STDMETHOD(OnRemoteDisplayInfoChange)()
    {
        QApplication::postEvent(m_pEventHandler, new UIRemoteDisplayInfoChangeEvent);
        return S_OK;
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

#if defined (Q_WS_MAC)
        /* Let's try the simple approach first - grab the focus.
         * Getting a window out of the dock (minimized or whatever it's called)
         * needs to be done on the GUI thread, so post it a note: */
        *puWinId = 0;
        if (!m_pEventHandler)
            return S_OK;

        ProcessSerialNumber psn = { 0, kCurrentProcess };
        OSErr rc = ::SetFrontProcess(&psn);
        if (!rc)
            QApplication::postEvent(m_pEventHandler, new UIShowWindowEvent);
        else
        {
            /* It failed for some reason, send the other process our PSN so it can try.
             * (This is just a precaution should Mac OS X start imposing the same sensible
             * focus stealing restrictions that other window managers implement). */
            AssertMsgFailed(("SetFrontProcess -> %#x\n", rc));
            if (::GetCurrentProcess(&psn))
                *puWinId = RT_MAKE_U64(psn.lowLongOfPSN, psn.highLongOfPSN);
        }
#else
        /* Return the ID of the top-level console window. */
        *puWinId = m_pEventHandler->winId();
#endif

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

UISession::UISession(UIMachine *pMachine, const CSession &session)
    : QObject(pMachine)
    , m_pMachine(pMachine)
    , m_session(session)
    , m_pCallback(new UIConsoleCallback(this))
    , m_callback(CConsoleCallback(m_pCallback))
{
    /* Check CSession object */
    AssertMsg(!m_session.isNull(), ("CSession is not set!\n"));

    /* Register console callback: */
    m_session.GetConsole().RegisterCallback(m_callback);
}

UISession::~UISession()
{
    /* Unregister console callback: */
    m_session.GetConsole().UnregisterCallback(m_callback);
    delete m_pCallback;
    m_pCallback = 0;
}

bool UISession::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case UIConsoleEventType_MousePointerShapeChange:
        {
#if 0 // TODO: Move to machine view!
            MousePointerChangeEvent *me = (MousePointerChangeEvent*)pEvent;
            /* Change cursor shape only when mouse integration is
             * supported (change mouse shape type event may arrive after
             * mouse capability change that disables integration. */
            if (m_bIsMouseAbsolute)
                setPointerShape (me);
            else
                /* Note: actually we should still remember the requested
                 * cursor shape.  If we can't do that, at least remember
                 * the requested visiblilty. */
                mHideHostPointer = !me->isVisible();
#endif
            UIMousePointerShapeChangeEvent *pConsoleEvent = static_cast<UIMousePointerShapeChangeEvent*>(pEvent);
            emit sigMousePointerShapeChange(pConsoleEvent->isVisible(), pConsoleEvent->hasAlpha(),
                                            pConsoleEvent->xHot(), pConsoleEvent->yHot(),
                                            pConsoleEvent->width(), pConsoleEvent->height(),
                                            pConsoleEvent->shapeData());
            return true;
        }

        case UIConsoleEventType_MouseCapabilityChange:
        {
#if 0 // TODO: Move to machine view!
            MouseCapabilityEvent *me = (MouseCapabilityEvent*)pEvent;
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
#endif
            UIMouseCapabilityChangeEvent *pConsoleEvent = static_cast<UIMouseCapabilityChangeEvent*>(pEvent);
            emit sigMouseCapabilityChange(pConsoleEvent->supportsAbsolute(), pConsoleEvent->needsHostCursor());
            return true;
        }

        case UIConsoleEventType_KeyboardLedsChange:
        {
#if 0 // TODO: Move to machine view!
            ModifierKeyChangeEvent *me = (ModifierKeyChangeEvent* )pEvent;
            if (me->numLock() != mNumLock)
                muNumLockAdaptionCnt = 2;
            if (me->capsLock() != mCapsLock)
                muCapsLockAdaptionCnt = 2;
            mNumLock    = me->numLock();
            mCapsLock   = me->capsLock();
            mScrollLock = me->scrollLock();
#endif
            UIKeyboardLedsChangeEvent *pConsoleEvent = static_cast<UIKeyboardLedsChangeEvent*>(pEvent);
            emit sigKeyboardLedsChange(pConsoleEvent->numLock(), pConsoleEvent->capsLock(), pConsoleEvent->scrollLock());
            return true;
        }

        case UIConsoleEventType_StateChange:
        {
#if 0 // TODO: Move to machine view!
            MachineUIStateChangeEvent *me = (MachineUIStateChangeEvent *) pEvent;
            LogFlowFunc (("MachineUIStateChangeEventType: state=%d\n",
                           me->machineState()));
            onStateChange (me->machineState());
            emit machineStateChanged (me->machineState());
#endif
            UIStateChangeEvent *pConsoleEvent = static_cast<UIStateChangeEvent*>(pEvent);
            emit sigStateChange(pConsoleEvent->machineState());
            return true;
        }

        case UIConsoleEventType_AdditionsStateChange:
        {
#if 0 // TODO: Move to machine view!
            GuestAdditionsChangeEvent *ge = (GuestAdditionsChangeEvent *) pEvent;
            LogFlowFunc (("UIAdditionsStateChangeEventType\n"));
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
#endif
            emit sigAdditionsStateChange();
            return true;
        }

        case UIConsoleEventType_NetworkAdapterChange:
        {
#if 0 // TODO: Move to machine view!
            /* no specific adapter information stored in this
             * event is currently used */
            emit networkStateChange();
#endif
            UINetworkAdapterChangeEvent *pConsoleEvent = static_cast<UINetworkAdapterChangeEvent*>(pEvent);
            emit sigNetworkAdapterChange(pConsoleEvent->networkAdapter());
            return true;
        }

        case UIConsoleEventType_SerialPortChange:
        {
            UISerialPortChangeEvent *pConsoleEvent = static_cast<UISerialPortChangeEvent*>(pEvent);
            emit sigSerialPortChange(pConsoleEvent->serialPort());
            return true;
        }

        case UIConsoleEventType_ParallelPortChange:
        {
            UIParallelPortChangeEvent *pConsoleEvent = static_cast<UIParallelPortChangeEvent*>(pEvent);
            emit sigParallelPortChange(pConsoleEvent->parallelPort());
            return true;
        }

        case UIConsoleEventType_StorageControllerChange:
        {
            emit sigStorageControllerChange();
            return true;
        }

        case UIConsoleEventType_MediumChange:
        {
#if 0 // TODO: Move to machine view!
            MediaDriveChangeEvent *mce = (MediaDriveChangeEvent *) pEvent;
            LogFlowFunc (("MediaChangeEvent\n"));

            emit mediaDriveChanged (mce->type());
#endif
            UIMediumChangeEvent *pConsoleEvent = static_cast<UIMediumChangeEvent*>(pEvent);
            emit sigMediumChange(pConsoleEvent->mediumAttahment());
            return true;
        }

        case UIConsoleEventType_CPUChange:
        {
            UICPUChangeEvent *pConsoleEvent = static_cast<UICPUChangeEvent*>(pEvent);
            emit sigCPUChange(pConsoleEvent->cpu(), pConsoleEvent->remove());
            return true;
        }

        case UIConsoleEventType_VRDPServerChange:
        {
            emit sigVRDPServerChange();
            return true;
        }

        case UIConsoleEventType_RemoteDisplayInfoChange:
        {
            emit sigRemoteDisplayInfoChange();
            return true;
        }

        case UIConsoleEventType_USBControllerChange:
        {
            emit sigUSBControllerChange();
            return true;
        }

        case UIConsoleEventType_USBDeviceStateChange:
        {
#if 0 // TODO: Move to machine view!
            UIUSBDeviceUIStateChangeEvent *ue = (UIUSBDeviceUIStateChangeEvent *)pEvent;

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
#endif
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
#if 0 // TODO: Move to machine view!
            UIRuntimeErrorEvent *pConsoleEvent = (UIRuntimeErrorEvent *) pEvent;
            vboxProblem().showRuntimeError(m_console, ee->fatal(), ee->errorID(), ee->message());
#endif
            UIRuntimeErrorEvent *pConsoleEvent = static_cast<UIRuntimeErrorEvent*>(pEvent);
            emit sigRuntimeError(pConsoleEvent->fatal(), pConsoleEvent->errorID(), pConsoleEvent->message());
            return true;
        }

#ifdef Q_WS_MAC
        /* posted OnShowWindow */
        case UIConsoleEventType_ShowWindow:
        {
            /* Dunno what Qt3 thinks a window that has minimized to the dock
             * should be - it is not hidden, neither is it minimized. OTOH it is
             * marked shown and visible, but not activated. This latter isn't of
             * much help though, since at this point nothing is marked activated.
             * I might have overlooked something, but I'm buggered what if I know
             * what. So, I'll just always show & activate the stupid window to
             * make it get out of the dock when the user wishes to show a VM. */
            window()->show();
            window()->activateWindow();
            return true;
        }
#endif

        default:
            break;
    }
    return QObject::event(pEvent);
}

qulonglong UISession::winId() const
{
    return machine()->machineLogic()->machineWindowWrapper()->machineWindow()->winId();
}
