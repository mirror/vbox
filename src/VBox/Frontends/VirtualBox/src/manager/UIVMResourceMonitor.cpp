/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMResourceMonitor class implementation.
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
#include <QHeaderView>
#include <QMenuBar>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIActionPoolManager.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationPerformanceMonitor.h"
#include "UIVMResourceMonitor.h"
#include "UIMessageCenter.h"
#include "UIToolBar.h"
#include "UIVirtualBoxEventHandler.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CMachine.h"
#include "CMachineDebugger.h"

/* Other VBox includes: */
#include <iprt/cidr.h>


enum VMResouceMonitorColumn
{
    VMResouceMonitorColumn_Name = 0,
    VMResouceMonitorColumn_CPUGuestLoad,
    VMResouceMonitorColumn_CPUVMMLoad,
    // VMResouceMonitorColumn_RAMUsedAndTotal,
    // VMResouceMonitorColumn_RAMUsedPercentage,
    VMResouceMonitorColumn_NetworkDownUpRate,
    // VMResouceMonitorColumn_NetworkDownUpTotal,
    // VMResouceMonitorColumn_DiskIOWriteReadRate,
    // VMResouceMonitorColumn_DiskIOWriteReadTotal,
    VMResouceMonitorColumn_Max
};

/*********************************************************************************************************************************
*   Class UIVMResouceMonitorItem definition.                                                                           *
*********************************************************************************************************************************/
class UIVMResourceMonitorItem
{
public:
    UIVMResourceMonitorItem(const QUuid &uid, const QString &strVMName);
    UIVMResourceMonitorItem(const QUuid &uid);
    UIVMResourceMonitorItem();
    bool operator==(const UIVMResourceMonitorItem& other) const;
    QUuid    m_VMuid;
    QString  m_strVMName;
    quint64 m_uCPUGuestLoad;
    quint64 m_uCPUVMMLoad;
    quint64 m_uNetworkDownRate;
    quint64 m_uNetworkUpRate;
    quint64 m_uNetworkDownTotal;
    quint64 m_uNetworkUpTotal;

    quint64 m_uDiskWriteRate;
    quint64 m_uDiskReadRate;
    quint64 m_uDiskWriteTotal;
    quint64 m_uDiskReadTotal;

    CMachineDebugger m_comDebugger;

};

/*********************************************************************************************************************************
*   Class UIVMResouceMonitorProxyModel definition.                                                                           *
*********************************************************************************************************************************/
class UIVMResourceMonitorProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIVMResourceMonitorProxyModel(QObject *parent = 0);

protected:

    //virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const /* override */;

private:

};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorModel definition.                                                                                   *
*********************************************************************************************************************************/
class UIVMResourceMonitorModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    UIVMResourceMonitorModel(QObject *parent = 0);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    QVariant data(const QModelIndex &index, int role) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltTimeout();

private:

    QVector<UIVMResourceMonitorItem> m_itemList;
    void initializeItems();

    QTimer *m_pTimer;
};

/*********************************************************************************************************************************
*   Class UIVMResouceMonitorItem implementation.                                                                           *
*********************************************************************************************************************************/
UIVMResourceMonitorItem::UIVMResourceMonitorItem(const QUuid &uid, const QString &strVMName)
    : m_VMuid(uid)
    , m_strVMName(strVMName)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
{
    CSession comSession = uiCommon().openSession(uid, KLockType_Shared);
    if (!comSession.isNull())
    {
        CConsole comConsole = comSession.GetConsole();
        if (!comConsole.isNull())
            m_comDebugger = comConsole.GetDebugger();
    }
}

UIVMResourceMonitorItem::UIVMResourceMonitorItem()
    : m_VMuid(QUuid())
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
{
}

UIVMResourceMonitorItem::UIVMResourceMonitorItem(const QUuid &uid)
    : m_VMuid(uid)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
{
}

bool UIVMResourceMonitorItem::operator==(const UIVMResourceMonitorItem& other) const
{
    if (m_VMuid == other.m_VMuid)
        return true;
    return false;
}


/*********************************************************************************************************************************
*   Class UIVMResouceMonitorProxyModel implementation.                                                                           *
*********************************************************************************************************************************/
UIVMResourceMonitorProxyModel::UIVMResourceMonitorProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorModel implementation.                                                                               *
*********************************************************************************************************************************/
UIVMResourceMonitorModel::UIVMResourceMonitorModel(QObject *parent /*= 0*/)
    :QAbstractTableModel(parent)
    , m_pTimer(new QTimer(this))
{
    initializeItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIVMResourceMonitorModel::sltMachineStateChanged);

    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIVMResourceMonitorModel::sltTimeout);
        m_pTimer->start(1000);
    }
}

int UIVMResourceMonitorModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_itemList.size();
}

int UIVMResourceMonitorModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return VMResouceMonitorColumn_Max;
}

QVariant UIVMResourceMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount())
        return QVariant();
    switch (index.column())
    {
        case VMResouceMonitorColumn_Name:
            return m_itemList[index.row()].m_strVMName;
            break;
        case VMResouceMonitorColumn_CPUGuestLoad:
            return m_itemList[index.row()].m_uCPUGuestLoad;
            break;
        case VMResouceMonitorColumn_CPUVMMLoad:
            return m_itemList[index.row()].m_uCPUVMMLoad;
            break;
        case VMResouceMonitorColumn_NetworkDownUpRate:
            return QString("%1/%2").arg(m_itemList[index.row()].m_uNetworkDownRate).arg(m_itemList[index.row()].m_uNetworkUpRate);
            break;
        default:
            break;
    }
    return QVariant();
}

QVariant UIVMResourceMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case VMResouceMonitorColumn_Name:
                return tr("VM Name");
                break;
            case VMResouceMonitorColumn_CPUGuestLoad:
                return tr("CPU Load (Guest)");
                break;
            case VMResouceMonitorColumn_CPUVMMLoad:
                return tr("CPU Load (VMM)");
                break;
            // case VMResouceMonitorColumn_RAMUsedAndTotal:
            //     return tr("RAM (Used/Total)");
            //     break;
            // case VMResouceMonitorColumn_RAMUsedPercentage:
            //     return tr("RAM % Used");
            //     break;
            case VMResouceMonitorColumn_NetworkDownUpRate:
                return tr("Network Up/Down Rate");
                break;

            default:
                return QString();
                break;
        }
    }
    return QVariant();
}

void UIVMResourceMonitorModel::initializeItems()
{
    foreach (const CMachine &comMachine, uiCommon().virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
        {
            if (comMachine.GetState() == KMachineState_Running)
            {
                m_itemList << UIVMResourceMonitorItem(comMachine.GetId(), comMachine.GetName());
            }
        }
    }
}

void UIVMResourceMonitorModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = m_itemList.indexOf(UIVMResourceMonitorItem(uId));
    /* Remove the machine in case machine is no longer working. */
    if (iIndex != -1 && state != KMachineState_Running)
    {
        m_itemList.remove(iIndex);
        emit layoutChanged();
    }

    /* Insert the machine if it is working. */
    if (iIndex == -1 && state == KMachineState_Running)
    {
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        m_itemList << UIVMResourceMonitorItem(uId, comMachine.GetName());
        emit layoutChanged();
    }
}

void UIVMResourceMonitorModel::sltTimeout()
{
    ULONG aPctExecuting;
    ULONG aPctHalted;
    ULONG aPctVMM;
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i].m_comDebugger.isNull())
        {
            m_itemList[i].m_comDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctVMM);
            m_itemList[i].m_uCPUGuestLoad = aPctExecuting;
            m_itemList[i].m_uCPUVMMLoad = aPctVMM;
        }
    }
    emit layoutChanged();
    /* dataChanged signal result in view update when there is a proxy model in between. */
    //emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorWidget implementation.                                                                              *
*********************************************************************************************************************************/

UIVMResourceMonitorWidget::UIVMResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                       bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTableWidget(0)
{
    /* Prepare: */
    prepare();
}

QMenu *UIVMResourceMonitorWidget::menu() const
{
    return m_pActionPool->action(UIActionIndexST_M_NetworkWindow)->menu();
}

void UIVMResourceMonitorWidget::retranslateUi()
{
}

void UIVMResourceMonitorWidget::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

void UIVMResourceMonitorWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIVMResourceMonitorWidget::prepare()
{
    prepareActions();
    prepareWidgets();
    loadSettings();
    retranslateUi();
}

void UIVMResourceMonitorWidget::prepareActions()
{
}

void UIVMResourceMonitorWidget::prepareWidgets()
{
    /* Create main-layout: */
    new QVBoxLayout(this);
    if (!layout())
        return;
    /* Configure layout: */
    layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    layout()->setSpacing(10);
#else
    layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    if (m_fShowToolbar)
        prepareToolBar();

    m_pModel = new UIVMResourceMonitorModel(this);
    m_pProxyModel = new UIVMResourceMonitorProxyModel(this);

    m_pTableWidget = new QTableView();
    if (m_pTableWidget && m_pModel && m_pProxyModel)
    {
        layout()->addWidget(m_pTableWidget);
        m_pProxyModel->setSourceModel(m_pModel);
        m_pTableWidget->setModel(m_pProxyModel);
        m_pTableWidget->setSelectionMode(QAbstractItemView::NoSelection);

        /* m_pTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
           m_pTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);*/
        m_pTableWidget->setShowGrid(false);
        m_pTableWidget->horizontalHeader()->setHighlightSections(false);
        m_pTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_pTableWidget->horizontalHeader()->setStretchLastSection(true);

        m_pTableWidget->verticalHeader()->setVisible(false);
        m_pTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pTableWidget->verticalHeader()->setDefaultSectionSize(m_pTableWidget->verticalHeader()->minimumSectionSize());
        m_pTableWidget->setAlternatingRowColors(true);
    }
}

void UIVMResourceMonitorWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
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


void UIVMResourceMonitorWidget::loadSettings()
{
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorFactory implementation.                                                                             *
*********************************************************************************************************************************/

UIVMResourceMonitorFactory::UIVMResourceMonitorFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UIVMResourceMonitorFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIVMResourceMonitor(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitor implementation.                                                                                    *
*********************************************************************************************************************************/

UIVMResourceMonitor::UIVMResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UIVMResourceMonitor::retranslateUi()
{
    setWindowTitle(tr("VM Resource Monitor"));
}

void UIVMResourceMonitor::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UIVMResourceMonitor::configureCentralWidget()
{
    UIVMResourceMonitorWidget *pWidget = new UIVMResourceMonitorWidget(EmbedTo_Dialog, m_pActionPool, true, this);
    AssertPtrReturnVoid(pWidget);
    {
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIVMResourceMonitor::configureButtonBox()
{
}

void UIVMResourceMonitor::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

UIVMResourceMonitorWidget *UIVMResourceMonitor::widget()
{
    return qobject_cast<UIVMResourceMonitorWidget*>(QIManagerDialog::widget());
}


#include "UIVMResourceMonitor.moc"
