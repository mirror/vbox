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
#include <QHeaderView>
#include <QTimer>

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataDefs.h"
#include "UIGlobalSession.h"
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

private:

    KMachineState    m_enmMachineState;
    CMachineDebugger m_comDebugger;
    CSession         m_comSession;
    CGuest           m_comGuest;
};

UIActivityOverviewAccessibleRowLocal::UIActivityOverviewAccessibleRowLocal(QITableView *pTableView, const QUuid &uMachineId,
                                                                           const QString &strMachineName, KMachineState enmMachineState)
    : UIActivityOverviewAccessibleRow(pTableView, uMachineId, strMachineName)
    , m_enmMachineState(enmMachineState)
{
    if (m_enmMachineState == KMachineState_Running)
        resetDebugger();

}

void UIActivityOverviewAccessibleRowLocal::updateCells()
{
    // if (m_cells.contains((int) VMActivityOverviewColumn_Name))
    // {
    //     m_cells[(int)VMActivityOverviewColumn_Name]->setText(m_strMachineName);
    // }
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

UIActivityOverviewAccessibleModel::~UIActivityOverviewAccessibleModel()
{
    qDeleteAll(m_rows);
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
    //queryPerformanceCollector();

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


/*********************************************************************************************************************************
*   UIActivityOverviewAccessibleProxyModel implementation.                                                                       *
*********************************************************************************************************************************/
UIActivityOverviewAccessibleProxyModel::UIActivityOverviewAccessibleProxyModel(QObject *pParent /* = 0 */)
    :  QSortFilterProxyModel(pParent)
{
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
    printf("%s\n", qPrintable(strText));
    m_strText = strText;
}

#include "UIVMActivityOverviewModelView.moc"
