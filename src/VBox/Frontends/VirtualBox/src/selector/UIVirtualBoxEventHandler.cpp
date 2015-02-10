/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class implementation.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
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
# include "UIVirtualBoxEventHandler.h"
# include "UIMainEventListener.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CEventSource.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* static */
UIVirtualBoxEventHandler *UIVirtualBoxEventHandler::m_pInstance = 0;

/* static */
UIVirtualBoxEventHandler* UIVirtualBoxEventHandler::instance()
{
    if (!m_pInstance)
        m_pInstance = new UIVirtualBoxEventHandler;
    return m_pInstance;
}

/* static */
void UIVirtualBoxEventHandler::destroy()
{
    if (m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = 0;
    }
}

UIVirtualBoxEventHandler::UIVirtualBoxEventHandler()
{
    /* Create Main-event listener instance: */
    ComObjPtr<UIMainEventListenerImpl> pListener;
    pListener.createObject();
    pListener->init(new UIMainEventListener, this);
    m_mainEventListener = CEventListener(pListener);

    /* Get VirtualBox: */
    const CVirtualBox vbox = vboxGlobal().virtualBox();
    AssertWrapperOk(vbox);
    /* Get event-source: */
    CEventSource eventSource = vbox.GetEventSource();
    AssertWrapperOk(eventSource);
    /* Register listener for expected event-types: */
    QVector<KVBoxEventType> events;
    events
        << KVBoxEventType_OnMachineStateChanged
        << KVBoxEventType_OnMachineDataChanged
        << KVBoxEventType_OnMachineRegistered
        << KVBoxEventType_OnSessionStateChanged
        << KVBoxEventType_OnSnapshotTaken
        << KVBoxEventType_OnSnapshotDeleted
        << KVBoxEventType_OnSnapshotChanged;
    eventSource.RegisterListener(m_mainEventListener, events, TRUE);
    AssertWrapperOk(eventSource);

    /* Prepare connections: */
    connect(pListener->getWrapped(), SIGNAL(sigMachineStateChange(QString, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigMachineDataChange(QString)),
            this, SIGNAL(sigMachineDataChange(QString)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigMachineRegistered(QString, bool)),
            this, SIGNAL(sigMachineRegistered(QString, bool)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigSessionStateChange(QString, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QString, KSessionState)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigSnapshotTake(QString, QString)),
            this, SIGNAL(sigSnapshotTake(QString, QString)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigSnapshotDelete(QString, QString)),
            this, SIGNAL(sigSnapshotDelete(QString, QString)),
            Qt::QueuedConnection);
    connect(pListener->getWrapped(), SIGNAL(sigSnapshotChange(QString, QString)),
            this, SIGNAL(sigSnapshotChange(QString, QString)),
            Qt::QueuedConnection);
}

UIVirtualBoxEventHandler::~UIVirtualBoxEventHandler()
{
    /* Get VirtualBox: */
    const CVirtualBox vbox = vboxGlobal().virtualBox();
    AssertWrapperOk(vbox);
    /* Get event-source: */
    CEventSource eventSource = vbox.GetEventSource();
    AssertWrapperOk(eventSource);
    /* Unregister listener: */
    eventSource.UnregisterListener(m_mainEventListener);
}

