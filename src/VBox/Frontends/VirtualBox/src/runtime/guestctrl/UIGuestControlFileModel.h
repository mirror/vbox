/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileModel class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlFileModel_h___
#define ___UIGuestControlFileModel_h___

/* Qt includes: */
#include <QAbstractItemModel>

/* GUI includes: */
#include "QITableView.h"

/* Forward declarations: */
class UIFileTableItem;
class UIGuestControlFileTable;

/** UIGuestControlFileModel serves as the model for a file structure.
    it supports a tree level hierarchy which can be displayed with
    QTableView and/or QTreeView. Note the file structure data is not
    kept by the model but rather by the containing widget which also servers
    as the interface to functionality this model provides.*/
class UIGuestControlFileModel : public QAbstractItemModel
{

    Q_OBJECT;

public:

    explicit UIGuestControlFileModel(QObject *parent = 0);
    ~UIGuestControlFileModel();

    QVariant data(const QModelIndex &index, int role) const /* override */;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    Qt::ItemFlags flags(const QModelIndex &index) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation,
    int role = Qt::DisplayRole) const /* override */;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const /* override */;
    QModelIndex index(UIFileTableItem* item);
    QModelIndex parent(const QModelIndex &index) const /* override */;
    int rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    void signalUpdate();
    QModelIndex rootIndex() const;
    void beginReset();
    void endReset();
    bool insertRows(int position, int rows, const QModelIndex &parent);

private:

    UIFileTableItem* rootItem() const;
    void setupModelData(const QStringList &lines, UIFileTableItem *parent);
    UIGuestControlFileTable* m_pParent;
    UIFileTableItem *m_pRootItem;
};


#endif /* !___UIGuestControlFileModel_h___ */

