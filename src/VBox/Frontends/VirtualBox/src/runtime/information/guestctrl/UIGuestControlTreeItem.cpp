/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestSessionTreeItem class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include "UIExtraDataManager.h"
# include "UIGuestControlTreeItem.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"
# include "CEventSource.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

QString sessionStatusString(KGuestSessionStatus status)
{
    QString statusString;
    switch (status)
    {
        case KGuestSessionStatus_Undefined:
            statusString = "Undefined";
            break;
        case KGuestSessionStatus_Starting:
            statusString = "Starting";
            break;
        case KGuestSessionStatus_Started:
            statusString = "Started";
            break;
        case KGuestSessionStatus_Terminating:
            statusString = "Terminating";
            break;
        case KGuestSessionStatus_Terminated:
            statusString = "Terminated";
            break;
        case KGuestSessionStatus_TimedOutKilled:
            statusString = "TimedOutKilled";
            break;
        case KGuestSessionStatus_TimedOutAbnormally:
            statusString = "TimedOutAbnormally";
            break;
        case KGuestSessionStatus_Down:
            statusString = "Down";
            break;
        case KGuestSessionStatus_Error:
            statusString = "Error";
            break;
        default:
            statusString = "Undefined";
            break;
    }
    return statusString;
}

QString processStatusString(KProcessStatus status)
{
    QString statusString;
    switch (status)
    {
        case KProcessStatus_Undefined:
            statusString = "Undefined";
            break;
        case KProcessStatus_Starting:
            statusString = "Starting";
            break;
        case KProcessStatus_Started:
            statusString = "Started";
            break;
        case KProcessStatus_Paused:
            statusString = "Terminating";
            break;
        case KProcessStatus_Terminating:
            statusString = "Terminating";
            break;
        case KProcessStatus_TerminatedNormally:
            statusString = "TerminatedNormally";
            break;
        case KProcessStatus_TerminatedSignal:
            statusString = "TerminatedSignal";
            break;
        case KProcessStatus_TerminatedAbnormally:
            statusString = "TerminatedAbnormally";
            break;
        case KProcessStatus_TimedOutKilled:
            statusString = "TimedOutKilled";
            break;
        case KProcessStatus_TimedOutAbnormally:
            statusString = "TimedOutAbnormally";
            break;
        case KProcessStatus_Down:
            statusString = "Down";
            break;
        case KProcessStatus_Error:
            statusString = "Error";
            break;
        default:
            break;
    }
    return statusString;
}
/*********************************************************************************************************************************
*   UIGuestControlTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/

UIGuestControlTreeItem::UIGuestControlTreeItem(QITreeWidget *pTreeWidget, const QStringList &strings /* = QStringList() */)
    :QITreeWidgetItem(pTreeWidget,strings)
{
}

UIGuestControlTreeItem::UIGuestControlTreeItem(UIGuestControlTreeItem *pTreeWidgetItem,
                                               const QStringList &strings/* = QStringList() */)
    :QITreeWidgetItem(pTreeWidgetItem, strings)
{

}

UIGuestControlTreeItem::~UIGuestControlTreeItem()
{
}

void UIGuestControlTreeItem::prepare()
{
    prepareListener();
    prepareConnections();
    setColumnText();
}

void UIGuestControlTreeItem::prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes)
{
    if (!comEventSource.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Register event listener for CProgress event source: */
    comEventSource.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSource, m_comEventListener);
    }
}

void UIGuestControlTreeItem::cleanupListener(CEventSource comEventSource)
{
    if (!comEventSource.isOk())
        return;
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Make sure VBoxSVC is available: */
    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(m_comEventListener);
}


/*********************************************************************************************************************************
*   UIGuestSessionTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/

UIGuestSessionTreeItem::UIGuestSessionTreeItem(QITreeWidget *pTreeWidget, CGuestSession& guestSession,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidget, strings)
    , m_comGuestSession(guestSession)
{
    prepare();
}

UIGuestSessionTreeItem::UIGuestSessionTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestSession& guestSession,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidgetItem, strings)
    , m_comGuestSession(guestSession)
{
    prepare();

}

UIGuestSessionTreeItem::~UIGuestSessionTreeItem()
{
    cleanupListener();
}

const CGuestSession& UIGuestSessionTreeItem::guestSession() const
{
    return m_comGuestSession;
}

void UIGuestSessionTreeItem::prepareConnections()
{
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIGuestSessionTreeItem::sltGuestSessionUpdated, Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessRegistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessRegistered, Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessUnregistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessUnregistered, Qt::DirectConnection);
}

void UIGuestSessionTreeItem::prepareListener()
{
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged
               << KVBoxEventType_OnGuestProcessRegistered;

    UIGuestControlTreeItem::prepareListener(m_comGuestSession.GetEventSource(), eventTypes);
}

void UIGuestSessionTreeItem::cleanupListener()
{
    UIGuestControlTreeItem::cleanupListener(m_comGuestSession.GetEventSource());
}

void UIGuestSessionTreeItem::sltGuestSessionUpdated()
{
    setColumnText();
    emit sigGuessSessionUpdated();
}

void UIGuestSessionTreeItem::sltGuestProcessRegistered(CGuestProcess guestProcess)
{
    if (!guestProcess.isOk())
        return;
    QStringList strList;
    strList
        << QString("PID: %1").arg(guestProcess.GetPID())
        << QString("Process Name: %1").arg(guestProcess.GetName())
        << QString("Process Status: %1").arg(processStatusString(guestProcess.GetStatus()));
    /*UIGuestProcessTreeItem *newItem = */new UIGuestProcessTreeItem(this, guestProcess, strList);
}

void UIGuestSessionTreeItem::sltGuestProcessUnregistered(CGuestProcess guestProcess)
{
    for (int i = 0; i < childCount(); ++i)
    {
        UIGuestProcessTreeItem* item = dynamic_cast<UIGuestProcessTreeItem*>(child(i));
        if (item && item->guestProcess() == guestProcess)
        {
            delete item;
            break;
        }
    }
}

void UIGuestSessionTreeItem::setColumnText()
{
    if (!m_comGuestSession.isOk())
        return;
    setText(0, QString("Session Id: %1").arg(m_comGuestSession.GetId()));
    setText(1, QString("Session Name: %1").arg(m_comGuestSession.GetName()));
    setText(2, QString("Session Status: %1").arg(sessionStatusString(m_comGuestSession.GetStatus())));
}

/*********************************************************************************************************************************
*   UIGuestProcessTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/
UIGuestProcessTreeItem::UIGuestProcessTreeItem(QITreeWidget *pTreeWidget, CGuestProcess& guestProcess,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidget, strings)
    , m_comGuestProcess(guestProcess)
{
    prepare();
}

UIGuestProcessTreeItem::UIGuestProcessTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestProcess& guestProcess,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidgetItem, strings)
    , m_comGuestProcess(guestProcess)
{
    prepare();
}

void UIGuestProcessTreeItem::prepareConnections()
{
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessStateChanged,
            this, &UIGuestProcessTreeItem::sltGuestProcessUpdated, Qt::DirectConnection);
}

UIGuestProcessTreeItem::~UIGuestProcessTreeItem()
{
    cleanupListener();
}

void UIGuestProcessTreeItem::prepareListener()
{
    QVector<KVBoxEventType> eventTypes;
    eventTypes  << KVBoxEventType_OnGuestProcessStateChanged
                << KVBoxEventType_OnGuestProcessInputNotify
                << KVBoxEventType_OnGuestProcessOutput;
    UIGuestControlTreeItem::prepareListener(m_comGuestProcess.GetEventSource(), eventTypes);
}


void UIGuestProcessTreeItem::cleanupListener()
{
    UIGuestControlTreeItem::cleanupListener(m_comGuestProcess.GetEventSource());
}

void UIGuestProcessTreeItem::sltGuestProcessUpdated()
{
    setColumnText();
}

 void UIGuestProcessTreeItem::setColumnText()
{
    if (!m_comGuestProcess.isOk())
        return;
    setText(0, QString("PID: %1").arg(m_comGuestProcess.GetPID()));
    setText(1, QString("Process Name: %1").arg(m_comGuestProcess.GetName()));
    setText(2, QString("Process Status: %1").arg(processStatusString(m_comGuestProcess.GetStatus())));

}

const CGuestProcess& UIGuestProcessTreeItem::guestProcess() const
{
    return m_comGuestProcess;
}
