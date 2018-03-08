/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlWidget class implementation.
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
# include <QSplitter>
# include <QVBoxLayout>


/* GUI includes: */
# include "QITreeWidget.h"
# include "UIExtraDataManager.h"
# include "UIGuestControlConsole.h"
# include "UIGuestControlInterface.h"
# include "UIGuestControlTreeItem.h"
# include "UIGuestControlWidget.h"
# include "UIVMInformationDialog.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"
# include "CEventSource.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

class UIGuestControlTreeWidget : public QITreeWidget
{

    Q_OBJECT;

signals:

    void sigCloseSessionOrProcess();

public:

    UIGuestControlTreeWidget(QWidget *pParent = 0)
        :QITreeWidget(pParent)
    {
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

    UIGuestControlTreeItem *selectedItem()
    {
        QList<QTreeWidgetItem*> selectedList = selectedItems();
        if (selectedList.isEmpty())
            return 0;
        UIGuestControlTreeItem *item =
            dynamic_cast<UIGuestControlTreeItem*>(selectedList[0]);
        /* Return the firstof the selected items */
        return item;
    }

protected:

    void contextMenuEvent(QContextMenuEvent *pEvent) /* override */
    {
        QMenu *menu = new QMenu(this);
        QList<QTreeWidgetItem *> selectedList = selectedItems();

        UIGuestSessionTreeItem *sessionTreeItem = 0;
        if (!selectedList.isEmpty())
            sessionTreeItem = dynamic_cast<UIGuestSessionTreeItem*>(selectedList[0]);
        QAction *pSessionCloseAction = 0;

        /* Create a guest session related context menu */
        if (sessionTreeItem)
        {
            pSessionCloseAction = menu->addAction(UIVMInformationDialog::tr("Close Session"));
            if (pSessionCloseAction)
                connect(pSessionCloseAction, &QAction::triggered,
                        this, &UIGuestControlTreeWidget::sigCloseSessionOrProcess);
        }
        UIGuestProcessTreeItem *processTreeItem = 0;
        if (!selectedList.isEmpty())
            processTreeItem = dynamic_cast<UIGuestProcessTreeItem*>(selectedList[0]);
        QAction *pProcessTerminateAction = 0;
        if (processTreeItem)
        {
            pProcessTerminateAction = menu->addAction(UIVMInformationDialog::tr("Terminate Process"));
            if (pProcessTerminateAction)
                connect(pProcessTerminateAction, &QAction::triggered,
                        this, &UIGuestControlTreeWidget::sigCloseSessionOrProcess);
        }
        if (pProcessTerminateAction || pSessionCloseAction)
            menu->addSeparator();
        // Add actions to expand/collapse all tree items
        QAction *pExpandAllAction = menu->addAction(UIVMInformationDialog::tr("Expand All"));
        if (pExpandAllAction)
            connect(pExpandAllAction, &QAction::triggered,
                    this, &UIGuestControlTreeWidget::sltExpandAll);
        QAction *pCollapseAllAction = menu->addAction(UIVMInformationDialog::tr("Collapse All"));
        if (pCollapseAllAction)
            connect(pCollapseAllAction, &QAction::triggered,
                    this, &UIGuestControlTreeWidget::sltCollapseAll);

        menu->exec(pEvent->globalPos());

        if (pSessionCloseAction)
            disconnect(pSessionCloseAction, &QAction::triggered,
                       this, &UIGuestControlTreeWidget::sigCloseSessionOrProcess);
        if (pProcessTerminateAction)
            disconnect(pProcessTerminateAction, &QAction::triggered,
                       this, &UIGuestControlTreeWidget::sigCloseSessionOrProcess);

        if (pExpandAllAction)
            disconnect(pExpandAllAction, &QAction::triggered,
                    this, &UIGuestControlTreeWidget::sltExpandAll);

        if (pCollapseAllAction)
            disconnect(pCollapseAllAction, &QAction::triggered,
                    this, &UIGuestControlTreeWidget::sltCollapseAll);

        delete menu;
    }

private slots:

    void sltExpandAll()
    {
        expandCollapseAll(true);
    }

    void sltCollapseAll()
    {
        expandCollapseAll(false);
    }

    void expandCollapseAll(bool bFlag)
    {
        for (int i = 0; i < topLevelItemCount(); ++i)
        {
            if (!topLevelItem(i))
                break;
            topLevelItem(i)->setExpanded(bFlag);
            for (int j = 0; j < topLevelItem(i)->childCount(); ++j)
            {
                if (topLevelItem(i)->child(j))
                {
                    topLevelItem(i)->child(j)->setExpanded(bFlag);
                }
            }
        }
    }

};

UIGuestControlWidget::UIGuestControlWidget(QWidget *pParent, const CGuest &comGuest)
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
    initGuestSessionTree();
}

void UIGuestControlWidget::prepareObjects()
{
    m_pControlInterface = new UIGuestControlInterface(this, m_comGuest);

    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);

    m_pSplitter = new QSplitter;

    if (!m_pSplitter)
        return;

    m_pSplitter->setOrientation(Qt::Vertical);

    m_pMainLayout->addWidget(m_pSplitter);


    m_pTreeWidget = new UIGuestControlTreeWidget;

    if (m_pTreeWidget)
    {
        m_pSplitter->addWidget(m_pTreeWidget);
        m_pTreeWidget->setColumnCount(3);
        QStringList labels;
        labels << "" << "" << "";

        m_pTreeWidget->setHeaderLabels(labels);
    }
    m_pConsole = new UIGuestControlConsole;
    if (m_pConsole)
    {
        m_pSplitter->addWidget(m_pConsole);
        setFocusProxy(m_pConsole);
    }

    m_pSplitter->setStretchFactor(0, 9);
    m_pSplitter->setStretchFactor(1, 4);

    updateTreeWidget();
}

void UIGuestControlWidget::updateTreeWidget()
{
    if (!m_pTreeWidget)
        return;

    m_pTreeWidget->clear();
    QVector<QITreeWidgetItem> treeItemVector;
    update();
}

void UIGuestControlWidget::prepareConnections()
{
    qRegisterMetaType<QVector<int> >();
    connect(m_pControlInterface, &UIGuestControlInterface::sigOutputString,
            this, &UIGuestControlWidget::sltConsoleOutputReceived);
    connect(m_pConsole, &UIGuestControlConsole::commandEntered,
            this, &UIGuestControlWidget::sltConsoleCommandEntered);

    if (m_pTreeWidget)
        connect(m_pTreeWidget, &UIGuestControlTreeWidget::sigCloseSessionOrProcess,
                this, &UIGuestControlWidget::sltCloseSessionOrProcess);

    if (m_pQtListener)
    {
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
                this, &UIGuestControlWidget::sltGuestSessionRegistered, Qt::DirectConnection);
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIGuestControlWidget::sltGuestSessionUnregistered, Qt::DirectConnection);
    }
}

void UIGuestControlWidget::sltGuestSessionsUpdated()
{
    updateTreeWidget();
}

void UIGuestControlWidget::sltConsoleCommandEntered(const QString &strCommand)
{
    if (m_pControlInterface)
    {
        m_pControlInterface->putCommand(strCommand);
    }
}

void UIGuestControlWidget::sltConsoleOutputReceived(const QString &strOutput)
{
    if (m_pConsole)
    {
        m_pConsole->putOutput(strOutput);
    }
}

void UIGuestControlWidget::sltCloseSessionOrProcess()
{
    if (!m_pTreeWidget)
        return;
    UIGuestControlTreeItem *selectedTreeItem =
        m_pTreeWidget->selectedItem();
    if (!selectedTreeItem)
        return;
    UIGuestProcessTreeItem *processTreeItem =
        dynamic_cast<UIGuestProcessTreeItem*>(selectedTreeItem);
    if (processTreeItem)
    {
        CGuestProcess guestProcess = processTreeItem->guestProcess();
        if (guestProcess.isOk())
        {
            guestProcess.Terminate();
        }
        return;
    }
    UIGuestSessionTreeItem *sessionTreeItem =
        dynamic_cast<UIGuestSessionTreeItem*>(selectedTreeItem);
    if (!sessionTreeItem)
        return;
    CGuestSession guestSession = sessionTreeItem->guestSession();
    if (!guestSession.isOk())
        return;
    guestSession.Close();
}

void UIGuestControlWidget::prepareListener()
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

void UIGuestControlWidget::initGuestSessionTree()
{
    if (!m_comGuest.isOk())
        return;

    QVector<CGuestSession> sessions = m_comGuest.GetSessions();
    for (int i = 0; i < sessions.size(); ++i)
    {
        addGuestSession(sessions.at(i));
    }
}

void UIGuestControlWidget::cleanupListener()
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


void UIGuestControlWidget::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    addGuestSession(guestSession);
}

void UIGuestControlWidget::addGuestSession(CGuestSession guestSession)
{
    UIGuestSessionTreeItem* sessionTreeItem = new UIGuestSessionTreeItem(m_pTreeWidget, guestSession);
    connect(sessionTreeItem, &UIGuestSessionTreeItem::sigGuessSessionUpdated,
            this, &UIGuestControlWidget::sltTreeItemUpdated);
    connect(sessionTreeItem, &UIGuestSessionTreeItem::sigGuestSessionErrorText,
            this, &UIGuestControlWidget::sltGuestControlErrorText);
}

void UIGuestControlWidget::sltGuestControlErrorText(QString strError)
{
    if (m_pConsole)
    {
        m_pConsole->putOutput(strError);
    }
}

void UIGuestControlWidget::sltTreeItemUpdated()
{
    if (m_pTreeWidget)
        m_pTreeWidget->update();
}

void UIGuestControlWidget::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    if (!m_pTreeWidget)
        return;

    UIGuestSessionTreeItem *selectedItem = NULL;


    for (int i = 0; i < m_pTreeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_pTreeWidget->topLevelItem( i );

        UIGuestSessionTreeItem *treeItem = dynamic_cast<UIGuestSessionTreeItem*>(item);
        if (treeItem && treeItem->guestSession() == guestSession)
        {
            selectedItem = treeItem;
            break;
        }
    }
    delete selectedItem;
}

#include "UIGuestControlWidget.moc"
