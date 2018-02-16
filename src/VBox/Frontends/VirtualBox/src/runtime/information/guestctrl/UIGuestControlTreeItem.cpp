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
    prepareConnections();
    prepareConnections();
}

void UIGuestControlTreeItem::prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes)
{
    AssertWrapperOk(comEventSource);

    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

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

void UIGuestControlTreeItem::cleanupListener(CEventSource comEventSource)
{
    AssertWrapperOk(comEventSource);
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

void UIGuestSessionTreeItem::prepareConnections()
{
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIGuestSessionTreeItem::sltGuestSessionUpdated);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessRegistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessRegistered);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessUnregistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessUnregistered);
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
}

void UIGuestSessionTreeItem::sltGuestProcessRegistered(CGuestProcess guestProcess)
{

}

void UIGuestSessionTreeItem::sltGuestProcessUnregistered(CGuestProcess guestProcess)
{
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
            this, &UIGuestProcessTreeItem::sltGuestProcessUpdated);
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
}
