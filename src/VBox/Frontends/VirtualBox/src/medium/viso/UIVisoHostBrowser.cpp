/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoHostBrowser class implementation.
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
#include <QFileSystemModel>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemDelegate>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QTableView>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIActionPool.h"
#include "UIVisoHostBrowser.h"

/* Other VBox includes: */
#include <iprt/assert.h>



/*********************************************************************************************************************************
*   UIHostBrowserDelegate definition.                                                                                            *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIHostBrowserDelegate : public QItemDelegate
{

    Q_OBJECT;

public:

    UIHostBrowserDelegate(QObject *pParent)
        : QItemDelegate(pParent){}

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};

/*********************************************************************************************************************************
*   UIVisoHostBrowserModel definition.                                                                                   *
*********************************************************************************************************************************/

class UIVisoHostBrowserModel : public QFileSystemModel
{
    Q_OBJECT;

public:

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const RT_OVERRIDE;
    UIVisoHostBrowserModel(QObject *pParent);

    virtual QStringList mimeTypes() const RT_OVERRIDE;
    /** Prepares the mime data  as a list of text consisting of dragged objects full file path. */
    QMimeData *mimeData(const QModelIndexList &indexes) const RT_OVERRIDE;

protected:

private:

};

/*********************************************************************************************************************************
*   UIHostBrowserProxyModel definition.                                                                                   *
*********************************************************************************************************************************/

class SHARED_LIBRARY_STUFF UIHostBrowserProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIHostBrowserProxyModel(QObject *pParent = 0)
        : QSortFilterProxyModel(pParent)
    {
    }

protected:

    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const RT_OVERRIDE;

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

        if(info.isFile())
        {
            if(info.isSymLink())
                return QIcon(":/file_manager_file_symlink_16px.png");
            else
                return QIcon(":/file_manager_file_16px.png");
        }
        else if(info.isDir())
        {
            if (filePath(index).contains(".."))
                return QIcon(":/arrow_up_10px_x2.png");
            /** A bad hack to detect drive roots and use HD icon. On Windows 10 QFileInfo()::isRoot()
             * and QDir()::isRoot() return true only for C:/ : */
#ifdef VBOX_WS_WIN
            else if (info.absoluteFilePath().length() <= 3)
                return QIcon(":/hd_32px.png");
#endif
            else if(info.isSymLink())
                return QIcon(":/file_manager_folder_symlink_16px.png");
            else
                return QIcon(":/file_manager_folder_16px.png");
        }
    }
    return QFileSystemModel::data(index, enmRole);
}

QStringList UIVisoHostBrowserModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd.text.list";
    return types;
}

QMimeData *UIVisoHostBrowserModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        if (index.isValid() && index.column() == 0)
        {
            QString strPath = fileInfo(index).filePath();
            if (!strPath.contains(".."))
                stream << fileInfo(index).filePath();
        }
    }

    mimeData->setData("application/vnd.text.list", encodedData);
    return mimeData;
}


/*********************************************************************************************************************************
*   UIHostBrowserProxyModel implementation.                                                                                      *
*********************************************************************************************************************************/

bool UIHostBrowserProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    UIVisoHostBrowserModel *pModel = qobject_cast<UIVisoHostBrowserModel*>(sourceModel());
    AssertReturn(pModel, true);

    if (pModel->filePath(left).contains(".."))
        return (sortOrder() == Qt::AscendingOrder);

    if (pModel->filePath(right).contains(".."))
        return (sortOrder() == Qt::DescendingOrder);

    QFileInfo leftInfo(pModel->fileInfo(left));
    QFileInfo rightInfo(pModel->fileInfo(right));

    if (leftInfo.isDir() && !rightInfo.isDir())
        return (sortOrder() == Qt::AscendingOrder);

    if (!leftInfo.isDir() && rightInfo.isDir())
        return (sortOrder() == Qt::DescendingOrder);

    return QSortFilterProxyModel::lessThan(left, right);
}


/*********************************************************************************************************************************
*   UIVisoHostBrowser implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoHostBrowser::UIVisoHostBrowser(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : UIVisoBrowserBase(pActionPool, pParent)
    , m_pModel(0)
    , m_pTableView(0)
    , m_pProxyModel(0)
{
    prepareObjects();
    prepareToolBar();
    prepareConnections();
}

UIVisoHostBrowser::~UIVisoHostBrowser()
{
}

void UIVisoHostBrowser::retranslateUi()
{
    setFileTableLabelText(QApplication::translate("UIVisoCreatorWidget","Host System"));
    if (m_pSubMenu)
        m_pSubMenu->setTitle(QApplication::translate("UIVisoCreatorWidget", "VISO Browser"));
}

void UIVisoHostBrowser::prepareObjects()
{
    UIVisoBrowserBase::prepareObjects();

    m_pModel = new UIVisoHostBrowserModel(this);
    AssertReturnVoid(m_pModel);
    m_pModel->setRootPath(QDir::rootPath());
    m_pModel->setReadOnly(true);
    m_pModel->setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System);

    m_pProxyModel = new UIHostBrowserProxyModel(this);
    AssertReturnVoid(m_pProxyModel);
    m_pProxyModel->setSourceModel(m_pModel);
    m_pProxyModel->setDynamicSortFilter(true);


    m_pTableView = new QTableView;
    AssertReturnVoid(m_pTableView);
    m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_pMainLayout->addWidget(m_pTableView, 2, 0, 8, 4);
    m_pTableView->setShowGrid(false);
    m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTableView->setAlternatingRowColors(true);
    m_pTableView->setTabKeyNavigation(false);
    m_pTableView->setItemDelegate(new UIHostBrowserDelegate(this));

    QHeaderView *pVerticalHeader = m_pTableView->verticalHeader();
    QHeaderView *pHorizontalHeader = m_pTableView->horizontalHeader();
    AssertReturnVoid(pVerticalHeader);
    AssertReturnVoid(pHorizontalHeader);

    m_pTableView->verticalHeader()->setVisible(false);
    /* Minimize the row height: */
    m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
    pHorizontalHeader->setHighlightSections(false);
    pHorizontalHeader->setSectionResizeMode(QHeaderView::Stretch);

    m_pTableView->setModel(m_pProxyModel);
    m_pTableView->setSortingEnabled(true);
    m_pTableView->sortByColumn(0, Qt::AscendingOrder);

    /* Hide the "type" column: */
    m_pTableView->hideColumn(2);

    m_pTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_pTableView->setDragDropMode(QAbstractItemView::DragOnly);

    retranslateUi();
}

void UIVisoHostBrowser::prepareConnections()
{
    UIVisoBrowserBase::prepareConnections();

    if (m_pTableView)
    {
        connect(m_pTableView, &QTableView::doubleClicked,
                this, &UIVisoBrowserBase::sltTableViewItemDoubleClick);
        connect(m_pTableView, &QTableView::customContextMenuRequested,
                this, &UIVisoHostBrowser::sltShowContextMenu);
    }

    if (m_pTableView->selectionModel())
        connect(m_pTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoHostBrowser::sltTableSelectionChanged);

    if (m_pGoHome)
        connect(m_pGoHome, &QAction::triggered, this, &UIVisoHostBrowser::sltGoHome);
    if (m_pGoUp)
        connect(m_pGoUp, &QAction::triggered, this, &UIVisoHostBrowser::sltGoUp);
    if (m_pGoForward)
        connect(m_pGoForward, &QAction::triggered, this, &UIVisoHostBrowser::sltGoForward);
    if (m_pGoBackward)
        connect(m_pGoBackward, &QAction::triggered, this, &UIVisoHostBrowser::sltGoBackward);
}

void UIVisoHostBrowser::prepareToolBar()
{
    m_pGoHome = m_pActionPool->action(UIActionIndex_M_VISOCreator_Host_GoHome);
    m_pGoUp = m_pActionPool->action(UIActionIndex_M_VISOCreator_Host_GoUp);
    m_pGoForward = m_pActionPool->action(UIActionIndex_M_VISOCreator_Host_GoForward);
    m_pGoBackward = m_pActionPool->action(UIActionIndex_M_VISOCreator_Host_GoBackward);

    AssertReturnVoid(m_pGoHome);
    AssertReturnVoid(m_pGoUp);
    AssertReturnVoid(m_pGoForward);
    AssertReturnVoid(m_pGoBackward);

    m_pToolBar->addAction(m_pGoBackward);
    m_pToolBar->addAction(m_pGoForward);
    m_pToolBar->addAction(m_pGoUp);
    m_pToolBar->addAction(m_pGoHome);
    //m_pToolBar->addSeparator();

    if (m_pGoUp)
        m_pGoUp->setEnabled(!isRoot());
    enableForwardBackwardActions();
}

void UIVisoHostBrowser::prepareMainMenu(QMenu *pMenu)
{
    AssertReturnVoid(pMenu);
    QMenu *pSubMenu = new QMenu(QApplication::translate("UIVisoCreatorWidget", "Host Browser"), pMenu);
    pMenu->addMenu(pSubMenu);
    AssertReturnVoid(pSubMenu);
    m_pSubMenu = pSubMenu;

    m_pSubMenu->addAction(m_pGoBackward);
    m_pSubMenu->addAction(m_pGoForward);
    m_pSubMenu->addAction(m_pGoUp);
    m_pSubMenu->addAction(m_pGoHome);
}

void UIVisoHostBrowser::sltTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    Q_UNUSED(selected);
    emit sigTableSelectionChanged(selectedPathList());
}

void UIVisoHostBrowser::sltShowContextMenu(const QPoint &point)
{
    Q_UNUSED(point);
    if (!sender())
        return;

    // QMenu menu;

    // if (sender() == m_pHostBrowser)
    //     menu.addAction(m_pAddAction);
    // }
    // else if (sender() == m_pVISOContentBrowser)
    // {
    //     menu.addAction(m_pRemoveAction);
    //     menu.addAction(m_pCreateNewDirectoryAction);
    //     menu.addAction(m_pResetAction);
    // }

    // menu.exec(pContextMenuRequester->mapToGlobal(point));
}

void UIVisoHostBrowser::sltGoHome()
{
    AssertReturnVoid(m_pModel);
    AssertReturnVoid(m_pProxyModel);
    QModelIndex homeIndex = m_pProxyModel->mapFromSource(m_pModel->index(QDir::homePath()));
    if (homeIndex.isValid())
        setTableRootIndex(homeIndex);
}

void UIVisoHostBrowser::sltGoUp()
{
    AssertReturnVoid(m_pModel);
    QDir currentDir = QDir(currentPath());
    currentDir.cdUp();
    QModelIndex upIndex = m_pProxyModel->mapFromSource(m_pModel->index(currentDir.absolutePath()));
    if (upIndex.isValid())
        setTableRootIndex(upIndex);
}

void UIVisoHostBrowser::tableViewItemDoubleClick(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    /* QFileInfo::isDir() returns true if QFileInfo is a folder or a symlink to folder: */
    QFileInfo fileInfo = m_pModel->fileInfo(m_pProxyModel->mapToSource(index));
    if (!fileInfo.isDir())
        return;
    if (QString::compare(fileInfo.fileName(), "..") == 0)
        setTableRootIndex(m_pProxyModel->parent(m_pTableView->rootIndex()));
    else
        setTableRootIndex(index);
}

void UIVisoHostBrowser::showHideHiddenObjects(bool bShow)
{
    if (bShow)
        m_pModel->setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System);
    else
        m_pModel->setFilter(QDir::AllEntries | QDir::NoDot);
}

QString UIVisoHostBrowser::currentPath() const
{
    if (!m_pTableView || !m_pModel)
        return QString();
    QModelIndex currentTableIndex = m_pTableView->rootIndex();
    return QDir::fromNativeSeparators(m_pModel->filePath(m_pProxyModel->mapToSource(currentTableIndex)));
}

void UIVisoHostBrowser::setCurrentPath(const QString &strPath)
{
    if (strPath.isEmpty() || !m_pModel)
        return;
    QModelIndex index = m_pProxyModel->mapFromSource(m_pModel->index(strPath));
    setTableRootIndex(index);
}

bool UIVisoHostBrowser::tableViewHasSelection() const
{
    if (!m_pTableView)
        return false;
    QItemSelectionModel *pSelectionModel = m_pTableView->selectionModel();
    if (!pSelectionModel)
        return false;
    return pSelectionModel->hasSelection();
}

void UIVisoHostBrowser::sltAddAction()
{
    if (!m_pTableView || !m_pModel)
        return;
    emit sigAddObjectsToViso(selectedPathList());
}

QStringList UIVisoHostBrowser::selectedPathList() const
{
    QItemSelectionModel *pSelectionModel = m_pTableView->selectionModel();
    if (!pSelectionModel)
        return QStringList();
    QModelIndexList selectedIndices = pSelectionModel->selectedRows(0);
    QStringList pathList;
    for (int i = 0; i < selectedIndices.size(); ++i)
    {
        QString strPath = m_pModel->filePath(m_pProxyModel->mapToSource(selectedIndices[i]));
        if (strPath.contains(".."))
            continue;
        pathList << strPath;
    }
    return pathList;
}

void UIVisoHostBrowser::setTableRootIndex(QModelIndex index /* = QModelIndex */)
{
    AssertReturnVoid(m_pTableView);
    AssertReturnVoid(m_pModel);
    AssertReturnVoid(m_pProxyModel);

    m_pTableView->setRootIndex(index);
    m_pTableView->clearSelection();
    QString strPath = m_pModel->filePath(m_pProxyModel->mapToSource(index));
    updateNavigationWidgetPath(strPath);
    if (m_pGoUp)
        m_pGoUp->setEnabled(!isRoot());

    m_pTableView->sortByColumn(m_pProxyModel->sortColumn(), m_pProxyModel->sortOrder());
}

void UIVisoHostBrowser::setPathFromNavigationWidget(const QString &strPath)
{
    if (!m_pModel)
        return;
    QModelIndex index = m_pProxyModel->mapFromSource(m_pModel->index(strPath));
    if (!index.isValid())
        return;
    setTableRootIndex(index);
}

QModelIndex UIVisoHostBrowser::currentRootIndex() const
{
    if (!m_pTableView)
        return QModelIndex();
    return m_pTableView->rootIndex();
}

bool UIVisoHostBrowser::isRoot() const
{
    return QDir(currentPath()).isRoot();
}

#include "UIVisoHostBrowser.moc"
