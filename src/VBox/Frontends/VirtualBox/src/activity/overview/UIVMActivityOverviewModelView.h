/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityOverviewModelView class declaration.
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

#ifndef FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewModelView_h
#define FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewModelView_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QUuid>

/* GUI includes: */
#include "QITableView.h"

/* COM includes: */
#include "KMachineState.h"
class UIActivityOverviewAccessibleCell;
class UIActivityOverviewAccessibleRow;

class UIVMActivityOverviewAccessibleTableView : public QITableView
{
    Q_OBJECT;

public:

    UIVMActivityOverviewAccessibleTableView(QWidget *pParent);
    void setMinimumColumnWidths(const QMap<int, int>& widths);
    /** Resizes all the columns in response to resizeEvent. Columns cannot be narrower than m_minimumColumnWidths values. */
    void resizeHeaders();
    /** Value is in pixels. Columns cannot be narrower than this width. */
    QMap<int, int> m_minimumColumnWidths;

};



class UIActivityOverviewAccessibleProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleProxyModel(QObject *parent = 0);

protected:

    // virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const RT_OVERRIDE;
    // bool filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const RT_OVERRIDE;

private:


};

class UIActivityOverviewAccessibleCell : public QITableViewCell
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleCell(QITableViewRow *pRow, int iColumnIndex);
    virtual QString text() const RT_OVERRIDE RT_FINAL;

private:

    /* VMActivityOverviewColumn enum: */
    int m_iColumnIndex;

};

class UIActivityOverviewAccessibleRow : public QITableViewRow
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleRow(QITableView *pTableView, const QUuid &machineId,
                                    const QString &strMachineName, KMachineState enmMachineState);



    ~UIActivityOverviewAccessibleRow();
    virtual int childCount() const RT_OVERRIDE RT_FINAL;

    virtual QITableViewCell *childItem(int iIndex) const RT_OVERRIDE RT_FINAL;

private:

    void initCells();
        //const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState)
    QVector<UIActivityOverviewAccessibleCell*> m_cells;
    QUuid m_machineId;
    QString m_strMachineName;
    KMachineState m_enmMachineState;
};



class UIActivityOverviewAccessibleModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    ~UIActivityOverviewAccessibleModel();
    UIActivityOverviewAccessibleModel(QObject *pParent, QITableView *pView);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE RT_FINAL;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE RT_FINAL;
    QVariant data(const QModelIndex &index, int role) const RT_OVERRIDE RT_FINAL;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const RT_OVERRIDE  RT_FINAL;
    void setColumnCaptions(const QMap<int, QString>& captions);

private:

    void initialize();
    void addRow(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState);
    QVector<UIActivityOverviewAccessibleRow*> m_rows;
    QITableView *m_pTableView;
    QMap<int, QString> m_columnTitles;
};


#endif /* !FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewModelView_h */
