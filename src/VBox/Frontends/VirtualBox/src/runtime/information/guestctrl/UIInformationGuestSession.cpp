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
# include <QApplication>
# include <QVBoxLayout>
# include <QSplitter>

/* GUI includes: */
# include "QITreeWidget.h"
# include "UIExtraDataManager.h"
# include "UIGuestControlConsole.h"
# include "UIGuestControlInterface.h"
# include "UIGuestControlTreeItem.h"
# include "UIInformationGuestSession.h"

# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"
# include "CEventSource.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationGuestSession::UIInformationGuestSession(QWidget *pParent, const CGuest &comGuest)
    : QWidget(pParent)
    , m_comGuest(comGuest)
    , m_pMainLayout(0)
    , m_pSplitter(0)
    , m_pTreeWidget(0)
    , m_pConsole(0)
    , m_pControlInterface(0)
    , m_pQtListener(0)
{
    prepareListener();
    prepareObjects();
    prepareConnections();
}

void UIInformationGuestSession::prepareObjects()
{
    m_pControlInterface = new UIGuestControlInterface(this, m_comGuest);

    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if(!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);

    m_pSplitter = new QSplitter;

    if(!m_pSplitter)
        return;

    m_pSplitter->setOrientation(Qt::Vertical);

    m_pMainLayout->addWidget(m_pSplitter);


    m_pTreeWidget = new QITreeWidget;

    if (m_pTreeWidget)
        m_pSplitter->addWidget(m_pTreeWidget);

    m_pConsole = new UIGuestControlConsole;
    if(m_pConsole)
    {
        m_pSplitter->addWidget(m_pConsole);
        setFocusProxy(m_pConsole);
    }

    m_pSplitter->setStretchFactor(0, 9);
    m_pSplitter->setStretchFactor(1, 4);

    updateTreeWidget();
}

void UIInformationGuestSession::updateTreeWidget()
{
    if (!m_pTreeWidget)
        return;

    m_pTreeWidget->clear();
    QVector<QITreeWidgetItem> treeItemVector;
    update();
}

void UIInformationGuestSession::prepareConnections()
{
    connect(m_pControlInterface, &UIGuestControlInterface::sigOutputString,
            this, &UIInformationGuestSession::sltConsoleOutputReceived);
    connect(m_pConsole, &UIGuestControlConsole::commandEntered,
            this, &UIInformationGuestSession::sltConsoleCommandEntered);
    if(m_pQtListener)
    {
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
                this, &UIInformationGuestSession::sltGuestSessionRegistered, Qt::DirectConnection);
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIInformationGuestSession::sltGuestSessionUnregistered, Qt::DirectConnection);
    }
}

void UIInformationGuestSession::sltGuestSessionsUpdated()
{
    updateTreeWidget();
}

void UIInformationGuestSession::sltConsoleCommandEntered(const QString &strCommand)
{
    if(m_pControlInterface)
    {
        m_pControlInterface->putCommand(strCommand);
    }
}

void UIInformationGuestSession::sltConsoleOutputReceived(const QString &strOutput)
{
    if(m_pConsole)
    {
        m_pConsole->putOutput(strOutput);
    }
}

void UIInformationGuestSession::prepareListener()
{
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Get CProgress event source: */
    CEventSource comEventSource = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSource);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionRegistered;


    /* Register event listener for CProgress event source: */
    comEventSource.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(comEventSource);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSource, m_comEventListener);
    }
}

void UIInformationGuestSession::cleanupListener()
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

    /* Get CProgress event source: */
    CEventSource comEventSource = m_comGuest.GetEventSource();
    AssertWrapperOk(comEventSource);

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(m_comEventListener);
}


void UIInformationGuestSession::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;

    new UIGuestSessionTreeItem(m_pTreeWidget, guestSession);
    //printf("sltGuestSessionRegistered \n");
    // addGuestSession(guestSession);
    // emit sigGuestSessionUpdated();
}

void UIInformationGuestSession::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    // removeGuestSession(guestSession);
    // emit sigGuestSessionUpdated();
}
