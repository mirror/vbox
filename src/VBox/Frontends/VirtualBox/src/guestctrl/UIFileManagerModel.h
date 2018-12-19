/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerModel class declaration.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFileManagerModel_h___
#define ___UIFileManagerModel_h___

/* Qt includes: */
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QITableView.h"

/* Forward declarations: */
class UIFileTableItem;
class UIFileManagerTable;

enum UIFileManagerModelColumn
{
    UIFileManagerModelColumn_Name = 0,
    UIFileManagerModelColumn_Size,
    UIFileManagerModelColumn_ChangeTime,
    UIFileManagerModelColumn_Owner,
    UIFileManagerModelColumn_Permissions,
    UIFileManagerModelColumn_Max
};

/** A QSortFilterProxyModel extension used in file tables. Modifies some
 *  of the base class behavior like lessThan(..) */
class UIGuestControlFileProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIGuestControlFileProxyModel(QObject *parent = 0);

    void setListDirectoriesOnTop(bool fListDirectoriesOnTop);
    bool listDirectoriesOnTop() const;

protected:

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const /* override */;

private:

    bool m_fListDirectoriesOnTop;
};

/** UIFileManagerModel serves as the model for a file structure.
 *  it supports a tree level hierarchy which can be displayed with
 *  QTableView and/or QTreeView. Note the file structure data is not
 *  kept by the model but rather by the containing widget which also servers
 *  as the interface to functionality that this model provides.*/
class UIFileManagerModel : public QAbstractItemModel
{

    Q_OBJECT;

public:

    explicit UIFileManagerModel(QObject *parent = 0);
    ~UIFileManagerModel();

    QVariant       data(const QModelIndex &index, int role) const /* override */;
    bool           setData(const QModelIndex &index, const QVariant &value, int role);

    Qt::ItemFlags  flags(const QModelIndex &index) const /* override */;
    QVariant       headerData(int section, Qt::Orientation orientation,
                              int role = Qt::DisplayRole) const /* override */;
    QModelIndex    index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const /* override */;
    QModelIndex    index(UIFileTableItem* item);
    QModelIndex    parent(const QModelIndex &index) const /* override */;
    int            rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int            columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    void           signalUpdate();
    QModelIndex    rootIndex() const;
    void           beginReset();
    void           endReset();
    void           setShowHumanReadableSizes(bool fShowHumanReadableSizes);
    bool           showHumanReadableSizes() const;

    static const char* strUpDirectoryString;

private:

    UIFileTableItem* rootItem() const;
    void setupModelData(const QStringList &lines, UIFileTableItem *parent);
    UIFileManagerTable *m_pParent;
    UIFileTableItem    *m_pRootItem;
    bool                m_fShowHumanReadableSizes;

};


#endif /* !___UIFileManagerModel_h___ */
