/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityToolWidget class implementation.
 */

/*
 * Copyright (C) 2009-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QApplication>
#include <QVBoxLayout>
#include <QStyle>
#include <QTabWidget>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIActionPoolManager.h"
#include "UIGlobalSession.h"
#include "UIVMActivityMonitor.h"
#include "UIVMActivityToolWidget.h"
#include "UIMessageCenter.h"
#include "UIVirtualMachineItem.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"
#include "UIVMActivityMonitorPaneContainer.h"
#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "assert.h"
#include "CMachine.h"


UIVMActivityToolWidget::UIVMActivityToolWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                 bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pExportToFileAction(0)
    , m_pPaneContainer(0)
    , m_pTabWidget(0)
{
    prepare();
    prepareActions();
    prepareToolBar();
    sltCurrentTabChanged(0);
}

QMenu *UIVMActivityToolWidget::menu() const
{
    return NULL;
}

bool UIVMActivityToolWidget::isCurrentTool() const
{
    return m_fIsCurrentTool;
}

void UIVMActivityToolWidget::setIsCurrentTool(bool fIsCurrentTool)
{
    m_fIsCurrentTool = fIsCurrentTool;
}

void UIVMActivityToolWidget::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pTabWidget = new QTabWidget(this);
    m_pTabWidget->setTabPosition(QTabWidget::East);
    m_pTabWidget->setTabBarAutoHide(true);

    pMainLayout->addWidget(m_pTabWidget);


    m_pPaneContainer = new UIVMActivityMonitorPaneContainer(this, m_enmEmbedding);
    m_pPaneContainer->hide();
    pMainLayout->addWidget(m_pPaneContainer);
    connect(m_pTabWidget, &QTabWidget::currentChanged,
            this, &UIVMActivityToolWidget::sltCurrentTabChanged);
}

void UIVMActivityToolWidget::setSelectedVMListItems(const QList<UIVirtualMachineItem*> &items)
{
    setMachines(items);
}

void UIVMActivityToolWidget::setMachines(const QList<UIVirtualMachineItem*> &machines)
{
    QVector<QUuid> machineIds;
    foreach (const UIVirtualMachineItem* pMachine, machines)
    {
        if (!pMachine)
            continue;
        machineIds << pMachine->id();
    }
    /* List of machines that are newly added to selected machine list: */
    QList<UIVirtualMachineItem*> newSelections;
    QVector<QUuid> unselectedMachines(m_machineIds);

    foreach (UIVirtualMachineItem* pMachine, machines)
    {
        if (!pMachine)
            continue;
        QUuid id = pMachine->id();
        unselectedMachines.removeAll(id);
        if (!m_machineIds.contains(id))
            newSelections << pMachine;
    }
    m_machineIds = machineIds;

    removeTabs(unselectedMachines);
    addTabs(newSelections);
}

void UIVMActivityToolWidget::prepareActions()
{
    QAction *pToResourcesAction =
        m_pActionPool->action(UIActionIndex_M_Activity_S_ToVMActivityOverview);
    if (pToResourcesAction)
        connect(pToResourcesAction, &QAction::triggered, this, &UIVMActivityToolWidget::sigSwitchToActivityOverviewPane);

    m_pExportToFileAction =
        m_pActionPool->action(UIActionIndex_M_Activity_S_Export);
    if (m_pExportToFileAction)
        connect(m_pExportToFileAction, &QAction::triggered, this, &UIVMActivityToolWidget::sltExportToFile);
    if (m_pActionPool->action(UIActionIndex_M_Activity_T_Preferences))
        connect(m_pActionPool->action(UIActionIndex_M_Activity_T_Preferences), &QAction::toggled,
                this, &UIVMActivityToolWidget::sltTogglePreferencesPane);
}

void UIVMActivityToolWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
    AssertPtrReturnVoid(m_pToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        layout()->addWidget(m_pToolBar);
#endif
    }
}

void UIVMActivityToolWidget::loadSettings()
{
}

void UIVMActivityToolWidget::removeTabs(const QVector<QUuid> &machineIdsToRemove)
{
    AssertReturnVoid(m_pTabWidget);
    QVector<UIVMActivityMonitor*> removeList;

    for (int i = m_pTabWidget->count() - 1; i >= 0; --i)
    {
        UIVMActivityMonitor *pMonitor = qobject_cast<UIVMActivityMonitor*>(m_pTabWidget->widget(i));
        if (!pMonitor)
            continue;
        if (machineIdsToRemove.contains(pMonitor->machineId()))
        {
            removeList << pMonitor;
            m_pTabWidget->removeTab(i);
        }
    }
    qDeleteAll(removeList.begin(), removeList.end());
}

void UIVMActivityToolWidget::addTabs(const QList<UIVirtualMachineItem*> &machines)
{
    AssertReturnVoid(m_pTabWidget);
    foreach (UIVirtualMachineItem* pMachine, machines)
    {
        if (!pMachine)
            continue;
        if (pMachine->toLocal())
        {
            CMachine comMachine = pMachine->toLocal()->machine();
            m_pTabWidget->addTab(new UIVMActivityMonitorLocal(m_enmEmbedding, this, comMachine), comMachine.GetName());
            continue;
        }
        if (pMachine->toCloud())
        {
            CCloudMachine comMachine = pMachine->toCloud()->machine();
            if (!comMachine.isOk())
                continue;
            m_pTabWidget->addTab(new UIVMActivityMonitorCloud(m_enmEmbedding, this, comMachine), comMachine.GetName());
            continue;
        }
    }
}

void UIVMActivityToolWidget::sltExportToFile()
{
    AssertReturnVoid(m_pTabWidget);
    UIVMActivityMonitor *pActivityMonitor = qobject_cast<UIVMActivityMonitor*>(m_pTabWidget->currentWidget());
    if (pActivityMonitor)
        pActivityMonitor->sltExportMetricsToFile();
}

void UIVMActivityToolWidget::sltCurrentTabChanged(int iIndex)
{
    AssertReturnVoid(m_pTabWidget);
    Q_UNUSED(iIndex);
    UIVMActivityMonitor *pActivityMonitor = qobject_cast<UIVMActivityMonitor*>(m_pTabWidget->currentWidget());
    if (pActivityMonitor)
    {
        CMachine comMachine = gpGlobalSession->virtualBox().FindMachine(pActivityMonitor->machineId().toString());
        if (!comMachine.isNull())
        {
            setExportActionEnabled(comMachine.GetState() == KMachineState_Running);
        }
    }
}

void UIVMActivityToolWidget::sltTogglePreferencesPane(bool fChecked)
{
    AssertReturnVoid(m_pPaneContainer);
    m_pPaneContainer->setVisible(fChecked);
}

void UIVMActivityToolWidget::setExportActionEnabled(bool fEnabled)
{
    if (m_pExportToFileAction)
        m_pExportToFileAction->setEnabled(fEnabled);
}
