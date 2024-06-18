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
#include "CPerformanceMetric.h"
#include "CPerformanceCollector.h"
#include "KMachineState.h"


class QTimer;
class UIActivityOverviewAccessibleCell;
class UIActivityOverviewAccessibleRow;

class UIVMActivityOverviewAccessibleTableView : public QITableView
{
    Q_OBJECT;

signals:

    void sigSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

public:

    UIVMActivityOverviewAccessibleTableView(QWidget *pParent);
    void setMinimumColumnWidths(const QMap<int, int>& widths);
    void updateColumVisibility();
    int selectedItemIndex() const;
    bool hasSelection() const;

private:

    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) RT_OVERRIDE;
    virtual void mousePressEvent(QMouseEvent *pEvent) RT_OVERRIDE;

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
    void dataUpdate();
    void setNotRunningVMVisibility(bool fShow);
    void setCloudVMVisibility(bool fShow);

protected:

    // virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const RT_OVERRIDE;
    bool filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const RT_OVERRIDE RT_FINAL;

private:

    bool m_fShowNotRunningVMs;
    bool m_fShowCloudVMs;

};

class UIActivityOverviewAccessibleCell : public QITableViewCell
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleCell(QITableViewRow *pRow, int iColumnIndex);
    virtual QString text() const RT_OVERRIDE RT_FINAL;
    int columnLength(int iColumnIndex) const;
    void setText(const QString &strText);

private:

    /* VMActivityOverviewColumn enum: */
    int m_iColumnIndex;
    QString m_strText;
};

class UIActivityOverviewAccessibleRow : public QITableViewRow
{

    Q_OBJECT;

public:

    UIActivityOverviewAccessibleRow(QITableView *pTableView, const QUuid &uMachineId,
                                    const QString &strMachineName);

    const QUuid &machineId() const;

    virtual void setMachineState(int iState) = 0;
    virtual bool isRunning() const = 0;
    virtual bool isCloudVM() const = 0;

    virtual ~UIActivityOverviewAccessibleRow();
    virtual int childCount() const RT_OVERRIDE RT_FINAL;

    virtual QITableViewCell *childItem(int iIndex) const RT_OVERRIDE RT_FINAL;
    int columnLength(int iColumnIndex) const;
    QString cellText(int iColumn) const;
    virtual QString machineStateString() const = 0;

protected:

    QUuid m_uMachineId;
    /* Key is VMActivityOverviewColumn enum item. */
    QMap<int, UIActivityOverviewAccessibleCell*> m_cells;

    QString m_strMachineName;
    quint64  m_uTotalRAM;
private:

    void initCells();


};



class UIActivityOverviewAccessibleModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    void sigDataUpdate();

public:

    ~UIActivityOverviewAccessibleModel();
    UIActivityOverviewAccessibleModel(QObject *pParent, QITableView *pView);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE RT_FINAL;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE RT_FINAL;
    QVariant data(const QModelIndex &index, int role) const RT_OVERRIDE RT_FINAL;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const RT_OVERRIDE  RT_FINAL;
    void setColumnCaptions(const QMap<int, QString>& captions);
    int itemIndex(const QUuid &uid);
    void setShouldUpdate(bool fShouldUpdate);
    void clearData();
    bool isVMRunning(int rowIndex) const;
    bool isCloudVM(int rowIndex) const;
    void setColumnVisible(const QMap<int, bool>& columnVisible);
    bool columnVisible(int iColumnId) const;

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltMachineRegistered(const QUuid &uId, bool fRegistered);
    void sltLocalVMUpdateTimeout();

private:

    void initialize();
    void addRow(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState);
    void removeRow(const QUuid& uMachineId);
    void setupPerformanceCollector();
    void queryPerformanceCollector();
    QVector<UIActivityOverviewAccessibleRow*> m_rows;
    QITableView *m_pTableView;
    QMap<int, QString> m_columnTitles;
    QMap<int, bool> m_columnVisible;
    QTimer *m_pLocalVMUpdateTimer;
    /** Maximum length of string length of data displayed in column. Updated in UIActivityOverviewModel::data(..). */
    mutable QMap<int, int> m_columnDataMaxLength;
    CPerformanceCollector m_performanceCollector;
    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
     * @{ */
       QVector<QString> m_nameList;
       QVector<CUnknown> m_objectList;
    /** @} */

};


#endif /* !FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewModelView_h */
