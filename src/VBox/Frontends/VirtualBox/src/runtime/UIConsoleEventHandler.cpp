/* $Id$ */
/** @file
 * VBox Qt GUI - UIConsoleEventHandler class implementation.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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

/* GUI includes: */
# include "UIConsoleEventHandler.h"
# include "UIMainEventListener.h"
# include "VBoxGlobal.h"
# include "UISession.h"
# ifdef Q_WS_MAC
#  include "VBoxUtils.h"
# endif /* Q_WS_MAC */

/* COM includes: */
# include "CConsole.h"
# include "CEventSource.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* static */
UIConsoleEventHandler *UIConsoleEventHandler::m_spInstance = 0;

/* static */
void UIConsoleEventHandler::create(UISession *pSession)
{
    if (!m_spInstance)
        m_spInstance = new UIConsoleEventHandler(pSession);
}

/* static */
void UIConsoleEventHandler::destroy()
{
    if (m_spInstance)
    {
        delete m_spInstance;
        m_spInstance = 0;
    }
}

UIConsoleEventHandler::UIConsoleEventHandler(UISession *pSession)
    : m_pSession(pSession)
{
    /* Prepare: */
    prepare();
}

UIConsoleEventHandler::~UIConsoleEventHandler()
{
    /* Cleanup: */
    cleanup();
}

void UIConsoleEventHandler::prepare()
{
    /* Make sure session is passed: */
    AssertPtrReturnVoid(m_pSession);

    /* Create Main event listener instance: */
    ComObjPtr<UIMainEventListenerImpl> pListener;
    pListener.createObject();
    pListener->init(new UIMainEventListener, this);
    m_mainEventListener = CEventListener(pListener);

    /* Get console: */
    const CConsole console = m_pSession->session().GetConsole();
    AssertReturnVoid(!console.isNull() && console.isOk());
    /* Get event-source: */
    CEventSource eventSource = console.GetEventSource();
    AssertReturnVoid(!eventSource.isNull() && eventSource.isOk());
    /* Register listener for expected event-types: */
    QVector<KVBoxEventType> events;
    events
        << KVBoxEventType_OnMousePointerShapeChanged
        << KVBoxEventType_OnMouseCapabilityChanged
        << KVBoxEventType_OnKeyboardLedsChanged
        << KVBoxEventType_OnStateChanged
        << KVBoxEventType_OnAdditionsStateChanged
        << KVBoxEventType_OnNetworkAdapterChanged
        << KVBoxEventType_OnStorageDeviceChanged
        << KVBoxEventType_OnMediumChanged
        << KVBoxEventType_OnVRDEServerChanged
        << KVBoxEventType_OnVRDEServerInfoChanged
        << KVBoxEventType_OnVideoCaptureChanged
        << KVBoxEventType_OnUSBControllerChanged
        << KVBoxEventType_OnUSBDeviceStateChanged
        << KVBoxEventType_OnSharedFolderChanged
        << KVBoxEventType_OnCPUExecutionCapChanged
        << KVBoxEventType_OnGuestMonitorChanged
        << KVBoxEventType_OnRuntimeError
        << KVBoxEventType_OnCanShowWindow
        << KVBoxEventType_OnShowWindow;
    eventSource.RegisterListener(m_mainEventListener, events, TRUE);


    /* Create queued (async) connections for non-waitable signals: */
    connect(pListener->getWrapped(), SIGNAL(sigMousePointerShapeChange(bool, bool, QPoint, QSize, QVector<uint8_t>)),
            this, SIGNAL(sigMousePointerShapeChange(bool, bool, QPoint, QSize, QVector<uint8_t>)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigMouseCapabilityChange(bool, bool, bool, bool)),
            this, SIGNAL(sigMouseCapabilityChange(bool, bool, bool, bool)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigKeyboardLedsChangeEvent(bool, bool, bool)),
            this, SIGNAL(sigKeyboardLedsChangeEvent(bool, bool, bool)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigStateChange(KMachineState)),
            this, SIGNAL(sigStateChange(KMachineState)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigAdditionsChange()),
            this, SIGNAL(sigAdditionsChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigNetworkAdapterChange(CNetworkAdapter)),
            this, SIGNAL(sigNetworkAdapterChange(CNetworkAdapter)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            this, SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigMediumChange(CMediumAttachment)),
            this, SIGNAL(sigMediumChange(CMediumAttachment)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigVRDEChange()),
            this, SIGNAL(sigVRDEChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigVideoCaptureChange()),
            this, SIGNAL(sigVideoCaptureChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigUSBControllerChange()),
            this, SIGNAL(sigUSBControllerChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigUSBDeviceStateChange(CUSBDevice, bool, CVirtualBoxErrorInfo)),
            this, SIGNAL(sigUSBDeviceStateChange(CUSBDevice, bool, CVirtualBoxErrorInfo)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigSharedFolderChange()),
            this, SIGNAL(sigSharedFolderChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigCPUExecutionCapChange()),
            this, SIGNAL(sigCPUExecutionCapChange()),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
            this, SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigRuntimeError(bool, QString, QString)),
            this, SIGNAL(sigRuntimeError(bool, QString, QString)),
            Qt::QueuedConnection);

    /* Create direct (sync) connections for waitable signals: */
    connect(pListener->getWrapped(), SIGNAL(sigCanShowWindow(bool &, QString &)),
            this, SLOT(sltCanShowWindow(bool &, QString &)),
            Qt::DirectConnection);
    connect(pListener->getWrapped(), SIGNAL(sigShowWindow(qint64 &)),
            this, SLOT(sltShowWindow(qint64 &)),
            Qt::DirectConnection);
}

void UIConsoleEventHandler::cleanup()
{
    /* Make sure session is passed: */
    AssertPtrReturnVoid(m_pSession);

    /* Get console: */
    const CConsole console = m_pSession->session().GetConsole();
    if (console.isNull() || !console.isOk())
        return;
    /* Get event-source: */
    CEventSource eventSource = console.GetEventSource();
    AssertWrapperOk(eventSource);
    /* Unregister listener: */
    eventSource.UnregisterListener(m_mainEventListener);
}

void UIConsoleEventHandler::sltCanShowWindow(bool & /* fVeto */, QString & /* strReason */)
{
    /* Nothing for now. */
}

void UIConsoleEventHandler::sltShowWindow(qint64 &winId)
{
#ifdef Q_WS_MAC
    /* First of all, just ask the GUI thread to show the machine-window: */
    winId = 0;
    if (::darwinSetFrontMostProcess())
        emit sigShowWindow();
    else
    {
        /* If it's failed for some reason, send the other process our PSN so it can try: */
        winId = ::darwinGetCurrentProcessId();
    }
#else /* !Q_WS_MAC */
    /* Return the ID of the top-level machine-window. */
    winId = (ULONG64)m_pSession->winId();
#endif /* !Q_WS_MAC */
}

