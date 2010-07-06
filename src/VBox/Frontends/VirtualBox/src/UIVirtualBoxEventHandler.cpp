/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVirtualBoxEventHandler class implementation
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

/* Local includes */
#include "UIVirtualBoxEventHandler.h"
#include "UIMainEventListener.h"
#include "VBoxGlobal.h"

/* Global includes */
//#include <iprt/thread.h>
//#include <iprt/stream.h>

/* static */
UIVirtualBoxEventHandler *UIVirtualBoxEventHandler::m_pInstance = 0;

/* static */
UIVirtualBoxEventHandler* UIVirtualBoxEventHandler::instance()
{
    if (!m_pInstance)
        m_pInstance = new UIVirtualBoxEventHandler();
    return m_pInstance;
}

/* static */
void UIVirtualBoxEventHandler::destroy()
{
    if (!m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = 0;
    }
}

UIVirtualBoxEventHandler::UIVirtualBoxEventHandler()
{
//    RTPrintf("Self add: %RTthrd\n", RTThreadSelf());
    const CVirtualBox &vbox = vboxGlobal().virtualBox();
    UIMainEventListener *pListener = new UIMainEventListener(this);
    m_mainEventListener = CEventListener(pListener);
    QVector<KVBoxEventType> events;
    events
        << KVBoxEventType_OnMachineStateChange
        << KVBoxEventType_OnMachineDataChange
        << KVBoxEventType_OnMachineRegistered
        << KVBoxEventType_OnSessionStateChange
        << KVBoxEventType_OnSnapshotChange;

    vbox.GetEventSource().RegisterListener(m_mainEventListener, events, TRUE);
    AssertWrapperOk(vbox);

    connect(pListener, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            Qt::QueuedConnection);

    connect(pListener, SIGNAL(sigMachineDataChange(QString)),
            this, SIGNAL(sigMachineDataChange(QString)),
            Qt::QueuedConnection);

    connect(pListener, SIGNAL(sigMachineRegistered(QString, bool)),
            this, SIGNAL(sigMachineRegistered(QString, bool)),
            Qt::QueuedConnection);

    connect(pListener, SIGNAL(sigSessionStateChange(QString, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QString, KSessionState)),
            Qt::QueuedConnection);

    connect(pListener, SIGNAL(sigSnapshotChange(QString, QString)),
            this, SIGNAL(sigSnapshotChange(QString, QString)),
            Qt::QueuedConnection);
}

UIVirtualBoxEventHandler::~UIVirtualBoxEventHandler()
{
    const CVirtualBox &vbox = vboxGlobal().virtualBox();
    vbox.GetEventSource().UnregisterListener(m_mainEventListener);
}

