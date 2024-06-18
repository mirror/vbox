/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityOverviewModelView class implementation.
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
#include <QHeaderView>
#include <QMouseEvent>
#include <QTimer>

/* GUI includes: */
#include "UIConverter.h"
#include "UICommon.h"
#include "UIExtraDataDefs.h"
#include "UIGlobalSession.h"
#include "UIMonitorCommon.h"
#include "UITranslator.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVMActivityOverviewModelView.h"


#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CSession.h"


/*********************************************************************************************************************************
*   Class UIActivityOverviewAccessibleRowLocal definition.                                                                       *
*********************************************************************************************************************************/

class UIActivityOverviewAccessibleRowLocal : public UIActivityOverviewAccessibleRow
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleRowLocal(QITableView *pTableView, const QUuid &uMachineId,
                                         const QString &strMachineName, KMachineState enmMachineState);
    ~UIActivityOverviewAccessibleRowLocal();
    virtual void setMachineState(int iState) RT_OVERRIDE RT_FINAL;

    virtual bool isRunning() const RT_OVERRIDE RT_FINAL;
    virtual bool isCloudVM() const RT_OVERRIDE RT_FINAL;
    virtual QString machineStateString() const RT_OVERRIDE RT_FINAL;
    void resetDebugger();
    void updateCells();
    bool isWithGuestAdditions();
    void setTotalRAM(quint64 uTotalRAM);
    void setFreeRAM(quint64 uFreeRAM);

private:

    void updateCellText(VMActivityOverviewColumn enmColumnIndex, const QString &strText);

    KMachineState    m_enmMachineState;
    CMachineDebugger m_comDebugger;
    CSession         m_comSession;
    CGuest           m_comGuest;


    quint64  m_uFreeRAM;

    quint64  m_uNetworkDownTotal;
    quint64  m_uNetworkUpTotal;

    quint64          m_uVMExitTotal;
    quint64          m_uDiskWriteTotal;
    quint64          m_uDiskReadTotal;

};


/*********************************************************************************************************************************
*   Class UIActivityOverviewAccessibleRowCloud definition.                                                                       *
*********************************************************************************************************************************/

/* A UIActivityOverviewItem derivation to show cloud vms in the table view: */
class UIActivityOverviewAccessibleRowCloud : public UIActivityOverviewAccessibleRow
{
    Q_OBJECT;
public:
    UIActivityOverviewAccessibleRowCloud(QITableView *pTableView, const QUuid &uMachineId,
                                         const QString &strMachineName, CCloudMachine &comCloudMachine);
    void updateMachineState();
    virtual bool isRunning() const RT_OVERRIDE RT_FINAL;
    virtual bool isCloudVM() const RT_OVERRIDE RT_FINAL;
    virtual QString machineStateString() const RT_OVERRIDE RT_FINAL;
    virtual void setMachineState(int iState) RT_OVERRIDE RT_FINAL;


protected:

private slots:

    void sltTimeout();
    void sltMetricNameListingComplete(QVector<QString> metricNameList);
    void sltMetricDataReceived(KMetricType enmMetricType,
                               const QVector<QString> &data, const QVector<QString> &timeStamps);
private:

    void getMetricList();
    void resetColumData();

    QTimer *m_pTimer;
    CCloudMachine m_comCloudMachine;
    KCloudMachineState m_enmMachineState;
    QVector<KMetricType> m_availableMetricTypes;
};


/*********************************************************************************************************************************
*   Class UIActivityOverviewAccessibleRowLocal implementation.                                                                   *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleRowLocal::UIActivityOverviewAccessibleRowLocal(QITableView *pTableView, const QUuid &uMachineId,
                                                                           const QString &strMachineName, KMachineState enmMachineState)
    : UIActivityOverviewAccessibleRow(pTableView, uMachineId, strMachineName)
    , m_enmMachineState(enmMachineState)
    , m_uFreeRAM(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uVMExitTotal(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
{
    if (m_enmMachineState == KMachineState_Running)
        resetDebugger();

}

void UIActivityOverviewAccessibleRowLocal::updateCells()
{
    /* CPU Load: */
    ULONG aPctHalted;
    ULONG  uCPUGuestLoad;
    ULONG uCPUVMMLoad;
    m_comDebugger.GetCPULoad(0x7fffffff, uCPUGuestLoad, aPctHalted, uCPUVMMLoad);
    updateCellText(VMActivityOverviewColumn_CPUVMMLoad, QString("%1%").arg(QString::number(uCPUVMMLoad)));
    updateCellText(VMActivityOverviewColumn_CPUGuestLoad, QString("%1%").arg(QString::number(uCPUGuestLoad)));

    /* RAM Utilization: */
    QString strRAMUsage;
    QString strRAMPercentage;
    int iDecimalCount = 2;
    quint64 uUsedRAM = m_uTotalRAM - m_uFreeRAM;
    float m_fRAMUsagePercentage = 0;

    if (m_uTotalRAM != 0)
        m_fRAMUsagePercentage = 100.f * (uUsedRAM / (float)m_uTotalRAM);

    if (isWithGuestAdditions())
    {
        strRAMUsage =
            QString("%1/%2").arg(UITranslator::formatSize(_1K * uUsedRAM, iDecimalCount)).
            arg(UITranslator::formatSize(_1K * m_uTotalRAM, iDecimalCount));
        strRAMPercentage =
            QString("%1%").arg(QString::number(m_fRAMUsagePercentage, 'f', 2));
    }
    else
    {
        strRAMPercentage = QApplication::translate("UIVMActivityOverviewWidget", "N/A");
        strRAMUsage = QApplication::translate("UIVMActivityOverviewWidget", "N/A");
    }

    updateCellText(VMActivityOverviewColumn_RAMUsedAndTotal, strRAMUsage);
    updateCellText(VMActivityOverviewColumn_RAMUsedPercentage, strRAMPercentage);

    /* Network rate: */
    quint64 uPrevDownTotal = m_uNetworkDownTotal;
    quint64 uPrevUpTotal = m_uNetworkUpTotal;
    UIMonitorCommon::getNetworkLoad(m_comDebugger, m_uNetworkDownTotal, m_uNetworkUpTotal);
    quint64 uNetworkDownRate = m_uNetworkDownTotal - uPrevDownTotal;
    quint64 uNetworkUpRate = m_uNetworkUpTotal - uPrevUpTotal;

    updateCellText(VMActivityOverviewColumn_NetworkUpRate, QString("%1").arg(UITranslator::formatSize(uNetworkUpRate, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_NetworkDownRate,QString("%1").arg(UITranslator::formatSize(uNetworkDownRate, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_NetworkUpTotal, QString("%1").arg(UITranslator::formatSize(m_uNetworkUpTotal, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_NetworkDownTotal, QString("%1").arg(UITranslator::formatSize(m_uNetworkDownTotal, iDecimalCount)));

    /* IO rate: */
    quint64 uPrevWriteTotal = m_uDiskWriteTotal;
    quint64 uPrevReadTotal = m_uDiskReadTotal;
    UIMonitorCommon::getDiskLoad(m_comDebugger, m_uDiskWriteTotal, m_uDiskReadTotal);
    quint64 uDiskWriteRate = m_uDiskWriteTotal - uPrevWriteTotal;
    quint64 uDiskReadRate = m_uDiskReadTotal - uPrevReadTotal;
    updateCellText(VMActivityOverviewColumn_DiskIOReadRate,QString("%1").arg(UITranslator::formatSize(uDiskReadRate, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_DiskIOWriteRate,QString("%1").arg(UITranslator::formatSize(uDiskWriteRate, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_DiskIOReadTotal,  QString("%1").arg(UITranslator::formatSize(m_uDiskReadTotal, iDecimalCount)));
    updateCellText(VMActivityOverviewColumn_DiskIOWriteTotal, QString("%1").arg(UITranslator::formatSize(m_uDiskWriteTotal, iDecimalCount)));

    /* VM Exits: */
    quint64 uPrevVMExitsTotal = m_uVMExitTotal;
    UIMonitorCommon::getVMMExitCount(m_comDebugger, m_uVMExitTotal);
    quint64 uVMExitRate = m_uVMExitTotal - uPrevVMExitsTotal;
    updateCellText(VMActivityOverviewColumn_VMExits, QString("%1/%2").arg(UITranslator::addMetricSuffixToNumber(uVMExitRate)).
                   arg(UITranslator::addMetricSuffixToNumber(m_uVMExitTotal)));


}

UIActivityOverviewAccessibleRowLocal::~UIActivityOverviewAccessibleRowLocal()
{
    if (!m_comSession.isNull())
        m_comSession.UnlockMachine();
}

void UIActivityOverviewAccessibleRowLocal::resetDebugger()
{
    m_comSession = uiCommon().openSession(m_uMachineId, KLockType_Shared);
    if (!m_comSession.isNull())
    {
        CConsole comConsole = m_comSession.GetConsole();
        if (!comConsole.isNull())
        {
            m_comGuest = comConsole.GetGuest();
            m_comDebugger = comConsole.GetDebugger();
        }
    }
}

void UIActivityOverviewAccessibleRowLocal::setMachineState(int iState)
{
    if (iState <= KMachineState_Null || iState >= KMachineState_Max)
        return;
    KMachineState enmState = static_cast<KMachineState>(iState);
    if (m_enmMachineState == enmState)
        return;
    m_enmMachineState = enmState;
    if (m_enmMachineState == KMachineState_Running)
        resetDebugger();
}

bool UIActivityOverviewAccessibleRowLocal::isRunning() const
{
    return m_enmMachineState == KMachineState_Running;
}

bool UIActivityOverviewAccessibleRowLocal::isCloudVM() const
{
    return false;
}

bool UIActivityOverviewAccessibleRowLocal::isWithGuestAdditions()
{
    if (m_comGuest.isNull())
        return false;
    return m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
}

void UIActivityOverviewAccessibleRowLocal::setTotalRAM(quint64 uTotalRAM)
{
    m_uTotalRAM = uTotalRAM;
}

void UIActivityOverviewAccessibleRowLocal::setFreeRAM(quint64 uFreeRAM)
{
    m_uFreeRAM = uFreeRAM;
}

void UIActivityOverviewAccessibleRowLocal::updateCellText(VMActivityOverviewColumn enmColumnIndex, const QString &strText)
{
    if (m_cells.value(enmColumnIndex, 0))
        m_cells[enmColumnIndex]->setText(strText);
}

QString UIActivityOverviewAccessibleRowLocal::machineStateString() const
{
    return gpConverter->toString(m_enmMachineState);
}


/*********************************************************************************************************************************
*   Class UIActivityOverviewAccessibleRowCloud implementation.                                                                   *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleRowCloud::UIActivityOverviewAccessibleRowCloud(QITableView *pTableView, const QUuid &uMachineId,
                                                                           const QString &strMachineName, CCloudMachine &comCloudMachine)
    : UIActivityOverviewAccessibleRow(pTableView, uMachineId, strMachineName)
    , m_comCloudMachine(comCloudMachine)
{
    updateMachineState();
    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIActivityOverviewAccessibleRowCloud::sltTimeout);
        m_pTimer->setInterval(60 * 1000);
    }
    resetColumData();
}

void UIActivityOverviewAccessibleRowCloud::updateMachineState()
{
    if (m_comCloudMachine.isOk())
        setMachineState(m_comCloudMachine.GetState());
}

bool UIActivityOverviewAccessibleRowCloud::isRunning() const
{
    return m_enmMachineState == KCloudMachineState_Running;
}

bool UIActivityOverviewAccessibleRowCloud::isCloudVM() const
{
    return true;
}

QString UIActivityOverviewAccessibleRowCloud::machineStateString() const
{
    if (!m_comCloudMachine.isOk())
        return QString();
    return gpConverter->toString(m_comCloudMachine.GetState());
}

void UIActivityOverviewAccessibleRowCloud::sltTimeout()
{
    int iDataSize = 1;
    foreach (const KMetricType &enmMetricType, m_availableMetricTypes)
    {
        UIProgressTaskReadCloudMachineMetricData *pTask = new UIProgressTaskReadCloudMachineMetricData(this, m_comCloudMachine,
                                                                                                       enmMetricType, iDataSize);
        connect(pTask, &UIProgressTaskReadCloudMachineMetricData::sigMetricDataReceived,
                this, &UIActivityOverviewAccessibleRowCloud::sltMetricDataReceived);
        pTask->start();
    }
}

void UIActivityOverviewAccessibleRowCloud::sltMetricDataReceived(KMetricType /*enmMetricType*/,
                                                        const QVector<QString> &data, const QVector<QString> &timeStamps)
{
    Q_UNUSED(timeStamps);
    if (data.isEmpty())
        return;

    if (data[0].toFloat() < 0)
        return;

    // int iDecimalCount = 2;
    // QLocale locale;
    // if (enmMetricType == KMetricType_CpuUtilization)
    // {
    //     //QString QLocale::toString(double i, char f = 'g', int prec = 6) const

    //     // m_columnData[VMActivityOverviewColumn_CPUGuestLoad] =
    //     //     QString("%1%").arg(QString::number(data[0].toFloat(), 'f', iDecimalCount));

    //     m_columnData[VMActivityOverviewColumn_CPUGuestLoad] =
    //         QString("%1%").arg(locale.toString(data[0].toFloat(), 'f', iDecimalCount));
    // }
    // else if (enmMetricType == KMetricType_MemoryUtilization)
    // {
    //      if (m_uTotalRAM != 0)
    //      {
    //          quint64 uUsedRAM = (quint64)data[0].toFloat() * (m_uTotalRAM / 100.f);
    //          m_columnData[VMActivityOverviewColumn_RAMUsedAndTotal] =
    //              QString("%1/%2").arg(UITranslator::formatSize(_1K * uUsedRAM, iDecimalCount)).
    //              arg(UITranslator::formatSize(_1K * m_uTotalRAM, iDecimalCount));
    //      }
    //      m_columnData[VMActivityOverviewColumn_RAMUsedPercentage] =
    //          QString("%1%").arg(QString::number(data[0].toFloat(), 'f', iDecimalCount));
    // }
    // else if (enmMetricType == KMetricType_NetworksBytesOut)
    //     m_columnData[VMActivityOverviewColumn_NetworkUpRate] =
    //         UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    // else if (enmMetricType == KMetricType_NetworksBytesIn)
    //     m_columnData[VMActivityOverviewColumn_NetworkDownRate] =
    //         UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    // else if (enmMetricType == KMetricType_DiskBytesRead)
    //     m_columnData[VMActivityOverviewColumn_DiskIOReadRate] =
    //         UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    // else if (enmMetricType == KMetricType_DiskBytesWritten)
    //     m_columnData[VMActivityOverviewColumn_DiskIOWriteRate] =
    //         UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);

    sender()->deleteLater();
}

void UIActivityOverviewAccessibleRowCloud::setMachineState(int iState)
{
    if (iState <= KCloudMachineState_Invalid || iState >= KCloudMachineState_Max)
        return;
    KCloudMachineState enmState = static_cast<KCloudMachineState>(iState);
    if (m_enmMachineState == enmState)
        return;
    m_enmMachineState = enmState;
    if (isRunning())
    {
        getMetricList();
        if (m_uTotalRAM == 0)
            m_uTotalRAM = UIMonitorCommon::determineTotalRAMAmount(m_comCloudMachine);
    }
    else
    {
        if (m_pTimer)
            m_pTimer->stop();
    }
}

void UIActivityOverviewAccessibleRowCloud::resetColumData()
{
    // for (int i = (int) VMActivityOverviewColumn_CPUGuestLoad;
    //      i < (int)VMActivityOverviewColumn_Max; ++i)
    //     m_columnData[i] = UIVMActivityOverviewWidget::tr("N/A");
}

void UIActivityOverviewAccessibleRowCloud::getMetricList()
{
    if (!isRunning())
        return;
    UIProgressTaskReadCloudMachineMetricList *pReadListProgressTask =
        new UIProgressTaskReadCloudMachineMetricList(this, m_comCloudMachine);
    AssertPtrReturnVoid(pReadListProgressTask);
    connect(pReadListProgressTask, &UIProgressTaskReadCloudMachineMetricList::sigMetricListReceived,
            this, &UIActivityOverviewAccessibleRowCloud::sltMetricNameListingComplete);
    pReadListProgressTask->start();
}

void UIActivityOverviewAccessibleRowCloud::sltMetricNameListingComplete(QVector<QString> metricNameList)
{
    AssertReturnVoid(m_pTimer);
    m_availableMetricTypes.clear();
    foreach (const QString &strName, metricNameList)
        m_availableMetricTypes << gpConverter->fromInternalString<KMetricType>(strName);

    if (!m_availableMetricTypes.isEmpty())
    {
        /* Dont wait 60 secs: */
        sltTimeout();
        m_pTimer->start();
    }
    else
    {
        m_pTimer->stop();
        resetColumData();
    }

    if (sender())
        sender()->deleteLater();

}


/*********************************************************************************************************************************
*   UIVMActivityOverviewAccessibleTableView implementation.                                                                      *
*********************************************************************************************************************************/

UIVMActivityOverviewAccessibleTableView::UIVMActivityOverviewAccessibleTableView(QWidget *pParent)
    : QITableView(pParent)
{
}

void UIVMActivityOverviewAccessibleTableView::updateColumVisibility()
{
    UIActivityOverviewAccessibleProxyModel *pProxyModel = qobject_cast<UIActivityOverviewAccessibleProxyModel*>(model());
    if (!pProxyModel)
        return;
    UIActivityOverviewAccessibleModel *pModel = qobject_cast<UIActivityOverviewAccessibleModel*>(pProxyModel->sourceModel());
    QHeaderView *pHeader = horizontalHeader();

    if (!pModel || !pHeader)
        return;
    for (int i = (int)VMActivityOverviewColumn_Name; i < (int)VMActivityOverviewColumn_Max; ++i)
    {
        if (!pModel->columnVisible(i))
            pHeader->hideSection(i);
        else
            pHeader->showSection(i);
    }
    resizeHeaders();
}

int UIVMActivityOverviewAccessibleTableView::selectedItemIndex() const
{
    UIActivityOverviewAccessibleProxyModel *pModel = qobject_cast<UIActivityOverviewAccessibleProxyModel*>(model());
    if (!pModel)
        return -1;

    QItemSelectionModel *pSelectionModel =  selectionModel();
    if (!pSelectionModel)
        return -1;
    QModelIndexList selectedItemIndices = pSelectionModel->selectedRows();
    if (selectedItemIndices.isEmpty())
        return -1;

    /* just use the the 1st index: */
    QModelIndex modelIndex = pModel->mapToSource(selectedItemIndices[0]);

    if (!modelIndex.isValid())
        return -1;
    return modelIndex.row();
}

bool UIVMActivityOverviewAccessibleTableView::hasSelection() const
{
    if (!selectionModel())
        return false;
    return selectionModel()->hasSelection();
}

void UIVMActivityOverviewAccessibleTableView::setMinimumColumnWidths(const QMap<int, int>& widths)
{
    m_minimumColumnWidths = widths;
    resizeHeaders();
}

void UIVMActivityOverviewAccessibleTableView::resizeEvent(QResizeEvent *pEvent)
{
    resizeHeaders();
    QTableView::resizeEvent(pEvent);
}

void UIVMActivityOverviewAccessibleTableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    emit sigSelectionChanged(selected, deselected);
    QTableView::selectionChanged(selected, deselected);
}

void UIVMActivityOverviewAccessibleTableView::mousePressEvent(QMouseEvent *pEvent)
{
    if (!indexAt(pEvent->position().toPoint()).isValid())
        clearSelection();
    QTableView::mousePressEvent(pEvent);
}

void UIVMActivityOverviewAccessibleTableView::resizeHeaders()
{
    QHeaderView* pHeader = horizontalHeader();
    if (!pHeader)
        return;
    int iSectionCount = pHeader->count();
    int iHiddenSectionCount = pHeader->hiddenSectionCount();
    int iWidth = width() / (iSectionCount - iHiddenSectionCount);
    for (int i = 0; i < iSectionCount; ++i)
    {
        if (pHeader->isSectionHidden(i))
            continue;
        int iMinWidth = m_minimumColumnWidths.value((VMActivityOverviewColumn)i, 0);
        pHeader->resizeSection(i, iWidth < iMinWidth ? iMinWidth : iWidth);
    }
}


/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleModel implementation.                                                                            *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleModel::UIActivityOverviewAccessibleModel(QObject *pParent, QITableView *pView)
    :QAbstractTableModel(pParent)
    , m_pTableView(pView)
    , m_pLocalVMUpdateTimer(new QTimer(this))
{
    initialize();
}

void UIActivityOverviewAccessibleModel::setColumnVisible(const QMap<int, bool>& columnVisible)
{
    m_columnVisible = columnVisible;
}

bool UIActivityOverviewAccessibleModel::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


bool UIActivityOverviewAccessibleModel::isVMRunning(int rowIndex) const
{
    if (rowIndex >= m_rows.size() || rowIndex < 0 || !m_rows[rowIndex])
        return false;
    return m_rows[rowIndex]->isRunning();
}

bool UIActivityOverviewAccessibleModel::isCloudVM(int rowIndex) const
{
    if (rowIndex >= m_rows.size() || rowIndex < 0 || !m_rows[rowIndex])
        return false;
    return m_rows[rowIndex]->isCloudVM();
}

void UIActivityOverviewAccessibleModel::setupPerformanceCollector()
{
    m_nameList.clear();
    m_objectList.clear();
    /* Initialize and configure CPerformanceCollector: */
    const ULONG iPeriod = 1;
    const int iMetricSetupCount = 1;
    if (m_performanceCollector.isNull())
        m_performanceCollector = gpGlobalSession->virtualBox().GetPerformanceCollector();
    for (int i = 0; i < m_rows.size(); ++i)
        m_nameList << "Guest/RAM/Usage*";
    /* This is for the host: */
    m_nameList << "CPU*";
    m_nameList << "FS*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceCollector.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
}

void UIActivityOverviewAccessibleModel::clearData()
{
    /* We have a request to detach COM stuff,
     * first of all we are removing all the items,
     * this will detach COM wrappers implicitly: */
    qDeleteAll(m_rows);
    m_rows.clear();
    /* Detaching perf. collector finally,
     * please do not use it after all: */
    m_performanceCollector.detach();
}

UIActivityOverviewAccessibleModel::~UIActivityOverviewAccessibleModel()
{
    clearData();
}

void UIActivityOverviewAccessibleModel::setShouldUpdate(bool fShouldUpdate)
{
    if (m_pLocalVMUpdateTimer)
    {
        if (fShouldUpdate)
            m_pLocalVMUpdateTimer->start();
        else
            m_pLocalVMUpdateTimer->stop();
    }
}

int UIActivityOverviewAccessibleModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_rows.size();
}

int UIActivityOverviewAccessibleModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_rows.isEmpty())
        return 0;
    if (m_rows[0])
        return m_rows[0]->childCount();
    return 0;
}

QVariant UIActivityOverviewAccessibleModel::data(const QModelIndex &index, int role) const
{
    int iRow = index.row();
    if (iRow < 0 || iRow >= m_rows.size())
        return QVariant();
    if (!m_rows[iRow])
        return QVariant();

    if (role == Qt::DisplayRole)
    {
        return m_rows[iRow]->cellText(index.column());

    }
    return QVariant();
}

void UIActivityOverviewAccessibleModel::initialize()
{
    for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
        m_columnDataMaxLength[i] = 0;

    if (m_pLocalVMUpdateTimer)
    {
        connect(m_pLocalVMUpdateTimer, &QTimer::timeout, this, &UIActivityOverviewAccessibleModel::sltLocalVMUpdateTimeout);
        m_pLocalVMUpdateTimer->start(1000);
    }

    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIActivityOverviewAccessibleModel::sltMachineStateChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
            this, &UIActivityOverviewAccessibleModel::sltMachineRegistered);
    foreach (const CMachine &comMachine, gpGlobalSession->virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
            addRow(comMachine.GetId(), comMachine.GetName(), comMachine.GetState());
    }
    setupPerformanceCollector();
}

void UIActivityOverviewAccessibleModel::addRow(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState)
{
    //QVector<UIActivityOverviewAccessibleRow*> m_rows;
    m_rows << new UIActivityOverviewAccessibleRowLocal(m_pTableView, uMachineId, strMachineName, enmState);
}

QVariant UIActivityOverviewAccessibleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return m_columnTitles.value((VMActivityOverviewColumn)section, QString());;
    return QVariant();
}

void UIActivityOverviewAccessibleModel::setColumnCaptions(const QMap<int, QString>& captions)
{
    m_columnTitles = captions;
}


void UIActivityOverviewAccessibleModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = itemIndex(uId);
    if (iIndex != -1 && iIndex < m_rows.size())
    {
        UIActivityOverviewAccessibleRowLocal *pItem = qobject_cast<UIActivityOverviewAccessibleRowLocal*>(m_rows[iIndex]);
        if (pItem)
        {
            pItem->setMachineState(state);
            if (state == KMachineState_Running)
                pItem->resetDebugger();
        }
    }
}

void UIActivityOverviewAccessibleModel::sltMachineRegistered(const QUuid &uId, bool fRegistered)
{
    if (fRegistered)
    {
        CMachine comMachine = gpGlobalSession->virtualBox().FindMachine(uId.toString());
        if (!comMachine.isNull())
            addRow(uId, comMachine.GetName(), comMachine.GetState());
    }
    else
        removeRow(uId);
    emit sigDataUpdate();
}

void UIActivityOverviewAccessibleModel::sltLocalVMUpdateTimeout()
{
    /* Host's RAM usage is obtained from IHost not from IPerformanceCollector: */
    //getHostRAMStats();

    /* Use IPerformanceCollector to update VM RAM usage and Host CPU and file IO stats: */
    queryPerformanceCollector();

    for (int i = 0; i < m_rows.size(); ++i)
    {
        UIActivityOverviewAccessibleRowLocal *pItem = qobject_cast<UIActivityOverviewAccessibleRowLocal*>(m_rows[i]);
        if (!pItem || !pItem->isRunning())
            continue;
        pItem->updateCells();
    }

    for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
    {
        for (int j = 0; j < m_rows.size(); ++j)
            if (m_columnDataMaxLength.value(i, 0) < m_rows[j]->columnLength(i))
                m_columnDataMaxLength[i] = m_rows[j]->columnLength(i);
    }

    emit sigDataUpdate();
    //emit sigHostStatsUpdate(m_hostStats);
}

int UIActivityOverviewAccessibleModel::itemIndex(const QUuid &uid)
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (!m_rows[i])
            continue;
        if (m_rows[i]->machineId() == uid)
            return i;
    }
    return -1;
}

void UIActivityOverviewAccessibleModel::removeRow(const QUuid& uMachineId)
{
    int iIndex = itemIndex(uMachineId);
    if (iIndex == -1)
        return;
    delete m_rows[iIndex];
    m_rows.remove(iIndex);
}

void UIActivityOverviewAccessibleModel::queryPerformanceCollector()
{
    QVector<QString>  aReturnNames;
    QVector<CUnknown>  aReturnObjects;
    QVector<QString>  aReturnUnits;
    QVector<ULONG>  aReturnScales;
    QVector<ULONG>  aReturnSequenceNumbers;
    QVector<ULONG>  aReturnDataIndices;
    QVector<ULONG>  aReturnDataLengths;

    QVector<LONG> returnData = m_performanceCollector.QueryMetricsData(m_nameList,
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
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive) || aReturnNames[i].contains("Free", Qt::CaseInsensitive))
            {
                {
                    CMachine comMachine = (CMachine)aReturnObjects[i];
                    if (comMachine.isNull())
                        continue;
                    int iIndex = itemIndex(comMachine.GetId());
                    if (iIndex == -1 || iIndex >= m_rows.size() || !m_rows[iIndex])
                        continue;

                    UIActivityOverviewAccessibleRowLocal *pItem = qobject_cast<UIActivityOverviewAccessibleRowLocal*>(m_rows[iIndex]);
                    if (!pItem)
                        continue;
                    if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                        pItem->setTotalRAM((quint64)fData);
                    else
                        pItem->setFreeRAM((quint64)fData);
                }
            }
        }
    //     else if (aReturnNames[i].contains("CPU/Load/User", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
    //     {
    //         CHost comHost = (CHost)aReturnObjects[i];
    //         if (!comHost.isNull())
    //             m_hostStats.m_iCPUUserLoad = fData;
    //     }
    //     else if (aReturnNames[i].contains("CPU/Load/Kernel", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
    //     {
    //         CHost comHost = (CHost)aReturnObjects[i];
    //         if (!comHost.isNull())
    //             m_hostStats.m_iCPUKernelLoad = fData;
    //     }
    //     else if (aReturnNames[i].contains("CPU/MHz", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
    //     {
    //         CHost comHost = (CHost)aReturnObjects[i];
    //         if (!comHost.isNull())
    //             m_hostStats.m_iCPUFreq = fData;
    //     }
    //    else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
    //             aReturnNames[i].contains("Total", Qt::CaseInsensitive) &&
    //             !aReturnNames[i].contains(":"))
    //    {
    //         CHost comHost = (CHost)aReturnObjects[i];
    //         if (!comHost.isNull())
    //             m_hostStats.m_iFSTotal = _1M * fData;
    //    }
    //    else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
    //             aReturnNames[i].contains("Free", Qt::CaseInsensitive) &&
    //             !aReturnNames[i].contains(":"))
    //    {
    //         CHost comHost = (CHost)aReturnObjects[i];
    //         if (!comHost.isNull())
    //             m_hostStats.m_iFSFree = _1M * fData;
    //    }
    }
}


/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleProxyModel implementation.                                                                       *
*********************************************************************************************************************************/
UIActivityOverviewAccessibleProxyModel::UIActivityOverviewAccessibleProxyModel(QObject *pParent /* = 0 */)
    :  QSortFilterProxyModel(pParent)
{
}

void UIActivityOverviewAccessibleProxyModel::dataUpdate()
{
    if (sourceModel())
        emit dataChanged(index(0,0), index(sourceModel()->rowCount(), sourceModel()->columnCount()));
    invalidate();
}

void UIActivityOverviewAccessibleProxyModel::setNotRunningVMVisibility(bool fShow)
{
    if (m_fShowNotRunningVMs == fShow)
        return;
    m_fShowNotRunningVMs = fShow;
    invalidateFilter();
}


void UIActivityOverviewAccessibleProxyModel::setCloudVMVisibility(bool fShow)
{
    if (m_fShowCloudVMs == fShow)
        return;
    m_fShowCloudVMs = fShow;
    invalidateFilter();
}

bool UIActivityOverviewAccessibleProxyModel::filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (m_fShowNotRunningVMs && m_fShowCloudVMs)
        return true;
    UIActivityOverviewAccessibleModel *pModel = qobject_cast<UIActivityOverviewAccessibleModel*>(sourceModel());
    if (!pModel)
        return true;

    if (!m_fShowNotRunningVMs && !pModel->isVMRunning(iSourceRow))
        return false;
    if (!m_fShowCloudVMs && pModel->isCloudVM(iSourceRow))
        return false;
    return true;
}



/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleRow implementation.                                                                              *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleRow::UIActivityOverviewAccessibleRow(QITableView *pTableView, const QUuid &uMachineId,
                                                                 const QString &strMachineName)
    : QITableViewRow(pTableView)
    , m_uMachineId(uMachineId)
    , m_strMachineName(strMachineName)
    , m_uTotalRAM(0)
{
    initCells();
}

int UIActivityOverviewAccessibleRow::childCount() const
{
    return m_cells.size();
}

QITableViewCell *UIActivityOverviewAccessibleRow::childItem(int iIndex) const
{
    return m_cells.value(iIndex, 0);
}

QString UIActivityOverviewAccessibleRow::cellText(int iColumn) const
{
    if (!m_cells.contains(iColumn))
        return QString();
    if (!m_cells[iColumn])
        return QString();
    return m_cells[iColumn]->text();
}

int UIActivityOverviewAccessibleRow::columnLength(int iColumnIndex) const
{
    UIActivityOverviewAccessibleCell *pCell = m_cells.value(iColumnIndex, 0);
    if (!pCell)
        return 0;
    return pCell->text().length();
}

UIActivityOverviewAccessibleRow::~UIActivityOverviewAccessibleRow()
{
    qDeleteAll(m_cells);
}

void UIActivityOverviewAccessibleRow::initCells()
{
    for (int i = (int) VMActivityOverviewColumn_Name; i < (int) VMActivityOverviewColumn_Max; ++i)
        m_cells[i] = new UIActivityOverviewAccessibleCell(this, i);
    m_cells[VMActivityOverviewColumn_Name]->setText(m_strMachineName);
}

const QUuid &UIActivityOverviewAccessibleRow::machineId() const
{
    return m_uMachineId;
}


/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleCell implementation.                                                                             *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleCell::UIActivityOverviewAccessibleCell(QITableViewRow *pRow, int iColumnIndex)
    :QITableViewCell(pRow)
    , m_iColumnIndex(iColumnIndex)
{
}

QString UIActivityOverviewAccessibleCell::text() const
{
    return m_strText;
}

int UIActivityOverviewAccessibleCell::columnLength(int /*iColumnIndex*/) const
{
    return 0;
    //return m_columnData.value(iColumnIndex, QString()).length();
}

void UIActivityOverviewAccessibleCell::setText(const QString &strText)
{
    m_strText = strText;
}

#include "UIVMActivityOverviewModelView.moc"
