/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class implementation.
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

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UIMainEventListener.h"
# include "UIVirtualBoxEventHandler.h"

/* COM includes: */
# include "CEventListener.h"
# include "CEventSource.h"
# include "CVirtualBox.h"
# include "CVirtualBoxClient.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Private QObject extension
  * providing UIVirtualBoxEventHandler with the CVirtualBoxClient and CVirtualBox event-sources. */
class UIVirtualBoxEventHandlerProxy : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about the VBoxSVC become @a fAvailable. */
    void sigVBoxSVCAvailabilityChange(bool fAvailable);

    /** Notifies about @a state change event for the machine with @a strId. */
    void sigMachineStateChange(const QUuid &aId, const KMachineState state);
    /** Notifies about data change event for the machine with @a strId. */
    void sigMachineDataChange(const QUuid &aId);
    /** Notifies about machine with @a strId was @a fRegistered. */
    void sigMachineRegistered(const QUuid &aId, const bool fRegistered);
    /** Notifies about @a state change event for the session of the machine with @a strId. */
    void sigSessionStateChange(const QUuid &aId, const KSessionState state);
    /** Notifies about snapshot with @a strSnapshotId was taken for the machine with @a strId. */
    void sigSnapshotTake(const QUuid &aId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was deleted for the machine with @a strId. */
    void sigSnapshotDelete(const QUuid &aId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was changed for the machine with @a strId. */
    void sigSnapshotChange(const QUuid &aId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was restored for the machine with @a strId. */
    void sigSnapshotRestore(const QUuid &aId, const QUuid &aSnapshotId);

public:

    /** Constructs event proxy object on the basis of passed @a pParent. */
    UIVirtualBoxEventHandlerProxy(QObject *pParent);
    /** Destructs event proxy object. */
    ~UIVirtualBoxEventHandlerProxy();

protected:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares listener. */
        void prepareListener();
        /** Prepares connections. */
        void prepareConnections();

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups listener. */
        void cleanupListener();
        /** Cleanups all. */
        void cleanup();
    /** @} */

private:

    /** Holds the COM event source instance. */
    CEventSource m_comEventSource;

    /** Holds the Qt event listener instance. */
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    /** Holds the COM event listener instance. */
    CEventListener m_comEventListener;
};


/*********************************************************************************************************************************
*   Class UIVirtualBoxEventHandlerProxy implementation.                                                                          *
*********************************************************************************************************************************/

UIVirtualBoxEventHandlerProxy::UIVirtualBoxEventHandlerProxy(QObject *pParent)
    : QObject(pParent)
{
    /* Prepare: */
    prepare();
}

UIVirtualBoxEventHandlerProxy::~UIVirtualBoxEventHandlerProxy()
{
    /* Cleanup: */
    cleanup();
}

void UIVirtualBoxEventHandlerProxy::prepare()
{
    /* Prepare: */
    prepareListener();
    prepareConnections();
}

void UIVirtualBoxEventHandlerProxy::prepareListener()
{
    /* Create Main event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Get VirtualBoxClient: */
    const CVirtualBoxClient comVBoxClient = vboxGlobal().virtualBoxClient();
    AssertWrapperOk(comVBoxClient);
    /* Get VirtualBoxClient event source: */
    CEventSource comEventSourceVBoxClient = comVBoxClient.GetEventSource();
    AssertWrapperOk(comEventSourceVBoxClient);

    /* Get VirtualBox: */
    const CVirtualBox comVBox = vboxGlobal().virtualBox();
    AssertWrapperOk(comVBox);
    /* Get VirtualBox event source: */
    CEventSource comEventSourceVBox = comVBox.GetEventSource();
    AssertWrapperOk(comEventSourceVBox);

    /* Create event source aggregator: */
    m_comEventSource = comEventSourceVBoxClient.CreateAggregator(QVector<CEventSource>()
                                                                 << comEventSourceVBoxClient
                                                                 << comEventSourceVBox);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes
        /* For VirtualBoxClient: */
        << KVBoxEventType_OnVBoxSVCAvailabilityChanged
        /* For VirtualBox: */
        << KVBoxEventType_OnMachineStateChanged
        << KVBoxEventType_OnMachineDataChanged
        << KVBoxEventType_OnMachineRegistered
        << KVBoxEventType_OnSessionStateChanged
        << KVBoxEventType_OnSnapshotTaken
        << KVBoxEventType_OnSnapshotDeleted
        << KVBoxEventType_OnSnapshotChanged
        << KVBoxEventType_OnSnapshotRestored;

    /* Register event listener for event source aggregator: */
    m_comEventSource.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(m_comEventSource);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(m_comEventSource, m_comEventListener);
    }
}

void UIVirtualBoxEventHandlerProxy::prepareConnections()
{
    /* Create direct (sync) connections for signals of main listener: */
    connect(m_pQtListener->getWrapped(), SIGNAL(sigVBoxSVCAvailabilityChange(bool)),
            this, SIGNAL(sigVBoxSVCAvailabilityChange(bool)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineDataChange(QUuid)),
            this, SIGNAL(sigMachineDataChange(QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineRegistered(QUuid, bool)),
            this, SIGNAL(sigMachineRegistered(QUuid, bool)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            Qt::DirectConnection);
}

void UIVirtualBoxEventHandlerProxy::cleanupConnections()
{
    /* Nothing for now. */
}

void UIVirtualBoxEventHandlerProxy::cleanupListener()
{
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Unregister event listener for event source aggregator: */
    m_comEventSource.UnregisterListener(m_comEventListener);
    m_comEventSource.detach();
}

void UIVirtualBoxEventHandlerProxy::cleanup()
{
    /* Cleanup: */
    cleanupConnections();
    cleanupListener();
}


/*********************************************************************************************************************************
*   Class UIVirtualBoxEventHandler implementation.                                                                               *
*********************************************************************************************************************************/

/* static */
UIVirtualBoxEventHandler *UIVirtualBoxEventHandler::s_pInstance = 0;

/* static */
UIVirtualBoxEventHandler *UIVirtualBoxEventHandler::instance()
{
    if (!s_pInstance)
        s_pInstance = new UIVirtualBoxEventHandler;
    return s_pInstance;
}

/* static */
void UIVirtualBoxEventHandler::destroy()
{
    if (s_pInstance)
    {
        delete s_pInstance;
        s_pInstance = 0;
    }
}

UIVirtualBoxEventHandler::UIVirtualBoxEventHandler()
    : m_pProxy(new UIVirtualBoxEventHandlerProxy(this))
{
    /* Prepare: */
    prepare();
}

void UIVirtualBoxEventHandler::prepare()
{
    /* Prepare connections: */
    prepareConnections();
}

void UIVirtualBoxEventHandler::prepareConnections()
{
    /* Create queued (async) connections for signals of event proxy object: */
    connect(m_pProxy, SIGNAL(sigVBoxSVCAvailabilityChange(bool)),
            this, SIGNAL(sigVBoxSVCAvailabilityChange(bool)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineDataChange(QUuid)),
            this, SIGNAL(sigMachineDataChange(QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineRegistered(QUuid, bool)),
            this, SIGNAL(sigMachineRegistered(QUuid, bool)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            Qt::QueuedConnection);
}


#include "UIVirtualBoxEventHandler.moc"

