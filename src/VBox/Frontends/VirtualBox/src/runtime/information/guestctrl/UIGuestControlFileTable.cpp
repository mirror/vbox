/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAction>
# include <QDir>
# include <QHeaderView>
# include <QItemDelegate>
# include <QGridLayout>

/* GUI includes: */
# include "QILabel.h"
# include "QILineEdit.h"
# include "UIIconPool.h"
# include "UIGuestControlFileTable.h"
# include "UIToolBar.h"
# include "UIVMInformationDialog.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuestDirectory.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

class UIFileDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:
        virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   UIFileTableItem definition.                                                                                                  *
*********************************************************************************************************************************/

class UIFileTableItem
{
public:
    explicit UIFileTableItem(const QList<QVariant> &data, UIFileTableItem *parentItem = 0);
    ~UIFileTableItem();

    void appendChild(UIFileTableItem *child);

    UIFileTableItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    UIFileTableItem *parentItem();

    bool isDirectory() const;
    bool isOpened() const;

    void setIsDirectory(bool flag);
    void setIsOpened(bool flag);

    const QString  &path() const;
    void setPath(const QString &path);

    /** True if this is directory and name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

private:
    QList<UIFileTableItem*> m_childItems;
    QList<QVariant>  m_itemData;
    UIFileTableItem *m_parentItem;
    bool             m_bIsDirectory;
    bool             m_bIsOpened;
    QString          m_strPath;
};


UIFileTableItem::UIFileTableItem(const QList<QVariant> &data, UIFileTableItem *parent)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_bIsDirectory(false)
    , m_bIsOpened(false)
{

}

UIFileTableItem::~UIFileTableItem()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

void UIFileTableItem::appendChild(UIFileTableItem *item)
{
    m_childItems.append(item);
}

UIFileTableItem *UIFileTableItem::child(int row)
{
    return m_childItems.value(row);
}

int UIFileTableItem::childCount() const
{
    return m_childItems.count();
}

int UIFileTableItem::columnCount() const
{
    return m_itemData.count();
}

QVariant UIFileTableItem::data(int column) const
{
    return m_itemData.value(column);
}

UIFileTableItem *UIFileTableItem::parentItem()
{
    return m_parentItem;
}

int UIFileTableItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<UIFileTableItem*>(this));

    return 0;
}

bool UIFileTableItem::isDirectory() const
{
    return m_bIsDirectory;
}

void UIFileTableItem::clearChildren()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

bool UIFileTableItem::isOpened() const
{
    return m_bIsOpened;
}

void UIFileTableItem::setIsDirectory(bool flag)
{
    m_bIsDirectory = flag;
}

void UIFileTableItem::setIsOpened(bool flag)
{
    m_bIsOpened = flag;
}

const QString  &UIFileTableItem::path() const
{
    return m_strPath;
}

void UIFileTableItem::setPath(const QString &path)
{
    m_strPath = path;
}

bool UIFileTableItem::isUpDirectory() const
{
    if (!m_bIsDirectory)
        return false;
    if (data(0) == QString(".."))
        return true;
    return false;
}


/*********************************************************************************************************************************
*   UIGuestControlFileModel implementation.                                                                                      *
*********************************************************************************************************************************/

UIGuestControlFileModel::UIGuestControlFileModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_pParent(qobject_cast<UIGuestControlFileTable*>(parent))
{
    QList<QVariant> rootData;
    // rootData << "Title" << "Summary";
    // rootItem = new UIFileTableItem(rootData);
}

UIFileTableItem* UIGuestControlFileModel::rootItem() const
{
    if (!m_pParent)
        return 0;
    return m_pParent->m_pRootItem;
}

UIGuestControlFileModel::~UIGuestControlFileModel()
{}

int UIGuestControlFileModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<UIFileTableItem*>(parent.internalPointer())->columnCount();
    else
    {
        if (!rootItem())
            return 0;
        else
            return rootItem()->columnCount();
    }
}

QVariant UIGuestControlFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return QVariant();

    if (role == Qt::DisplayRole)
    {
        /* dont show anything but the name for up directories: */
        if (item->isUpDirectory() && index.column() != 0)
            return QVariant();
        return item->data(index.column());
    }

    if (role == Qt::DecorationRole && index.column() == 0)
    {
        if (item->isDirectory())
        {
            if (item->isUpDirectory())
                return QIcon(":/arrow_up_10px_x2.png");
            else
                return QIcon(":/sf_32px.png");
        }
        else
            return QIcon(":/vm_open_filemanager_16px");
    }

    return QVariant();
}

Qt::ItemFlags UIGuestControlFileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}

QVariant UIGuestControlFileModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (!rootItem())
            return QVariant();
        else
            return rootItem()->data(section);
    }
    return QVariant();
}

QModelIndex UIGuestControlFileModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    UIFileTableItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem();
    else
        parentItem = static_cast<UIFileTableItem*>(parent.internalPointer());
    if (!parentItem)
        return QModelIndex();

    UIFileTableItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}


QModelIndex UIGuestControlFileModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    UIFileTableItem *childItem = static_cast<UIFileTableItem*>(index.internalPointer());
    UIFileTableItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem())
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int UIGuestControlFileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    UIFileTableItem *parentItem = 0;
    if (!parent.isValid())
        parentItem = rootItem();
    else
        parentItem = static_cast<UIFileTableItem*>(parent.internalPointer());
    if (!parentItem)
        return 0;
    return parentItem->childCount();
}

void UIGuestControlFileModel::signalUpdate()
{
    emit layoutChanged();
}

QModelIndex UIGuestControlFileModel::rootIndex() const
{
    if (!rootItem())
        return QModelIndex();
    return createIndex(rootItem()->child(0)->row(), 0,
                       rootItem()->child(0));
}

void UIGuestControlFileModel::beginReset()
{
    beginResetModel();
}

void UIGuestControlFileModel::endReset()
{
    endResetModel();
}


/*********************************************************************************************************************************
*   UIGuestControlFileTable implementation.                                                                                      *
*********************************************************************************************************************************/

UIGuestControlFileTable::UIGuestControlFileTable(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pRootItem(0)
    , m_pView(0)
    , m_pModel(0)
    , m_pTree(0)
    , m_pLocationLabel(0)
    , m_pMainLayout(0)
    , m_pCurrentLocationEdit(0)
    , m_pToolBar(0)
    , m_pRefresh(0)
    , m_pDelete(0)
    , m_pNewFolder(0)
    , m_pGoUp(0)
    , m_pCopy(0)
    , m_pCut(0)
    , m_pPaste(0)

{
    prepareObjects();
    prepareActions();
}

UIGuestControlFileTable::~UIGuestControlFileTable()
{
    delete m_pRootItem;
}

void UIGuestControlFileTable::reset()
{
    if (m_pModel)
        m_pModel->beginReset();
    delete m_pRootItem;
    m_pRootItem = 0;
    if (m_pModel)
        m_pModel->endReset();
    if (m_pCurrentLocationEdit)
        m_pCurrentLocationEdit->clear();
}

void UIGuestControlFileTable::prepareObjects()
{
    m_pMainLayout = new QGridLayout();
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_pMainLayout);

    m_pToolBar = new UIToolBar;
    if (m_pToolBar)
    {
        m_pMainLayout->addWidget(m_pToolBar, 0, 0, 1, 5);
    }

    m_pLocationLabel = new QILabel;
    if (m_pLocationLabel)
    {
        m_pMainLayout->addWidget(m_pLocationLabel, 1, 0, 1, 1);
    }

    m_pCurrentLocationEdit = new QILineEdit;
    if (m_pCurrentLocationEdit)
    {
        m_pMainLayout->addWidget(m_pCurrentLocationEdit, 1, 1, 1, 4);
        m_pCurrentLocationEdit->setReadOnly(true);
    }

    m_pModel = new UIGuestControlFileModel(this);
    if (!m_pModel)
        return;


    m_pView = new QTableView;
    if (m_pView)
    {
        m_pView->setShowGrid(false);
        m_pView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_pView->verticalHeader()->setVisible(false);

        m_pMainLayout->addWidget(m_pView, 2, 0, 5, 5);
        m_pView->setModel(m_pModel);
        m_pView->setItemDelegate(new UIFileDelegate);
        /* Minimize the row height: */
        m_pView->verticalHeader()->setDefaultSectionSize(m_pView->verticalHeader()->minimumSectionSize());
        /* Make the columns take all the avaible space: */
        m_pView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        connect(m_pView, &QTableView::doubleClicked,
                this, &UIGuestControlFileTable::sltItemDoubleClicked);

    }
}

void UIGuestControlFileTable::prepareActions()
{
    if (!m_pToolBar)
        return;

    m_pGoUp = new QAction(this);
    if (m_pGoUp)
    {
        connect(m_pGoUp, &QAction::triggered, this, &UIGuestControlFileTable::sltGoUp);
        m_pGoUp->setIcon(UIIconPool::iconSet(QString(":/arrow_up_10px_x2.png")));
        m_pToolBar->addAction(m_pGoUp);
    }

    m_pRefresh = new QAction(this);
    if (m_pRefresh)
    {
        connect(m_pRefresh, &QAction::triggered, this, &UIGuestControlFileTable::sltRefresh);
        m_pRefresh->setIcon(UIIconPool::iconSet(QString(":/refresh_22px.png")));
        m_pToolBar->addAction(m_pRefresh);
    }


    m_pDelete = new QAction(this);
    if (m_pDelete)
    {
        connect(m_pDelete, &QAction::triggered, this, &UIGuestControlFileTable::sltDelete);
        m_pDelete->setIcon(UIIconPool::iconSet(QString(":/vm_delete_32px.png")));
        m_pToolBar->addAction(m_pDelete);
    }

    m_pNewFolder = new QAction(this);
    m_pNewFolder->setIcon(UIIconPool::iconSet(QString(":/sf_32px.png")));
    m_pToolBar->addAction(m_pNewFolder);

    m_pCopy = new QAction(this);
    m_pCopy->setIcon(UIIconPool::iconSet(QString(":/fd_copy_22px.png")));
    m_pToolBar->addAction(m_pCopy);

    m_pCut = new QAction(this);
    m_pCut->setIcon(UIIconPool::iconSet(QString(":/fd_move_22px.png")));
    m_pToolBar->addAction(m_pCut);


    m_pPaste = new QAction(this);
    m_pPaste->setIcon(UIIconPool::iconSet(QString(":/shared_clipboard_16px.png")));
    m_pToolBar->addAction(m_pPaste);
}

void UIGuestControlFileTable::updateCurrentLocationEdit(const QString& strLocation)
{
    if (!m_pCurrentLocationEdit)
        return;
    m_pCurrentLocationEdit->setText(strLocation);
}

void UIGuestControlFileTable::changeLocation(const QModelIndex &index)
{
    if (!index.isValid() || !m_pView)
        return;
    m_pView->setRootIndex(index);
    m_pView->clearSelection();

    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (item)
    {
        updateCurrentLocationEdit(item->path());
    }
    m_pModel->signalUpdate();
}

void UIGuestControlFileTable::initializeFileTree()
{
    if (m_pRootItem)
        reset();

    QList<QVariant> headData;
    headData << "Name" << "Size";
    m_pRootItem = new UIFileTableItem(headData);

    QList<QVariant> startDirData;
    startDirData << "/" << 4096;
    UIFileTableItem* startItem = new UIFileTableItem(startDirData, m_pRootItem);
    startItem->setPath("/");
    m_pRootItem->appendChild(startItem);

    startItem->setIsDirectory(true);
    startItem->setIsOpened(false);
    /* Read the root directory and get the list: */

    readDirectory("/", startItem, true);
    m_pView->setRootIndex(m_pModel->rootIndex());
    m_pModel->signalUpdate();

}

void UIGuestControlFileTable::insertItemsToTree(QMap<QString,UIFileTableItem*> &map,
                                                UIFileTableItem *parent, bool isDirectoryMap, bool isStartDir)
{
    /* Make sure we have a ".." item within directories, and make sure it does not include for the start dir: */
    if (isDirectoryMap)
    {
        if (!map.contains("..")  && !isStartDir)
        {
            QList<QVariant> data;
            data << ".." << 4096;
            UIFileTableItem *item = new UIFileTableItem(data, parent);
            item->setIsDirectory(true);
            item->setIsOpened(false);
            map.insert("..", item);
        }
        else if (map.contains("..")  && isStartDir)
        {
            map.remove("..");
        }
    }
    for (QMap<QString,UIFileTableItem*>::const_iterator iterator = map.begin();
        iterator != map.end(); ++iterator)
    {
        if (iterator.key() == "." || iterator.key().isEmpty())
            continue;
        parent->appendChild(iterator.value());
    }
}

void UIGuestControlFileTable::sltItemDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() ||  !m_pModel || !m_pView)
        return;

    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return;
    /* check if we need to go up: */
    if (item->isUpDirectory())
    {
        QModelIndex parentIndex = m_pModel->parent(m_pModel->parent(index));
        if (parentIndex.isValid())
            changeLocation(parentIndex);
        return;
    }

    if (!item->isDirectory())
        return;
    if (!item->isOpened())
       readDirectory(item->path(),item);

    changeLocation(index);
}

void UIGuestControlFileTable::sltGoUp()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentRoot = m_pView->rootIndex();
    if (!currentRoot.isValid())
        return;
    if (currentRoot != m_pModel->rootIndex())
        m_pView->setRootIndex(currentRoot.parent());
}

void UIGuestControlFileTable::sltRefresh()
{
    refresh();
}

void UIGuestControlFileTable::refresh()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentIndex = m_pView->rootIndex();
    if (!currentIndex.isValid())
        return;
    UIFileTableItem *treeItem =
        static_cast<UIFileTableItem*>(currentIndex.internalPointer());
    if (!treeItem)
        return;
    bool isRootDir = (m_pModel->rootIndex() == currentIndex);
    m_pModel->beginReset();
    /* For now we clear the whole subtree (that isrecursively) which is an overkill: */
    treeItem->clearChildren();
    readDirectory(treeItem->path(), treeItem, isRootDir);
    m_pModel->endReset();
    m_pView->setRootIndex(currentIndex);
}

void UIGuestControlFileTable::sltDelete()
{
    if (!m_pView || !m_pModel)
        return;
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return;

    QModelIndexList selectedItemIndices = selectionModel->selectedIndexes();
    for(int i = 0; i < selectedItemIndices.size(); ++i)
    {
        deleteByIndex(selectedItemIndices.at(i));
    }
    refresh();
}

void UIGuestControlFileTable::deleteByIndex(const QModelIndex &itemIndex)
{
    if (!itemIndex.isValid())
        return;
    UIFileTableItem *treeItem = static_cast<UIFileTableItem*>(itemIndex.internalPointer());
    if (!treeItem)
        return;
    deleteByItem(treeItem);
}

void UIGuestControlFileTable::retranslateUi()
{
    if (m_pRefresh)
    {
        m_pRefresh->setText(UIVMInformationDialog::tr("Refresh"));
        m_pRefresh->setToolTip(UIVMInformationDialog::tr("Refresh the current directory"));
        m_pRefresh->setStatusTip(UIVMInformationDialog::tr("Refresh the current directory"));
    }
    if (m_pDelete)
    {
        m_pDelete->setText(UIVMInformationDialog::tr("Delete"));
        m_pDelete->setToolTip(UIVMInformationDialog::tr("Delete the selected item(s)"));
        m_pDelete->setStatusTip(UIVMInformationDialog::tr("Delete the selected item(s)"));
    }

    if (m_pNewFolder)
    {
        m_pNewFolder->setText(UIVMInformationDialog::tr("Create a new folder"));
        m_pNewFolder->setToolTip(UIVMInformationDialog::tr("Create a new folder"));
        m_pNewFolder->setStatusTip(UIVMInformationDialog::tr("Create a new folder"));

    }
    if (m_pGoUp)
    {
        m_pGoUp->setText(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setToolTip(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setStatusTip(UIVMInformationDialog::tr("Move one level up"));

    }

    if (m_pCopy)
    {
        m_pCopy->setText(UIVMInformationDialog::tr("Copy the selected item"));
        m_pCopy->setToolTip(UIVMInformationDialog::tr("Copy the selected item(s)"));
        m_pCopy->setStatusTip(UIVMInformationDialog::tr("Copy the selected item(s)"));

    }

    if (m_pCut)
    {
        m_pCut->setText(UIVMInformationDialog::tr("Cut the selected item(s)"));
        m_pCut->setToolTip(UIVMInformationDialog::tr("Cut the selected item(s)"));
        m_pCut->setStatusTip(UIVMInformationDialog::tr("Cut the selected item(s)"));

    }

    if ( m_pPaste)
    {
        m_pPaste->setText(UIVMInformationDialog::tr("Paste the copied item(s)"));
        m_pPaste->setToolTip(UIVMInformationDialog::tr("Paste the copied item(s)"));
        m_pPaste->setStatusTip(UIVMInformationDialog::tr("Paste the copied item(s)"));
    }

}


/*********************************************************************************************************************************
*   UIGuestFileTable implementation.                                                                                             *
*********************************************************************************************************************************/

UIGuestFileTable::UIGuestFileTable(QWidget *pParent /*= 0*/)
    :UIGuestControlFileTable(pParent)
{
    retranslateUi();
}

void UIGuestFileTable::initGuestFileTable(const CGuestSession &session)
{
    if (!session.isOk())
        return;
    if (session.GetStatus() != KGuestSessionStatus_Started)
        return;
    m_comGuestSession = session;


    initializeFileTree();
}

void UIGuestFileTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(UIVMInformationDialog::tr("Guest System"));
    UIGuestControlFileTable::retranslateUi();
}

void UIGuestFileTable::readDirectory(const QString& strPath,
                                     UIFileTableItem *parent, bool isStartDir /*= false*/)

{
    if (!parent)
        return;

    CGuestDirectory directory;
    QVector<KDirectoryOpenFlag> flag;
    flag.push_back(KDirectoryOpenFlag_None);

    directory = m_comGuestSession.DirectoryOpen(strPath, /*aFilter*/ "", flag);
    parent->setIsOpened(true);

    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        QMap<QString, UIFileTableItem*> directories;
        QMap<QString, UIFileTableItem*> files;

        while (fsInfo.isOk())
        {
            QList<QVariant> data;
            data << fsInfo.GetName() << static_cast<qulonglong>(fsInfo.GetObjectSize());
            UIFileTableItem *item = new UIFileTableItem(data, parent);
            item->setPath(QString("%1%3%2").arg(strPath).arg("/").arg(fsInfo.GetName()));
            if (fsInfo.GetType() == KFsObjType_Directory)
            {
                directories.insert(fsInfo.GetName(), item);
                item->setIsDirectory(true);
                item->setIsOpened(false);
            }
            else
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsDirectory(false);
                item->setIsOpened(false);
            }
            fsInfo = directory.Read();
        }
        insertItemsToTree(directories, parent, true, isStartDir);
        insertItemsToTree(files, parent, false, isStartDir);
        updateCurrentLocationEdit(strPath);
    }
}

void UIGuestFileTable::deleteByItem(UIFileTableItem *item)
{
    if (!item)
        return;
    if (!m_comGuestSession.isOk())
        return;
    if (item->isUpDirectory())
        return;
    QVector<KDirectoryRemoveRecFlag> flags(KDirectoryRemoveRecFlag_ContentAndDir);

    if (item->isDirectory())
    {
        m_comGuestSession.DirectoryRemoveRecursive(item->path(), flags);
    }
    else
    {
        m_comGuestSession.FsObjRemove(item->path());
    }
}

/*********************************************************************************************************************************
*   UIHostFileTable implementation.                                                                                              *
*********************************************************************************************************************************/

UIHostFileTable::UIHostFileTable(QWidget *pParent /* = 0 */)
    :UIGuestControlFileTable(pParent)
{
    initializeFileTree();
    retranslateUi();
}

void UIHostFileTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(UIVMInformationDialog::tr("Host System"));
    UIGuestControlFileTable::retranslateUi();
}

void UIHostFileTable::readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir /*= false*/)
{
    if (!parent)
        return;

    QDir directory(strPath);
    parent->setIsOpened(true);
    if (!directory.exists())
        return;
    QFileInfoList entries = directory.entryInfoList();
    QMap<QString, UIFileTableItem*> directories;
    QMap<QString, UIFileTableItem*> files;


    for (int i = 0; i < entries.size(); ++i)
    {
        const QFileInfo &fileInfo = entries.at(i);

        QList<QVariant> data;
        data << fileInfo.baseName() << fileInfo.size();
        UIFileTableItem *item = new UIFileTableItem(data, parent);
        item->setPath(fileInfo.absoluteFilePath());
        if (fileInfo.isDir())
        {
            directories.insert(fileInfo.baseName(), item);
            item->setIsDirectory(true);
            item->setIsOpened(false);
        }
        else
        {
            files.insert(fileInfo.baseName(), item);
            item->setIsDirectory(false);
            item->setIsOpened(false);
        }

    }
    insertItemsToTree(directories, parent, true, isStartDir);
    insertItemsToTree(files, parent, false, isStartDir);
    updateCurrentLocationEdit(strPath);
}

void UIHostFileTable::deleteByItem(UIFileTableItem */*item */)
{

}

#include "UIGuestControlFileTable.moc"
