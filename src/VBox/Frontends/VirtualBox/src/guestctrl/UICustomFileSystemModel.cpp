/* $Id$ */
/** @file
 * VBox Qt GUI - UICustomFileSystemModel class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDateTime>
# include <QHeaderView>

/* GUI includes: */
# include "UIErrorString.h"
# include "UICustomFileSystemModel.h"
# include "UIFileManagerTable.h"
# include "UIFileManager.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

const char* UICustomFileSystemModel::strUpDirectoryString = "..";


/*********************************************************************************************************************************
*   UIFileTableItem implementation.                                                                                              *
*********************************************************************************************************************************/

UIFileTableItem::UIFileTableItem(const QVector<QVariant> &data,
                                 UIFileTableItem *parent, KFsObjType type)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_bIsOpened(false)
    , m_isTargetADirectory(false)
    , m_type(type)
    , m_isDriveItem(false)
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
    m_childMap.insert(item->name(), item);
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

QString UIFileTableItem::name() const
{
    if (m_itemData.isEmpty() || !m_itemData[0].canConvert(QMetaType::QString))
        return QString();
    return m_itemData[0].toString();
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
    return m_type == KFsObjType_Directory;
}

bool UIFileTableItem::isSymLink() const
{
    return m_type == KFsObjType_Symlink;
}

bool UIFileTableItem::isFile() const
{
    return m_type == KFsObjType_File;
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
    if (data(0) == UICustomFileSystemModel::strUpDirectoryString)
        return true;
    return false;
}

KFsObjType UIFileTableItem::type() const
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

bool UIFileTableItem::isSymLinkToADirectory() const
{
    return m_isTargetADirectory;
}

void UIFileTableItem::setIsSymLinkToADirectory(bool flag)
{
    m_isTargetADirectory = flag;
}

bool UIFileTableItem::isSymLinkToAFile() const
{
    return isSymLink() && !m_isTargetADirectory;
}

void UIFileTableItem::setIsDriveItem(bool flag)
{
    m_isDriveItem = flag;
}

bool UIFileTableItem::isDriveItem() const
{
    return m_isDriveItem;
}

UICustomFileSystemProxyModel::UICustomFileSystemProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
    , m_fListDirectoriesOnTop(false)
{
}

void UICustomFileSystemProxyModel::setListDirectoriesOnTop(bool fListDirectoriesOnTop)
{
    m_fListDirectoriesOnTop = fListDirectoriesOnTop;
}

/*********************************************************************************************************************************
*   UICustomFileSystemProxyModel implementation.                                                                                  *
*********************************************************************************************************************************/

bool UICustomFileSystemProxyModel::listDirectoriesOnTop() const
{
    return m_fListDirectoriesOnTop;
}

bool UICustomFileSystemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    UIFileTableItem *pLeftItem = static_cast<UIFileTableItem*>(left.internalPointer());
    UIFileTableItem *pRightItem = static_cast<UIFileTableItem*>(right.internalPointer());

    if (pLeftItem && pRightItem)
    {
        /* List the directories before the files if options say so: */
        if (m_fListDirectoriesOnTop)
        {
            if ((pLeftItem->isDirectory() || pLeftItem->isSymLinkToADirectory()) && !pRightItem->isDirectory())
                return (sortOrder() == Qt::AscendingOrder);
            if ((pRightItem->isDirectory() || pRightItem->isSymLinkToADirectory()) && !pLeftItem->isDirectory())
                return (sortOrder() == Qt::DescendingOrder);
        }
        /* Up directory item should be always the first item: */
        if (pLeftItem->isUpDirectory())
            return (sortOrder() == Qt::AscendingOrder);
        else if (pRightItem->isUpDirectory())
            return (sortOrder() == Qt::DescendingOrder);

        /* If the sort column is QDateTime than handle it correctly: */
        if (sortColumn() == UICustomFileSystemModelColumn_ChangeTime)
        {
            QVariant dataLeft = pLeftItem->data(UICustomFileSystemModelColumn_ChangeTime);
            QVariant dataRight = pRightItem->data(UICustomFileSystemModelColumn_ChangeTime);
            QDateTime leftDateTime = dataLeft.toDateTime();
            QDateTime rightDateTime = dataRight.toDateTime();
            return (leftDateTime < rightDateTime);
        }
        /* When we show human readble sizes in size column sorting gets confused, so do it here: */
        else if(sortColumn() == UICustomFileSystemModelColumn_Size)
        {
            qulonglong leftSize = pLeftItem->data(UICustomFileSystemModelColumn_Size).toULongLong();
            qulonglong rightSize = pRightItem->data(UICustomFileSystemModelColumn_Size).toULongLong();
            return (leftSize < rightSize);

        }
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

UICustomFileSystemModel::UICustomFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_pParent(qobject_cast<UIFileManagerTable*>(parent))
    , m_fShowHumanReadableSizes(false)
{
}

UIFileTableItem* UICustomFileSystemModel::rootItem() const
{
    if (!m_pParent)
        return 0;
    return m_pParent->m_pRootItem;
}

UICustomFileSystemModel::~UICustomFileSystemModel()
{}

int UICustomFileSystemModel::columnCount(const QModelIndex &parent) const
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

bool UICustomFileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole)
    {
        if (index.column() == 0 && value.canConvert(QMetaType::QString))
        {
            UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
            if (!item || !m_pParent)
                return false;
            if (m_pParent->renameItem(item, value.toString()))
            {
                item->setData(value, index.column());
                emit dataChanged(index, index);
            }
            else
            {
                if (m_pParent)
                    m_pParent->emitLogOutput(QString(item->path()).append(" could not be renamed"), FileManagerLogType_Error);
            }
            return true;
        }
    }
    return false;
}

QVariant UICustomFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        /* dont show anything but the name for up directories: */
        if (item->isUpDirectory() && index.column() != UICustomFileSystemModelColumn_Name)
            return QVariant();
        /* Format date/time column: */
        if (item->data(index.column()).canConvert(QMetaType::QDateTime))
        {
            QDateTime dateTime = item->data(index.column()).toDateTime();
            if (dateTime.isValid())
                return dateTime.toString("dd.MM.yyyy hh:mm:ss");
        }
        /* Decide whether to show human-readable file object sizes: */
        if (index.column() == UICustomFileSystemModelColumn_Size)
        {
            if (m_fShowHumanReadableSizes)
            {
                qulonglong size = item->data(index.column()).toULongLong();
                return vboxGlobal().formatSize(size);
            }
            else
                return item->data(index.column());
        }
        return item->data(index.column());
    }
    /* Show the up directory array: */
    if (role == Qt::DecorationRole && index.column() == 0)
    {
        if (item->isDirectory())
        {
            if (item->isUpDirectory())
                return QIcon(":/arrow_up_10px_x2.png");
            else if(item->isDriveItem())
                return QIcon(":/hd_32px.png");
            else
                return QIcon(":/file_manager_folder_16px.png");
        }
        else if (item->isFile())
            return QIcon(":/file_manager_file_16px.png");
        else if (item->isSymLink())
        {
            if (item->isSymLinkToADirectory())
                return QIcon(":/file_manager_folder_symlink_16px.png");
            else
                return QIcon(":/file_manager_file_symlink_16px.png");
        }
    }

    return QVariant();
}

Qt::ItemFlags UICustomFileSystemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return QAbstractItemModel::flags(index);

    if (!item->isUpDirectory() && index.column() == 0)
        return QAbstractItemModel::flags(index)  | Qt::ItemIsEditable;
    return QAbstractItemModel::flags(index);
}

QVariant UICustomFileSystemModel::headerData(int section, Qt::Orientation orientation,
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

QModelIndex UICustomFileSystemModel::index(UIFileTableItem* item)
{
    if (!item)
        return QModelIndex();
    return createIndex(item->row(), 0, item);
}

QModelIndex UICustomFileSystemModel::index(int row, int column, const QModelIndex &parent)
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


QModelIndex UICustomFileSystemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    UIFileTableItem *childItem = static_cast<UIFileTableItem*>(index.internalPointer());
    UIFileTableItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem())
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int UICustomFileSystemModel::rowCount(const QModelIndex &parent) const
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

void UICustomFileSystemModel::signalUpdate()
{
    emit layoutChanged();
}

QModelIndex UICustomFileSystemModel::rootIndex() const
{
    if (!rootItem())
        return QModelIndex();
    return createIndex(rootItem()->child(0)->row(), 0,
                       rootItem()->child(0));
}

void UICustomFileSystemModel::beginReset()
{
    beginResetModel();
}

void UICustomFileSystemModel::endReset()
{
    endResetModel();
}

void UICustomFileSystemModel::setShowHumanReadableSizes(bool fShowHumanReadableSizes)
{
    m_fShowHumanReadableSizes = fShowHumanReadableSizes;
}

bool UICustomFileSystemModel::showHumanReadableSizes() const
{
    return m_fShowHumanReadableSizes;
}
