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
#include <QTextEdit>
#include <QTableView>

/* GUI includes: */
#include "UIVisoHostBrowser.h"


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
*   UIVisoHostBrowser implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoHostBrowser::UIVisoHostBrowser(QWidget *pParent /* = 0 */)
    : UIVisoBrowserBase(pParent)
    , m_pModel(0)
    , m_pTableView(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoHostBrowser::~UIVisoHostBrowser()
{
}

void UIVisoHostBrowser::retranslateUi()
{
}

void UIVisoHostBrowser::prepareObjects()
{
    UIVisoBrowserBase::prepareObjects();

    m_pModel = new UIVisoHostBrowserModel(this);
    m_pModel->setRootPath(QDir::rootPath());
    m_pModel->setReadOnly(true);
    m_pModel->setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden | QDir::System);

    m_pTableView = new QTableView;
    if (m_pTableView)
    {
        m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pMainLayout->addWidget(m_pTableView, 1, 0, 8, 4);
        m_pTableView->setShowGrid(false);
        m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setTabKeyNavigation(false);
        m_pTableView->setItemDelegate(new UIHostBrowserDelegate(this));
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

        m_pTableView->setModel(m_pModel);
        //setTableRootIndex();
        /* Hide the "type" column: */
        m_pTableView->hideColumn(2);

        m_pTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_pTableView->setDragDropMode(QAbstractItemView::DragOnly);
    }

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
                this, &UIVisoHostBrowser::sltFileTableViewContextMenu);
    }

    if (m_pTableView->selectionModel())
        connect(m_pTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoHostBrowser::sltTableSelectionChanged);
}

void UIVisoHostBrowser::sltTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    Q_UNUSED(selected);
    emit sigTableSelectionChanged(selectedPathList());
}

void UIVisoHostBrowser::tableViewItemDoubleClick(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    /* QFileInfo::isDir() returns true if QFileInfo is a folder or a symlink to folder: */
    QFileInfo fileInfo = m_pModel->fileInfo(index);
    if (!fileInfo.isDir())
        return;
    if (QString::compare(fileInfo.fileName(), "..") == 0)
        setTableRootIndex(m_pModel->parent(m_pTableView->rootIndex()));
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
    QModelIndex currentTableIndex = m_pTableView->selectionModel()->currentIndex();
    return QDir::fromNativeSeparators(m_pModel->filePath(currentTableIndex));
}

void UIVisoHostBrowser::setCurrentPath(const QString &strPath)
{
    if (strPath.isEmpty() || !m_pModel)
        return;
    QModelIndex index = m_pModel->index(strPath);
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
        QString strPath = m_pModel->filePath(selectedIndices[i]);
        if (strPath.contains(".."))
            continue;
        pathList << strPath;
    }
    return pathList;
}

void UIVisoHostBrowser::setTableRootIndex(QModelIndex index /* = QModelIndex */)
{
    if (!m_pTableView || !m_pModel)
        return;
    m_pTableView->setRootIndex(index);
    m_pTableView->clearSelection();
    updateNavigationWidgetPath(m_pModel->filePath(index));
}

void UIVisoHostBrowser::setPathFromNavigationWidget(const QString &strPath)
{
    if (!m_pModel)
        return;
    QModelIndex index = m_pModel->index(strPath);
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
#include "UIVisoHostBrowser.moc"
