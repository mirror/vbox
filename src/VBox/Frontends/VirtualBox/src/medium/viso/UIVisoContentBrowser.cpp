/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoContentBrowser class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/* Qt includes: */
#include <QDir>
#include <QItemDelegate>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QTableView>
#include <QTextStream>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIActionPool.h"
#include "UICustomFileSystemModel.h"
#include "UIPathOperations.h"
#include "UIVisoContentBrowser.h"

/* iprt includes: */
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/vfs.h>
#include <iprt/file.h>
#include <iprt/fsvfs.h>
#include <iprt/mem.h>
#include <iprt/err.h>

const ULONG uAllowedFileSize = _4K;

struct ISOFileObject
{
    QString strName;
    KFsObjType enmObjectType;
};


static void readISODir(RTVFSDIR &hVfsDir, QList<ISOFileObject> &fileObjectList)
{
    size_t cbDirEntry = sizeof(RTDIRENTRYEX);
    PRTDIRENTRYEX pDirEntry = (PRTDIRENTRYEX)RTMemTmpAlloc(cbDirEntry);
    size_t cbDirEntryAlloced = cbDirEntry;
    for(;;)
    {
        if (pDirEntry)
        {
            int vrc = RTVfsDirReadEx(hVfsDir, pDirEntry, &cbDirEntry, RTFSOBJATTRADD_UNIX);
            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_BUFFER_OVERFLOW)
                {
                    RTMemTmpFree(pDirEntry);
                    cbDirEntryAlloced = RT_ALIGN_Z(RT_MIN(cbDirEntry, cbDirEntryAlloced) + 64, 64);
                    pDirEntry  = (PRTDIRENTRYEX)RTMemTmpAlloc(cbDirEntryAlloced);
                    if (pDirEntry)
                        continue;
                    /// @todo log error
                    //rcExit = RTMsgErrorExitFailure("Out of memory (direntry buffer)");
                }
                /// @todo log error
                // else if (rc != VERR_NO_MORE_FILES)
                //     rcExit = RTMsgErrorExitFailure("RTVfsDirReadEx failed: %Rrc\n", rc);
                break;
            }
            else
            {
                ISOFileObject fileObject;

                if (RTFS_IS_DIRECTORY(pDirEntry->Info.Attr.fMode))
                    fileObject.enmObjectType =  KFsObjType_Directory;
                else
                    fileObject.enmObjectType = KFsObjType_File;
                fileObject.strName = pDirEntry->szName;
                fileObjectList << fileObject;
            }
        }
    }
    RTMemTmpFree(pDirEntry);
}

static QList<ISOFileObject> openAndReadISODir(const QString &strISOFilePath, QString strDirPath = QString())
{
    QList<ISOFileObject> fileObjectList;

    RTVFSFILE hVfsFileIso;
    int vrc = RTVfsFileOpenNormal(strISOFilePath.toUtf8().constData(), RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hVfsFileIso);
    if (RT_SUCCESS(vrc))
    {
        RTERRINFOSTATIC ErrInfo;
        RTVFS hVfsIso;
        vrc = RTFsIso9660VolOpen(hVfsFileIso, 0 /*fFlags*/, &hVfsIso, RTErrInfoInitStatic(&ErrInfo));
        if (RT_SUCCESS(vrc))
        {
            RTVFSDIR hVfsSrcRootDir;
            vrc = RTVfsOpenRoot(hVfsIso, &hVfsSrcRootDir);
            if (RT_SUCCESS(vrc))
            {
                if (strDirPath.isEmpty())
                    readISODir(hVfsSrcRootDir, fileObjectList);
                else
                {
                    RTVFSDIR hVfsDir;
                    vrc = RTVfsDirOpenDir(hVfsSrcRootDir, strDirPath.toUtf8().constData(), 0 /* fFlags */, &hVfsDir);
                    if (RT_SUCCESS(vrc))
                    {
                        readISODir(hVfsDir, fileObjectList);
                        RTVfsDirRelease(hVfsDir);
                    }
                }

                RTVfsDirRelease(hVfsSrcRootDir);
            }
            RTVfsRelease(hVfsIso);
        }
        RTVfsFileRelease(hVfsFileIso);
    }
    return fileObjectList;
}


/*********************************************************************************************************************************
*   UIContentBrowserDelegate definition.                                                                                         *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIContentBrowserDelegate : public QItemDelegate
{

    Q_OBJECT;

public:

    UIContentBrowserDelegate(QObject *pParent)
        : QItemDelegate(pParent){}

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   UIVisoContentTableView definition.                                                                                      *
*********************************************************************************************************************************/

/** An QTableView extension mainly used to handle dropeed file objects from the host browser. */
class UIVisoContentTableView : public QTableView
{
    Q_OBJECT;

signals:

    void sigNewItemsDropped(QStringList pathList);

public:

    UIVisoContentTableView(QWidget *pParent = 0);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
};


/*********************************************************************************************************************************
*   UIVisoContentTableView implementation.                                                                                       *
*********************************************************************************************************************************/
UIVisoContentTableView::UIVisoContentTableView(QWidget *pParent /* = 0 */)
    :QTableView(pParent)
{
}

void UIVisoContentTableView::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void UIVisoContentTableView::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if (pEvent->mimeData()->hasFormat("application/vnd.text.list"))
        pEvent->accept();
    else
        pEvent->ignore();
}

void UIVisoContentTableView::dropEvent(QDropEvent *pEvent)
{
    if (pEvent->mimeData()->hasFormat("application/vnd.text.list"))
    {
        QByteArray itemData = pEvent->mimeData()->data("application/vnd.text.list");
        QDataStream stream(&itemData, QIODevice::ReadOnly);
        QStringList pathList;

        while (!stream.atEnd()) {
            QString text;
            stream >> text;
            pathList << text;
        }
        emit sigNewItemsDropped(pathList);
    }
}


/*********************************************************************************************************************************
*   UIVisoContentBrowser implementation.                                                                                         *
*********************************************************************************************************************************/

UIVisoContentBrowser::UIVisoContentBrowser(UIActionPool *pActionPool, QWidget *pParent)
    : UIVisoBrowserBase(pActionPool, pParent)
    , m_pTableView(0)
    , m_pModel(0)
    , m_pTableProxyModel(0)
    , m_pRemoveAction(0)
    , m_pCreateNewDirectoryAction(0)
    , m_pRenameAction(0)
    , m_pResetAction(0)
{
    prepareObjects();
    prepareToolBar();
    prepareConnections();
    retranslateUi();

    /* Assuming the root items only child is the one with the path '/', navigate into it. */
    /* Hack alert. for some reason without invalidating proxy models mapFromSource return invalid index. */
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();

    if (rootItem() && rootItem()->childCount() > 0)
    {
        UICustomFileSystemItem *pStartItem = static_cast<UICustomFileSystemItem*>(rootItem()->children()[0]);
        if (pStartItem)
        {
            QModelIndex iindex = m_pModel->index(pStartItem);
            if (iindex.isValid())
                tableViewItemDoubleClick(convertIndexToTableIndex(iindex));
        }
    }
}

UIVisoContentBrowser::~UIVisoContentBrowser()
{
}

void UIVisoContentBrowser::importISOContentToViso(const QString &strISOFilePath,
                                                  UICustomFileSystemItem *pParentItem /* = 0 */,
                                                  const QString &strDirPath /* = QString() */)
{
    /* We can import only a ISO file into VISO:*/
    if (!importedISOPath().isEmpty())
        return;
    if (!pParentItem)
    {
        pParentItem = startItem();
        setTableRootIndex(m_pModel->index(pParentItem));
    }
    if (!m_pTableView || !pParentItem)
        return;

    /* If this is not the root directory add an "up" file object explicity since RTVfsDirReadEx does not return one:*/
    if (!strDirPath.isEmpty())
    {
        UICustomFileSystemItem* pAddedItem = new UICustomFileSystemItem(UICustomFileSystemModel::strUpDirectoryString,
                                                                        pParentItem,
                                                                        KFsObjType_Directory);
        pAddedItem->setData(strISOFilePath, UICustomFileSystemModelData_ISOFilePath);
    }
    QList<ISOFileObject> objectList = openAndReadISODir(strISOFilePath, strDirPath);

    /* Update import ISO path and effectively add it to VISO file content if ISO reading has succeeds: */
    if (!objectList.isEmpty())
        setImportedISOPath(strISOFilePath);

    for (int i = 0; i < objectList.size(); ++i)
    {
        if (objectList[i].strName == "." || objectList[i].strName == "..")
            continue;

        QFileInfo fileInfo(objectList[i].strName);
        if (pParentItem->child(fileInfo.fileName()))
            continue;

        UICustomFileSystemItem* pAddedItem = new UICustomFileSystemItem(fileInfo.fileName(), pParentItem,
                                                                        objectList[i].enmObjectType);
        /* VISOPAth and LocalPath is the same since we allow importing ISO content only to VISO root:*/
        QString path = UIPathOperations::mergePaths(pParentItem->path(), fileInfo.fileName());
        pAddedItem->setData(path, UICustomFileSystemModelData_LocalPath);
        pAddedItem->setData(strISOFilePath, UICustomFileSystemModelData_ISOFilePath);
        pAddedItem->setData(path, UICustomFileSystemModelData_VISOPath);
        pAddedItem->setIsOpened(false);
        // if (fileInfo.isSymLink())
        // {
        //     pAddedItem->setTargetPath(fileInfo.symLinkTarget());
        //     pAddedItem->setIsSymLinkToADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
        // }
        //createVisoEntry(pAddedItem);
    }
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
    emit sigISOContentImportedOrRemoved(true /* imported*/);
}

void UIVisoContentBrowser::removeISOContentFromViso()
{
    UICustomFileSystemItem* pParentItem = startItem();
    AssertReturnVoid(pParentItem);
    AssertReturnVoid(m_pModel);

    QList<UICustomFileSystemItem*> itemsToDelete;
    /* Delete all children of startItem that were imported from an ISO: */
    for (int i = 0; i < pParentItem->childCount(); ++i)
    {
        UICustomFileSystemItem* pItem = pParentItem->child(i);
        if (!pItem || pItem->data(UICustomFileSystemModelData_ISOFilePath).toString().isEmpty())
            continue;
        itemsToDelete << pItem;
    }

    foreach (UICustomFileSystemItem *pItem, itemsToDelete)
            m_pModel->deleteItem(pItem);
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();

    setImportedISOPath();
    emit sigISOContentImportedOrRemoved(false /* imported*/);
}

void UIVisoContentBrowser::addObjectsToViso(const QStringList &pathList)
{
    if (!m_pTableView)
        return;

    /* Insert items to the current directory shown in the table view: */
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
        pAddedItem->setData(strPath, UICustomFileSystemModelData_LocalPath);
        pAddedItem->setData(UIPathOperations::mergePaths(pParentItem->path(), fileInfo.fileName()),
                           UICustomFileSystemModelData_VISOPath);
        pAddedItem->setIsOpened(false);
        if (fileInfo.isSymLink())
        {
            pAddedItem->setTargetPath(fileInfo.symLinkTarget());
            pAddedItem->setIsSymLinkToADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
        }
        createVisoEntry(pAddedItem);
    }
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
}

void UIVisoContentBrowser::createVisoEntry(UICustomFileSystemItem *pItem, bool bRemove /* = false */)
{
    if (!pItem)
        return;
    if (pItem->data(UICustomFileSystemModelData_VISOPath).toString().isEmpty())
        return;


    if (!bRemove && pItem->data(UICustomFileSystemModelData_LocalPath).toString().isEmpty())
        return;
    if (!bRemove)
        m_entryMap.insert(pItem->data(UICustomFileSystemModelData_VISOPath).toString(),
                          pItem->data(UICustomFileSystemModelData_LocalPath).toString());
    else
        m_entryMap.insert(pItem->data(UICustomFileSystemModelData_VISOPath).toString(),
                          ":remove:");
}

QStringList UIVisoContentBrowser::entryList()
{
    QStringList entryList;
    for (QMap<QString, QString>::const_iterator iterator = m_entryMap.begin(); iterator != m_entryMap.end(); ++iterator)
    {
        if (iterator.value().isEmpty())
            continue;
        QString strEntry = QString("%1=%2").arg(iterator.key()).arg(iterator.value());
        entryList << strEntry;
    }
    return entryList;
}

void UIVisoContentBrowser::retranslateUi()
{
    UICustomFileSystemItem *pRootItem = rootItem();
    if (pRootItem)
    {
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Name"), UICustomFileSystemModelData_Name);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Size"), UICustomFileSystemModelData_Size);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Change Time"), UICustomFileSystemModelData_ChangeTime);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Owner"), UICustomFileSystemModelData_Owner);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Permissions"), UICustomFileSystemModelData_Permissions);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "Local Path"), UICustomFileSystemModelData_LocalPath);
        pRootItem->setData(QApplication::translate("UIVisoCreatorWidget", "VISO Path"), UICustomFileSystemModelData_VISOPath);
    }
    setFileTableLabelText(QApplication::translate("UIVisoCreatorWidget","VISO Content"));
}

void UIVisoContentBrowser::tableViewItemDoubleClick(const QModelIndex &index)
{
    if (!index.isValid() || !m_pTableProxyModel)
        return;
    UICustomFileSystemItem *pClickedItem =
        static_cast<UICustomFileSystemItem*>(m_pTableProxyModel->mapToSource(index).internalPointer());
    if (!pClickedItem)
        return;
    if (!pClickedItem->isDirectory())
        return;
    QString strISOPath = pClickedItem->data(UICustomFileSystemModelData_ISOFilePath).toString();
    if (pClickedItem->isUpDirectory())
        goUp();
    else if (!strISOPath.isEmpty())
    {
        importISOContentToViso(strISOPath, pClickedItem, pClickedItem->data(UICustomFileSystemModelData_LocalPath).toString());
        setTableRootIndex(index);
    }
    else
    {
        scanHostDirectory(pClickedItem);
        setTableRootIndex(index);
    }
}

void UIVisoContentBrowser::sltCreateNewDirectory()
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
    const QList<UICustomFileSystemItem*> children = pParentItem->children();
    foreach (const UICustomFileSystemItem *item, children)
    {
        if (item->fileObjectName() == strNewDirectoryName)
            return;
    }

    UICustomFileSystemItem* pAddedItem = new UICustomFileSystemItem(strNewDirectoryName, pParentItem,
                                                                    KFsObjType_Directory);
    pAddedItem->setData(UIPathOperations::mergePaths(pParentItem->path(), strNewDirectoryName), UICustomFileSystemModelData_VISOPath);

    pAddedItem->setIsOpened(false);
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();

    renameFileObject(pAddedItem);
}

void UIVisoContentBrowser::sltRemoveItems()
{
    removeItems(tableSelectedItems());
}

void UIVisoContentBrowser::removeItems(const QList<UICustomFileSystemItem*> itemList)
{
    foreach(UICustomFileSystemItem *pItem, itemList)
    {
        if (!pItem)
            continue;
        QString strIsoPath = pItem->data(UICustomFileSystemModelData_VISOPath).toString();
        if (strIsoPath.isEmpty())
            continue;

        bool bFoundInMap = false;
        for (QMap<QString, QString>::iterator iterator = m_entryMap.begin(); iterator != m_entryMap.end(); )
        {
            if (iterator.key().startsWith(strIsoPath))
            {
                iterator = m_entryMap.erase(iterator);
                bFoundInMap = true;
            }
            else
                ++iterator;
        }
        if (!bFoundInMap)
            createVisoEntry(pItem, true /* bool bRemove */);
    }

    foreach(UICustomFileSystemItem *pItem, itemList)
    {
        if (!pItem)
            continue;
        /* Remove the item from the m_pModel: */
        if (m_pModel)
            m_pModel->deleteItem(pItem);
    }
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

    initializeModel();

    m_pTableView = new UIVisoContentTableView;
    if (m_pTableView)
    {
        m_pMainLayout->addWidget(m_pTableView, 2, 0, 6, 4);
        m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_pTableView->setShowGrid(false);
        m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setTabKeyNavigation(false);
        m_pTableView->setItemDelegate(new UIContentBrowserDelegate(this));
        QHeaderView *pVerticalHeader = m_pTableView->verticalHeader();
        if (pVerticalHeader)
        {
            m_pTableView->verticalHeader()->setVisible(false);
            /* Minimize the row height: */
            m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
        }
        QHeaderView *pHorizontalHeader = m_pTableView->horizontalHeader();
        if (pHorizontalHeader)
        {
            pHorizontalHeader->setHighlightSections(false);
            pHorizontalHeader->setSectionResizeMode(QHeaderView::Stretch);
        }

        m_pTableView->setModel(m_pTableProxyModel);
        setTableRootIndex();
        m_pTableView->hideColumn(UICustomFileSystemModelData_Owner);
        m_pTableView->hideColumn(UICustomFileSystemModelData_Permissions);
        m_pTableView->hideColumn(UICustomFileSystemModelData_Size);
        m_pTableView->hideColumn(UICustomFileSystemModelData_ChangeTime);
        m_pTableView->hideColumn(UICustomFileSystemModelData_ISOFilePath);

        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);

        m_pTableView->setDragEnabled(false);
        m_pTableView->setAcceptDrops(true);
        m_pTableView->setDropIndicatorShown(true);
        m_pTableView->setDragDropMode(QAbstractItemView::DropOnly);
    }
}

void UIVisoContentBrowser::prepareToolBar()
{
    m_pRemoveAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Remove);
    m_pCreateNewDirectoryAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_CreateNewDirectory);
    m_pRenameAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Rename);
    m_pResetAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Reset);
    m_pGoUp = m_pActionPool->action(UIActionIndex_M_VISOCreator_VisoContent_GoUp);
    m_pGoForward = m_pActionPool->action(UIActionIndex_M_VISOCreator_VisoContent_GoForward);
    m_pGoBackward = m_pActionPool->action(UIActionIndex_M_VISOCreator_VisoContent_GoBackward);

    AssertReturnVoid(m_pRemoveAction);
    AssertReturnVoid(m_pCreateNewDirectoryAction);
    AssertReturnVoid(m_pRenameAction);
    AssertReturnVoid(m_pResetAction);
    AssertReturnVoid(m_pToolBar);
    AssertReturnVoid(m_pGoUp);
    AssertReturnVoid(m_pGoForward);
    AssertReturnVoid(m_pGoBackward);

    m_pRemoveAction->setEnabled(tableViewHasSelection());
    m_pRenameAction->setEnabled(tableViewHasSelection());

    m_pToolBar->addAction(m_pGoBackward);
    m_pToolBar->addAction(m_pGoForward);
    m_pToolBar->addAction(m_pGoUp);
    m_pToolBar->addSeparator();
    m_pToolBar->addAction(m_pRemoveAction);

    m_pToolBar->addAction(m_pCreateNewDirectoryAction);
    m_pToolBar->addAction(m_pRenameAction);
    m_pToolBar->addAction(m_pResetAction);

    enableForwardBackwardActions();
}

void UIVisoContentBrowser::prepareMainMenu(QMenu *pMenu)
{
    AssertReturnVoid(pMenu);

    pMenu->addAction(m_pRemoveAction);
    pMenu->addAction(m_pRenameAction);
    pMenu->addAction(m_pCreateNewDirectoryAction);
    pMenu->addAction(m_pResetAction);
}

const QString &UIVisoContentBrowser::importedISOPath() const
{
    return m_strImportedISOPath;
}

void UIVisoContentBrowser::setImportedISOPath(const QString &strPath /* = QString() */)
{
    if (m_strImportedISOPath == strPath)
        return;
    m_strImportedISOPath = strPath;
}

void UIVisoContentBrowser::prepareConnections()
{
    UIVisoBrowserBase::prepareConnections();

    if (m_pTableView)
    {
        connect(m_pTableView, &UIVisoContentTableView::doubleClicked,
                this, &UIVisoBrowserBase::sltTableViewItemDoubleClick);
        connect(m_pTableView, &UIVisoContentTableView::sigNewItemsDropped,
                this, &UIVisoContentBrowser::sltDroppedItems);
        connect(m_pTableView, &QTableView::customContextMenuRequested,
                this, &UIVisoContentBrowser::sltShowContextMenu);
    }

    if (m_pTableView->selectionModel())
        connect(m_pTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoContentBrowser::sltTableSelectionChanged);
    if (m_pModel)
        connect(m_pModel, &UICustomFileSystemModel::sigItemRenamed,
                this, &UIVisoContentBrowser::sltItemRenameAttempt);

    if (m_pCreateNewDirectoryAction)
        connect(m_pCreateNewDirectoryAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltCreateNewDirectory);
    if (m_pRemoveAction)
        connect(m_pRemoveAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltRemoveItems);
    if (m_pResetAction)
        connect(m_pResetAction, &QAction::triggered,
                this, &UIVisoContentBrowser::sltResetAction);
    if (m_pRenameAction)
        connect(m_pRenameAction, &QAction::triggered,
                this,&UIVisoContentBrowser::sltItemRenameAction);

    if (m_pGoUp)
        connect(m_pGoUp, &QAction::triggered, this, &UIVisoContentBrowser::sltGoUp);
    if (m_pGoForward)
        connect(m_pGoForward, &QAction::triggered, this, &UIVisoContentBrowser::sltGoForward);
    if (m_pGoBackward)
        connect(m_pGoBackward, &QAction::triggered, this, &UIVisoContentBrowser::sltGoBackward);
}

UICustomFileSystemItem* UIVisoContentBrowser::rootItem()
{
    if (!m_pModel)
        return 0;
    return m_pModel->rootItem();
}

UICustomFileSystemItem* UIVisoContentBrowser::startItem()
{
    UICustomFileSystemItem* pRoot = rootItem();

    if (!pRoot || pRoot->childCount() <= 0)
        return 0;
    return pRoot->child(0);
}

void UIVisoContentBrowser::initializeModel()
{
    if (m_pModel)
        m_pModel->reset();
    if (!rootItem())
        return;

    const QString startPath = QString("/%1").arg(m_strVisoName);

    UICustomFileSystemItem *pStartItem = new UICustomFileSystemItem(startPath, rootItem(), KFsObjType_Directory);
    pStartItem->setPath("/");
    pStartItem->setIsOpened(false);
}

void UIVisoContentBrowser::setTableRootIndex(QModelIndex index /* = QModelIndex */)
{
    if (!m_pTableView || !index.isValid())
        return;

    QModelIndex tableIndex;
    tableIndex = convertIndexToTableIndex(index);
    if (tableIndex.isValid())
        m_pTableView->setRootIndex(tableIndex);
    updateNavigationWidgetPath(currentPath());
    if (m_pGoUp)
        m_pGoUp->setEnabled(!onStartItem());
}

void UIVisoContentBrowser::setPathFromNavigationWidget(const QString &strPath)
{
    if (strPath == currentPath())
        return;
    UICustomFileSystemItem *pItem = searchItemByPath(strPath);

    if (pItem)
    {
        QModelIndex index = convertIndexToTableIndex(m_pModel->index(pItem));
        if (index.isValid())
            setTableRootIndex(index);
    }
}

UICustomFileSystemItem* UIVisoContentBrowser::searchItemByPath(const QString &strPath)
{
    UICustomFileSystemItem *pItem = startItem();
    QStringList path = UIPathOperations::pathTrail(strPath);

    foreach (const QString strName, path)
    {
        if (!pItem)
            return 0;
        pItem = pItem->child(strName);
    }
    return pItem;
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

bool UIVisoContentBrowser::tableViewHasSelection() const
{
    if (!m_pTableView)
        return false;
    QItemSelectionModel *pSelectionModel = m_pTableView->selectionModel();
    if (!pSelectionModel)
        return false;
    return pSelectionModel->hasSelection();
}

void UIVisoContentBrowser::parseVisoFileContent(const QString &strFileName)
{
    sltResetAction();
    QFile file(strFileName);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    if (file.size() > uAllowedFileSize)
        return;
    QTextStream stream(&file);
    QString strFileContent = stream.readAll();
    strFileContent.replace(' ', '\n');
    QStringList list;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    list = strFileContent.split("\n", Qt::SkipEmptyParts);
#else
    list = strFileContent.split("\n", QString::SkipEmptyParts);
#endif
    QMap<QString, QString> fileEntries;
    foreach (const QString &strPart, list)
    {
        /* We currently do not support different on-ISO names for different namespaces. */
        if (strPart.startsWith("/") && strPart.count('=') <= 1)
        {
            QStringList fileEntry;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            fileEntry = strPart.split("=", Qt::SkipEmptyParts);
#else
            fileEntry = strPart.split("=", QString::SkipEmptyParts);
#endif
            if (fileEntry.size() == 1)
            {
                QFileInfo fileInfo(fileEntry[0]);
                if (fileInfo.exists())
                {
                    QString isoName = QString("/%1").arg(fileInfo.fileName());
                    fileEntries[isoName] = fileEntry[0];
                }
            }
            else if (fileEntry.size() == 2)
            {
                if (QFileInfo(fileEntry[1]).exists())
                    fileEntries[fileEntry[0]] = fileEntry[1];
            }
        }
    }
    file.close();
}

QModelIndex UIVisoContentBrowser::convertIndexToTableIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return QModelIndex();

    if (index.model() == m_pTableProxyModel)
        return index;
    else if (index.model() == m_pModel)
        return m_pTableProxyModel->mapFromSource(index);
    return QModelIndex();
}

void UIVisoContentBrowser::scanHostDirectory(UICustomFileSystemItem *directoryItem)
{
    if (!directoryItem)
        return;
    /* the clicked item can be a directory created with the VISO content. in that case local path data
       should be empty: */
    if (directoryItem->type() != KFsObjType_Directory ||
        directoryItem->data(UICustomFileSystemModelData_LocalPath).toString().isEmpty())
        return;
    QDir directory(directoryItem->data(UICustomFileSystemModelData_LocalPath).toString());
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
            newItem->setData(fileInfo.filePath(), UICustomFileSystemModelData_LocalPath);

            newItem->setData(UIPathOperations::mergePaths(directoryItem->path(), fileInfo.fileName()),
                             UICustomFileSystemModelData_VISOPath);
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
    const QString strName(QDir::toNativeSeparators("/"));

    rootItem()->child(0)->setData(strName, UICustomFileSystemModelData_Name);
    /* If the table root index is the start item then we have to update the location selector text here: */
    // if (m_pTableProxyModel->mapToSource(m_pTableView->rootIndex()).internalPointer() == rootItem()->child(0))
    //     updateLocationSelectorText(strName);
    m_pTableProxyModel->invalidate();
}

void UIVisoContentBrowser::renameFileObject(UICustomFileSystemItem *pItem)
{
    m_pTableView->edit(m_pTableProxyModel->mapFromSource(m_pModel->index(pItem)));
}

void UIVisoContentBrowser::sltItemRenameAction()
{
    QList<UICustomFileSystemItem*> selectedItems = tableSelectedItems();
    if (selectedItems.empty())
        return;
    renameFileObject(selectedItems.at(0));
}

void UIVisoContentBrowser::sltItemRenameAttempt(UICustomFileSystemItem *pItem, QString strOldName, QString strNewName)
{
    if (!pItem || !pItem->parentItem())
        return;
    QList<UICustomFileSystemItem*> children = pItem->parentItem()->children();
    bool bDuplicate = false;
    foreach (const UICustomFileSystemItem *item, children)
    {
        if (item->fileObjectName() == strNewName && item != pItem)
            bDuplicate = true;
    }
    QString strNewPath = UIPathOperations::mergePaths(pItem->parentItem()->path(), pItem->fileObjectName());

    if (bDuplicate)
    {
        /* Restore the previous name in case the @strNewName is a duplicate: */
        pItem->setData(strOldName, static_cast<int>(UICustomFileSystemModelData_Name));
    }
    else
    {
        /* In case renaming is done (no dublicates) then modify the map that holds VISO items by:
           adding the renamed item, removing the old one (if it exists) and also add a :remove: to
           VISO file for the old path since in some cases, when remaned item is not top level, it still
           appears in ISO. So we remove it explicitly: */
        QString oldItemPath = pItem->data(UICustomFileSystemModelData_VISOPath).toString();
        m_entryMap.insert(strNewPath, pItem->data(UICustomFileSystemModelData_LocalPath).toString());
        m_entryMap.remove(oldItemPath);
        if (!pItem->data(UICustomFileSystemModelData_LocalPath).toString().isEmpty())
            m_entryMap.insert(oldItemPath, ":remove:");
    }

    pItem->setData(strNewPath, UICustomFileSystemModelData_VISOPath);

    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
}

void UIVisoContentBrowser::sltTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    emit sigTableSelectionChanged(selected.isEmpty());

    if (m_pRemoveAction)
        m_pRemoveAction->setEnabled(!selected.isEmpty());
    if (m_pRenameAction)
        m_pRenameAction->setEnabled(!selected.isEmpty());
}

void UIVisoContentBrowser::sltResetAction()
{
    if (!rootItem() || !rootItem()->child(0))
        return;
    rootItem()->child(0)->removeChildren();
    m_entryMap.clear();
    if (m_pTableProxyModel)
        m_pTableProxyModel->invalidate();
    m_strImportedISOPath.clear();
}

void UIVisoContentBrowser::sltDroppedItems(QStringList pathList)
{
    addObjectsToViso(pathList);
}

void UIVisoContentBrowser::sltShowContextMenu(const QPoint &point)
{
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    AssertReturnVoid(pSender);

    QMenu menu;

    menu.addAction(m_pRemoveAction);
    menu.addAction(m_pCreateNewDirectoryAction);
    menu.addAction(m_pResetAction);
    menu.exec(pSender->mapToGlobal(point));
}

void UIVisoContentBrowser::sltGoUp()
{
    goUp();
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

QString UIVisoContentBrowser::currentPath() const
{
    if (!m_pTableView || !m_pTableView->rootIndex().isValid())
        return QString();
    QModelIndex index = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
    UICustomFileSystemItem *pItem = static_cast<UICustomFileSystemItem*>((index).internalPointer());
    if (!pItem)
        return QString();
    return pItem->data(UICustomFileSystemModelData_VISOPath).toString();
}

bool UIVisoContentBrowser::onStartItem()
{
    if (!m_pTableView || !m_pModel)
        return false;
    QModelIndex index = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
    UICustomFileSystemItem *pItem = static_cast<UICustomFileSystemItem*>((index).internalPointer());
    if (!index.isValid() || !pItem)
        return false;
    if (pItem != startItem())
        return false;
    return true;
}

void UIVisoContentBrowser::goUp()
{
    AssertReturnVoid(m_pTableProxyModel);
    AssertReturnVoid(m_pTableView);
    QModelIndex currentRoot = m_pTableProxyModel->mapToSource(m_pTableView->rootIndex());
    if (!currentRoot.isValid())
        return;
    /* Go up if we are not already in root: */
    if (!onStartItem())
    {
        setTableRootIndex(currentRoot.parent());
    }
}
#include "UIVisoContentBrowser.moc"
