/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoHostBrowser class implementation.
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
#include <QDir>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QTableView>
#include <QTreeView>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIToolBar.h"
#include "UIVisoHostBrowser.h"


/*********************************************************************************************************************************
*   UIVisoHostBrowserModel definition.                                                                                   *
*********************************************************************************************************************************/

class UIVisoHostBrowserModel : public QFileSystemModel
{
    Q_OBJECT;

public:
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const /* override */;
    UIVisoHostBrowserModel(QObject *pParent);

protected:

private:

};

/*********************************************************************************************************************************
*   UIVisoHostBrowserModel implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoHostBrowserModel::UIVisoHostBrowserModel(QObject *pParent /* = 0 */)
    :QFileSystemModel(pParent)
{
}

QVariant UIVisoHostBrowserModel::data(const QModelIndex &index, int enmRole /* = Qt::DisplayRole */) const
{
    if (enmRole == Qt::DecorationRole && index.column() == 0)
    {
        QFileInfo info = fileInfo(index);

        if(info.isSymLink() && info.isDir())
            return QIcon(":/file_manager_folder_symlink_16px.png");
        else if(info.isSymLink() && info.isFile())
            return QIcon(":/file_manager_file_symlink_16px.png");
        else if(info.isFile())
            return QIcon(":/file_manager_file_16px.png");
        else if(info.isDir())
            return QIcon(":/file_manager_folder_16px.png");
    }
    return QFileSystemModel::data(index, enmRole);
}

/*********************************************************************************************************************************
*   UIVisoHostBrowser implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoHostBrowser::UIVisoHostBrowser(QWidget *pParent)
    : QIWithRetranslateUI<UIVisoBrowserBase>(pParent)
    , m_pTreeModel(0)
    , m_pTableModel(0)
    , m_pAddAction(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoHostBrowser::~UIVisoHostBrowser()
{
}

void UIVisoHostBrowser::retranslateUi()
{
    if (m_pTitleLabel)
        m_pTitleLabel->setText(QApplication::translate("UIVisoCreator", "Host file system"));
    if (m_pAddAction)
        m_pAddAction->setToolTip(QApplication::translate("UIVisoCreator", "Add selected file objects to ISO"));
}

void UIVisoHostBrowser::prepareObjects()
{
    UIVisoBrowserBase::prepareObjects();

    m_pTreeModel = new UIVisoHostBrowserModel(this);
    m_pTreeModel->setRootPath(QDir::rootPath());
    m_pTreeModel->setReadOnly(true);
    m_pTreeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    m_pTableModel = new UIVisoHostBrowserModel(this);
    m_pTableModel->setRootPath(QDir::rootPath());
    m_pTableModel->setReadOnly(true);
    m_pTableModel->setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System);

    if (m_pTreeView)
    {
        m_pTreeView->setModel(m_pTreeModel);
        m_pTreeView->setRootIndex(m_pTreeModel->index(m_pTreeModel->rootPath()).parent());
        m_pTreeView->setCurrentIndex(m_pTreeModel->index(QDir::homePath()));
        /* Show only the 0th column that is "name': */
        m_pTreeView->hideColumn(1);
        m_pTreeView->hideColumn(2);
        m_pTreeView->hideColumn(3);
    }

    if (m_pTableView)
    {
        m_pTableView->setModel(m_pTableModel);
        setTableRootIndex();
        /* Hide the "type" column: */
        m_pTableView->hideColumn(2);
    }

    m_pAddAction = new QAction(this);
    if (m_pAddAction)
    {
        m_pVerticalToolBar->addAction(m_pAddAction);
        m_pAddAction->setIcon(UIIconPool::iconSetFull(":/file_manager_copy_to_guest_24px.png",
                                                      ":/file_manager_copy_to_guest_16px.png",
                                                      ":/file_manager_copy_to_guest_disabled_24px.png",
                                                      ":/file_manager_copy_to_guest_disabled_16px.png"));

        m_pAddAction->setEnabled(false);
    }

    retranslateUi();
}

void UIVisoHostBrowser::prepareConnections()
{
    UIVisoBrowserBase::prepareConnections();
    if (m_pTableView->selectionModel())
        connect(m_pTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoHostBrowser::sltHandleTableSelectionChanged);
    if (m_pAddAction)
        connect(m_pAddAction, &QAction::triggered,
                this, &UIVisoHostBrowser::sltHandleAddAction);
}

void UIVisoHostBrowser::sltHandleTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    if (m_pAddAction)
        m_pAddAction->setEnabled(!selected.isEmpty());
}

void UIVisoHostBrowser::tableViewItemDoubleClick(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    QFileInfo fileInfo = m_pTableModel->fileInfo(index);
    /* QFileInfo::isDir() returns true if QFileInfo is a folder or a symlink to folder: */
    if (!fileInfo.isDir())
        return;
    setTableRootIndex(index);

    m_pTreeView->blockSignals(true);
    setTreeCurrentIndex(index);
    m_pTreeView->blockSignals(false);
}

void UIVisoHostBrowser::treeSelectionChanged(const QModelIndex &selectedTreeIndex)
{
    setTableRootIndex(selectedTreeIndex);
}

void UIVisoHostBrowser::showHideHiddenObjects(bool bShow)
{
    if (bShow)
    {
        m_pTreeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
        m_pTableModel->setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System);
    }
    else
    {
        m_pTreeModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
        m_pTableModel->setFilter(QDir::AllEntries | QDir::NoDot);
    }
}

void UIVisoHostBrowser::sltHandleAddAction()
{
    if (!m_pTableView || !m_pTableModel)
        return;
    QItemSelectionModel *pSelectionModel = m_pTableView->selectionModel();
    if (!pSelectionModel)
        return;
    QModelIndexList selectedIndices = pSelectionModel->selectedRows(0);
    QStringList pathList;
    for (int i = 0; i < selectedIndices.size(); ++i)
    {
        QString strPath = m_pTableModel->filePath(selectedIndices[i]);
        if (strPath.contains(".."))
            continue;
        pathList << strPath;
    }
    emit sigAddObjectsToViso(pathList);
}

void UIVisoHostBrowser::setTableRootIndex(QModelIndex index /* = QModelIndex */)
{
    if (!m_pTreeView || !m_pTreeView->selectionModel() || !m_pTableView)
        return;
    QString strCurrentTreePath;
    if (!index.isValid())
    {
        QModelIndex currentTreeIndex = m_pTreeView->selectionModel()->currentIndex();
        strCurrentTreePath = m_pTreeModel->filePath(currentTreeIndex);
    }
    else
        strCurrentTreePath = m_pTreeModel->filePath(index);
    if (!strCurrentTreePath.isEmpty())
        m_pTableView->setRootIndex(m_pTableModel->index(strCurrentTreePath));
}

void UIVisoHostBrowser::setTreeCurrentIndex(QModelIndex index /* = QModelIndex() */)
{
    QString strCurrentTablePath;
    if (!index.isValid())
    {
        QModelIndex currentTableIndex = m_pTableView->selectionModel()->currentIndex();
        strCurrentTablePath = m_pTableModel->filePath(currentTableIndex);
    }
    else
        strCurrentTablePath = m_pTableModel->filePath(index);
    m_pTreeView->setCurrentIndex(m_pTreeModel->index(strCurrentTablePath));
}


#include "UIVisoHostBrowser.moc"
