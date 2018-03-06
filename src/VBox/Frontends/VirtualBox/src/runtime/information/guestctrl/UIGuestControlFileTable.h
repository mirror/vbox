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
#include <QTreeView>
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QITableView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAction;
class QILabel;
class QILineEdit;
class QGridLayout;
class UIFileTableItem;
class UIGuestControlFileTable;
class UIToolBar;

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
    Qt::ItemFlags flags(const QModelIndex &index) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation,
    int role = Qt::DisplayRole) const /* override */;
    QModelIndex index(int row, int column,
    const QModelIndex &parent = QModelIndex()) const /* override */;
    QModelIndex parent(const QModelIndex &index) const /* override */;
    int rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    void signalUpdate();
    QModelIndex rootIndex() const;
    void beginReset();
    void endReset();

private:

    UIFileTableItem* rootItem() const;
    void setupModelData(const QStringList &lines, UIFileTableItem *parent);
    UIGuestControlFileTable* m_pParent;
    UIFileTableItem *m_pRootItem;
};

/** This class serves a base class for file table. Currently a guest version
    and a host version are derived from this base. Each of these children
    populates the UIGuestControlFileModel by scanning the file system
    differently. The file structre kept in this class as a tree and all
    the interfacing is done thru this class.*/
class UIGuestControlFileTable : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigLogOutput(QString);

public:

    UIGuestControlFileTable(QWidget *pParent = 0);
    virtual ~UIGuestControlFileTable();
    /** Delete all the tree nodes */
    void reset();

protected:

    void retranslateUi();
    void updateCurrentLocationEdit(const QString& strLocation);
    void changeLocation(const QModelIndex &index);
    void initializeFileTree();
    void insertItemsToTree(QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent,
                           bool isDirectoryMap, bool isStartDir);
    virtual void readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) = 0;
    virtual void refresh();
    virtual void deleteByItem(UIFileTableItem *item) = 0;
    void keyPressEvent(QKeyEvent * pEvent);
    UIFileTableItem         *m_pRootItem;

    /** Using QITableView causes the following problem when I click on the table items
        Qt WARNING: Cannot creat accessible child interface for object:  UIGuestControlFileView.....
        so for now subclass QTableView */
    QTableView              *m_pView;
    UIGuestControlFileModel *m_pModel;
    QTreeView               *m_pTree;
    QILabel                 *m_pLocationLabel;

protected slots:

    void sltItemDoubleClicked(const QModelIndex &index);
    void sltGoUp();
    void sltGoHome();
    void sltRefresh();
    void sltDelete();
    void sltRename();

private:

    void            prepareObjects();
    void            prepareActions();
    void            deleteByIndex(const QModelIndex &itemIndex);
    void            goIntoDirectory(const QModelIndex &itemIndex);
    QGridLayout    *m_pMainLayout;
    QILineEdit     *m_pCurrentLocationEdit;
    UIToolBar      *m_pToolBar;
    QAction        *m_pGoUp;
    QAction        *m_pGoHome;
    QAction        *m_pRefresh;
    QAction        *m_pDelete;
    QAction        *m_pRename;
    QAction        *m_pNewFolder;

    QAction        *m_pCopy;
    QAction        *m_pCut;
    QAction        *m_pPaste;

    friend class UIGuestControlFileModel;
};

/** This class scans the guest file system by using the VBox API
    and populates the UIGuestControlFileModel*/
class UIGuestFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIGuestFileTable(QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);

protected:

    void retranslateUi() /* override */;
    virtual void readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) /* override */;
    virtual void deleteByItem(UIFileTableItem *item) /* override */;

private:

    CGuestSession m_comGuestSession;
};

/** This class scans the host file system by using the Qt
    and populates the UIGuestControlFileModel*/
class UIHostFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIHostFileTable(QWidget *pParent = 0);

protected:

    void retranslateUi() /* override */;
    virtual void readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) /* override */;
    virtual void deleteByItem(UIFileTableItem *item) /* override */;
};

#endif /* !___UIGuestControlFileTable_h___ */
