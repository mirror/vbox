/* $Id$ */
/** @file
 * VBox Qt GUI - UIMainEventListener class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QMutex>
# include <QThread>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIMainEventListener.h"

/* COM includes: */
# include "COMEnums.h"
# include "CCanShowWindowEvent.h"
# include "CCursorPositionChangedEvent.h"
# include "CEvent.h"
# include "CEventSource.h"
# include "CEventListener.h"
# include "CExtraDataCanChangeEvent.h"
# include "CExtraDataChangedEvent.h"
# include "CGuestMonitorChangedEvent.h"
# include "CGuestProcessIOEvent.h"
# include "CGuestProcessRegisteredEvent.h"
# include "CGuestProcessStateChangedEvent.h"
# include "CGuestSessionRegisteredEvent.h"
# include "CGuestSessionStateChangedEvent.h"
# include "CKeyboardLedsChangedEvent.h"
# include "CMachineDataChangedEvent.h"
# include "CMachineStateChangedEvent.h"
# include "CMachineRegisteredEvent.h"
# include "CMediumChangedEvent.h"
# include "CMouseCapabilityChangedEvent.h"
# include "CMousePointerShapeChangedEvent.h"
# include "CNetworkAdapterChangedEvent.h"
# include "CProgressPercentageChangedEvent.h"
# include "CProgressTaskCompletedEvent.h"
# include "CRuntimeErrorEvent.h"
# include "CSessionStateChangedEvent.h"
# include "CShowWindowEvent.h"
# include "CSnapshotChangedEvent.h"
# include "CSnapshotDeletedEvent.h"
# include "CSnapshotRestoredEvent.h"
# include "CSnapshotTakenEvent.h"
# include "CStateChangedEvent.h"
# include "CStorageDeviceChangedEvent.h"
# include "CUSBDevice.h"
# include "CUSBDeviceStateChangedEvent.h"
# include "CVBoxSVCAvailabilityChangedEvent.h"
# include "CVirtualBoxErrorInfo.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Private QThread extension allowing to listen for Main events in separate thread.
  * This thread listens for a Main events infinitely unless creator calls for #setShutdown. */
class UIMainEventListeningThread : public QThread
{
    Q_OBJECT;

public:

    /** Constructs Main events listener thread redirecting events from @a comSource to @a comListener. */
    UIMainEventListeningThread(const CEventSource &comSource, const CEventListener &comListener);
    /** Destructs Main events listener thread. */
    ~UIMainEventListeningThread();

protected:

    /** Contains the thread excution body. */
    virtual void run() /* override */;

    /** Returns whether the thread asked to shutdown prematurely. */
    bool isShutdown() const;
    /** Defines whether the thread asked to @a fShutdown prematurely. */
    void setShutdown(bool fShutdown);

private:

    /** Holds the Main event source reference. */
    CEventSource m_comSource;
    /** Holds the Main event listener reference. */
    CEventListener m_comListener;

    /** Holds the mutex instance which protects thread access. */
    mutable QMutex m_mutex;
    /** Holds whether the thread asked to shutdown prematurely. */
    bool m_fShutdown;
};


/*********************************************************************************************************************************
*   Class UIMainEventListeningThread implementation.                                                                             *
*********************************************************************************************************************************/

UIMainEventListeningThread::UIMainEventListeningThread(const CEventSource &comSource, const CEventListener &comListener)
    : m_comSource(comSource)
    , m_comListener(comListener)
    , m_fShutdown(false)
{
}

UIMainEventListeningThread::~UIMainEventListeningThread()
{
    /* Make a request to shutdown: */
    setShutdown(true);

    /* And wait 30 seconds for run() to finish: */
    wait(30000);
}

void UIMainEventListeningThread::run()
{
    /* Initialize COM: */
    COMBase::InitializeCOM(false);

    /* Copy source wrapper to this thread: */
    CEventSource comSource = m_comSource;
    /* Copy listener wrapper to this thread: */
    CEventListener comListener = m_comListener;

    /* While we are not in shutdown: */
    while (!isShutdown())
    {
        /* Fetch the event from the queue: */
        CEvent comEvent = comSource.GetEvent(comListener, 500);
        if (!comEvent.isNull())
        {
            /* Process the event and tell the listener: */
            comListener.HandleEvent(comEvent);
            if (comEvent.GetWaitable())
                comSource.EventProcessed(comListener, comEvent);
        }
    }

    /* Cleanup COM: */
    COMBase::CleanupCOM();
}

bool UIMainEventListeningThread::isShutdown() const
{
    m_mutex.lock();
    bool fShutdown = m_fShutdown;
    m_mutex.unlock();
    return fShutdown;
}

void UIMainEventListeningThread::setShutdown(bool fShutdown)
{
    m_mutex.lock();
    m_fShutdown = fShutdown;
    m_mutex.unlock();
}


/*********************************************************************************************************************************
*   Class UIMainEventListener implementation.                                                                                    *
*********************************************************************************************************************************/

UIMainEventListener::UIMainEventListener()
{
    /* Register meta-types for required enums. */
    qRegisterMetaType<KMachineState>("KMachineState");
    qRegisterMetaType<KSessionState>("KSessionState");
    qRegisterMetaType< QVector<uint8_t> >("QVector<uint8_t>");
    qRegisterMetaType<CNetworkAdapter>("CNetworkAdapter");
    qRegisterMetaType<CMediumAttachment>("CMediumAttachment");
    qRegisterMetaType<CUSBDevice>("CUSBDevice");
    qRegisterMetaType<CVirtualBoxErrorInfo>("CVirtualBoxErrorInfo");
    qRegisterMetaType<KGuestMonitorChangedEventType>("KGuestMonitorChangedEventType");
    qRegisterMetaType<CGuestSession>("CGuestSession");
}

void UIMainEventListener::registerSource(const CEventSource &comSource, const CEventListener &comListener)
{
    /* Make sure source and listener are valid: */
    AssertReturnVoid(!comSource.isNull());
    AssertReturnVoid(!comListener.isNull());

    /* Create thread for passed source: */
    m_threads << new UIMainEventListeningThread(comSource, comListener);
    /* And start it: */
    m_threads.last()->start();
}

void UIMainEventListener::unregisterSources()
{
    /* Wipe out the threads: */
    qDeleteAll(m_threads);
}

STDMETHODIMP UIMainEventListener::HandleEvent(VBoxEventType_T, IEvent *pEvent)
{
    /* Try to acquire COM cleanup protection token first: */
    if (!vboxGlobal().comTokenTryLockForRead())
        return S_OK;

    CEvent comEvent(pEvent);
    //printf("Event received: %d\n", comEvent.GetType());
    switch (comEvent.GetType())
    {
        case KVBoxEventType_OnVBoxSVCAvailabilityChanged:
        {
            CVBoxSVCAvailabilityChangedEvent comEventSpecific(pEvent);
            emit sigVBoxSVCAvailabilityChange(comEventSpecific.GetAvailable());
            break;
        }

        case KVBoxEventType_OnMachineStateChanged:
        {
            CMachineStateChangedEvent comEventSpecific(pEvent);
            emit sigMachineStateChange(comEventSpecific.GetMachineId(), comEventSpecific.GetState());
            break;
        }
        case KVBoxEventType_OnMachineDataChanged:
        {
            CMachineDataChangedEvent comEventSpecific(pEvent);
            emit sigMachineDataChange(comEventSpecific.GetMachineId());
            break;
        }
        case KVBoxEventType_OnMachineRegistered:
        {
            CMachineRegisteredEvent comEventSpecific(pEvent);
            emit sigMachineRegistered(comEventSpecific.GetMachineId(), comEventSpecific.GetRegistered());
            break;
        }
        case KVBoxEventType_OnSessionStateChanged:
        {
            CSessionStateChangedEvent comEventSpecific(pEvent);
            emit sigSessionStateChange(comEventSpecific.GetMachineId(), comEventSpecific.GetState());
            break;
        }
        case KVBoxEventType_OnSnapshotTaken:
        {
            CSnapshotTakenEvent comEventSpecific(pEvent);
            emit sigSnapshotTake(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVBoxEventType_OnSnapshotDeleted:
        {
            CSnapshotDeletedEvent comEventSpecific(pEvent);
            emit sigSnapshotDelete(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVBoxEventType_OnSnapshotChanged:
        {
            CSnapshotChangedEvent comEventSpecific(pEvent);
            emit sigSnapshotChange(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVBoxEventType_OnSnapshotRestored:
        {
            CSnapshotRestoredEvent comEventSpecific(pEvent);
            emit sigSnapshotRestore(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
//        case KVBoxEventType_OnMediumRegistered:
//        case KVBoxEventType_OnGuestPropertyChange:

        case KVBoxEventType_OnExtraDataCanChange:
        {
            CExtraDataCanChangeEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigExtraDataCanChange(comEventSpecific.GetMachineId(), comEventSpecific.GetKey(),
                                       comEventSpecific.GetValue(), fVeto, strReason);
            if (fVeto)
                comEventSpecific.AddVeto(strReason);
            break;
        }
        case KVBoxEventType_OnExtraDataChanged:
        {
            CExtraDataChangedEvent comEventSpecific(pEvent);
            emit sigExtraDataChange(comEventSpecific.GetMachineId(), comEventSpecific.GetKey(), comEventSpecific.GetValue());
            break;
        }

        case KVBoxEventType_OnMousePointerShapeChanged:
        {
            CMousePointerShapeChangedEvent comEventSpecific(pEvent);
            emit sigMousePointerShapeChange(comEventSpecific.GetVisible(), comEventSpecific.GetAlpha(),
                                            QPoint(comEventSpecific.GetXhot(), comEventSpecific.GetYhot()),
                                            QSize(comEventSpecific.GetWidth(), comEventSpecific.GetHeight()),
                                            comEventSpecific.GetShape());
            break;
        }
        case KVBoxEventType_OnMouseCapabilityChanged:
        {
            CMouseCapabilityChangedEvent comEventSpecific(pEvent);
            emit sigMouseCapabilityChange(comEventSpecific.GetSupportsAbsolute(), comEventSpecific.GetSupportsRelative(),
                                          comEventSpecific.GetSupportsMultiTouch(), comEventSpecific.GetNeedsHostCursor());
            break;
        }
        case KVBoxEventType_OnCursorPositionChanged:
        {
            CCursorPositionChangedEvent comEventSpecific(pEvent);
            emit sigCursorPositionChange(comEventSpecific.GetHasData(),
                                         (unsigned long)comEventSpecific.GetX(), (unsigned long)comEventSpecific.GetY());
            break;
        }
        case KVBoxEventType_OnKeyboardLedsChanged:
        {
            CKeyboardLedsChangedEvent comEventSpecific(pEvent);
            emit sigKeyboardLedsChangeEvent(comEventSpecific.GetNumLock(),
                                            comEventSpecific.GetCapsLock(),
                                            comEventSpecific.GetScrollLock());
            break;
        }
        case KVBoxEventType_OnStateChanged:
        {
            CStateChangedEvent comEventSpecific(pEvent);
            emit sigStateChange(comEventSpecific.GetState());
            break;
        }
        case KVBoxEventType_OnAdditionsStateChanged:
        {
            emit sigAdditionsChange();
            break;
        }
        case KVBoxEventType_OnNetworkAdapterChanged:
        {
            CNetworkAdapterChangedEvent comEventSpecific(pEvent);
            emit sigNetworkAdapterChange(comEventSpecific.GetNetworkAdapter());
            break;
        }
        case KVBoxEventType_OnStorageDeviceChanged:
        {
            CStorageDeviceChangedEvent comEventSpecific(pEvent);
            emit sigStorageDeviceChange(comEventSpecific.GetStorageDevice(),
                                        comEventSpecific.GetRemoved(),
                                        comEventSpecific.GetSilent());
            break;
        }
        case KVBoxEventType_OnMediumChanged:
        {
            CMediumChangedEvent comEventSpecific(pEvent);
            emit sigMediumChange(comEventSpecific.GetMediumAttachment());
            break;
        }
        case KVBoxEventType_OnVRDEServerChanged:
        case KVBoxEventType_OnVRDEServerInfoChanged:
        {
            emit sigVRDEChange();
            break;
        }
        case KVBoxEventType_OnVideoCaptureChanged:
        {
            emit sigVideoCaptureChange();
            break;
        }
        case KVBoxEventType_OnUSBControllerChanged:
        {
            emit sigUSBControllerChange();
            break;
        }
        case KVBoxEventType_OnUSBDeviceStateChanged:
        {
            CUSBDeviceStateChangedEvent comEventSpecific(pEvent);
            emit sigUSBDeviceStateChange(comEventSpecific.GetDevice(),
                                         comEventSpecific.GetAttached(),
                                         comEventSpecific.GetError());
            break;
        }
        case KVBoxEventType_OnSharedFolderChanged:
        {
            emit sigSharedFolderChange();
            break;
        }
        case KVBoxEventType_OnCPUExecutionCapChanged:
        {
            emit sigCPUExecutionCapChange();
            break;
        }
        case KVBoxEventType_OnGuestMonitorChanged:
        {
            CGuestMonitorChangedEvent comEventSpecific(pEvent);
            emit sigGuestMonitorChange(comEventSpecific.GetChangeType(), comEventSpecific.GetScreenId(),
                                       QRect(comEventSpecific.GetOriginX(), comEventSpecific.GetOriginY(),
                                             comEventSpecific.GetWidth(), comEventSpecific.GetHeight()));
            break;
        }
        case KVBoxEventType_OnRuntimeError:
        {
            CRuntimeErrorEvent comEventSpecific(pEvent);
            emit sigRuntimeError(comEventSpecific.GetFatal(), comEventSpecific.GetId(), comEventSpecific.GetMessage());
            break;
        }
        case KVBoxEventType_OnCanShowWindow:
        {
            CCanShowWindowEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigCanShowWindow(fVeto, strReason);
            if (fVeto)
                comEventSpecific.AddVeto(strReason);
            else
                comEventSpecific.AddApproval(strReason);
            break;
        }
        case KVBoxEventType_OnShowWindow:
        {
            CShowWindowEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            qint64 winId = comEventSpecific.GetWinId();
            if (winId != 0)
                break; /* Already set by some listener. */
            emit sigShowWindow(winId);
            comEventSpecific.SetWinId(winId);
            break;
        }
        case KVBoxEventType_OnAudioAdapterChanged:
        {
            emit sigAudioAdapterChange();
            break;
        }
        case KVBoxEventType_OnProgressPercentageChanged:
        {
            CProgressPercentageChangedEvent comEventSpecific(pEvent);
            emit sigProgressPercentageChange(comEventSpecific.GetProgressId(), (int)comEventSpecific.GetPercent());
            break;
        }
        case KVBoxEventType_OnProgressTaskCompleted:
        {
            CProgressTaskCompletedEvent comEventSpecific(pEvent);
            emit sigProgressTaskComplete(comEventSpecific.GetProgressId());
            break;
        }
        case KVBoxEventType_OnGuestSessionRegistered:
        {

            CGuestSessionRegisteredEvent comEventSpecific(pEvent);
            if (comEventSpecific.GetRegistered())
                emit sigGuestSessionRegistered(comEventSpecific.GetSession());
            else
                emit sigGuestSessionUnregistered(comEventSpecific.GetSession());
            break;
        }
        case KVBoxEventType_OnGuestProcessRegistered:
        {
            CGuestProcessRegisteredEvent comEventSpecific(pEvent);
            if (comEventSpecific.GetRegistered())
                emit sigGuestProcessRegistered(comEventSpecific.GetProcess());
            else
                emit sigGuestProcessUnregistered(comEventSpecific.GetProcess());
            break;
        }
        case KVBoxEventType_OnGuestSessionStateChanged:
        {
            CGuestSessionStateChangedEvent comEventSpecific(pEvent);
            emit sigGuestSessionStatedChanged(comEventSpecific);
            break;
        }
        case KVBoxEventType_OnGuestProcessInputNotify:
        case KVBoxEventType_OnGuestProcessOutput:
        {
            break;
        }
        case KVBoxEventType_OnGuestProcessStateChanged:
        {
            CGuestProcessStateChangedEvent comEventSpecific(pEvent);
            comEventSpecific.GetError();
            emit sigGuestProcessStateChanged(comEventSpecific);
            break;
        }

        case KVBoxEventType_OnGuestFileRegistered:
        case KVBoxEventType_OnGuestFileStateChanged:
        case KVBoxEventType_OnGuestFileOffsetChanged:
        case KVBoxEventType_OnGuestFileRead:
        case KVBoxEventType_OnGuestFileWrite:
        {
            break;
        }

        default: break;
    }

    /* Unlock COM cleanup protection token: */
    vboxGlobal().comTokenUnlock();

    return S_OK;
}

#include "UIMainEventListener.moc"

