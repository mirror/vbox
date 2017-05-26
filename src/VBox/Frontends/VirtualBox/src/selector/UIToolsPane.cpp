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
# include <QStackedLayout>
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
    , m_pStackedLayout(0)
    , m_pPaneSnapshots(0)
    , m_pLayoutControls(0)
    , m_pTabBar(0)
    , m_pMenuToolbar(0)
    , m_pMenu(0)
{
    /* Prepare: */
    prepare();
}

UIToolsPane::~UIToolsPane()
{
    /* Cleanup: */
    cleanup();
}

void UIToolsPane::setMachine(const CMachine &comMachine)
{
    /* Update snapshots pane: */
    AssertPtrReturnVoid(m_pPaneSnapshots);
    m_pPaneSnapshots->setMachine(comMachine);
}

void UIToolsPane::retranslateUi()
{
    /* Translate menu: */
    m_pMenu->setTitle(tr("Tools"));
    m_pMenu->setToolTip(tr("Holds a list of tools"));

    /* Translate actions: */
    m_actions[ToolType_SnapshotManager]->setText(tr("Snapshot Manager"));
    m_actions[ToolType_VirtualMediaManager]->setText(tr("Virtual Media Manager"));
    m_actions[ToolType_HostNetworkManager]->setText(tr("Host Network Manager"));

    /* Translate tab-bar: */
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
    {
        const ToolType enmType = m_pTabBar->tabData(iTabIndex).value<ToolType>();
        m_pTabBar->setTabText(iTabIndex, m_actions.value(enmType)->text());
    }
}

void UIToolsPane::sltHandleMenuToolbarTrigger()
{
    /* Get the sender: */
    QAction *pAction = sender() ? qobject_cast<QAction*>(sender()) : 0;
    AssertPtrReturnVoid(pAction);

    /* Acquire sender's type: */
    const ToolType enmType = pAction->property("ToolType").value<ToolType>();
    AssertReturnVoid(enmType != ToolType_Invalid);

    /* Activate corresponding tab: */
    activateTabBarTab(enmType, true);
}

void UIToolsPane::sltHandleTabBarTabMoved(int iFrom, int iTo)
{
    /* Swap stack-widget pages as well: */
    QWidget *pWidget = m_pStackedLayout->widget(iFrom);
    m_pStackedLayout->removeWidget(pWidget);
    m_pStackedLayout->insertWidget(iTo, pWidget);
}

void UIToolsPane::sltHandleTabBarCurrentChange(int iIndex)
{
    /* Activate corresponding indexes: */
    m_pStackedLayout->setCurrentIndex(iIndex);
}

void UIToolsPane::sltHandleTabBarButtonClick()
{
    /* Get the sender: */
    QIToolButton *pButton = sender() ? qobject_cast<QIToolButton*>(sender()) : 0;
    AssertPtrReturnVoid(pButton);

    /* Acquire sender's type: */
    const ToolType enmType = pButton->property("ToolType").value<ToolType>();
    AssertReturnVoid(enmType != ToolType_Invalid);

    /* Search for the tab with such type: */
    int iActualTabIndex = -1;
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
        if (m_pTabBar->tabData(iTabIndex).value<ToolType>() == enmType)
            iActualTabIndex = iTabIndex;
    AssertReturnVoid(iActualTabIndex != -1);

    /* Delete the tab and corresponding widget: */
    m_pTabBar->removeTab(iActualTabIndex);
    QWidget *pWidget = m_pStackedLayout->widget(iActualTabIndex);
    m_pStackedLayout->removeWidget(pWidget);
    delete pWidget;

    /* Make sure action is unchecked: */
    m_actions[enmType]->blockSignals(true);
    m_actions[enmType]->setChecked(false);
    m_actions[enmType]->blockSignals(false);
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

        /* Prepare stacked-layout: */
        prepareStackedLayout();

        /* Create controls layout: */
        m_pLayoutControls = new QHBoxLayout;
        AssertPtrReturnVoid(m_pLayoutControls);
        {
            /* Configure layout: */
            m_pLayoutControls->setSpacing(0);
            m_pLayoutControls->setContentsMargins(0, 0, 0, 0);

            /* Prepare tab-bar: */
            prepareTabBar();

            /* Add stretch: */
            m_pLayoutControls->addStretch();

            /* Prepare menu-toolbar: */
            prepareMenuToolbar();

            /* Add and activate snapshots pane: */
            activateTabBarTab(ToolType_SnapshotManager, false);

            /* Add into layout: */
            m_pLayoutMain->addLayout(m_pLayoutControls);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIToolsPane::prepareStackedLayout()
{
    /* Create stacked-layout: */
    m_pStackedLayout = new QStackedLayout;
    AssertPtrReturnVoid(m_pStackedLayout);
    {
        /* Add into layout: */
        m_pLayoutMain->addLayout(m_pStackedLayout);
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
#ifdef VBOX_WS_MAC
        /* Snapshots pane tab looks quite alone under macOS: */
        m_pTabBar->setAutoHide(true);
#endif
        m_pTabBar->setDrawBase(false);
        m_pTabBar->setExpanding(false);
        m_pTabBar->setShape(QTabBar::RoundedSouth);
        m_pTabBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
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
        m_actions[ToolType_SnapshotManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/snapshot_manager_22px.png",
                                                                                          ":/snapshot_manager_16px.png"),
                                                                  QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolType_SnapshotManager]->setCheckable(true);
            m_actions[ToolType_SnapshotManager]->
                setProperty("ToolType", QVariant::fromValue(ToolType_SnapshotManager));
        }

        /* Create Virtual Media manager action: */
        m_actions[ToolType_VirtualMediaManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/diskimage_22px.png",
                                                                                              ":/diskimage_16px.png"),
                                                                      QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolType_VirtualMediaManager]->setCheckable(true);
            m_actions[ToolType_VirtualMediaManager]->
                setProperty("ToolType", QVariant::fromValue(ToolType_VirtualMediaManager));
        }

        /* Create Host Network manager action: */
        m_actions[ToolType_HostNetworkManager] = m_pMenu->addAction(UIIconPool::iconSetFull(":/host_iface_manager_22px.png",
                                                                                             ":/host_iface_manager_16px.png"),
                                                                     QString(), this, &UIToolsPane::sltHandleMenuToolbarTrigger);
        {
            m_actions[ToolType_HostNetworkManager]->setCheckable(true);
            m_actions[ToolType_HostNetworkManager]->
                setProperty("ToolType", QVariant::fromValue(ToolType_HostNetworkManager));
        }

        /* Add as tool-button into tool-bar: */
        m_pMenuToolbar->setMenuAction(m_pMenu->menuAction());
    }
}

void UIToolsPane::cleanup()
{
    /* Remove all tab prematurelly: */
    while (m_pTabBar->count())
    {
        m_pTabBar->removeTab(0);
        QWidget *pWidget = m_pStackedLayout->widget(0);
        m_pStackedLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolsPane::activateTabBarTab(ToolType enmType, bool fCloseable)
{
    /* Search for a tab with such type: */
    int iActualTabIndex = -1;
    for (int iTabIndex = 0; iTabIndex < m_pTabBar->count(); ++iTabIndex)
        if (m_pTabBar->tabData(iTabIndex).value<ToolType>() == enmType)
            iActualTabIndex = iTabIndex;

    /* If tab with such type doesn't exist: */
    m_pTabBar->blockSignals(true);
    if (iActualTabIndex == -1)
    {
        /* Append stack-widget with corresponding page: */
        switch (enmType)
        {
            case ToolType_SnapshotManager:
                m_pPaneSnapshots = new UISnapshotPane;
                m_pStackedLayout->addWidget(m_pPaneSnapshots);
                break;
            case ToolType_VirtualMediaManager:
                m_pStackedLayout->addWidget(new UIMediumManagerWidget(EmbedTo_Stack));
                break;
            case ToolType_HostNetworkManager:
                m_pStackedLayout->addWidget(new UIHostNetworkManagerWidget(EmbedTo_Stack));
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
#ifdef VBOX_WS_MAC
                    /* Close buttons should be a bit smaller on macOS: */
                    pButtonClose->setIconSize(QSize(12, 12));
#endif
                    pButtonClose->setIcon(UIIconPool::iconSet(":/close_16px.png"));
                    pButtonClose->setProperty("ToolType", QVariant::fromValue(enmType));
                    connect(pButtonClose, &QIToolButton::clicked, this, &UIToolsPane::sltHandleTabBarButtonClick);
                    /* Add into tab-bar: */
                    m_pTabBar->setTabButton(iActualTabIndex, QTabBar::RightSide, pButtonClose);
                }
            }
#ifdef VBOX_WS_MAC
            /* Create placeholder of the same height if on macOS: */
            else
            {
                QWidget *pWidget = new QWidget;
                pWidget->setFixedSize(1, 26);
                m_pTabBar->setTabButton(iActualTabIndex, QTabBar::RightSide, pWidget);
            }
#endif
            /* Store the data: */
            m_pTabBar->setTabData(iActualTabIndex, QVariant::fromValue(enmType));
        }
    }

    /* Activate corresponding indexes: */
    m_pStackedLayout->setCurrentIndex(iActualTabIndex);
    m_pTabBar->setCurrentIndex(iActualTabIndex);
    m_pTabBar->blockSignals(false);

    /* Make sure action is checked: */
    m_actions[enmType]->blockSignals(true);
    m_actions[enmType]->setChecked(true);
    m_actions[enmType]->blockSignals(false);
}

