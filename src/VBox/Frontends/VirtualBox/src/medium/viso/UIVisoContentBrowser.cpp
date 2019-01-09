/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoContentBrowser class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/* Qt includes: */
#include <QAction>
#include <QAbstractItemModel>
#include <QDateTime>
#include <QDir>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QSplitter>
#include <QTableView>
#include <QTreeView>

/* GUI includes: */
#include "UICustomFileSystemModel.h"
#include "UIIconPool.h"
#include "UIPathOperations.h"
#include "UIToolBar.h"
#include "UIVisoContentBrowser.h"


/*********************************************************************************************************************************
*   UIVisoContentTreeProxyModel definition.                                                                                      *
*********************************************************************************************************************************/

class UIVisoContentTreeProxyModel : public UICustomFileSystemProxyModel
{

    Q_OBJECT;

public:

    UIVisoContentTreeProxyModel(QObject *parent = 0);

protected:

    /** Used to filter-out files and show only directories. */
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const /* override */;

};


/*********************************************************************************************************************************
*   UIVisoContentTreeProxyModel implementation.                                                                                  *
*********************************************************************************************************************************/

UIVisoContentTreeProxyModel::UIVisoContentTreeProxyModel(QObject *parent /* = 0 */)
    :UICustomFileSystemProxyModel(parent)
{
}

bool UIVisoContentTreeProxyModel::filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const /* override */
{
    QModelIndex itemIndex = sourceModel()->index(iSourceRow, 0, sourceParent);
    if (!itemIndex.isValid())
        return false;

    UICustomFileSystemItem *item = static_cast<UICustomFileSystemItem*>(itemIndex.internalPointer());
    if (!item)
        return false;

    if (item->isUpDirectory())
        return false;
    if (item->isDirectory() || item->isSymLinkToADirectory())
        return true;

    return false;
}


/*********************************************************************************************************************************
*   UIVisoContentBrowser implementation.                                                                                         *
*********************************************************************************************************************************/

UIVisoContentBrowser::UIVisoContentBrowser(QWidget *pParent)
    : QIWithRetranslateUI<UIVisoBrowserBase>(pParent)
    , m_pModel(0)
    , m_pTableProxyModel(0)
    , m_pTreeProxyModel(0)
    , m_pRemoveAction(0)
    , m_pNewDirectoryAction(0)
    , m_pRenameAction(0)
    , m_pResetAction(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoContentBrowser::~UIVisoContentBrowser()
{
}

void UIVisoContentBrowser::addObjectsToViso(QStringList pathList)
{
    if (!m_pTableView)
        return;

    QModelIndex parentIndex = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
    if (!parentIndex.isValid())
         return;

    UICustomFileSystemItem *pParentItem = static_cast<UICustomFileSystemItem*>(parentIndex.internalPointer());
    if (!pParentItem)
        return;
    foreach (const QString &strPath, pathList)
    {
        QFileInfo fileInfo(strPath);
        if (!fileInfo.exists())
            continue;
        if (pParentItem->child(fileInfo.fileName()))
            continue;

        UICustomFileSystemItem* pAddedItem = new UICustomFileSystemItem(fileInfo.fileName(), pParentItem,
                                                                        fileType(fileInfo));
        pAddedItem->setData(strPath, UICustomFileSystemModelColumn_LocalPath);
        pAddedItem->setData(UIPathOperations::mergePaths(pParentItem->path(), fileInfo.fileName()),
                           UICustomFileSystemModelColumn_Path);
        pAddedItem->setIsOpened(false);
        if (fileInfo.isSymLink())
        {
            pAddedItem->setTargetPath(fileInfo.symLinkTarget());
            pAddedItem->setIsSymLinkToADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
        }
        createAnIsoEntry(pAddedItem);
    }
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
    if (m_pTreeProxyModel)
    {
        m_pTreeProxyModel->invalidate();
        m_pTreeView->setExpanded(m_pTreeView->currentIndex(), true);
    }
}

void UIVisoContentBrowser::createAnIsoEntry(UICustomFileSystemItem *pItem, bool bRemove /* = false */)
{
    if (!pItem)
        return;
    if (pItem->data(UICustomFileSystemModelColumn_Path).toString().isEmpty())
        return;

    if (!bRemove && pItem->data(UICustomFileSystemModelColumn_LocalPath).toString().isEmpty())
        return;
    if (!bRemove)
        m_entryMap.insert(pItem->data(UICustomFileSystemModelColumn_Path).toString(),
                          pItem->data(UICustomFileSystemModelColumn_LocalPath).toString());
    else
        m_entryMap.insert(pItem->data(UICustomFileSystemModelColumn_Path).toString(),
                          ":remove:");
}

QStringList UIVisoContentBrowser::entryList()
{
    QStringList entryList;
    for (QMap<QString, QString>::const_iterator iterator = m_entryMap.begin(); iterator != m_entryMap.end(); ++iterator)
    {
        QString strEntry = QString("%1=%2").arg(iterator.key()).arg(iterator.value());
        entryList << strEntry;
    }
    return entryList;
}

void UIVisoContentBrowser::retranslateUi()
{
    if (m_pTitleLabel)
        m_pTitleLabel->setText(QApplication::translate("UIVisoCreator", "VISO content"));
    if (m_pRemoveAction)
        m_pRemoveAction->setToolTip(QApplication::translate("UIVisoCreator", "Remove selected file objects from VISO"));
    if (m_pNewDirectoryAction)
        m_pNewDirectoryAction->setToolTip(QApplication::translate("UIVisoCreator", "Create a new directory under the current location"));
    if (m_pResetAction)
        m_pResetAction->setToolTip(QApplication::translate("UIVisoCreator", "Reset ISO content."));
    if (m_pRenameAction)
        m_pRenameAction->setToolTip(QApplication::translate("UIVisoCreator", "Rename the selected object"));


    UICustomFileSystemItem *pRootItem = rootItem();
    if (pRootItem)
    {
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Name"), UICustomFileSystemModelColumn_Name);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Size"), UICustomFileSystemModelColumn_Size);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Change Time"), UICustomFileSystemModelColumn_ChangeTime);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Owner"), UICustomFileSystemModelColumn_Owner);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Permissions"), UICustomFileSystemModelColumn_Permissions);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "Local Path"), UICustomFileSystemModelColumn_LocalPath);
        pRootItem->setData(QApplication::translate("UIVisoCreator", "ISO Path"), UICustomFileSystemModelColumn_Path);
    }
}

void UIVisoContentBrowser::tableViewItemDoubleClick(const QModelIndex &index)
{
    if (!index.isValid() || !m_pTableProxyModel)
        return;
    UICustomFileSystemItem *pClickedItem =
        static_cast<UICustomFileSystemItem*>(m_pTableProxyModel->mapToSource(index).internalPointer());
    if (pClickedItem->isUpDirectory())
    {
        QModelIndex currentRoot = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
        /* Go up if we are not already there: */
        if (currentRoot != m_pModel->rootIndex())
        {
            setTableRootIndex(currentRoot.parent());
            setTreeCurrentIndex(currentRoot.parent());
        }
    }
    else
    {
        scanHostDirectory(pClickedItem);
        setTableRootIndex(index);
        setTreeCurrentIndex(index);
    }
}

void UIVisoContentBrowser::sltHandleCreateNewDirectory()
{
    if (!m_pTableView)
        return;
    QString strNewDirectoryName("NewDirectory");

    QModelIndex parentIndex = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
    if (!parentIndex.isValid())
         return;

    UICustomFileSystemItem *pParentItem = static_cast<UICustomFileSystemItem*>(parentIndex.internalPointer());
    if (!pParentItem)
        return;

    /*  Check to see if we already have a directory named strNewDirectoryName: */
    const QList<const UICustomFileSystemItem*> children = pParentItem->children();
    foreach (const UICustomFileSystemItem *item, children)
    {
        if (item->name() == strNewDirectoryName)
            return;
    }

    UICustomFileSystemItem* pAddedItem = new UICustomFileSystemItem(strNewDirectoryName, pParentItem,
                                                                    KFsObjType_Directory);
    pAddedItem->setData(UIPathOperations::mergePaths(pParentItem->path(), strNewDirectoryName), UICustomFileSystemModelColumn_Path);

    pAddedItem->setIsOpened(false);
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();

    renameFileObject(pAddedItem);
}

void UIVisoContentBrowser::sltHandleRemoveItems()
{
    removeItems(tableSelectedItems());
}

void UIVisoContentBrowser::removeItems(const QList<UICustomFileSystemItem*> itemList)
{
    foreach(UICustomFileSystemItem *pItem, itemList)
    {
        if (!pItem)
            continue;
        bool bFoundInMap = false;
        for (QMap<QString, QString>::iterator iterator = m_entryMap.begin(); iterator != m_entryMap.end(); )
        {
            QString strIsoPath = pItem->data(UICustomFileSystemModelColumn_Path).toString();
            if (strIsoPath.isEmpty())
                continue;
            if (iterator.key().startsWith(strIsoPath))
            {
                iterator = m_entryMap.erase(iterator);
                bFoundInMap = true;
            }
            else
                ++iterator;
        }
        if (!bFoundInMap)
            createAnIsoEntry(pItem, true /* bool bRemove */);
    }

    foreach(UICustomFileSystemItem *pItem, itemList)
    {
        if (!pItem)
            continue;
        /* Remove the item from the m_pModel: */
        if (m_pModel)
            m_pModel->deleteItem(pItem);
    }
    if (m_pTreeProxyModel)
        m_pTreeProxyModel->invalidate();
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();

}

void UIVisoContentBrowser::prepareObjects()
{
    UIVisoBrowserBase::prepareObjects();

    m_pModel = new UICustomFileSystemModel(this);
    m_pTableProxyModel = new UICustomFileSystemProxyModel(this);
    if (m_pTableProxyModel)
    {
        m_pTableProxyModel->setSourceModel(m_pModel);
        m_pTableProxyModel->setListDirectoriesOnTop(true);
    }

    m_pTreeProxyModel = new UIVisoContentTreeProxyModel(this);
    if (m_pTreeProxyModel)
    {
        m_pTreeProxyModel->setSourceModel(m_pModel);
    }

    initializeModel();

    if (m_pTreeView)
    {
        m_pTreeView->setModel(m_pTreeProxyModel);
        m_pTreeView->setCurrentIndex(m_pTreeProxyModel->mapFromSource(m_pModel->rootIndex()));
        m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Show only the 0th column that is "name': */
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_Owner);
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_Permissions);
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_Size);
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_ChangeTime);
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_Path);
        m_pTreeView->hideColumn(UICustomFileSystemModelColumn_LocalPath);
    }

    if (m_pTableView)
    {
        m_pTableView->setModel(m_pTableProxyModel);
        setTableRootIndex();
        m_pTableView->hideColumn(UICustomFileSystemModelColumn_Owner);
        m_pTableView->hideColumn(UICustomFileSystemModelColumn_Permissions);
        m_pTableView->hideColumn(UICustomFileSystemModelColumn_Size);
        m_pTableView->hideColumn(UICustomFileSystemModelColumn_ChangeTime);

        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);
    }

    m_pRemoveAction = new QAction(this);
    if (m_pRemoveAction)
    {
        m_pVerticalToolBar->addAction(m_pRemoveAction);
        m_pRemoveAction->setIcon(UIIconPool::iconSetFull(":/file_manager_delete_24px.png", ":/file_manager_delete_16px.png",
                                                     ":/file_manager_delete_disabled_24px.png", ":/file_manager_delete_disabled_16px.png"));
        m_pRemoveAction->setEnabled(false);
    }

    m_pNewDirectoryAction = new QAction(this);
    if (m_pNewDirectoryAction)
    {
        m_pVerticalToolBar->addAction(m_pNewDirectoryAction);
        m_pNewDirectoryAction->setIcon(UIIconPool::iconSetFull(":/file_manager_new_directory_24px.png", ":/file_manager_new_directory_16px.png",
                                                           ":/file_manager_new_directory_disabled_24px.png", ":/file_manager_new_directory_disabled_16px.png"));
        m_pNewDirectoryAction->setEnabled(true);
    }

    m_pRenameAction = new QAction(this);
    if (m_pRenameAction)
    {
        m_pVerticalToolBar->addAction(m_pRenameAction);
        m_pRenameAction->setIcon(UIIconPool::iconSet(":/file_manager_rename_16px.png", ":/file_manager_rename_disabled_16px.png"));
        m_pRenameAction->setEnabled(false);
    }

    m_pVerticalToolBar->addSeparator();

    m_pResetAction = new QAction(this);
    if (m_pResetAction)
    {
        m_pVerticalToolBar->addAction(m_pResetAction);
        m_pResetAction->setIcon(UIIconPool::iconSet(":/cd_remove_16px.png", ":/cd_remove_disabled_16px.png"));
        m_pResetAction->setEnabled(true);
    }

    retranslateUi();
}

void UIVisoContentBrowser::prepareConnections()
{
    UIVisoBrowserBase::prepareConnections();
    if (m_pNewDirectoryAction)
        connect(m_pNewDirectoryAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltHandleCreateNewDirectory);
    if (m_pModel)
        connect(m_pModel, &UICustomFileSystemModel::sigItemRenamed,
                this, &UIVisoContentBrowser::sltHandleItemRenameAttempt);
    if (m_pRemoveAction)
        connect(m_pRemoveAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltHandleRemoveItems);
    if (m_pResetAction)
        connect(m_pResetAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltHandleResetAction);

    if (m_pTableView->selectionModel())
        connect(m_pTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoContentBrowser::sltHandleTableSelectionChanged);

}

UICustomFileSystemItem* UIVisoContentBrowser::rootItem()
{
    if (!m_pModel)
        return 0;
    return m_pModel->rootItem();
}


void UIVisoContentBrowser::initializeModel()
{
    if (m_pModel)
        m_pModel->reset();
    if (!rootItem())
        return;

    const QString startPath = QString("/%1").arg(m_strVisoName);

    UICustomFileSystemItem* startItem = new UICustomFileSystemItem(startPath, rootItem(), KFsObjType_Directory);
    startItem->setPath("/");
    startItem->setIsOpened(false);
}

void UIVisoContentBrowser::setTableRootIndex(QModelIndex index /* = QModelIndex */)
{
    if (!m_pTableView)
        return;
    if (index.isValid())
    {
        QModelIndex tableIndex = convertIndexToTableIndex(index);
        if (tableIndex.isValid())
            m_pTableView->setRootIndex(tableIndex);
        return;
    }
    QItemSelectionModel *selectionModel = m_pTreeView->selectionModel();
    if (selectionModel)
    {
        if (!selectionModel->selectedIndexes().isEmpty())
        {
            QModelIndex treeIndex = selectionModel->selectedIndexes().at(0);
            QModelIndex tableIndex = convertIndexToTableIndex(treeIndex);
            if (tableIndex.isValid())
                m_pTableView->setRootIndex(tableIndex);
        }
    }
}

void UIVisoContentBrowser::setTreeCurrentIndex(QModelIndex index /* = QModelIndex() */)
{
    if (!m_pTreeView)
        return;
    QItemSelectionModel *pSelectionModel = m_pTreeView->selectionModel();
    if (!pSelectionModel)
        return;
    m_pTreeView->blockSignals(true);
    pSelectionModel->blockSignals(true);
    QModelIndex treeIndex;
    if (index.isValid())
    {
        treeIndex = convertIndexToTreeIndex(index);
    }
    else
    {
        QItemSelectionModel *selectionModel = m_pTableView->selectionModel();
        if (selectionModel)
        {
            if (!selectionModel->selectedIndexes().isEmpty())
            {
                QModelIndex tableIndex = selectionModel->selectedIndexes().at(0);
                treeIndex = convertIndexToTreeIndex(tableIndex);
            }
        }
    }

    if (treeIndex.isValid())
    {
        m_pTreeView->setCurrentIndex(treeIndex);
        m_pTreeView->setExpanded(treeIndex, true);
        m_pTreeView->scrollTo(index, QAbstractItemView::PositionAtCenter);
        m_pTreeProxyModel->invalidate();
    }

    pSelectionModel->blockSignals(false);
    m_pTreeView->blockSignals(false);
}



void UIVisoContentBrowser::treeSelectionChanged(const QModelIndex &selectedTreeIndex)
{
    if (!m_pTableProxyModel || !m_pTreeProxyModel)
        return;

    /* Check if we need to scan the directory in the host system: */
    UICustomFileSystemItem *pClickedItem =
        static_cast<UICustomFileSystemItem*>(m_pTreeProxyModel->mapToSource(selectedTreeIndex).internalPointer());
    scanHostDirectory(pClickedItem);
    setTableRootIndex(selectedTreeIndex);
    m_pTableProxyModel->invalidate();
    m_pTreeProxyModel->invalidate();
}

void UIVisoContentBrowser::showHideHiddenObjects(bool bShow)
{
    Q_UNUSED(bShow);
}

void UIVisoContentBrowser::setVisoName(const QString &strName)
{
    if (m_strVisoName == strName)
        return;
    m_strVisoName = strName;
    updateStartItemName();
}

QModelIndex UIVisoContentBrowser::convertIndexToTableIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return QModelIndex();

    if (index.model() == m_pTableProxyModel)
        return index;
    else if (index.model() == m_pModel)
        return m_pTableProxyModel->mapFromSource(index);
    /* else if (index.model() == m_pTreeProxyModel): */
    return m_pTableProxyModel->mapFromSource(m_pTreeProxyModel->mapToSource(index));
}

QModelIndex UIVisoContentBrowser::convertIndexToTreeIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return QModelIndex();

    if (index.model() == m_pTreeProxyModel)
        return index;
    else if (index.model() == m_pModel)
        return m_pTreeProxyModel->mapFromSource(index);
    /* else if (index.model() == m_pTableProxyModel): */
    return m_pTreeProxyModel->mapFromSource(m_pTableProxyModel->mapToSource(index));
}

void UIVisoContentBrowser::scanHostDirectory(UICustomFileSystemItem *directoryItem)
{
    if (!directoryItem)
        return;
    /* the clicked item can be a directory created with the VISO content. in that case local path data
       should be empty: */
    if (directoryItem->type() != KFsObjType_Directory ||
        directoryItem->data(UICustomFileSystemModelColumn_LocalPath).toString().isEmpty())
        return;
    QDir directory(directoryItem->data(UICustomFileSystemModelColumn_LocalPath).toString());
    if (directory.exists() && !directoryItem->isOpened())
    {
        QFileInfoList directoryContent = directory.entryInfoList();
        for (int i = 0; i < directoryContent.size(); ++i)
        {
            const QFileInfo &fileInfo = directoryContent[i];
            if (fileInfo.fileName() == ".")
                continue;
            UICustomFileSystemItem *newItem = new UICustomFileSystemItem(fileInfo.fileName(),
                                                                         directoryItem,
                                                                       fileType(fileInfo));
            newItem->setData(fileInfo.filePath(), UICustomFileSystemModelColumn_LocalPath);

            newItem->setData(UIPathOperations::mergePaths(directoryItem->path(), fileInfo.fileName()),
                             UICustomFileSystemModelColumn_Path);
            if (fileInfo.isSymLink())
            {
                newItem->setTargetPath(fileInfo.symLinkTarget());
                newItem->setIsSymLinkToADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
            }
        }
        directoryItem->setIsOpened(true);
    }
}

/* static */ KFsObjType UIVisoContentBrowser::fileType(const QFileInfo &fsInfo)
{
    if (!fsInfo.exists())
        return KFsObjType_Unknown;
    /* first check if it is symlink becacuse for Qt
       being smylin and directory/file is not mutually exclusive: */
    if (fsInfo.isSymLink())
        return KFsObjType_Symlink;
    else if (fsInfo.isFile())
        return KFsObjType_File;
    else if (fsInfo.isDir())
        return KFsObjType_Directory;

    return KFsObjType_Unknown;
}

void UIVisoContentBrowser::updateStartItemName()
{
    if (!rootItem() || !rootItem()->child(0))
        return;
    const QString strName = QString("/%1").arg(m_strVisoName);

    rootItem()->child(0)->setData(strName, UICustomFileSystemModelColumn_Name);
    m_pTreeProxyModel->invalidate();
    m_pTableProxyModel->invalidate();
}

void UIVisoContentBrowser::renameFileObject(UICustomFileSystemItem *pItem)
{
    m_pTableView->edit(m_pTableProxyModel->mapFromSource(m_pModel->index(pItem)));
}

void UIVisoContentBrowser::sltHandleItemRenameAttempt(UICustomFileSystemItem *pItem, QString strOldName, QString strNewName)
{
    if (!pItem || !pItem->parentItem())
        return;
    QList<const UICustomFileSystemItem*> children = pItem->parentItem()->children();
    bool bDuplicate = false;
    foreach (const UICustomFileSystemItem *item, children)
    {
        if (item->name() == strNewName && item != pItem)
            bDuplicate = true;
    }

    if (bDuplicate)
    {
        /* Restore the previous name in case the @strNewName is a duplicate: */
        pItem->setData(strOldName, static_cast<int>(UICustomFileSystemModelColumn_Name));
    }

    pItem->setData(UIPathOperations::mergePaths(pItem->parentItem()->path(), pItem->name()), UICustomFileSystemModelColumn_Path);

    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
}

void UIVisoContentBrowser::sltHandleTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    if (m_pRemoveAction)
        m_pRemoveAction->setEnabled(!selected.isEmpty());
    if (m_pRenameAction)
        m_pRenameAction->setEnabled(!selected.isEmpty());
}

void UIVisoContentBrowser::sltHandleResetAction()
{
    if (!rootItem() || !rootItem()->child(0))
        return;
    rootItem()->child(0)->removeChildren();
    m_entryMap.clear();
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
    if (m_pTreeProxyModel)
        m_pTreeProxyModel->invalidate();
}

void UIVisoContentBrowser::reset()
{
    m_entryMap.clear();
}

QList<UICustomFileSystemItem*> UIVisoContentBrowser::tableSelectedItems()
{
    QList<UICustomFileSystemItem*> selectedItems;
    if (!m_pTableProxyModel)
        return selectedItems;
    QItemSelectionModel *selectionModel = m_pTableView->selectionModel();
    if (!selectionModel || selectionModel->selectedIndexes().isEmpty())
        return selectedItems;
    QModelIndexList list = selectionModel->selectedRows();
    foreach (QModelIndex index, list)
    {
        UICustomFileSystemItem *pItem =
            static_cast<UICustomFileSystemItem*>(m_pTableProxyModel->mapToSource(index).internalPointer());
        if (pItem)
            selectedItems << pItem;
    }
    return selectedItems;
}

#include "UIVisoContentBrowser.moc"
