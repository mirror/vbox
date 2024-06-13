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


/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UIGlobalSession.h"
#include "UIVMActivityOverviewModelView.h"


#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CMachine.h"


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
{
    initialize();
}

UIActivityOverviewAccessibleModel::~UIActivityOverviewAccessibleModel()
{
    qDeleteAll(m_rows);
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

QVariant UIActivityOverviewAccessibleModel::data(const QModelIndex &/*index*/, int role) const
{
    if (role == Qt::DisplayRole)
        return "Foo";
    return QVariant();
}

void UIActivityOverviewAccessibleModel::initialize()
{
    foreach (const CMachine &comMachine, gpGlobalSession->virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
            addRow(comMachine.GetId(), comMachine.GetName(), comMachine.GetState());
    }
}

void UIActivityOverviewAccessibleModel::addRow(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState)
{
    //QVector<UIActivityOverviewAccessibleRow*> m_rows;
    m_rows << new UIActivityOverviewAccessibleRow(m_pTableView, uMachineId, strMachineName, enmState);
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

UIActivityOverviewAccessibleRow::UIActivityOverviewAccessibleRow(QITableView *pTableView, const QUuid &machineId,
                                                                 const QString &strMachineName, KMachineState enmMachineState)
    : QITableViewRow(pTableView)
    , m_machineId(machineId)
    , m_strMachineName(strMachineName)
    , m_enmMachineState(enmMachineState)

{
    initCells();
}

int UIActivityOverviewAccessibleRow::childCount() const
{
    return m_cells.size();
}

QITableViewCell *UIActivityOverviewAccessibleRow::childItem(int iIndex) const
{
    if (iIndex < 0 || iIndex >= m_cells.size())
        return 0;
    return m_cells[iIndex];
}

UIActivityOverviewAccessibleRow::~UIActivityOverviewAccessibleRow()
{
    qDeleteAll(m_cells);
}

void UIActivityOverviewAccessibleRow::initCells()
{
    for (int i = (int) VMActivityOverviewColumn_Name; i < (int) VMActivityOverviewColumn_Max; ++i)
    {
        m_cells << new UIActivityOverviewAccessibleCell(this, i);
    }
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
    return "Foo";
}
