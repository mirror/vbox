/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsPane class implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
# include <QStackedWidget>
# include <QTabBar>

/* GUI includes */
# include "QIToolButton.h"
# include "UIHostNetworkManager.h"
# include "UIIconPool.h"
# include "UIMediumManager.h"
# include "UIMenuToolBar.h"
# include "UISnapshotPane.h"
# include "UIToolsPane.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsPane::UIToolsPane(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLayoutMain(0)
    , m_pStackedWidget(0)
    , m_pPaneSnapshots(0)
    , m_pLayoutControls(0)
    , m_pTabBar(0)
    , m_pMenuToolbar(0)
    , m_pMenu(0)
{
    /* Prepare: */
    prepare();
}

void UIToolsPane::setMachine(const CMachine &comMachine)
{
    /* Update the panes: */
    AssertPtrReturnVoid(m_pPaneSnapshots);
    m_pPaneSnapshots->setMachine(comMachine);
}

void UIToolsPane::retranslateUi()
{
    /* Translate menu: */
    m_pMenu->menuAction()->setText(tr("More Tools"));
    m_pMenu->setToolTip(tr("Holds a list of tools"));
    m_pMenu->setTitle(tr("Tools"));

    /* Translate actions: */
    m_actions[ToolsType_SnapshotManager]->setText(tr("Snapshot Manager"));
    m_actions[ToolsType_VirtualMediaManager]->setText(tr("Virtual Media Manager"));
    m_actions[ToolsType_HostNetworkManager]->setText(tr("Host Network Manager"));

    /* Translate tab-bar: */
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
    {
        const ToolsType enmType = m_pTabBar->tabData(iTabIndex).value<ToolsType>();
        m_pTabBar->setTabText(iTabIndex, m_actions.value(enmType)->text());
    }
}

void UIToolsPane::sltHandleMenuToolbarTrigger()
{
    /* Get the sender: */
    QAction *pAction = sender() ? qobject_cast<QAction*>(sender()) : 0;
    AssertPtrReturnVoid(pAction);

    /* Acquire sender's type: */
    const ToolsType enmType = pAction->property("ToolsType").value<ToolsType>();
    AssertReturnVoid(enmType != ToolsType_Invalid);

    /* Add corresponding tab: */
    addTabBarTab(enmType, true);
}

void UIToolsPane::sltHandleTabBarTabMoved(int iFrom, int iTo)
{
    /* Swap stack-widget pages as well: */
    QWidget *pWidget = m_pStackedWidget->widget(iFrom);
    m_pStackedWidget->removeWidget(pWidget);
    m_pStackedWidget->insertWidget(iTo, pWidget);
}

void UIToolsPane::sltHandleTabBarCurrentChange(int iIndex)
{
    /* Activate corresponding indexes: */
    m_pStackedWidget->setCurrentIndex(iIndex);
}

void UIToolsPane::sltHandleTabBarButtonClick()
{
    /* Get the sender: */
    QIToolButton *pButton = sender() ? qobject_cast<QIToolButton*>(sender()) : 0;
    AssertPtrReturnVoid(pButton);

    /* Acquire sender's type: */
    const ToolsType enmType = pButton->property("ToolsType").value<ToolsType>();
    AssertReturnVoid(enmType != ToolsType_Invalid);

    /* Search for the tab with such type: */
    int iActualTabIndex = -1;
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
        if (m_pTabBar->tabData(iTabIndex).value<ToolsType>() == enmType)
            iActualTabIndex = iTabIndex;
    AssertReturnVoid(iActualTabIndex != -1);

    /* Delete the tab and corresponding widget: */
    m_pTabBar->removeTab(iActualTabIndex);
    QWidget *pWidget = m_pStackedWidget->widget(iActualTabIndex);
    m_pStackedWidget->removeWidget(pWidget);
    delete pWidget;
}

void UIToolsPane::prepare()
{
    /* Create main layout: */
    m_pLayoutMain = new QVBoxLayout(this);
    AssertPtrReturnVoid(m_pLayoutMain);
    {
        /* Configure layout: */
        m_pLayoutMain->setSpacing(0);
        m_pLayoutMain->setContentsMargins(3, 4, 5, 0);

        /* Prepare stacked-widget: */
        prepareStackedWidget();

        /* Create controls layout: */
        m_pLayoutControls = new QHBoxLayout;
        AssertPtrReturnVoid(m_pLayoutControls);
        {
            /* Configure layout: */
            m_pLayoutControls->setSpacing(0);
            m_pLayoutControls->setContentsMargins(0, 0, 0, 0);

            /* Prepare tab-bar: */
            prepareTabBar();
            /* Prepare menu-toolbar: */
            prepareMenuToolbar();
            /* Add Snapshot Manager pane: */
            addTabBarTab(ToolsType_SnapshotManager, false);

            /* Add into layout: */
            m_pLayoutMain->addLayout(m_pLayoutControls);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIToolsPane::prepareStackedWidget()
{
    /* Create stacked-widget: */
    m_pStackedWidget = new QStackedWidget;
    AssertPtrReturnVoid(m_pStackedWidget);
    {
        /* Add into layout: */
        m_pLayoutMain->addWidget(m_pStackedWidget);
    }
}

void UIToolsPane::prepareTabBar()
{
    /* Create tab-bar: */
    m_pTabBar = new QTabBar;
    AssertPtrReturnVoid(m_pTabBar);
    {
        /* Configure tab-bar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.375;
        m_pTabBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pTabBar->setMovable(true);
        m_pTabBar->setDrawBase(false);
        m_pTabBar->setExpanding(false);
        m_pTabBar->setShape(QTabBar::RoundedSouth);
        m_pTabBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        connect(m_pTabBar, &QTabBar::tabMoved, this, &UIToolsPane::sltHandleTabBarTabMoved);
        connect(m_pTabBar, &QTabBar::currentChanged, this, &UIToolsPane::sltHandleTabBarCurrentChange);

        /* Add into layout: */
        m_pLayoutControls->addWidget(m_pTabBar);
    }
}

void UIToolsPane::prepareMenuToolbar()
{
    /* Create menu-toolbar: */
    m_pMenuToolbar = new UIMenuToolBar;
    AssertPtrReturnVoid(m_pMenuToolbar);
    {
        /* Configure menu-toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pMenuToolbar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pMenuToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_pMenuToolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        m_pMenuToolbar->setAlignmentType(UIMenuToolBar::AlignmentType_BottomRight);

        /* Prepare menu: */
        prepareMenu();

        /* Add into layout: */
        m_pLayoutControls->addWidget(m_pMenuToolbar);
    }
}

void UIToolsPane::prepareMenu()
{
    /* Create menu: */
    m_pMenu = new QMenu(m_pMenuToolbar);
    AssertPtrReturnVoid(m_pMenu);
    {
        /* Configure menus: */
        m_pMenu->setIcon(UIIconPool::iconSet(":/vm_settings_16px.png"));

        /* Create SnapShot manager action: */
        m_actions[ToolsType_SnapshotManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/snapshot_manager_22px.png",
                                                                                          ":/snapshot_manager_16px.png"),
                                                                  QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolsType_SnapshotManager]->
                setProperty("ToolsType", QVariant::fromValue(ToolsType_SnapshotManager));
        }

        /* Create Virtual Media manager action: */
        m_actions[ToolsType_VirtualMediaManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/diskimage_22px.png",
                                                                                              ":/diskimage_16px.png"),
                                                                      QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolsType_VirtualMediaManager]->
                setProperty("ToolsType", QVariant::fromValue(ToolsType_VirtualMediaManager));
        }

        /* Create Host Network manager action: */
        m_actions[ToolsType_HostNetworkManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/host_iface_manager_22px.png",
                                                                                             ":/host_iface_manager_16px.png"),
                                                                     QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolsType_HostNetworkManager]->
                setProperty("ToolsType", QVariant::fromValue(ToolsType_HostNetworkManager));
        }

        /* Add as tool-button into tool-bar: */
        m_pMenuToolbar->setMenuAction(m_pMenu->menuAction());
    }
}

void UIToolsPane::addTabBarTab(ToolsType enmType, bool fCloseable)
{
    /* Search for a tab with such type: */
    int iActualTabIndex = -1;
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
        if (m_pTabBar->tabData(iTabIndex).value<ToolsType>() == enmType)
            iActualTabIndex = iTabIndex;

    /* If tab with such type doesn't exist: */
    m_pTabBar->blockSignals(true);
    if (iActualTabIndex == -1)
    {
        /* Append stack-widget with corresponding page: */
        switch (enmType)
        {
            case ToolsType_SnapshotManager:
                m_pPaneSnapshots = new UISnapshotPane;
                m_pStackedWidget->addWidget(m_pPaneSnapshots);
                break;
            case ToolsType_VirtualMediaManager:
                m_pStackedWidget->addWidget(new UIMediumManagerWidget(EmbedTo_Stack));
                break;
            case ToolsType_HostNetworkManager:
                m_pStackedWidget->addWidget(new UIHostNetworkManagerWidget(EmbedTo_Stack));
                break;
            default:
                AssertFailedReturnVoid();
        }

        /* Append tab-bar with corresponding tab: */
        iActualTabIndex = m_pTabBar->addTab(m_actions.value(enmType)->icon(), m_actions.value(enmType)->text());
        {
            /* Create close button if requested: */
            if (fCloseable)
            {
                QIToolButton *pButtonClose = new QIToolButton;
                AssertPtrReturnVoid(pButtonClose);
                {
                    /* Configure button: */
                    pButtonClose->setIcon(UIIconPool::iconSet(":/close_16px.png"));
                    pButtonClose->setProperty("ToolsType", QVariant::fromValue(enmType));
                    connect(pButtonClose, &QIToolButton::clicked, this, &UIToolsPane::sltHandleTabBarButtonClick);
                    /* Add into tab-bar: */
                    m_pTabBar->setTabButton(iActualTabIndex, QTabBar::RightSide, pButtonClose);
                }
            }
            /* Store the data: */
            m_pTabBar->setTabData(iActualTabIndex, QVariant::fromValue(enmType));
        }
    }

    /* Activate corresponding indexes: */
    m_pStackedWidget->setCurrentIndex(iActualTabIndex);
    m_pTabBar->setCurrentIndex(iActualTabIndex);
    m_pTabBar->blockSignals(false);
}

