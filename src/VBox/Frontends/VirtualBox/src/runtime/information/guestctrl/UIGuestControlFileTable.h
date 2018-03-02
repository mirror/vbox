/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class declaration.
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

#ifndef ___UIGuestControlFileTable_h___
#define ___UIGuestControlFileTable_h___

/* Qt includes: */
#include <QAbstractItemModel>
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
// #include "CEventListener.h"
// #include "CEventSource.h"
// #include "CGuest.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QITableView.h"

/* Forward declarations: */
class QVBoxLayout;
class UIFileTableItem;
class UIGuestControlFileTable;


class UIGuestControlFileModel : public QAbstractItemModel
{

    Q_OBJECT;

public:

    explicit UIGuestControlFileModel(QObject *parent = 0);
    ~UIGuestControlFileModel();

    QVariant data(const QModelIndex &index, int role) const /* override */;
    Qt::ItemFlags flags(const QModelIndex &index) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation,
    int role = Qt::DisplayRole) const /* override */;
    QModelIndex index(int row, int column,
    const QModelIndex &parent = QModelIndex()) const /* override */;
    QModelIndex parent(const QModelIndex &index) const /* override */;
    int rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    void signalUpdate();

private:

    UIFileTableItem* rootItem() const;
    void setupModelData(const QStringList &lines, UIFileTableItem *parent);
    UIGuestControlFileTable* m_pParent;
};


class UIGuestControlFileView : public QTableView
{
    Q_OBJECT;

public:

    UIGuestControlFileView(QWidget *pParent = 0);

private:

};

class UIGuestControlFileTable : public QWidget
{
    Q_OBJECT;

public:

    UIGuestControlFileTable(QWidget *pParent = 0);
    virtual ~UIGuestControlFileTable();

protected:

    UIFileTableItem         *m_pRootItem;
    UIGuestControlFileView  *m_pView;
    UIGuestControlFileModel *m_pModel;

private:

    void                    prepareObjects();
    QVBoxLayout             *m_pMainLayout;
    friend class UIGuestControlFileModel;
};


class UIGuestFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIGuestFileTable(QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);

private:
    void readDirectory(const QString& strPath,
                       UIFileTableItem *parent);
    void insertToTree(const QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent);
    CGuestSession m_comGuestSession;
};

#endif /* !___UIGuestControlFileTable_h___ */
