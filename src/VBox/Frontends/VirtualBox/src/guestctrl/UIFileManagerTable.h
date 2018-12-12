/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerTable class declaration.
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

#ifndef ___UIFileManagerTable_h___
#define ___UIFileManagerTable_h___

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
#include "UIGuestControlDefs.h"

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
class UIActionPool;
class UIFileTableItem;
class UIFileManagerModel;
class UIGuestControlFileProxyModel;
class UIGuestControlFileView;
class UIToolBar;

/** @todo r=bird: Why don't you just use KFsObjType? */
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
    /** Merges prefix and suffix by making sure they have a single '/' in between */
    static QString mergePaths(const QString &path, const QString &baseName);
    /** Returns the last part of the @p path. That is the filename or directory name without the path */
    static QString getObjectName(const QString &path);
    /** Removes the object name and return the path */
    static QString getPathExceptObjectName(const QString &path);
    /** Replaces the last part of the @p previusPath with newBaseName */
    static QString constructNewItemPath(const QString &previousPath, const QString &newBaseName);
    /** Splits the path and return it as a QStringList, top most being the 0th element. No delimiters */
    static QStringList pathTrail(const QString &path);
    static const QChar delimiter;
    static const QChar dosDelimiter;

    /** Tries to determine if the path starts with DOS style drive letters. */
    static bool doesPathStartWithDriveLetter(const QString &path);

};

/** A UIFileTableItem instance is a tree node representing a file object (file, directory, etc). The tree contructed
    by these instances is the data source for the UIFileManagerModel. */
class UIFileTableItem
{
public:

    /** @p data contains values to be shown in table view's colums. data[0] is assumed to be
     *  the name of the file object which is the file name including extension or name of the
     *  directory */
    explicit UIFileTableItem(const QVector<QVariant> &data,
                             UIFileTableItem *parentItem, FileObjectType type);
    ~UIFileTableItem();

    void appendChild(UIFileTableItem *child);

    UIFileTableItem *child(int row) const;
    /** Searches for the child by path and returns it if found. */
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

    /** Returns true if this is directory and name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

    FileObjectType   type() const;

    const QString &targetPath() const;
    void setTargetPath(const QString &path);

    bool isSymLinkToADirectory() const;
    void setIsSymLinkToADirectory(bool flag);

    bool isSymLinkToAFile() const;

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
    QVector<QVariant>  m_itemData;
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
 *  populates the UIFileManagerModel by scanning the file system
 *  differently. The file structre kept in this class as a tree. */
class UIFileManagerTable : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigLogOutput(QString strLog, FileManagerLogType eLogType);
    void sigDeleteConfirmationOptionChanged();

public:

    UIFileManagerTable(UIActionPool *pActionPool, QWidget *pParent = 0);
    virtual ~UIFileManagerTable();
    /** Deletes all the tree nodes */
    void        reset();
    void        emitLogOutput(const QString& strOutput, FileManagerLogType eLogType);
    /** Returns the path of the rootIndex */
    QString     currentDirectoryPath() const;
    /** Returns the paths of the selected items (if any) as a list */
    QStringList selectedItemPathList();
    virtual void refresh();
    void         relist();
    static const unsigned    m_iKiloByte;
    static QString humanReadableSize(ULONG64 size);

public slots:

    void sltReceiveDirectoryStatistics(UIDirectoryStatistics statictics);
    void sltCreateNewDirectory();
    /* index is passed by the item view and represents the double clicked object's 'proxy' model index */
    void sltItemDoubleClicked(const QModelIndex &index);
    void sltItemClicked(const QModelIndex &index);
    void sltGoUp();
    void sltGoHome();
    void sltRefresh();
    void sltDelete();
    void sltRename();
    void sltCopy();
    void sltCut();
    void sltPaste();
    void sltShowProperties();
    void sltSelectAll();
    void sltInvertSelection();

protected:

    /** This enum is used when performing a gueest-to-guest or host-to-host
     *  file operations. Paths of source file objects are kept in a single buffer
     *  and a flag to determine if it is a cut or copy operation is needed */
    enum FileOperationType
    {
        FileOperationType_Copy,
        FileOperationType_Cut,
        FileOperationType_None,
        FileOperationType_Max
    };

    void retranslateUi();
    void updateCurrentLocationEdit(const QString& strLocation);
    /* @p index is for model not for 'proxy' model */
    void changeLocation(const QModelIndex &index);
    void initializeFileTree();
    void insertItemsToTree(QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent,
                           bool isDirectoryMap, bool isStartDir);
    virtual void     readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) = 0;
    virtual void     deleteByItem(UIFileTableItem *item) = 0;
    virtual void     deleteByPath(const QStringList &pathList) = 0;
    virtual void     goToHomeDirectory() = 0;
    virtual bool     renameItem(UIFileTableItem *item, QString newBaseName) = 0;
    virtual bool     createDirectory(const QString &path, const QString &directoryName) = 0;
    virtual QString  fsObjectPropertyString() = 0;
    virtual void     showProperties() = 0;
    /** For non-windows system does nothing and for windows systems populates m_driveLetterList with
     *  drive letters */
    virtual void     determineDriveLetters() = 0;
    virtual void     prepareToolbar() = 0;
    virtual void     createFileViewContextMenu(const QWidget *pWidget, const QPoint &point) = 0;
    virtual bool     event(QEvent *pEvent) /* override */;

    /** @name Copy/Cut guest-to-guest (host-to-host) stuff.
     * @{ */
        /** Disable/enable paste action depending on the m_eFileOperationType. */
        virtual void  setPasteActionEnabled(bool fEnabled) = 0;
        virtual void  pasteCutCopiedObjects() = 0;
        /** stores the type of the pending guest-to-guest (host-to-host) file operation. */
        FileOperationType m_eFileOperationType;
    /** @} */

    QString          fileTypeString(FileObjectType type);
    /* @p item index is item location in model not in 'proxy' model */
    void             goIntoDirectory(const QModelIndex &itemIndex);
    /** Follows the path trail, opens directories as it descends */
    void             goIntoDirectory(const QStringList &pathTrail);
    /** Goes into directory pointed by the @p item */
    void             goIntoDirectory(UIFileTableItem *item);
    UIFileTableItem* indexData(const QModelIndex &index) const;
    bool             eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    CGuestFsObjInfo  guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const;
    void             setSelectionDependentActionsEnabled(bool fIsEnabled);
    /** Creates a QList out of the parameters wrt. UIFileManagerModelColumn enum */
    QVector<QVariant>  createTreeItemData(const QString &strName, ULONG64 size, const QDateTime &changeTime,
                                        const QString &strOwner, const QString &strPermissions);

    UIFileTableItem         *m_pRootItem;
    QILabel                 *m_pLocationLabel;
    UIPropertiesDialog      *m_pPropertiesDialog;
    UIActionPool            *m_pActionPool;
    UIToolBar               *m_pToolBar;

    /** Stores the drive letters the file system has (for windows system). For non-windows
     *  systems this is empty and for windows system it should at least contain C:/ */
    QStringList              m_driveLetterList;
    /** The set of actions which need some selection to work on. Like cut, copy etc. */
    QSet<QAction*>           m_selectionDependentActions;
    /** The absolue path list of the file objects which user has chosen to cut/copy. this
     *  list will be cleaned after a paste operation or overwritten by a subsequent cut/copy.
     *  Currently only used by the guest side. */
    QStringList              m_copyCutBuffer;

private slots:

    void sltCreateFileViewContextMenu(const QPoint &point);
    void sltSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);
    void sltLocationComboCurrentChange(const QString &strLocation);
    void sltSearchTextChanged(const QString &strText);

private:

    void             prepareObjects();
    /** @p itemIndex is assumed to be 'model' index not 'proxy model' index */
    void             deleteByIndex(const QModelIndex &itemIndex);
    /** Returns the UIFileTableItem for path / which is a direct (and single) child of m_pRootItem */
    UIFileTableItem *getStartDirectoryItem();
    /** Shows a modal dialog with a line edit for user to enter a new directory name and return the entered string*/
    QString         getNewDirectoryName();
    void            deSelectUpDirectoryItem();
    void            setSelectionForAll(QItemSelectionModel::SelectionFlags flags);
    void            setSelection(const QModelIndex &indexInProxyModel);
    /** The start directory requires a special attention since on file systems with drive letters
     *  drive letter are direct children of the start directory. On other systems start directory is '/' */
    void            populateStartDirectory(UIFileTableItem *startItem);
    /** Root index of the m_pModel */
    QModelIndex     currentRootIndex() const;
    /* Searches the content of m_pSearchLineEdit within the current items' names and selects the item if found. */
    void            performSelectionSearch(const QString &strSearchText);
    /** Clears the m_pSearchLineEdit and hides it. */
    void            disableSelectionSearch();
    /** Checks if delete confirmation dialog is shown and users choice. Returns true
     *  if deletion can continue */
    bool            checkIfDeleteOK();

    UIFileManagerModel      *m_pModel;
    UIGuestControlFileView       *m_pView;
    UIGuestControlFileProxyModel *m_pProxyModel;

    QGridLayout     *m_pMainLayout;
    QComboBox       *m_pLocationComboBox;
    QILineEdit      *m_pSearchLineEdit;
    QILabel         *m_pWarningLabel;

    friend class     UIFileManagerModel;
};

#endif /* !___UIFileManagerTable_h___ */
