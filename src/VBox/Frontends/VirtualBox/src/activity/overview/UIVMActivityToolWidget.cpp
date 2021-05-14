/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityToolWidget class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QAbstractTableModel>
#include <QCheckBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIActionPoolManager.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIVMActivityMonitor.h"
#include "UIVMActivityToolWidget.h"
#include "UIMessageCenter.h"
#include "QIToolBar.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItem.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CPerformanceMetric.h"

/* Other VBox includes: */
#include <iprt/cidr.h>


class UIVMActivityListWidget : public QWidget
{

    Q_OBJECT;
public:

    UIVMActivityListWidget(QWidget *pParent = 0)
        : QWidget(pParent)
    {
    }
};
/*********************************************************************************************************************************
*   Class UIVMActivityToolWidget implementation.                                                                             *
*********************************************************************************************************************************/

UIVMActivityToolWidget::UIVMActivityToolWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                 bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QTabWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
{
    prepare();
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

void UIVMActivityToolWidget::retranslateUi()
{
}

void UIVMActivityToolWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QTabWidget>::showEvent(pEvent);
}

void UIVMActivityToolWidget::prepare()
{
    setTabBarAutoHide(true);
}

void UIVMActivityToolWidget::setSelectedVMListItems(const QList<UIVirtualMachineItem*> &items)
{
    QVector<QUuid> selectedMachines;

    foreach (const UIVirtualMachineItem *item, items)
    {
        if (!item)
            continue;
        selectedMachines << item->id();
    }
    setMachines(selectedMachines);
}

void UIVMActivityToolWidget::setMachines(const QVector<QUuid> &machineIds)
{
    /* List of machines that are newly added to selected machine list: */
    QVector<QUuid> newSelections;
    QVector<QUuid> unselectedMachines(m_machineIds);

    foreach (const QUuid &id, machineIds)
    {
        unselectedMachines.removeAll(id);
        if (!m_machineIds.contains(id))
            newSelections << id;
    }
    m_machineIds = machineIds;

    hide();
    removeTabs(unselectedMachines);
    addTabs(newSelections);
    show();
}


void UIVMActivityToolWidget::prepareActions()
{
    QAction *pToResourcesAction =
        m_pActionPool->action(UIActionIndex_M_Activity_S_ToVMActivityOverview);
    if (pToResourcesAction)
        connect(pToResourcesAction, &QAction::triggered, this, &UIVMActivityToolWidget::sigSwitchToResourcesPane);

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

void UIVMActivityToolWidget::showVMActivityMonitor(CMachine &comMachine)
{
    if (comMachine.isNull())
        return;
    // if (!m_pActivityMonitor)
    // {
    //     m_pActivityMonitor = new UIVMActivityMonitor(m_enmEmbedding, this,
    //                                                  comMachine, m_pActionPool, false /* Show toolbar */);
    //     addWidget(m_pActivityMonitor);
    // }
    // else
    //     m_pActivityMonitor->setMachine(comMachine);

    // setCurrentWidget(m_pActivityMonitor);
}

void UIVMActivityToolWidget::showVMActivityList()
{
    // if (!m_pActivityList)
    // {
    //     m_pActivityList = new UIVMActivityListWidget(this);
    //     addWidget(m_pActivityList);
    // }
    // setCurrentWidget(m_pActivityList);
}

void UIVMActivityToolWidget::removeTabs(const QVector<QUuid> &machineIdsToRemove)
{
    QVector<UIVMActivityMonitor*> removeList;
    QVector<UIVMActivityMonitor*> keepList;
    for (int i = 0; i < count(); ++i)
    {
        UIVMActivityMonitor *pMonitor = qobject_cast<UIVMActivityMonitor*>(widget(i));
        if (!pMonitor)
            continue;
        if (machineIdsToRemove.contains(pMonitor->machineId()))
            removeList << pMonitor;
        else
            keepList << pMonitor;
    }
    clear();
    qDeleteAll(removeList.begin(), removeList.end());

    foreach (UIVMActivityMonitor *pMonitor, keepList)
        addTab(pMonitor, pMonitor->machineName());
}

void UIVMActivityToolWidget::addTabs(const QVector<QUuid> &machineIdsToAdd)
{
    foreach (const QUuid &id, machineIdsToAdd)
    {
        CMachine comMachine = uiCommon().virtualBox().FindMachine(id.toString());
        if (comMachine.isNull())
            continue;
        addTab(new UIVMActivityMonitor(m_enmEmbedding, this, comMachine, m_pActionPool), comMachine.GetName());
    }
}

#include "UIVMActivityToolWidget.moc"
