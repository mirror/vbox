/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestProcessControlWidget class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITreeWidget.h"
#include "UIActionPool.h"
#include "UIExtraDataManager.h"
#include "UIGuestControlConsole.h"
#include "UIGuestControlInterface.h"
#include "UIGuestControlTreeItem.h"
#include "UIGuestProcessControlWidget.h"
#include "UIToolBar.h"
#include "UIVMInformationDialog.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CGuest.h"
#include "CEventSource.h"


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
            pSessionCloseAction = menu->addAction(tr("Close Session"));
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
            pProcessTerminateAction = menu->addAction(tr("Terminate Process"));
            if (pProcessTerminateAction)
                connect(pProcessTerminateAction, &QAction::triggered,
                        this, &UIGuestControlTreeWidget::sigCloseSessionOrProcess);
        }
        if (pProcessTerminateAction || pSessionCloseAction)
            menu->addSeparator();
        // Add actions to expand/collapse all tree items
        QAction *pExpandAllAction = menu->addAction(tr("Expand All"));
        if (pExpandAllAction)
            connect(pExpandAllAction, &QAction::triggered,
                    this, &UIGuestControlTreeWidget::sltExpandAll);
        QAction *pCollapseAllAction = menu->addAction(tr("Collapse All"));
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

UIGuestProcessControlWidget::UIGuestProcessControlWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                         const CGuest &comGuest, QWidget *pParent, bool fShowToolbar /* = false */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_comGuest(comGuest)
    , m_pMainLayout(0)
    , m_pSplitter(0)
    , m_pTreeWidget(0)
    , m_pConsole(0)
    , m_pControlInterface(0)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_pToolBar(0)
    , m_pQtListener(0)
    , m_fShowToolbar(fShowToolbar)
{
    prepareListener();
    prepareObjects();
    prepareConnections();
    prepareToolBar();
    initGuestSessionTree();
    loadSettings();
}

UIGuestProcessControlWidget::~UIGuestProcessControlWidget()
{
    saveSettings();
}

void UIGuestProcessControlWidget::retranslateUi()
{
}


void UIGuestProcessControlWidget::prepareObjects()
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

    m_pSplitter->setStretchFactor(0, 2);
    m_pSplitter->setStretchFactor(1, 1);

    updateTreeWidget();
}

void UIGuestProcessControlWidget::updateTreeWidget()
{
    if (!m_pTreeWidget)
        return;

    m_pTreeWidget->clear();
    QVector<QITreeWidgetItem> treeItemVector;
    update();
}

void UIGuestProcessControlWidget::prepareConnections()
{
    qRegisterMetaType<QVector<int> >();
    connect(m_pControlInterface, &UIGuestControlInterface::sigOutputString,
            this, &UIGuestProcessControlWidget::sltConsoleOutputReceived);
    connect(m_pConsole, &UIGuestControlConsole::commandEntered,
            this, &UIGuestProcessControlWidget::sltConsoleCommandEntered);

    if (m_pTreeWidget)
        connect(m_pTreeWidget, &UIGuestControlTreeWidget::sigCloseSessionOrProcess,
                this, &UIGuestProcessControlWidget::sltCloseSessionOrProcess);

    if (m_pQtListener)
    {
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
                this, &UIGuestProcessControlWidget::sltGuestSessionRegistered);
        connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIGuestProcessControlWidget::sltGuestSessionUnregistered);
    }
}

void UIGuestProcessControlWidget::sltGuestSessionsUpdated()
{
    updateTreeWidget();
}

void UIGuestProcessControlWidget::sltConsoleCommandEntered(const QString &strCommand)
{
    if (m_pControlInterface)
    {
        m_pControlInterface->putCommand(strCommand);
    }
}

void UIGuestProcessControlWidget::sltConsoleOutputReceived(const QString &strOutput)
{
    if (m_pConsole)
    {
        m_pConsole->putOutput(strOutput);
    }
}

void UIGuestProcessControlWidget::sltCloseSessionOrProcess()
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

void UIGuestProcessControlWidget::prepareListener()
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

void UIGuestProcessControlWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        /* Add toolbar actions: */
        m_pToolBar->addSeparator();
        m_pToolBar->addSeparator();

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolBar);
#endif
    }
}

void UIGuestProcessControlWidget::initGuestSessionTree()
{
    if (!m_comGuest.isOk())
        return;

    QVector<CGuestSession> sessions = m_comGuest.GetSessions();
    for (int i = 0; i < sessions.size(); ++i)
    {
        addGuestSession(sessions.at(i));
    }
}

void UIGuestProcessControlWidget::cleanupListener()
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


void UIGuestProcessControlWidget::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (!guestSession.isOk())
        return;
    addGuestSession(guestSession);
}

void UIGuestProcessControlWidget::addGuestSession(CGuestSession guestSession)
{
    UIGuestSessionTreeItem* sessionTreeItem = new UIGuestSessionTreeItem(m_pTreeWidget, guestSession);
    connect(sessionTreeItem, &UIGuestSessionTreeItem::sigGuessSessionUpdated,
            this, &UIGuestProcessControlWidget::sltTreeItemUpdated);
    connect(sessionTreeItem, &UIGuestSessionTreeItem::sigGuestSessionErrorText,
            this, &UIGuestProcessControlWidget::sltGuestControlErrorText);
}

void UIGuestProcessControlWidget::sltGuestControlErrorText(QString strError)
{
    if (m_pConsole)
    {
        m_pConsole->putOutput(strError);
    }
}

void UIGuestProcessControlWidget::sltTreeItemUpdated()
{
    if (m_pTreeWidget)
        m_pTreeWidget->update();
}

void UIGuestProcessControlWidget::sltGuestSessionUnregistered(CGuestSession guestSession)
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

void UIGuestProcessControlWidget::saveSettings()
{
    if (!m_pSplitter)
        return;
    gEDataManager->setGuestControlProcessControlSplitterHints(m_pSplitter->sizes());
}

void UIGuestProcessControlWidget::loadSettings()
{
    if (!m_pSplitter)
        return;
    QList<int> splitterHints = gEDataManager->guestControlProcessControlSplitterHints();
    if (splitterHints.size() != 2)
        return;
    if (splitterHints[0] != 0 && splitterHints[1] != 0)
        m_pSplitter->setSizes(splitterHints);
}

#include "UIGuestProcessControlWidget.moc"
