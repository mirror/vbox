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
class UIGuestControlFileModel;
class UIGuestControlFileTable;
class UIToolBar;

/*********************************************************************************************************************************
*   UIFileTableItem definition.                                                                                                  *
*********************************************************************************************************************************/

class UIFileTableItem
{
public:
    explicit UIFileTableItem(const QList<QVariant> &data, bool isDirectory, UIFileTableItem *parentItem);
    ~UIFileTableItem();

    void appendChild(UIFileTableItem *child);

    UIFileTableItem *child(int row) const;
    /** Return a child (if possible) by path */
    UIFileTableItem *child(const QString &path) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void setData(const QVariant &data, int index);
    int row() const;
    UIFileTableItem *parentItem();

    bool isDirectory() const;
    bool isOpened() const;
    void setIsOpened(bool flag);

    const QString  &path() const;
    void setPath(const QString &path);
    /** Merge prefix and suffix by making sure they have a single '/' in between */
    void setPath(const QString &prexix, const QString &suffix);

    /** True if this is directory and name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

private:
    QList<UIFileTableItem*>         m_childItems;
    /** Used to find children by path */
    QMap<QString, UIFileTableItem*> m_childMap;
    QList<QVariant>  m_itemData;
    UIFileTableItem *m_parentItem;
    bool             m_bIsDirectory;
    bool             m_bIsOpened;
    /** Full absolute path of the item. Without the trailing '/' */
    QString          m_strPath;
    /** For directories base name is the name of the lowest level directory
        in strPath. eg. for 'm_strPath = /opt/qt5.6/examples' 'm_strBaseName = examples'
        for files it is the name of the file */
    QString          m_strBaseName;
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
    void emitLogOutput(const QString& strOutput);

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
    virtual void goToHomeDirectory() = 0;
    virtual bool renameItem(UIFileTableItem *item, QString newBaseName) = 0;
    virtual bool createDirectory(const QString &path, const QString &directoryName) = 0;
    void             goIntoDirectory(const QModelIndex &itemIndex);
    /** Follow the path trail, open directories as we go and descend */
    void             goIntoDirectory(const QVector<QString> &pathTrail);
    /** Go into directory pointed by the @p item */
    void             goIntoDirectory(UIFileTableItem *item);
    UIFileTableItem* indexData(const QModelIndex &index) const;
    void keyPressEvent(QKeyEvent * pEvent);

    /** Replace the last part of the @p previusPath with newBaseName */
    QString constructNewItemPath(const QString &previousPath, const QString &newBaseName);
    QString mergePaths(const QString &Path, const QString &baseName);

    UIFileTableItem         *m_pRootItem;

    /** Using QITableView causes the following problem when I click on the table items
        Qt WARNING: Cannot creat accessible child interface for object:  UIGuestControlFileView.....
        so for now subclass QTableView */
    QTableView              *m_pView;
    UIGuestControlFileModel *m_pModel;
    QILabel                 *m_pLocationLabel;
    QAction                  *m_pGoHome;
protected slots:

    void sltItemDoubleClicked(const QModelIndex &index);
    void sltGoUp();
    void sltGoHome();
    void sltRefresh();
    void sltDelete();
    void sltRename();
    void sltCreateNewDirectory();

private:

    void             prepareObjects();
    void             prepareActions();
    void             deleteByIndex(const QModelIndex &itemIndex);
    /** Return the UIFileTableItem for path / which is a direct (and single) child of m_pRootItem */
    UIFileTableItem *getStartDirectoryItem();
    /** Shows a modal dialog with a line edit for user to enter a new directory name and return the entered string*/
    QString         getNewDirectoryName();
    QGridLayout     *m_pMainLayout;
    QILineEdit      *m_pCurrentLocationEdit;
    UIToolBar       *m_pToolBar;
    QAction         *m_pGoUp;

    QAction         *m_pRefresh;
    QAction         *m_pDelete;
    QAction         *m_pRename;
    QAction         *m_pCreateNewDirectory;

    QAction         *m_pCopy;
    QAction         *m_pCut;
    QAction         *m_pPaste;

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
    virtual void goToHomeDirectory() /* override */;
    virtual bool renameItem(UIFileTableItem *item, QString newBaseName);
    virtual bool createDirectory(const QString &path, const QString &directoryName);

private:

    void configureObjects();
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
    virtual void goToHomeDirectory() /* override */;
    virtual bool renameItem(UIFileTableItem *item, QString newBaseName);
    virtual bool createDirectory(const QString &path, const QString &directoryName);
};

#endif /* !___UIGuestControlFileTable_h___ */
