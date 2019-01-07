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
#include "QIToolButton.h"
#include "UICustomFileSystemModel.h"
#include "UIIconPool.h"
#include "UIPathOperations.h"
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
    , m_pNewDirectoryButton(0)
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
        m_entryList << createAnIsoEntry(pAddedItem);
    }
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
    if (m_pTreeProxyModel)
    {
        m_pTreeProxyModel->invalidate();
        m_pTreeView->setExpanded(m_pTreeView->currentIndex(), true);
    }
}

QString UIVisoContentBrowser::createAnIsoEntry(UICustomFileSystemItem *pItem)
{
    QString strEntry;
    if (!pItem)
        return strEntry;
    if (pItem->data(UICustomFileSystemModelColumn_Path).toString().isEmpty() ||
        pItem->data(UICustomFileSystemModelColumn_LocalPath).toString().isEmpty())
        return strEntry;
    strEntry = QString("%1=%2").arg(pItem->data(UICustomFileSystemModelColumn_Path).toString()).
        arg(pItem->data(UICustomFileSystemModelColumn_LocalPath).toString());
    return strEntry;
}

QStringList UIVisoContentBrowser::entryList()
{
    return m_entryList;
}

void UIVisoContentBrowser::retranslateUi()
{
    if (m_pTitleLabel)
        m_pTitleLabel->setText(QApplication::translate("UIVisoCreator", "VISO content"));
    if (m_pAddRemoveButton)
        m_pAddRemoveButton->setToolTip(QApplication::translate("UIVisoCreator", "Remove selected file objects from VISO"));
    if (m_pNewDirectoryButton)
        m_pNewDirectoryButton->setToolTip(QApplication::translate("UIVisoCreator", "Create a new directory under the current directory"));

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
    QList<const UICustomFileSystemItem*> children = pParentItem->children();
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
        // m_pTreeView->setRootIndex(m_pProxyModel->mapFromSource(m_pModel->rootIndex()));
        m_pTreeView->setCurrentIndex(m_pTreeProxyModel->mapFromSource(m_pModel->rootIndex()));

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

    if (m_pAddRemoveButton)
        m_pAddRemoveButton->setIcon(UIIconPool::iconSet(":/attachment_remove_16px.png", ":/attachment_remove_disabled_16px.png"));

    m_pNewDirectoryButton = new QIToolButton;
    if (m_pNewDirectoryButton)
    {
        m_pRightContainerLayout->addWidget(m_pNewDirectoryButton, 1, 5, 1, 1);
        m_pNewDirectoryButton->setIcon(UIIconPool::iconSet(":/attachment_add_16px.png", ":/attachment_add_disabled_16px.png"));
        m_pNewDirectoryButton->setEnabled(true);
    }

    retranslateUi();
}

void UIVisoContentBrowser::prepareConnections()
{
    UIVisoBrowserBase::prepareConnections();
    if (m_pNewDirectoryButton)
        connect(m_pNewDirectoryButton, &QIToolButton::clicked,
                this, &UIVisoContentBrowser::sltHandleCreateNewDirectory);
    if (m_pModel)
        connect(m_pModel, &UICustomFileSystemModel::sigItemRenamed,
                this, &UIVisoContentBrowser::sltHandleItemRenameAttempt);
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

void UIVisoContentBrowser::reset()
{
    m_entryList.clear();
}
#include "UIVisoContentBrowser.moc"
