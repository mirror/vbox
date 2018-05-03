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
#include <QItemSelectionModel>
#include <QMutex>
#include <QThread>
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QIDialog.h"
#include "QITableView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAction;
class QFileInfo;

class QComboBox;
class QILabel;
class QILineEdit;
class QGridLayout;
class QSortFilterProxyModel;
class QTextEdit;
class QVBoxLayout;
class UIFileTableItem;
class UIGuestControlFileModel;
class UIGuestControlFileProxyModel;
class UIGuestControlFileView;
class UIToolBar;

enum FileObjectType
{
    FileObjectType_File = 0,
    FileObjectType_Directory,
    FileObjectType_SymLink,
    FileObjectType_Other,
    FileObjectType_Unknown,
    FileObjectType_Max
};

/** A simple struck to store some statictics for a directory. Mainly used by  UIDirectoryDiskUsageComputer instances. */
class UIDirectoryStatistics
{
public:
    UIDirectoryStatistics();
    ULONG64    m_totalSize;
    unsigned   m_uFileCount;
    unsigned   m_uDirectoryCount;
    unsigned   m_uSymlinkCount;
};

Q_DECLARE_METATYPE(UIDirectoryStatistics);


/** Examines the paths in @p strStartPath and collects some staticstics from them recursively (in case directories)
 *  Runs on a worker thread to avoid GUI freezes. UIGuestFileTable and UIHostFileTable uses specialized children
 *  of this class since the calls made on file objects are different. */
class UIDirectoryDiskUsageComputer : public QThread
{
    Q_OBJECT;

signals:

    void sigResultUpdated(UIDirectoryStatistics);

public:

    UIDirectoryDiskUsageComputer(QObject *parent, QStringList strStartPath);
    /** Sets the m_fOkToContinue to false. This results an early termination
      * of the  directoryStatisticsRecursive member function. */
    void stopRecursion();

protected:

    /** Read the directory with the path @p path recursively and collect #of objects and  total size */
    virtual void directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics) = 0;
    virtual void           run() /* override */;
    /** Returns the m_fOkToContinue flag */
    bool                  isOkToContinue() const;
    /** Stores a list of paths whose statistics are accumulated, can be file, directory etc: */
    QStringList           m_pathList;
    UIDirectoryStatistics m_resultStatistics;
    QMutex                m_mutex;

private:

    bool     m_fOkToContinue;
};


/** A QIDialog child to display properties of a file object */
class UIPropertiesDialog : public QIDialog
{

    Q_OBJECT;

public:

    UIPropertiesDialog(QWidget *pParent = 0, Qt::WindowFlags flags = 0);
    void setPropertyText(const QString &strProperty);
    void addDirectoryStatistics(UIDirectoryStatistics statictics);

private:

    QVBoxLayout    *m_pMainLayout;
    QTextEdit *m_pInfoEdit;
    QString   m_strProperty;
};

/** A collection of simple utility functions to manipulate path strings */
class UIPathOperations
{
public:
    static QString removeMultipleDelimiters(const QString &path);
    static QString removeTrailingDelimiters(const QString &path);
    static QString addTrailingDelimiters(const QString &path);
    static QString addStartDelimiter(const QString &path);

    static QString sanitize(const QString &path);
    /** Merge prefix and suffix by making sure they have a single '/' in between */
    static QString mergePaths(const QString &path, const QString &baseName);
    /** Returns the last part of the @p path. That is the filename or directory name without the path */
    static QString getObjectName(const QString &path);
    /** Remove the object name and return the path */
    static QString getPathExceptObjectName(const QString &path);
    /** Replace the last part of the @p previusPath with newBaseName */
    static QString constructNewItemPath(const QString &previousPath, const QString &newBaseName);
    /** Split the path and return it as a QStringList, top most being the 0th element. No delimiters */
    static QStringList pathTrail(const QString &path);
    static const QChar delimiter;
    static const QChar dosDelimiter;

    /** Try to guess if the path starts with DOS style drive letters */
    static bool doesPathStartWithDriveLetter(const QString &path);

};

/** A UIFileTableItem instance is a tree node representing a file object (file, directory, etc). The tree contructed
    by these instances is the data source for the UIGuestControlFileModel. */
class UIFileTableItem
{
public:

    /** @p data contains values to be shown in table view's colums. data[0] is assumed to be
     *  the name of the file object which is the file name including extension or name of the
     *  directory */
    explicit UIFileTableItem(const QList<QVariant> &data,
                             UIFileTableItem *parentItem, FileObjectType type);
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
    bool isSymLink() const;
    bool isFile() const;

    bool isOpened() const;
    void setIsOpened(bool flag);

    const QString  &path() const;
    void setPath(const QString &path);

    /** True if this is directory and name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

    FileObjectType   type() const;

    const QString &targetPath() const;
    void setTargetPath(const QString &path);

    bool isTargetADirectory() const;
    void setIsTargetADirectory(bool flag);

    const QString &owner() const;
    void setOwner(const QString &owner);

    QString name() const;

    void setIsDriveItem(bool flag);
    bool isDriveItem() const;

private:

    QList<UIFileTableItem*>         m_childItems;
    /** Used to find children by name */
    QMap<QString, UIFileTableItem*> m_childMap;
    /** It is required that m_itemData[0] is name (QString) of the file object */
    QList<QVariant>  m_itemData;
    UIFileTableItem *m_parentItem;
    bool             m_bIsOpened;
    /** Full absolute path of the item. Without the trailing '/' */
    QString          m_strPath;
    /** If this is a symlink m_targetPath keeps the absolute path of the target */
    QString          m_strTargetPath;
    /** True if this is a symlink and the target is a directory */
    bool             m_isTargetADirectory;
    FileObjectType   m_type;
    /** True if only this item represents a DOS style drive letter item */
    bool             m_isDriveItem;
};


/** This class serves a base class for file table. Currently a guest version
 *  and a host version are derived from this base. Each of these children
 *  populates the UIGuestControlFileModel by scanning the file system
 *  differently. The file structre kept in this class as a tree. */
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
    /** Returns the path of the rootIndex */
    QString     currentDirectoryPath() const;
    /** Returns the paths of the selected items (if any) as a list */
    QStringList selectedItemPathList();
    virtual void refresh();

    static const unsigned    m_iKiloByte;
    static QString humanReadableSize(ULONG64 size);


protected:

    void retranslateUi();
    void updateCurrentLocationEdit(const QString& strLocation);
    /* @p index is for model not for 'proxy' model */
    void changeLocation(const QModelIndex &index);
    void initializeFileTree();
    void insertItemsToTree(QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent,
                           bool isDirectoryMap, bool isStartDir);
    virtual void     readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) = 0;
    virtual void     deleteByItem(UIFileTableItem *item) = 0;
    virtual void     goToHomeDirectory() = 0;
    virtual bool     renameItem(UIFileTableItem *item, QString newBaseName) = 0;
    virtual bool     createDirectory(const QString &path, const QString &directoryName) = 0;
    virtual QString  fsObjectPropertyString() = 0;
    virtual void     showProperties() = 0;
    /** For non-windows system does nothing and for windows systems populates m_driveLetterList with
     *  drive letters */
    virtual void     determineDriveLetters() = 0;
    QString          fileTypeString(FileObjectType type);
    /* @p item index is item location in model not in 'proxy' model */
    void             goIntoDirectory(const QModelIndex &itemIndex);
    /** Follows the path trail, opens directories as it descends */
    void             goIntoDirectory(const QStringList &pathTrail);
    /** Goes into directory pointed by the @p item */
    void             goIntoDirectory(UIFileTableItem *item);
    UIFileTableItem* indexData(const QModelIndex &index) const;
    void             keyPressEvent(QKeyEvent * pEvent);
    CGuestFsObjInfo  guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const;

    UIFileTableItem         *m_pRootItem;
    QILabel                 *m_pLocationLabel;
    QAction                 *m_pCopy;
    QAction                 *m_pCut;
    QAction                 *m_pPaste;
    UIPropertiesDialog      *m_pPropertiesDialog;
    /** Stores the drive letters the file system has (for windows system). For non-windows
     *  systems this is empty and for windows system it should at least contain C:/ */
    QStringList m_driveLetterList;

protected slots:

    void sltReceiveDirectoryStatistics(UIDirectoryStatistics statictics);

private slots:
    /* index is passed by the item view and represents the double clicked object's 'proxy' model index */
    void sltItemDoubleClicked(const QModelIndex &index);
    void sltGoUp();
    void sltGoHome();
    void sltRefresh();
    void sltDelete();
    void sltRename();
    void sltCopy();
    void sltCut();
    void sltPaste();
    void sltShowProperties();
    void sltCreateNewDirectory();
    void sltSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);
    void sltLocationComboCurrentChange(const QString &strLocation);
    void sltSelectAll();
    void sltInvertSelection();

private:

    void             prepareObjects();
    void             prepareActions();
    /* @itemIndex is assumed to be 'model' index not 'proxy model' index */
    void             deleteByIndex(const QModelIndex &itemIndex);
    /** Returns the UIFileTableItem for path / which is a direct (and single) child of m_pRootItem */
    UIFileTableItem *getStartDirectoryItem();
    /** Shows a modal dialog with a line edit for user to enter a new directory name and return the entered string*/
    QString         getNewDirectoryName();
    void            enableSelectionDependentActions();
    void            disableSelectionDependentActions();
    void            deSelectUpDirectoryItem();
    void            setSelectionForAll(QItemSelectionModel::SelectionFlags flags);
    /** Start directory requires a special attention since on file systems with drive letters
     *  drive letter are direct children of the start directory. On other systems start directory is '/' */
    void            populateStartDirectory(UIFileTableItem *startItem);
    QModelIndex     currentRootIndex() const;
    UIGuestControlFileModel      *m_pModel;
    UIGuestControlFileView       *m_pView;
    UIGuestControlFileProxyModel *m_pProxyModel;

    QGridLayout     *m_pMainLayout;
    QComboBox       *m_pLocationComboBox;
    UIToolBar       *m_pToolBar;
    QAction         *m_pGoUp;
    QAction         *m_pGoHome;
    QAction         *m_pRefresh;
    QAction         *m_pDelete;
    QAction         *m_pRename;
    QAction         *m_pCreateNewDirectory;
    QAction         *m_pShowProperties;
    QAction         *m_pSelectAll;
    QAction         *m_pInvertSelection;
    /** The vector of action which need some selection to work on like cut, copy etc. */
    QVector<QAction*> m_selectionDependentActions;
    /** The absolue path list of the file objects which user has chosen to cut/copy. this
     *  list will be cleaned after a paste operation or overwritten by a subsequent cut/copy */
    QStringList       m_copyCutBuffer;
    friend class UIGuestControlFileModel;
};

#endif /* !___UIGuestControlFileTable_h___ */
