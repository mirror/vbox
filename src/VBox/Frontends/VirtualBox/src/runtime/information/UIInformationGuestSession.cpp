/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationGuestSession class implementation.
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

/* Qt includes: */
# include <QVBoxLayout>
# include <QApplication>

/* GUI includes: */
# include "QITreeWidget.h"
# include "UIInformationGuestSession.h"
# include "UIInformationDataItem.h"
# include "UIInformationItem.h"
# include "UIInformationView.h"
# include "UIExtraDataManager.h"
# include "UIInformationModel.h"
# include "UIMainEventListener.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CEventListener.h"
# include "CEventSource.h"
# include "CGuest.h"
# include "CGuestSession.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/******************************************************************************************************
*   Class UIGuestEventHandler definition.                                                             *
******************************************************************************************************/

class UIGuestEventHandler : public QObject
{

    Q_OBJECT;

signals:

    void sigGuestSessionRegistered(CGuestSession guestSession);

public:

    UIGuestEventHandler(QObject *parent, const CGuest &guest);

private:

    void  prepareListener();
    void  cleanupListener();

    CGuest  m_comGuest;
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    CEventListener m_comEventListener;

};

/******************************************************************************************************
*   Class UIGuestEventHandler implementation.                                                         *
*****************************************************************************************************/

UIGuestEventHandler::UIGuestEventHandler(QObject *parent, const CGuest &guest)
    :QObject(parent),
     m_comGuest(guest)
{
    prepareListener();
}

void UIGuestEventHandler::prepareListener()
{
    if(!m_comGuest.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
            this, &UIGuestEventHandler::sigGuestSessionRegistered);

    /* Get CGuest event source: */
    CEventSource comEventSourceGuest = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSourceGuest);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionRegistered;

        /*<< KVBoxEventType_OnGuestSessionStateChanged
               << KVBoxEventType_OnGuestProcessRegistered
               << KVBoxEventType_OnGuestProcessStateChanged
               << KVBoxEventType_OnGuestProcessInputNotify
               << KVBoxEventType_OnGuestProcessOutput
               << KVBoxEventType_OnGuestFileRegistered
               << KVBoxEventType_OnGuestFileStateChanged
               << KVBoxEventType_OnGuestFileOffsetChanged
               << KVBoxEventType_OnGuestFileRead
               << KVBoxEventType_OnGuestFileWrite;*/


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

void  UIGuestEventHandler::cleanupListener()
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
    CEventSource comEventSourceGuest = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSourceGuest);

    /* Unregister event listener for CProgress event source: */
    comEventSourceGuest.UnregisterListener(m_comEventListener);
}

/******************************************************************************************************
*   Class UIGuestSessionEventHandler definition.                                                      *
******************************************************************************************************/

class UIGuestSessionEventHandler : public QObject
{

    Q_OBJECT;

public:

    UIGuestSessionEventHandler(QObject *parent, const CGuestSession &guestSession);

private slots:


private:

    void  prepareListener();
    void  cleanupListener();

    CGuestSession  m_comGuestSession;
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    CEventListener m_comEventListener;

};

/******************************************************************************************************
*   Class UIGuestSessionEventHandler implemetation.                                                   *
******************************************************************************************************/

UIGuestSessionEventHandler::UIGuestSessionEventHandler(QObject *parent, const CGuestSession &guestSession)
    :QObject(parent),
     m_comGuestSession(guestSession)
{
    prepareListener();
}

void UIGuestSessionEventHandler::prepareListener()
{
    if(!m_comGuestSession.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Get CGuest event source: */
    CEventSource comEventSourceGuestSession = m_comGuestSession.GetEventSource();
    AssertWrapperOk(comEventSourceGuestSession);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged
               << KVBoxEventType_OnGuestProcessRegistered
               << KVBoxEventType_OnGuestProcessStateChanged
               << KVBoxEventType_OnGuestProcessInputNotify
               << KVBoxEventType_OnGuestProcessOutput
               << KVBoxEventType_OnGuestFileRegistered
               << KVBoxEventType_OnGuestFileStateChanged
               << KVBoxEventType_OnGuestFileOffsetChanged
               << KVBoxEventType_OnGuestFileRead
               << KVBoxEventType_OnGuestFileWrite;


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


void UIGuestSessionEventHandler::cleanupListener()
{
}

UIInformationGuestSession::UIInformationGuestSession(QWidget *pParent, const CMachine &machine,
                                                     const CConsole &console)
    : QWidget(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pGuestEventHandler(0)
{
    prepareLayout();
    prepareWidgets();
    prepareEventHandler();
}

void UIInformationGuestSession::prepareLayout()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (m_pMainLayout)
    {
        /* Configure layout: */
        m_pMainLayout->setSpacing(0);
    }
}

void UIInformationGuestSession::prepareWidgets()
{
    CGuest guest = m_console.GetGuest();
    QVector<CGuestSession> guestSessionVector = guest.GetSessions();

    QITreeWidget *treeWidget = new QITreeWidget();
    treeWidget->setColumnCount(2);
    QList<QITreeWidgetItem *> items;
    for (int i = 0; i < 10; ++i)
    {
        QStringList strList;
        strList << QString("item %1").arg(i) << "dataa";
        QITreeWidgetItem* item = new QITreeWidgetItem(treeWidget, strList);
        //items.append(new QITreeWidgetItem((QITreeWidget*)0, QStringList(QString("item: %1").arg(i))));
        treeWidget->insertTopLevelItem(i, item);

        if(i == 3)
        {
            QStringList strList;
            strList << "child" << "dataa";
            new QITreeWidgetItem(item, strList);

        }
    }
    m_pMainLayout->addWidget(treeWidget);
}

void UIInformationGuestSession::prepareEventHandler()
{
    CGuest guest = m_console.GetGuest();
    m_pGuestEventHandler = new UIGuestEventHandler(this, m_console.GetGuest());
    connect(m_pGuestEventHandler, &UIGuestEventHandler::sigGuestSessionRegistered,
            this, &UIInformationGuestSession::sltGuestSessionRegistered);
}

void UIInformationGuestSession::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if(!guestSession.isOk())
        return;
    m_pGuestSessionHandleVector.push_back(new UIGuestSessionEventHandler(this, guestSession));

}

#include "UIInformationGuestSession.moc"
