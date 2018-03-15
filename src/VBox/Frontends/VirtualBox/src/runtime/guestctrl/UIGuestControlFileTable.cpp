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
# include <QDateTime>
# include <QDir>
# include <QHeaderView>
# include <QItemDelegate>
# include <QGridLayout>
# include <QPushButton>

/* GUI includes: */
# include "QIDialog.h"
# include "QIDialogButtonBox.h"
# include "QILabel.h"
# include "QILineEdit.h"
# include "UIErrorString.h"
# include "UIIconPool.h"
# include "UIGuestControlFileTable.h"
# include "UIGuestControlFileModel.h"
# include "UIToolBar.h"
# include "UIVMInformationDialog.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuestFsObjInfo.h"
# include "CGuestDirectory.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** A collection of utility functions for some path string manipulations */
class UIPathOperations
{
public:
    static QString removeMultipleDelimiters(const QString &path);
    static QString removeTrailingDelimiters(const QString &path);
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

    static const QChar delimiter;
};

const QChar UIPathOperations::delimiter = QChar('/');

QString UIPathOperations::removeMultipleDelimiters(const QString &path)
{
    QString newPath(path);
    QString doubleDelimiter(2, delimiter);

    while (newPath.contains(doubleDelimiter) && !newPath.isEmpty())
        newPath = newPath.replace(doubleDelimiter, delimiter);
    return newPath;
}

QString UIPathOperations::removeTrailingDelimiters(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return QString();
    QString newPath(path);
    /* Make sure for we dont have any trailing slashes: */
    while (newPath.length() > 1 && newPath.at(newPath.length() - 1) == UIPathOperations::delimiter)
        newPath.chop(1);
    return newPath;
}

QString UIPathOperations::addStartDelimiter(const QString &path)
{
    if (path.isEmpty())
        return QString(path);
    QString newPath(path);
    if (newPath.at(0) != delimiter)
        newPath.insert(0, delimiter);
    return newPath;
}

QString UIPathOperations::sanitize(const QString &path)
{
    return addStartDelimiter(removeTrailingDelimiters(removeMultipleDelimiters(path)));
}

QString UIPathOperations::mergePaths(const QString &path, const QString &baseName)
{
    QString newBase(baseName);
    newBase = newBase.remove(delimiter);

    /* make sure we have one and only one trailing '/': */
    QString newPath(sanitize(path));
    if(newPath.isEmpty())
        newPath = delimiter;
    if(newPath.at(newPath.length() - 1) != delimiter)
        newPath += UIPathOperations::delimiter;
    newPath += newBase;
    return sanitize(newPath);
}

QString UIPathOperations::getObjectName(const QString &path)
{
    if (path.length() <= 1)
        return QString(path);

    QString strTemp(sanitize(path));
    if (strTemp.length() < 2)
        return strTemp;
    int lastSlashPosition = strTemp.lastIndexOf(UIPathOperations::delimiter);
    if (lastSlashPosition == -1)
        return QString();
    return strTemp.right(strTemp.length() - lastSlashPosition - 1);
}

QString UIPathOperations::getPathExceptObjectName(const QString &path)
{
    if (path.length() <= 1)
        return QString(path);

    QString strTemp(sanitize(path));
    int lastSlashPosition = strTemp.lastIndexOf(UIPathOperations::delimiter);
    if (lastSlashPosition == -1)
        return QString();
    return strTemp.left(lastSlashPosition + 1);
}

QString UIPathOperations::constructNewItemPath(const QString &previousPath, const QString &newBaseName)
{
    if (previousPath.length() <= 1)
         return QString(previousPath);
    return sanitize(mergePaths(getPathExceptObjectName(previousPath), newBaseName));
}


/*********************************************************************************************************************************
*   UIFileDelegate definition.                                                                                                   *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIFileDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:
        virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   UIFileStringInputDialog definition.                                                                                          *
*********************************************************************************************************************************/

/** A QIDialog child including a line edit whose text exposed when the dialog is accepted */
class UIStringInputDialog : public QIDialog
{

    Q_OBJECT;

public:

    UIStringInputDialog(QWidget *pParent = 0, Qt::WindowFlags flags = 0);
    QString getString() const;

private:

    QILineEdit      *m_pLineEdit;

    // virtual void accept() /* override */;
    // virtual void reject() /* override */;

};


/*********************************************************************************************************************************
*   UIFileStringInputDialog implementation.                                                                                      *
*********************************************************************************************************************************/

UIStringInputDialog::UIStringInputDialog(QWidget *pParent /* = 0 */, Qt::WindowFlags flags /* = 0 */)
    :QIDialog(pParent, flags)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_pLineEdit = new QILineEdit(this);
    layout->addWidget(m_pLineEdit);

    QIDialogButtonBox *pButtonBox =
        new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    layout->addWidget(pButtonBox);
        // {
        //     /* Configure button-box: */
    connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIStringInputDialog::accept);
    connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIStringInputDialog::reject);

}

QString UIStringInputDialog::getString() const
{
    if (!m_pLineEdit)
        return QString();
    return m_pLineEdit->text();
}


/*********************************************************************************************************************************
*   UIFileTableItem implementation.                                                                                              *
*********************************************************************************************************************************/


UIFileTableItem::UIFileTableItem(const QList<QVariant> &data,
                                 UIFileTableItem *parent, FileObjectType type)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_bIsOpened(false)
    , m_isTargetADirectory(false)
    , m_type(type)
{
}

UIFileTableItem::~UIFileTableItem()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

void UIFileTableItem::appendChild(UIFileTableItem *item)
{
    if (!item)
        return;
    m_childItems.append(item);
    m_childMap.insert(item->path(), item);
}

UIFileTableItem *UIFileTableItem::child(int row) const
{
    return m_childItems.value(row);
}

UIFileTableItem *UIFileTableItem::child(const QString &path) const
{
    if (!m_childMap.contains(path))
        return 0;
    return m_childMap.value(path);
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

void UIFileTableItem::setData(const QVariant &data, int index)
{
    if (index >= m_itemData.length())
        return;
    m_itemData[index] = data;
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
    return m_type == FileObjectType_Directory;
}

bool UIFileTableItem::isSymLink() const
{
    return m_type == FileObjectType_SymLink;
}

bool UIFileTableItem::isFile() const
{
    return m_type == FileObjectType_File;
}

void UIFileTableItem::clearChildren()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_childMap.clear();
}

bool UIFileTableItem::isOpened() const
{
    return m_bIsOpened;
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
    if (path.isNull() || path.isEmpty())
        return;
    m_strPath = path;
    UIPathOperations::removeTrailingDelimiters(m_strPath);
}

bool UIFileTableItem::isUpDirectory() const
{
    if (!isDirectory())
        return false;
    if (data(0) == QString(".."))
        return true;
    return false;
}

FileObjectType UIFileTableItem::type() const
{
    return m_type;
}

const QString &UIFileTableItem::targetPath() const
{
    return m_strTargetPath;
}

void UIFileTableItem::setTargetPath(const QString &path)
{
    m_strTargetPath = path;
}

bool UIFileTableItem::isTargetADirectory() const
{
    return m_isTargetADirectory;
}

void UIFileTableItem::setIsTargetADirectory(bool flag)
{
    m_isTargetADirectory = flag;
}


/*********************************************************************************************************************************
*   UIGuestControlFileTable implementation.                                                                                      *
*********************************************************************************************************************************/

UIGuestControlFileTable::UIGuestControlFileTable(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pRootItem(0)
    , m_pView(0)
    , m_pModel(0)
    , m_pLocationLabel(0)
    , m_pMainLayout(0)
    , m_pCurrentLocationEdit(0)
    , m_pToolBar(0)
    , m_pGoUp(0)
    , m_pGoHome(0)
    , m_pRefresh(0)
    , m_pDelete(0)
    , m_pRename(0)
    , m_pCreateNewDirectory(0)
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

void UIGuestControlFileTable::emitLogOutput(const QString& strOutput)
{
    emit sigLogOutput(strOutput);
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
        m_pView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pView->verticalHeader()->setDefaultSectionSize(m_pView->verticalHeader()->minimumSectionSize());
        /* Make the columns take all the avaible space: */
        //m_pView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

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

    m_pGoHome = new QAction(this);
    if (m_pGoHome)
    {
        connect(m_pGoHome, &QAction::triggered, this, &UIGuestControlFileTable::sltGoHome);
        m_pGoHome->setIcon(UIIconPool::iconSet(QString(":/nw_24px.png")));
        m_pToolBar->addAction(m_pGoHome);
    }

    m_pRefresh = new QAction(this);
    if (m_pRefresh)
    {
        connect(m_pRefresh, &QAction::triggered, this, &UIGuestControlFileTable::sltRefresh);
        m_pRefresh->setIcon(UIIconPool::iconSet(QString(":/refresh_22px.png")));
        m_pToolBar->addAction(m_pRefresh);
    }

    m_pToolBar->addSeparator();

    m_pDelete = new QAction(this);
    if (m_pDelete)
    {
        connect(m_pDelete, &QAction::triggered, this, &UIGuestControlFileTable::sltDelete);
        m_pDelete->setIcon(UIIconPool::iconSet(QString(":/vm_delete_32px.png")));
        m_pToolBar->addAction(m_pDelete);
    }

    m_pRename = new QAction(this);
    if (m_pRename)
    {
        connect(m_pRename, &QAction::triggered, this, &UIGuestControlFileTable::sltRename);
        m_pRename->setIcon(UIIconPool::iconSet(QString(":/name_16px_x2.png")));
        m_pToolBar->addAction(m_pRename);
    }

    m_pCreateNewDirectory = new QAction(this);
    if (m_pCreateNewDirectory)
    {
        connect(m_pCreateNewDirectory, &QAction::triggered, this, &UIGuestControlFileTable::sltCreateNewDirectory);
        m_pCreateNewDirectory->setIcon(UIIconPool::iconSet(QString(":/sf_add_16px.png")));
        m_pToolBar->addAction(m_pCreateNewDirectory);
    }

    m_pCopy = new QAction(this);
    if (m_pCopy)
    {
        m_pCopy->setIcon(UIIconPool::iconSet(QString(":/fd_copy_22px.png")));
        m_pToolBar->addAction(m_pCopy);
        //m_pCopy->setEnabled(false);
    }

    m_pCut = new QAction(this);
    if (m_pCut)
    {
        m_pCut->setIcon(UIIconPool::iconSet(QString(":/fd_move_22px.png")));
        m_pToolBar->addAction(m_pCut);
        m_pCut->setEnabled(false);
    }

    m_pPaste = new QAction(this);
    if (m_pPaste)
    {
        m_pPaste->setIcon(UIIconPool::iconSet(QString(":/shared_clipboard_16px.png")));
        m_pToolBar->addAction(m_pPaste);
        m_pPaste->setEnabled(false);
    }
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
    headData << "Name" << "Size" << "Change Time";
    m_pRootItem = new UIFileTableItem(headData, 0, FileObjectType_Directory);
    QList<QVariant> startDirData;
    startDirData << "/" << 4096 << QDateTime();
    UIFileTableItem* startItem = new UIFileTableItem(startDirData, m_pRootItem, FileObjectType_Directory);

    startItem->setPath("/");
    m_pRootItem->appendChild(startItem);

    startItem->setIsOpened(false);
    /* Read the root directory and get the list: */

    readDirectory("/", startItem, true);
    m_pView->setRootIndex(m_pModel->rootIndex());
    m_pModel->signalUpdate();

}

void UIGuestControlFileTable::insertItemsToTree(QMap<QString,UIFileTableItem*> &map,
                                                UIFileTableItem *parent, bool isDirectoryMap, bool isStartDir)
{
    /* Make sure we have a ".." item within directories, and make sure it is not there for the start dir: */
    if (isDirectoryMap)
    {
        if (!map.contains("..")  && !isStartDir)
        {
            QList<QVariant> data;
            data << ".." << 4096;
            UIFileTableItem *item = new UIFileTableItem(data, parent, FileObjectType_Directory);
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
    goIntoDirectory(index);
}

void UIGuestControlFileTable::sltGoUp()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentRoot = m_pView->rootIndex();
    if (!currentRoot.isValid())
        return;
    if (currentRoot != m_pModel->rootIndex())
        changeLocation(currentRoot.parent());
}

void UIGuestControlFileTable::sltGoHome()
{
    goToHomeDirectory();
}

void UIGuestControlFileTable::sltRefresh()
{
    refresh();
}

void UIGuestControlFileTable::goIntoDirectory(const QModelIndex &itemIndex)
{
    if (!m_pModel)
        return;

    /* Make sure the colum is 0: */
    QModelIndex index = m_pModel->index(itemIndex.row(), 0, itemIndex.parent());
    if (!index.isValid())
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

void UIGuestControlFileTable::goIntoDirectory(const QList<QString> &pathTrail)
{
    UIFileTableItem *parent = getStartDirectoryItem();

    for(int i = 0; i < pathTrail.size(); ++i)
    {
        if (!parent)
            return;
        /* Make sure parent is already opened: */
        if (!parent->isOpened())
            readDirectory(parent->path(), parent, parent == getStartDirectoryItem());
        /* search the current path item among the parent's children: */
        UIFileTableItem *item = parent->child(pathTrail.at(i));
        if (!item)
            return;
        parent = item;
    }
    if (!parent)
        return;
    if (!parent->isOpened())
        readDirectory(parent->path(), parent, parent == getStartDirectoryItem());
    goIntoDirectory(parent);
}

void UIGuestControlFileTable::goIntoDirectory(UIFileTableItem *item)
{
    if (!item || !m_pModel)
        return;
    goIntoDirectory(m_pModel->index(item));
}

UIFileTableItem* UIGuestControlFileTable::indexData(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return static_cast<UIFileTableItem*>(index.internalPointer());
}

void UIGuestControlFileTable::refresh()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentIndex = m_pView->rootIndex();

    UIFileTableItem *treeItem = indexData(currentIndex);
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

    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    for(int i = 0; i < selectedItemIndices.size(); ++i)
    {
        deleteByIndex(selectedItemIndices.at(i));
    }
    /** @todo dont refresh here, just delete the rows and update the table view: */
    refresh();
}

void UIGuestControlFileTable::sltRename()
{
    if (!m_pView)
        return;
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return;

    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    if (selectedItemIndices.size() == 0)
        return;
    UIFileTableItem *item = indexData(selectedItemIndices.at(0));
    if (!item || item->isUpDirectory())
        return;
    m_pView->edit(selectedItemIndices.at(0));
}

void UIGuestControlFileTable::sltCreateNewDirectory()
{
    if (!m_pModel || !m_pView)
        return;
    QModelIndex currentIndex = m_pView->rootIndex();
    if (!currentIndex.isValid())
        return;
    UIFileTableItem *item = static_cast<UIFileTableItem*>(currentIndex.internalPointer());
    if (!item)
        return;

    QString newDirectoryName = getNewDirectoryName();
    if (newDirectoryName.isEmpty())
        return;

    if (createDirectory(item->path(), newDirectoryName))
    {
        /** @todo instead of refreshing here (an overkill) just add the
           rows and update the tabel view: */
        sltRefresh();
    }
}

void UIGuestControlFileTable::deleteByIndex(const QModelIndex &itemIndex)
{
    UIFileTableItem *treeItem = indexData(itemIndex);
    if (!treeItem)
        return;
    deleteByItem(treeItem);
}

void UIGuestControlFileTable::retranslateUi()
{
    if (m_pGoUp)
    {
        m_pGoUp->setText(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setToolTip(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setStatusTip(UIVMInformationDialog::tr("Move one level up"));
    }

    if (m_pGoHome)
    {
        m_pGoHome->setText(UIVMInformationDialog::tr("Go to home directory"));
        m_pGoHome->setToolTip(UIVMInformationDialog::tr("Go to home directory"));
        m_pGoHome->setStatusTip(UIVMInformationDialog::tr("Go to home directory"));
    }

    if (m_pRename)
    {
        m_pRename->setText(UIVMInformationDialog::tr("Rename the selected item"));
        m_pRename->setToolTip(UIVMInformationDialog::tr("Rename the selected item"));
        m_pRename->setStatusTip(UIVMInformationDialog::tr("Rename the selected item"));
    }

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

    if (m_pCreateNewDirectory)
    {
        m_pCreateNewDirectory->setText(UIVMInformationDialog::tr("Create a new directory"));
        m_pCreateNewDirectory->setToolTip(UIVMInformationDialog::tr("Create a new directory"));
        m_pCreateNewDirectory->setStatusTip(UIVMInformationDialog::tr("Create a new directory"));

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


void UIGuestControlFileTable::keyPressEvent(QKeyEvent * pEvent)
{
    /* Browse into directory with enter: */
    if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
    {
        if (m_pView && m_pModel)
        {
            /* Get the selected item. If there are 0 or more than 1 selection do nothing: */
            QItemSelectionModel *selectionModel =  m_pView->selectionModel();
            if (selectionModel)
            {
                QModelIndexList selectedItemIndices = selectionModel->selectedRows();
                if (selectedItemIndices.size() == 1)
                    goIntoDirectory(selectedItemIndices.at(0));
            }
        }
    }
    else if (pEvent->key() == Qt::Key_Delete)
    {
        sltDelete();
    }
    QWidget::keyPressEvent(pEvent);
}

UIFileTableItem *UIGuestControlFileTable::getStartDirectoryItem()
{
    if (!m_pRootItem)
        return 0;
    if (m_pRootItem->childCount() <= 0)
        return 0;
    return m_pRootItem->child(0);
}


QString UIGuestControlFileTable::getNewDirectoryName()
{
    UIStringInputDialog *dialog = new UIStringInputDialog();
    if (dialog->execute())
    {
        QString strDialog = dialog->getString();
        delete dialog;
        return strDialog;
    }
    delete dialog;
    return QString();
}

QString UIGuestControlFileTable::currentDirectoryPath() const
{
    if (!m_pView)
        return QString();
    QModelIndex currentRoot = m_pView->rootIndex();
    if (!currentRoot.isValid())
        return QString();
    UIFileTableItem *item = static_cast<UIFileTableItem*>(currentRoot.internalPointer());
    if (!item)
        return QString();
    /* be paranoid: */
    if (!item->isDirectory())
        return QString();
    return item->path();
}

QStringList UIGuestControlFileTable::selectedItemPathList()
{
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return QStringList();

    QStringList pathList;
    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    for(int i = 0; i < selectedItemIndices.size(); ++i)
    {
        UIFileTableItem *item = static_cast<UIFileTableItem*>(selectedItemIndices.at(i).internalPointer());
        if (!item)
            continue;
        pathList.push_back(item->path());
    }
    return pathList;
}

CGuestFsObjInfo UIGuestControlFileTable::guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const
{
    if (comGuestSession.isNull())
        return CGuestFsObjInfo();
    CGuestFsObjInfo comFsObjInfo = comGuestSession.FsObjQueryInfo(path, true /*aFollowSymlinks*/);
    if (!comFsObjInfo.isOk())
        return CGuestFsObjInfo();
    return comFsObjInfo;
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
            QDateTime changeTime = QDateTime::fromMSecsSinceEpoch(fsInfo.GetChangeTime()/1000000);

            data << fsInfo.GetName() << static_cast<qulonglong>(fsInfo.GetObjectSize()) << changeTime;
            FileObjectType fileType = getFileType(fsInfo);
            UIFileTableItem *item = new UIFileTableItem(data, parent, fileType);
            item->setPath(UIPathOperations::mergePaths(strPath, fsInfo.GetName()));
            if (fileType == FileObjectType_Directory)
            {
                directories.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            else if(fileType == FileObjectType_File)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            /** @todo Seems like our API is not able to detect symlinks: */
            else if(fileType == FileObjectType_SymLink)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }

            fsInfo = directory.Read();
        }
        insertItemsToTree(directories, parent, true, isStartDir);
        insertItemsToTree(files, parent, false, isStartDir);
        updateCurrentLocationEdit(strPath);
    }
    directory.Close();
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
        m_comGuestSession.FsObjRemove(item->path());
    if (!m_comGuestSession.isOk())
        emit sigLogOutput(QString(item->path()).append(" could not be deleted"));

}

void UIGuestFileTable::goToHomeDirectory()
{
    if (m_comGuestSession.isNull())
        return;
    if (!m_pRootItem || m_pRootItem->childCount() <= 0)
        return;
    UIFileTableItem *startDirItem = m_pRootItem->child(0);
    if (!startDirItem)
        return;

    QString userHome = UIPathOperations::sanitize(m_comGuestSession.GetUserHome());
    QList<QString> pathTrail = userHome.split(UIPathOperations::delimiter);

    goIntoDirectory(pathTrail);
}

bool UIGuestFileTable::renameItem(UIFileTableItem *item, QString newBaseName)
{

    if (!item || item->isUpDirectory() || newBaseName.isEmpty() || !m_comGuestSession.isOk())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QVector<KFsObjRenameFlag> aFlags(KFsObjRenameFlag_Replace);

    m_comGuestSession.FsObjRename(item->path(), newPath, aFlags);
    if (!m_comGuestSession.isOk())
        return false;
    item->setPath(newPath);
    return true;
}

bool UIGuestFileTable::createDirectory(const QString &path, const QString &directoryName)
{
    if (!m_comGuestSession.isOk())
        return false;

    QString newDirectoryPath = UIPathOperations::mergePaths(path, directoryName);
    QVector<KDirectoryCreateFlag> flags(KDirectoryCreateFlag_None);

    m_comGuestSession.DirectoryCreate(newDirectoryPath, 777/*aMode*/, flags);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(newDirectoryPath.append(" could not be created"));
        return false;
    }
    emit sigLogOutput(newDirectoryPath.append(" has been created"));
    return true;
}

void UIGuestFileTable::copyGuestToHost(const QString& hostDestinationPath)
{
    QStringList selectedPathList = selectedItemPathList();
    for (int i = 0; i < selectedPathList.size(); ++i)
        copyGuestToHost(selectedPathList.at(i), hostDestinationPath);
}

void UIGuestFileTable::copyHostToGuest(const QStringList &hostSourcePathList)
{
    for (int i = 0; i < hostSourcePathList.size(); ++i)
        copyHostToGuest(hostSourcePathList.at(i), currentDirectoryPath());
}

bool UIGuestFileTable::copyGuestToHost(const QString &guestSourcePath, const QString& hostDestinationPath)
{
    if (m_comGuestSession.isNull())
        return false;

    /* Currently API expects a path including a file name for file copy*/
    CGuestFsObjInfo fileInfo = guestFsObjectInfo(guestSourcePath, m_comGuestSession);
    KFsObjType objectType = fileInfo.GetType();
    if (objectType == KFsObjType_File)
    {
        QVector<KFileCopyFlag> flags(KFileCopyFlag_FollowLinks);
        /* API expects a full file path as destionation: */
        QString destinatioFilePath =  UIPathOperations::mergePaths(hostDestinationPath, UIPathOperations::getObjectName(guestSourcePath));
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress =*/ m_comGuestSession.FileCopyFromGuest(guestSourcePath, destinatioFilePath, flags);

    }
    else if (objectType == KFsObjType_Directory)
    {
        QVector<KDirectoryCopyFlag> aFlags(KDirectoryCopyFlag_CopyIntoExisting);
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress = */ m_comGuestSession.DirectoryCopyFromGuest(guestSourcePath, hostDestinationPath, aFlags);
    }
    if (!m_comGuestSession.isOk())
        return false;
    return true;
}

bool UIGuestFileTable::copyHostToGuest(const QString &hostSourcePath, const QString &guestDestinationPath)
{
    if (m_comGuestSession.isNull())
        return false;
    QFileInfo hostFileInfo(hostSourcePath);
    if (!hostFileInfo.exists())
        return false;

    /* Currently API expects a path including a file name for file copy*/
    if (hostFileInfo.isFile() || hostFileInfo.isSymLink())
    {
        QVector<KFileCopyFlag> flags(KFileCopyFlag_FollowLinks);
        /* API expects a full file path as destionation: */
        QString destinationFilePath =  UIPathOperations::mergePaths(guestDestinationPath, UIPathOperations::getObjectName(hostSourcePath));
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress =*/ m_comGuestSession.FileCopyFromGuest(hostSourcePath, destinationFilePath, flags);
    }
    else if(hostFileInfo.isDir())
    {
        QVector<KDirectoryCopyFlag> aFlags(KDirectoryCopyFlag_CopyIntoExisting);
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress = */ m_comGuestSession.DirectoryCopyToGuest(hostSourcePath, guestDestinationPath, aFlags);
    }
    if (!m_comGuestSession.isOk())
        return false;
    return true;
}

FileObjectType UIGuestFileTable::getFileType(const CFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return FileObjectType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return FileObjectType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return FileObjectType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return FileObjectType_SymLink;

    return FileObjectType_Other;
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
    //directory.setFilter(QDir::NoDotAndDotDot);
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
        data << fileInfo.fileName() << fileInfo.size() << fileInfo.lastModified();
        UIFileTableItem *item = new UIFileTableItem(data, parent, getFileType(fileInfo));
        item->setPath(fileInfo.absoluteFilePath());
        /* if the item is a symlink set the target path and
           check the target if it is a directory: */
        if (fileInfo.isSymLink())
        {
            item->setTargetPath(fileInfo.symLinkTarget());
            item->setIsTargetADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
        }
        if (fileInfo.isDir())
        {
            directories.insert(fileInfo.fileName(), item);
            item->setIsOpened(false);
        }
        else
        {
            files.insert(fileInfo.fileName(), item);
            item->setIsOpened(false);
        }
    }
    insertItemsToTree(directories, parent, true, isStartDir);
    insertItemsToTree(files, parent, false, isStartDir);
    updateCurrentLocationEdit(strPath);
}

void UIHostFileTable::deleteByItem(UIFileTableItem *item)
{
    if (!item->isDirectory())
    {
        QDir itemToDelete;//(item->path());
        itemToDelete.remove(item->path());
    }
    QDir itemToDelete(item->path());
    itemToDelete.setFilter(QDir::NoDotAndDotDot);
    /* Try to delete item recursively (in case of directories).
       note that this is no good way of deleting big directory
       trees. We need a better error reporting and a kind of progress
       indicator: */
    /** @todo replace this recursive delete by a better implementation: */
    bool deleteSuccess = itemToDelete.removeRecursively();

     if (!deleteSuccess)
         emit sigLogOutput(QString(item->path()).append(" could not be deleted"));
}

void UIHostFileTable::goToHomeDirectory()
{
    if (!m_pRootItem || m_pRootItem->childCount() <= 0)
        return;
    UIFileTableItem *startDirItem = m_pRootItem->child(0);
    if (!startDirItem)
        return;

    // UIFileTableItem *rootDirectoryItem
    QDir homeDirectory(QDir::homePath());
    QList<QString> pathTrail;//(QDir::rootPath());
    do{

        pathTrail.push_front(homeDirectory.absolutePath());
        homeDirectory.cdUp();
    }while(!homeDirectory.isRoot());

    goIntoDirectory(pathTrail);
}

bool UIHostFileTable::renameItem(UIFileTableItem *item, QString newBaseName)
{
    if (!item || item->isUpDirectory() || newBaseName.isEmpty())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QDir tempDir;
    if (tempDir.rename(item->path(), newPath))
    {
        item->setPath(newPath);
        return true;
    }
    return false;
}

bool UIHostFileTable::createDirectory(const QString &path, const QString &directoryName)
{
    QDir parentDir(path);
    if (!parentDir.mkdir(directoryName))
    {
        emit sigLogOutput(UIPathOperations::mergePaths(path, directoryName).append(" could not be created"));
        return false;
    }

    return true;
}

FileObjectType UIHostFileTable::getFileType(const QFileInfo &fsInfo)
{
    if (!fsInfo.exists())
        return FileObjectType_Unknown;
    /* first check if it is symlink becacuse for Qt
       being smylin and directory/file is not mutually exclusive: */
    if (fsInfo.isSymLink())
        return FileObjectType_SymLink;
    else if (fsInfo.isFile())
        return FileObjectType_File;
    else if (fsInfo.isDir())
        return FileObjectType_Directory;

    return FileObjectType_Other;
}

#include "UIGuestControlFileTable.moc"
