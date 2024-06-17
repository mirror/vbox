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
#include <QTimer>

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataDefs.h"
#include "UIGlobalSession.h"
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
    void resetDebugger();
    void updateCells();
    bool isWithGuestAdditions();
    void setTotalRAM(quint64 uTotalRAM);
    void setFreeRAM(quint64 uFreeRAM);

private:

    KMachineState    m_enmMachineState;
    CMachineDebugger m_comDebugger;
    CSession         m_comSession;
    CGuest           m_comGuest;

    quint64  m_uTotalRAM;
    quint64  m_uFreeRAM;
};

UIActivityOverviewAccessibleRowLocal::UIActivityOverviewAccessibleRowLocal(QITableView *pTableView, const QUuid &uMachineId,
                                                                           const QString &strMachineName, KMachineState enmMachineState)
    : UIActivityOverviewAccessibleRow(pTableView, uMachineId, strMachineName)
    , m_enmMachineState(enmMachineState)
    , m_uTotalRAM(0)
    , m_uFreeRAM(0)
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

    if (m_cells.value(VMActivityOverviewColumn_CPUVMMLoad, 0))
        m_cells[VMActivityOverviewColumn_CPUVMMLoad]->setText(QString("%1%").arg(QString::number(uCPUVMMLoad)));
    if (m_cells.value(VMActivityOverviewColumn_CPUGuestLoad, 0))
        m_cells[VMActivityOverviewColumn_CPUGuestLoad]->setText(QString("%1%").arg(QString::number(uCPUGuestLoad)));

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

    if (m_cells.value(VMActivityOverviewColumn_RAMUsedAndTotal, 0))
        m_cells[VMActivityOverviewColumn_RAMUsedAndTotal]->setText(strRAMUsage);
    if (m_cells.value(VMActivityOverviewColumn_RAMUsedPercentage, 0))
        m_cells[VMActivityOverviewColumn_RAMUsedPercentage]->setText(strRAMPercentage);
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

/*********************************************************************************************************************************
*   UIVMActivityOverviewAccessibleTableView implementation.                                                                      *
*********************************************************************************************************************************/

UIVMActivityOverviewAccessibleTableView::UIVMActivityOverviewAccessibleTableView(QWidget *pParent)
    : QITableView(pParent)
{
}

void UIVMActivityOverviewAccessibleTableView::setMinimumColumnWidths(const QMap<int, int>& widths)
{
    m_minimumColumnWidths = widths;
    resizeHeaders();
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

    // for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
    // {
    //     for (int j = 0; j < m_rows.size(); ++j)
    //         if (m_columnDataMaxLength.value(i, 0) < m_rows[j]->columnLength(i))
    //             m_columnDataMaxLength[i] = m_rows[j]->columnLength(i);
    // }

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


/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleRow implementation.                                                                              *
*********************************************************************************************************************************/

UIActivityOverviewAccessibleRow::UIActivityOverviewAccessibleRow(QITableView *pTableView, const QUuid &uMachineId,
                                                                 const QString &strMachineName)
    : QITableViewRow(pTableView)
    , m_uMachineId(uMachineId)
    , m_strMachineName(strMachineName)
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
