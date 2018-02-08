/* $Id$ */
/** @file
 * VBox Qt GUI - UIConsoleEventHandler class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
# include "UIGuestSessionsEventHandler.h"
# include "UIMainEventListener.h"
# include "VBoxGlobal.h"


/* COM includes: */
# include "COMEnums.h"
# include "CEventListener.h"
# include "CEventSource.h"
# include "CGuest.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

class UIGuestSession;
class UIGuestProcess;

/** Key is CGuestSession::GetId() */
typedef QMap<ULONG, UIGuestSession*> GuestSessionMap;
/** Key is PID */
typedef QMap<ULONG, UIGuestProcess*> GuestProcessMap;


/******************************************************************************************************
*   UIGuestProcess definition.                                                            *
******************************************************************************************************/

class UIGuestProcess : public QObject
{

    Q_OBJECT;

signals:

    void sigGuestProcessUpdated();

public:

    UIGuestProcess(QObject *parent, const CGuestProcess &guestProcess);
    ~UIGuestProcess();
    QString guestProcessName() const;
    ULONG   guestPID() const;
    QString guestProcessStatus() const;
private slots:

    // void sltGuestProcessRegistered(CGuestProcess guestProcess);
    // void sltGuestProcessUnregistered(CGuestProcess guestProcess);

private:

    void  prepareListener();
    void  cleanupListener();

    CGuestProcess  m_comGuestProcess;
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    CEventListener m_comEventListener;
};

/******************************************************************************************************
*   UIGuestSession definition.                                                            *
******************************************************************************************************/

class UIGuestSession : public QObject
{

    Q_OBJECT;

signals:

    void sigGuestSessionUpdated();

public:

    UIGuestSession(QObject *parent, const CGuestSession &guestSession);
    ~UIGuestSession();
    QString guestSessionName() const;
    QString guestSessionStatus() const;
    const GuestProcessMap& guestProcessMap() const;

private slots:

    void sltGuestProcessRegistered(CGuestProcess guestProcess);
    void sltGuestProcessUnregistered(CGuestProcess guestProcess);

private:

    void  prepareListener();
    void  cleanupListener();
    void  initialize();
    void  addGuestProcess(const CGuestProcess &guestProcess);
    void  removeGuestProcess(const CGuestProcess &guestProcess);

    CGuestSession  m_comGuestSession;
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    CEventListener m_comEventListener;
    GuestProcessMap m_guestProcessMap;
};

/******************************************************************************************************
*   UIGuestSessionsEventHandlerImp definition.                                                     *
******************************************************************************************************/

class UIGuestSessionsEventHandlerImp : public QObject
{
    Q_OBJECT;

signals:

    void sigGuestSessionUpdated();

public:

    UIGuestSessionsEventHandlerImp(QObject *parent, const CGuest &comGuest);
    ~UIGuestSessionsEventHandlerImp();
    const GuestSessionMap& getGuestSessionMap() const;

private slots:

    void sltGuestSessionRegistered(CGuestSession guestSession);
    void sltGuestSessionUnregistered(CGuestSession guestSession);

private:

    void  prepareListener();
    void  cleanupListener();
    void  initialize();

    void addGuestSession(const CGuestSession &guestSession);
    void removeGuestSession(const CGuestSession &guestSession);


    CGuest  m_comGuest;
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    CEventListener m_comEventListener;
    GuestSessionMap m_guestSessionMap;
};


/******************************************************************************************************
*   UIGuestProcess definition.                                                            *
******************************************************************************************************/
UIGuestProcess::UIGuestProcess(QObject *parent, const CGuestProcess &guestProcess)
    :QObject(parent)
    , m_comGuestProcess(guestProcess)
{
    qRegisterMetaType<CGuestProcess>("CGuestProcess");
    prepareListener();
}

UIGuestProcess::~UIGuestProcess()
{
    cleanupListener();
}

void  UIGuestProcess::prepareListener()
{
    if (!m_comGuestProcess.isOk())
        return;

    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessStateChanged,
            this, &UIGuestProcess::sigGuestProcessUpdated);

    CEventSource comEventSourceGuestProcess = m_comGuestProcess.GetEventSource();
    AssertWrapperOk(comEventSourceGuestProcess);

    QVector<KVBoxEventType> eventTypes;
    eventTypes  << KVBoxEventType_OnGuestProcessStateChanged
                << KVBoxEventType_OnGuestProcessInputNotify
                << KVBoxEventType_OnGuestProcessOutput;

    comEventSourceGuestProcess.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(comEventSourceGuestProcess);

    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
         m_pQtListener->getWrapped()->registerSource(comEventSourceGuestProcess, m_comEventListener);
    }
}


void  UIGuestProcess::cleanupListener()
{
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
        m_pQtListener->getWrapped()->unregisterSources();

    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    CEventSource comEventSourceGuestProcess = m_comGuestProcess.GetEventSource();
    AssertWrapperOk(comEventSourceGuestProcess);
    comEventSourceGuestProcess.UnregisterListener(m_comEventListener);
}

QString UIGuestProcess::guestProcessName() const
{
    if (!m_comGuestProcess.isOk())
        return QString();
    return m_comGuestProcess.GetExecutablePath();
}

ULONG UIGuestProcess::guestPID() const
{
    if (!m_comGuestProcess.isOk())
        return 0;
    return m_comGuestProcess.GetPID();
}

QString UIGuestProcess::guestProcessStatus() const
{
    if (!m_comGuestProcess.isOk())
        return QString("InvalidProcess");
    QString statusString;
    switch (m_comGuestProcess.GetStatus())
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
            statusString = "Paused";
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
        default:
            statusString = "Error";
            break;
    }
    return statusString;
}

/******************************************************************************************************
*   UIGuestSession implementation.                                                            *
******************************************************************************************************/

UIGuestSession::UIGuestSession(QObject *parent, const CGuestSession &guestSession)
    : QObject(parent)
    , m_comGuestSession(guestSession)
{
    qRegisterMetaType<CGuestProcess>("CGuestProcess");
    prepareListener();
    initialize();
}

UIGuestSession::~UIGuestSession()
{
    cleanupListener();
}

void  UIGuestSession::prepareListener()
{
    if (!m_comGuestSession.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIGuestSession::sigGuestSessionUpdated);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessRegistered,
            this, &UIGuestSession::sltGuestProcessRegistered);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessUnregistered,
            this, &UIGuestSession::sltGuestProcessUnregistered);

    /* Get CGuest event source: */
    CEventSource comEventSourceGuestSession = m_comGuestSession.GetEventSource();
    AssertWrapperOk(comEventSourceGuestSession);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged
               << KVBoxEventType_OnGuestProcessRegistered;
               // << KVBoxEventType_OnGuestProcessStateChanged
               // << KVBoxEventType_OnGuestProcessInputNotify
               // << KVBoxEventType_OnGuestProcessOutput
               // << KVBoxEventType_OnGuestFileRegistered
               // << KVBoxEventType_OnGuestFileStateChanged
               // << KVBoxEventType_OnGuestFileOffsetChanged
               // << KVBoxEventType_OnGuestFileRead
               // << KVBoxEventType_OnGuestFileWrite;

    /* Register event listener for CGuestSession event source: */
    comEventSourceGuestSession.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(comEventSourceGuestSession);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSourceGuestSession, m_comEventListener);
    }
}

void  UIGuestSession::cleanupListener()
{
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Make sure VBoxSVC is available: */
    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    /* Get CGuestSession event source: */
    CEventSource comEventSourceGuestSession = m_comGuestSession.GetEventSource();
    AssertWrapperOk(comEventSourceGuestSession);

    /* Unregister event listener for CProgress event source: */
    comEventSourceGuestSession.UnregisterListener(m_comEventListener);
}

QString UIGuestSession::guestSessionName() const
{
    if (!m_comGuestSession.isOk())
        return QString();
    return m_comGuestSession.GetName();
}

QString UIGuestSession::guestSessionStatus() const
{
    if (!m_comGuestSession.isOk())
        return QString("InvalidSession");
    QString statusString;
    switch (m_comGuestSession.GetStatus())
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

void UIGuestSession::sltGuestProcessRegistered(CGuestProcess guestProcess)
{
    addGuestProcess(guestProcess);
    emit sigGuestSessionUpdated();
}

void UIGuestSession::sltGuestProcessUnregistered(CGuestProcess guestProcess)
{
    removeGuestProcess(guestProcess);
    emit sigGuestSessionUpdated();
}


void  UIGuestSession::initialize()
{
    QVector<CGuestProcess> processVector = m_comGuestSession.GetProcesses();
    for (int i = 0; i < processVector.size(); ++i)
        addGuestProcess(processVector.at(i));
    emit sigGuestSessionUpdated();
}

void  UIGuestSession::addGuestProcess(const CGuestProcess &guestProcess)
{
    if (m_guestProcessMap.contains(guestProcess.GetPID()))
       return;
    UIGuestProcess *newGuestProcess = new UIGuestProcess(this, guestProcess);
    connect(newGuestProcess, &UIGuestProcess::sigGuestProcessUpdated,
            this, &UIGuestSession::sigGuestSessionUpdated);
    m_guestProcessMap.insert(guestProcess.GetPID(), newGuestProcess);
}

void  UIGuestSession::removeGuestProcess(const CGuestProcess &guestProcess)
{
    if (!m_guestProcessMap.contains(guestProcess.GetPID()))
       return;
    m_guestProcessMap.remove(guestProcess.GetPID());
}

const GuestProcessMap& UIGuestSession::guestProcessMap() const
{
    return m_guestProcessMap;
}

/******************************************************************************************************
*   UIGuestSessionsEventHandlerImp implementation.                                                      *
******************************************************************************************************/

UIGuestSessionsEventHandlerImp::UIGuestSessionsEventHandlerImp(QObject *parent, const CGuest &comGuest)
    : QObject(parent)
    , m_comGuest(comGuest)
{
    prepareListener();
    initialize();
}

UIGuestSessionsEventHandlerImp::~UIGuestSessionsEventHandlerImp()
{
    cleanupListener();
}

void UIGuestSessionsEventHandlerImp::prepareListener()
{
    if (!m_comGuest.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
            this, &UIGuestSessionsEventHandlerImp::sltGuestSessionRegistered);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
            this, &UIGuestSessionsEventHandlerImp::sltGuestSessionUnregistered);

    /* Get CGuest event source: */
    CEventSource comEventSourceGuest = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSourceGuest);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionRegistered;

    /* Register event listener for CGuest event source: */
    comEventSourceGuest.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(comEventSourceGuest);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSourceGuest, m_comEventListener);
    }
}

void UIGuestSessionsEventHandlerImp::cleanupListener()
{
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        m_pQtListener->getWrapped()->unregisterSources();
    }

    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    CEventSource comEventSourceGuest = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSourceGuest);

    comEventSourceGuest.UnregisterListener(m_comEventListener);
}


void UIGuestSessionsEventHandlerImp::initialize()
{
    if (!m_comGuest.isOk())
        return;
    const QVector<CGuestSession>& sessionVector = m_comGuest.GetSessions();
    for (int i = 0; i < sessionVector.size(); ++i)
    {
        addGuestSession(sessionVector.at(i));
    }
    emit sigGuestSessionUpdated();
}


void UIGuestSessionsEventHandlerImp::addGuestSession(const CGuestSession &guestSession)
{
    if (m_guestSessionMap.contains(guestSession.GetId()))
        return;
    UIGuestSession* newGuestSession = new UIGuestSession(this, guestSession);
    connect(newGuestSession, &UIGuestSession::sigGuestSessionUpdated,
            this, &UIGuestSessionsEventHandlerImp::sigGuestSessionUpdated);

    m_guestSessionMap.insert(guestSession.GetId(), newGuestSession);
}

void UIGuestSessionsEventHandlerImp::removeGuestSession(const CGuestSession &guestSession)
{
    if (!m_guestSessionMap.contains(guestSession.GetId()))
        return;
    m_guestSessionMap.remove(guestSession.GetId());
}


void UIGuestSessionsEventHandlerImp::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    addGuestSession(guestSession);
    emit sigGuestSessionUpdated();
}

void UIGuestSessionsEventHandlerImp::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    removeGuestSession(guestSession);
    emit sigGuestSessionUpdated();
}

const GuestSessionMap& UIGuestSessionsEventHandlerImp::getGuestSessionMap() const
{
    return m_guestSessionMap;
}

UIGuestSessionsEventHandler::UIGuestSessionsEventHandler(QObject *parent, const CGuest &comGuest)
    :QObject(parent)
    , m_pPrivateImp(new UIGuestSessionsEventHandlerImp(this, comGuest))
{
    connect(m_pPrivateImp, &UIGuestSessionsEventHandlerImp::sigGuestSessionUpdated,
            this, &UIGuestSessionsEventHandler::sigGuestSessionsUpdated);

}

UIGuestSessionsEventHandler::~UIGuestSessionsEventHandler()
{
    delete m_pPrivateImp;
}

void UIGuestSessionsEventHandler::populateGuestSessionsTree(QITreeWidget *pTreeWidget)
{
    if (!m_pPrivateImp || !pTreeWidget)
        return;
    pTreeWidget->setColumnCount(3);
    QStringList headerStrings;
    headerStrings << "Session Id/PID"
                  << "Session Name / EXE Path" << "Status";
    pTreeWidget->setHeaderLabels(headerStrings);
    const GuestSessionMap& guestSessionMap = m_pPrivateImp->getGuestSessionMap();
    QStringList itemStringList;
    for (GuestSessionMap::const_iterator iterator = guestSessionMap.begin();
        iterator != guestSessionMap.end(); ++iterator)
    {
        itemStringList << QString("Session Id %1").arg(iterator.key())
                       << iterator.value()->guestSessionName()
                       << iterator.value()->guestSessionStatus();
        const GuestProcessMap& processMap = iterator.value()->guestProcessMap();
        QITreeWidgetItem *treeItem = new QITreeWidgetItem(pTreeWidget, itemStringList);
        pTreeWidget->insertTopLevelItem(iterator.key(), treeItem);
        for (GuestProcessMap::const_iterator processIterator = processMap.begin();
            processIterator != processMap.end(); ++processIterator)
        {
            QStringList processStringList;
            processStringList << QString("%1").arg(processIterator.value()->guestPID())
                              << processIterator.value()->guestProcessName()
                              << processIterator.value()->guestProcessStatus();
            new QTreeWidgetItem(treeItem, processStringList);
        }

        itemStringList.clear();
    }
}

#include "UIGuestSessionsEventHandler.moc"
