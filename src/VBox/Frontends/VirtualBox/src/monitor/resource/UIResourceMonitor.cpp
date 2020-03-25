/* $Id$ */
/** @file
 * VBox Qt GUI - UIResourceMonitor class implementation.
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
#include <QItemDelegate>
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
#include "UIPerformanceMonitor.h"
#include "UIResourceMonitor.h"
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
#include "CPerformanceMetric.h"

/* Other VBox includes: */
#include <iprt/cidr.h>

/* The first element must be 0 and the rest must be consecutive: */
enum VMResouceMonitorColumn
{
    VMResouceMonitorColumn_Name = 0,
    VMResouceMonitorColumn_CPUGuestLoad,
    VMResouceMonitorColumn_CPUVMMLoad,
    VMResouceMonitorColumn_RAMUsedAndTotal,
    VMResouceMonitorColumn_RAMUsedPercentage,
    VMResouceMonitorColumn_NetworkUpRate,
    VMResouceMonitorColumn_NetworkDownRate,
    VMResouceMonitorColumn_NetworkUpTotal,
    VMResouceMonitorColumn_NetworkDownTotal,
    VMResouceMonitorColumn_DiskIOReadRate,
    VMResouceMonitorColumn_DiskIOWriteRate,
    VMResouceMonitorColumn_DiskIOReadTotal,
    VMResouceMonitorColumn_DiskIOWriteTotal,
    VMResouceMonitorColumn_VMExits,
    VMResouceMonitorColumn_Max
};

struct ResourceColumn
{
    QString m_strName;
    bool    m_fEnabled;
};
/*********************************************************************************************************************************
*   Class UIVMResouceMonitorItem definition.                                                                           *
*********************************************************************************************************************************/
class UIResourceMonitorItem
{
public:
    UIResourceMonitorItem(const QUuid &uid, const QString &strVMName);
    UIResourceMonitorItem(const QUuid &uid);
    UIResourceMonitorItem();
    bool operator==(const UIResourceMonitorItem& other) const;
    QUuid    m_VMuid;
    QString  m_strVMName;
    quint64  m_uCPUGuestLoad;
    quint64  m_uCPUVMMLoad;

    quint64  m_uTotalRAM;
    quint64  m_uFreeRAM;
    quint64  m_uUsedRAM;
    float    m_fRAMUsagePercentage;

    quint64 m_uNetworkDownRate;
    quint64 m_uNetworkUpRate;
    quint64 m_uNetworkDownTotal;
    quint64 m_uNetworkUpTotal;

    quint64 m_uDiskWriteRate;
    quint64 m_uDiskReadRate;
    quint64 m_uDiskWriteTotal;
    quint64 m_uDiskReadTotal;

    quint64 m_uVMExitRate;
    quint64 m_uVMExitTotal;

    CMachineDebugger m_comDebugger;

private:

    void setupPerformanceCollector();

};


/*********************************************************************************************************************************
*   Class UIVMResouceMonitorProxyModel definition.                                                                           *
*********************************************************************************************************************************/
class UIResourceMonitorProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIResourceMonitorProxyModel(QObject *parent = 0);
    void dataUpdate();
    void setColumnShown(const QVector<bool>& columnShown);

protected:

    //virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const /* override */;
    virtual bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const /* override */;

private:

    QVector<bool> m_columnShown;

};


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel definition.                                                                                     *
*********************************************************************************************************************************/
class UIResourceMonitorModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    void sigDataUpdate();

public:

    UIResourceMonitorModel(QObject *parent = 0);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    QVariant data(const QModelIndex &index, int role) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    void setColumnCaptions(const QVector<QString>& captions);

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltTimeout();

private:

    void initializeItems();
    void setupPerformanceCollector();
    void queryRAMLoad();
    void addItem(const QUuid& uMachineId, const QString& strMachineName);
    void removeItem(const QUuid& uMachineId);

    QVector<UIResourceMonitorItem> m_itemList;
    /* Used to find machines by uid. key is the machine uid and int is the index to m_itemList */
    QMap<QUuid, int>               m_itemMap;
    QVector<QString>               m_columnCaptions;
    QTimer *m_pTimer;
    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
      * @{ */
        QVector<QString> m_nameList;
        QVector<CUnknown> m_objectList;
    /** @} */
    CPerformanceCollector m_performanceMonitor;

};


/*********************************************************************************************************************************
*   UIVMResouceMonitorDelegate definition.                                                                                       *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIVMResouceMonitorDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};

/*********************************************************************************************************************************
*   Class UIVMResouceMonitorItem implementation.                                                                           *
*********************************************************************************************************************************/
UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid, const QString &strVMName)
    : m_VMuid(uid)
    , m_strVMName(strVMName)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uFreeRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
    CSession comSession = uiCommon().openSession(uid, KLockType_Shared);
    if (!comSession.isNull())
    {
        CConsole comConsole = comSession.GetConsole();
        if (!comConsole.isNull())
            m_comDebugger = comConsole.GetDebugger();
    }
}

UIResourceMonitorItem::UIResourceMonitorItem()
    : m_VMuid(QUuid())
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid)
    : m_VMuid(uid)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

bool UIResourceMonitorItem::operator==(const UIResourceMonitorItem& other) const
{
    if (m_VMuid == other.m_VMuid)
        return true;
    return false;
}



/*********************************************************************************************************************************
*   Class UIVMResouceMonitorProxyModel implementation.                                                                           *
*********************************************************************************************************************************/
UIResourceMonitorProxyModel::UIResourceMonitorProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}

void UIResourceMonitorProxyModel::dataUpdate()
{
    if (sourceModel())
        emit dataChanged(index(0,0), index(sourceModel()->rowCount(), sourceModel()->columnCount()));
    invalidate();
}

bool UIResourceMonitorProxyModel::filterAcceptsColumn(int iSourceColumn, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (iSourceColumn >= m_columnShown.size())
        return true;
    return m_columnShown[iSourceColumn];
}

void UIResourceMonitorProxyModel::setColumnShown(const QVector<bool>& columnShown)
{
    emit layoutAboutToBeChanged();
    m_columnShown = columnShown;
    invalidateFilter();
    emit layoutChanged();
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel implementation.                                                                                 *
*********************************************************************************************************************************/
UIResourceMonitorModel::UIResourceMonitorModel(QObject *parent /*= 0*/)
    :QAbstractTableModel(parent)
    , m_pTimer(new QTimer(this))
{
    initializeItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIResourceMonitorModel::sltMachineStateChanged);

    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIResourceMonitorModel::sltTimeout);
        m_pTimer->start(1000);
    }
}

int UIResourceMonitorModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_itemList.size();
}

int UIResourceMonitorModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return VMResouceMonitorColumn_Max;
}

QVariant UIResourceMonitorModel::data(const QModelIndex &index, int role) const
{
    int iDecimalCount = 2;
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
        case VMResouceMonitorColumn_RAMUsedAndTotal:
            return QString("%1/%2").arg(uiCommon().formatSize(_1K * m_itemList[index.row()].m_uUsedRAM, iDecimalCount)).
                arg(uiCommon().formatSize(_1K * m_itemList[index.row()].m_uTotalRAM, iDecimalCount));
            break;
        case VMResouceMonitorColumn_RAMUsedPercentage:
            return QString("%1%").arg(QString::number(m_itemList[index.row()].m_fRAMUsagePercentage, 'f', 2));
            break;
        case VMResouceMonitorColumn_NetworkUpRate:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uNetworkUpRate, iDecimalCount));
            break;
        case VMResouceMonitorColumn_NetworkDownRate:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uNetworkDownRate, iDecimalCount));
            break;
        case VMResouceMonitorColumn_NetworkUpTotal:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uNetworkUpTotal, iDecimalCount));
            break;
        case VMResouceMonitorColumn_NetworkDownTotal:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uNetworkDownTotal, iDecimalCount));
            break;
        case VMResouceMonitorColumn_DiskIOReadRate:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uDiskReadRate, iDecimalCount));
            break;
        case VMResouceMonitorColumn_DiskIOWriteRate:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uDiskWriteRate, iDecimalCount));
            break;
        case VMResouceMonitorColumn_DiskIOReadTotal:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uDiskReadTotal, iDecimalCount));
            break;
        case VMResouceMonitorColumn_DiskIOWriteTotal:
            return QString("%1").arg(uiCommon().formatSize(m_itemList[index.row()].m_uDiskWriteTotal, iDecimalCount));
            break;
        case VMResouceMonitorColumn_VMExits:
           return QString("%1/%2").arg(UICommon::addMetricSuffixToNumber(m_itemList[index.row()].m_uVMExitRate)).
               arg(UICommon::addMetricSuffixToNumber(m_itemList[index.row()].m_uVMExitTotal));
            break;
        default:
            break;
    }
    return QVariant();
}

QVariant UIResourceMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_columnCaptions.size())
        return m_columnCaptions[section];
    return QVariant();
}

void UIResourceMonitorModel::setColumnCaptions(const QVector<QString>& captions)
{
    m_columnCaptions = captions;
}

void UIResourceMonitorModel::initializeItems()
{
    foreach (const CMachine &comMachine, uiCommon().virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
        {
            if (comMachine.GetState() == KMachineState_Running)
                addItem(comMachine.GetId(), comMachine.GetName());
        }
    }
    setupPerformanceCollector();
}

void UIResourceMonitorModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = m_itemMap.value(uId, -1);
    /* Remove the machine in case machine is no longer working. */
    if (iIndex != -1 && state != KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        removeItem(uId);
        emit layoutChanged();
        setupPerformanceCollector();
    }
    /* Insert the machine if it is working. */
    if (iIndex == -1 && state == KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        if (!comMachine.isNull())
            addItem(uId, comMachine.GetName());
        emit layoutChanged();
        setupPerformanceCollector();
    }
}

void UIResourceMonitorModel::sltTimeout()
{
    ULONG aPctExecuting;
    ULONG aPctHalted;
    ULONG aPctVMM;
    /* RAM usage: */
    if (!m_performanceMonitor.isNull())
        queryRAMLoad();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i].m_comDebugger.isNull())
        {
            /* CPU Load: */
            m_itemList[i].m_comDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctVMM);
            m_itemList[i].m_uCPUGuestLoad = aPctExecuting;
            m_itemList[i].m_uCPUVMMLoad = aPctVMM;

            /* Network rate: */
            quint64 uPrevDownTotal = m_itemList[i].m_uNetworkDownTotal;
            quint64 uPrevUpTotal = m_itemList[i].m_uNetworkUpTotal;
            UIMonitorCommon::getNetworkLoad(m_itemList[i].m_comDebugger,
                                            m_itemList[i].m_uNetworkDownTotal, m_itemList[i].m_uNetworkUpTotal);
            m_itemList[i].m_uNetworkDownRate = m_itemList[i].m_uNetworkDownTotal - uPrevDownTotal;
            m_itemList[i].m_uNetworkUpRate = m_itemList[i].m_uNetworkUpTotal - uPrevUpTotal;

            /* IO rate: */
            quint64 uPrevWriteTotal = m_itemList[i].m_uDiskWriteTotal;
            quint64 uPrevReadTotal = m_itemList[i].m_uDiskReadTotal;
            UIMonitorCommon::getDiskLoad(m_itemList[i].m_comDebugger,
                                         m_itemList[i].m_uDiskWriteTotal, m_itemList[i].m_uDiskReadTotal);
            m_itemList[i].m_uDiskWriteRate = m_itemList[i].m_uDiskWriteTotal - uPrevWriteTotal;
            m_itemList[i].m_uDiskReadRate = m_itemList[i].m_uDiskReadTotal - uPrevReadTotal;

           /* VM Exits: */
            quint64 uPrevVMExitsTotal = m_itemList[i].m_uVMExitTotal;
            UIMonitorCommon::getVMMExitCount(m_itemList[i].m_comDebugger, m_itemList[i].m_uVMExitTotal);
            m_itemList[i].m_uVMExitRate = m_itemList[i].m_uVMExitTotal - uPrevVMExitsTotal;
        }
    }
    emit sigDataUpdate();
}

void UIResourceMonitorModel::setupPerformanceCollector()
{
    m_nameList.clear();
    m_objectList.clear();
    /* Initialize and configure CPerformanceCollector: */
    const ULONG iPeriod = 1;
    const int iMetricSetupCount = 1;
    if (m_performanceMonitor.isNull())
        m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    for (int i = 0; i < m_itemList.size(); ++i)
        m_nameList << "Guest/RAM/Usage*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceMonitor.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
}

void UIResourceMonitorModel::queryRAMLoad()
{
    QVector<QString>  aReturnNames;
    QVector<CUnknown>  aReturnObjects;
    QVector<QString>  aReturnUnits;
    QVector<ULONG>  aReturnScales;
    QVector<ULONG>  aReturnSequenceNumbers;
    QVector<ULONG>  aReturnDataIndices;
    QVector<ULONG>  aReturnDataLengths;
    /* Make a query to CPerformanceCollector to fetch some metrics (e.g RAM usage): */
    QVector<LONG> returnData = m_performanceMonitor.QueryMetricsData(m_nameList,
                                                                        m_objectList,
                                                                        aReturnNames,
                                                                        aReturnObjects,
                                                                        aReturnUnits,
                                                                        aReturnScales,
                                                                        aReturnSequenceNumbers,
                                                                        aReturnDataIndices,
                                                                        aReturnDataLengths);
    /* Parse the result we get from CPerformanceCollector to get respective values: */
    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;
        /* Read the last of the return data disregarding the rest since we are caching the data in GUI side: */
        float fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];
        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive) || aReturnNames[i].contains("Free", Qt::CaseInsensitive))
            {
                {
                    CMachine comMachine = (CMachine)aReturnObjects[i];
                    if (comMachine.isNull())
                        continue;
                    int iIndex = m_itemMap.value(comMachine.GetId(), -1);
                    if (iIndex == -1 || iIndex >= m_itemList.size())
                        continue;
                    if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                        m_itemList[iIndex].m_uTotalRAM = fData;
                    else
                        m_itemList[iIndex].m_uFreeRAM = fData;
                }
            }
    }
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        m_itemList[i].m_uUsedRAM = m_itemList[i].m_uTotalRAM - m_itemList[i].m_uFreeRAM;
        if (m_itemList[i].m_uTotalRAM != 0)
            m_itemList[i].m_fRAMUsagePercentage = 100.f * (m_itemList[i].m_uUsedRAM / (float)m_itemList[i].m_uTotalRAM);
    }
}

void UIResourceMonitorModel::addItem(const QUuid& uMachineId, const QString& strMachineName)
{
    int iIndex = m_itemList.size();
    m_itemList.append(UIResourceMonitorItem(uMachineId, strMachineName));
    m_itemMap[uMachineId] = iIndex;
}

void UIResourceMonitorModel::removeItem(const QUuid& uMachineId)
{
    int iIndex = m_itemMap.value(uMachineId, -1);
    if (iIndex == -1)
        return;
    m_itemList.remove(iIndex);
    m_itemMap.remove(uMachineId);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIResourceMonitorWidget::UIResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                       bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTableView(0)
{
    /* Prepare: */
    prepare();
}

QMenu *UIResourceMonitorWidget::menu() const
{
    return m_pActionPool->action(UIActionIndexST_M_NetworkWindow)->menu();
}

void UIResourceMonitorWidget::retranslateUi()
{
    m_columnCaptions.resize(VMResouceMonitorColumn_Max);
    m_columnCaptions[VMResouceMonitorColumn_Name] = tr("VM Name");
    m_columnCaptions[VMResouceMonitorColumn_CPUGuestLoad] = tr("CPU Guest");
    m_columnCaptions[VMResouceMonitorColumn_CPUVMMLoad] = tr("CPU VMM");
    m_columnCaptions[VMResouceMonitorColumn_RAMUsedAndTotal] = tr("RAM Used/Total");
    m_columnCaptions[VMResouceMonitorColumn_RAMUsedPercentage] = tr("RAM \%");
    m_columnCaptions[VMResouceMonitorColumn_NetworkUpRate] = tr("Network Up Rate");
    m_columnCaptions[VMResouceMonitorColumn_NetworkDownRate] = tr("Network Down Rate");
    m_columnCaptions[VMResouceMonitorColumn_NetworkUpTotal] = tr("Network Up Total");
    m_columnCaptions[VMResouceMonitorColumn_NetworkDownTotal] = tr("Network Down Total");
    m_columnCaptions[VMResouceMonitorColumn_DiskIOReadRate] = tr("Disk Read Rate");
    m_columnCaptions[VMResouceMonitorColumn_DiskIOWriteRate] = tr("Disk Write Rate");
    m_columnCaptions[VMResouceMonitorColumn_DiskIOReadTotal] = tr("Disk Read Total");
    m_columnCaptions[VMResouceMonitorColumn_DiskIOWriteTotal] = tr("Disk Write Total");
    m_columnCaptions[VMResouceMonitorColumn_VMExits] = tr("VM Exits");
    if (m_pModel)
        m_pModel->setColumnCaptions(m_columnCaptions);
}

void UIResourceMonitorWidget::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

void UIResourceMonitorWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIResourceMonitorWidget::prepare()
{
    m_columnShown.resize(VMResouceMonitorColumn_Max);
    for (int i = 0; i < m_columnShown.size(); ++i)
        m_columnShown[i] = true;
    prepareWidgets();
    loadSettings();
    retranslateUi();
}

void UIResourceMonitorWidget::prepareWidgets()
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

    m_pModel = new UIResourceMonitorModel(this);
    m_pProxyModel = new UIResourceMonitorProxyModel(this);

    m_pTableView = new QTableView();
    if (m_pTableView && m_pModel && m_pProxyModel)
    {
        layout()->addWidget(m_pTableView);
        m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_pTableView, &QTableView::customContextMenuRequested,
                this, &UIResourceMonitorWidget::sltCreateContextMenu);
        m_pProxyModel->setSourceModel(m_pModel);
        m_pTableView->setModel(m_pProxyModel);
        m_pTableView->setItemDelegate(new UIVMResouceMonitorDelegate);
        m_pTableView->setSelectionMode(QAbstractItemView::NoSelection);
        /* m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
           m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);*/
        m_pTableView->setShowGrid(false);
        m_pTableView->horizontalHeader()->setHighlightSections(false);
        m_pTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        //m_pTableView->horizontalHeader()->setStretchLastSection(true);

        m_pTableView->verticalHeader()->setVisible(false);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);
        connect(m_pModel, &UIResourceMonitorModel::sigDataUpdate, this, &UIResourceMonitorWidget::sltHandleDataUpdate);

        m_pProxyModel->setColumnShown(m_columnShown);
    }
}

void UIResourceMonitorWidget::prepareToolBar()
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

void UIResourceMonitorWidget::loadSettings()
{
}

void UIResourceMonitorWidget::sltCreateContextMenu(const QPoint &point)
{
    if (!m_pTableView)
        return;
    QMenu menu;
    for (int i = 0; i < VMResouceMonitorColumn_Max; ++i)
    {
        QAction *pAction = menu.addAction(m_columnCaptions[i]);
        if (!pAction)
            continue;
        pAction->setData(i);
        pAction->setCheckable(true);
        if (i < m_columnShown.size())
            pAction->setChecked(m_columnShown[i]);
        if (i == (int)VMResouceMonitorColumn_Name)
            pAction->setEnabled(false);
        connect(pAction, &QAction::triggered, this, &UIResourceMonitorWidget::sltHandleColumnAction);
    }
    menu.exec(m_pTableView->mapToGlobal(point));
}

void UIResourceMonitorWidget::sltHandleColumnAction(bool fChecked)
{
    QAction* pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    int iColumnId = pSender->data().toInt();
    if (iColumnId >= m_columnShown.size())
        return;
    setColumnShown(iColumnId, fChecked);
}

void UIResourceMonitorWidget::sltHandleDataUpdate()
{
    if (m_pProxyModel)
        m_pProxyModel->dataUpdate();
}

void UIResourceMonitorWidget::setColumnShown(int iColumnId, bool fShown)
{
    if (iColumnId >= m_columnShown.size())
        return;
    if (m_columnShown[iColumnId] == fShown)
        return;
    m_columnShown[iColumnId] = fShown;
    if (m_pProxyModel)
        m_pProxyModel->setColumnShown(m_columnShown);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorFactory implementation.                                                                               *
*********************************************************************************************************************************/

UIResourceMonitorFactory::UIResourceMonitorFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UIResourceMonitorFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIResourceMonitor(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitor implementation.                                                                                      *
*********************************************************************************************************************************/

UIResourceMonitor::UIResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UIResourceMonitor::retranslateUi()
{
    setWindowTitle(tr("VM Resource Monitor"));
}

void UIResourceMonitor::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UIResourceMonitor::configureCentralWidget()
{
    UIResourceMonitorWidget *pWidget = new UIResourceMonitorWidget(EmbedTo_Dialog, m_pActionPool, true, this);
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

void UIResourceMonitor::configureButtonBox()
{
}

void UIResourceMonitor::finalize()
{
    retranslateUi();
}

UIResourceMonitorWidget *UIResourceMonitor::widget()
{
    return qobject_cast<UIResourceMonitorWidget*>(QIManagerDialog::widget());
}


#include "UIResourceMonitor.moc"
