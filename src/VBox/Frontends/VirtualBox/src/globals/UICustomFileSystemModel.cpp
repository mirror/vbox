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
# include "UIPathOperations.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

const char* UICustomFileSystemModel::strUpDirectoryString = "..";


/*********************************************************************************************************************************
*   UICustomFileSystemItem implementation.                                                                                       *
*********************************************************************************************************************************/

UICustomFileSystemItem::UICustomFileSystemItem(const QVector<QVariant> &data,
                                 UICustomFileSystemItem *parent, KFsObjType type)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_bIsOpened(false)
    , m_isTargetADirectory(false)
    , m_type(type)
    , m_isDriveItem(false)
{
}

UICustomFileSystemItem::~UICustomFileSystemItem()
{
    reset();
}

void UICustomFileSystemItem::appendChild(UICustomFileSystemItem *item)
{
    if (!item)
        return;
    m_childItems.append(item);
    m_childMap.insert(item->name(), item);
}

void UICustomFileSystemItem::reset()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_childMap.clear();
    m_bIsOpened = false;
}

UICustomFileSystemItem *UICustomFileSystemItem::child(int row) const
{
    return m_childItems.value(row);
}

UICustomFileSystemItem *UICustomFileSystemItem::child(const QString &path) const
{
    if (!m_childMap.contains(path))
        return 0;
    return m_childMap.value(path);
}

int UICustomFileSystemItem::childCount() const
{
    return m_childItems.count();
}

int UICustomFileSystemItem::columnCount() const
{
    return m_itemData.count();
}

QVariant UICustomFileSystemItem::data(int column) const
{
    return m_itemData.value(column);
}

QString UICustomFileSystemItem::name() const
{
    if (m_itemData.isEmpty() || !m_itemData[0].canConvert(QMetaType::QString))
        return QString();
    return m_itemData[0].toString();
}

void UICustomFileSystemItem::setData(const QVariant &data, int index)
{
    if (index >= m_itemData.length())
        return;
    m_itemData[index] = data;
}

UICustomFileSystemItem *UICustomFileSystemItem::parentItem()
{
    return m_parentItem;
}

int UICustomFileSystemItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<UICustomFileSystemItem*>(this));
    return 0;
}

bool UICustomFileSystemItem::isDirectory() const
{
    return m_type == KFsObjType_Directory;
}

bool UICustomFileSystemItem::isSymLink() const
{
    return m_type == KFsObjType_Symlink;
}

bool UICustomFileSystemItem::isFile() const
{
    return m_type == KFsObjType_File;
}

void UICustomFileSystemItem::clearChildren()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_childMap.clear();
}

bool UICustomFileSystemItem::isOpened() const
{
    return m_bIsOpened;
}

void UICustomFileSystemItem::setIsOpened(bool flag)
{
    m_bIsOpened = flag;
}

const QString  &UICustomFileSystemItem::path() const
{
    return m_strPath;
}

void UICustomFileSystemItem::setPath(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return;
    m_strPath = path;
    UIPathOperations::removeTrailingDelimiters(m_strPath);
}

bool UICustomFileSystemItem::isUpDirectory() const
{
    if (!isDirectory())
        return false;
    if (data(0) == UICustomFileSystemModel::strUpDirectoryString)
        return true;
    return false;
}

KFsObjType UICustomFileSystemItem::type() const
{
    return m_type;
}

const QString &UICustomFileSystemItem::targetPath() const
{
    return m_strTargetPath;
}

void UICustomFileSystemItem::setTargetPath(const QString &path)
{
    m_strTargetPath = path;
}

bool UICustomFileSystemItem::isSymLinkToADirectory() const
{
    return m_isTargetADirectory;
}

void UICustomFileSystemItem::setIsSymLinkToADirectory(bool flag)
{
    m_isTargetADirectory = flag;
}

bool UICustomFileSystemItem::isSymLinkToAFile() const
{
    return isSymLink() && !m_isTargetADirectory;
}

void UICustomFileSystemItem::setIsDriveItem(bool flag)
{
    m_isDriveItem = flag;
}

bool UICustomFileSystemItem::isDriveItem() const
{
    return m_isDriveItem;
}

/* static */QVector<QVariant> UICustomFileSystemItem::createTreeItemData(const QString &strName, unsigned long long size,
                                                                         const QDateTime &changeTime,
                                                                         const QString &strOwner, const QString &strPermissions)
{
    QVector<QVariant> data;
    data.resize(UICustomFileSystemModelColumn_Max);
    data[UICustomFileSystemModelColumn_Name]        = strName;
    data[UICustomFileSystemModelColumn_Size]        = (qulonglong)size;
    data[UICustomFileSystemModelColumn_ChangeTime]  = changeTime;
    data[UICustomFileSystemModelColumn_Owner]       = strOwner;
    data[UICustomFileSystemModelColumn_Permissions] = strPermissions;
    return data;
}


/*********************************************************************************************************************************
*   UICustomFileSystemProxyModel implementation.                                                                                 *
*********************************************************************************************************************************/

bool UICustomFileSystemProxyModel::listDirectoriesOnTop() const
{
    return m_fListDirectoriesOnTop;
}

bool UICustomFileSystemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    UICustomFileSystemItem *pLeftItem = static_cast<UICustomFileSystemItem*>(left.internalPointer());
    UICustomFileSystemItem *pRightItem = static_cast<UICustomFileSystemItem*>(right.internalPointer());

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
*   UICustomFileSystemModel implementation.                                                                                      *
*********************************************************************************************************************************/

UICustomFileSystemModel::UICustomFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_fShowHumanReadableSizes(false)
{
    initializeTree();
}

UICustomFileSystemItem* UICustomFileSystemModel::rootItem()
{
    return m_pRootItem;
}

const UICustomFileSystemItem* UICustomFileSystemModel::rootItem() const
{
    return m_pRootItem;
}

UICustomFileSystemModel::~UICustomFileSystemModel()
{
    delete m_pRootItem;
}

int UICustomFileSystemModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<UICustomFileSystemItem*>(parent.internalPointer())->columnCount();
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
            UICustomFileSystemItem *pItem = static_cast<UICustomFileSystemItem*>(index.internalPointer());
            if (!pItem)
                return false;
            QString strOldName = pItem->name();
            pItem->setData(value, index.column());
            emit dataChanged(index, index);
            emit sigItemRenamed(pItem, strOldName, value.toString());
            return true;
        }
    }
    return false;
}

QVariant UICustomFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    UICustomFileSystemItem *item = static_cast<UICustomFileSystemItem*>(index.internalPointer());
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
    UICustomFileSystemItem *item = static_cast<UICustomFileSystemItem*>(index.internalPointer());
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

QModelIndex UICustomFileSystemModel::index(UICustomFileSystemItem* item)
{
    if (!item)
        return QModelIndex();
    return createIndex(item->row(), 0, item);
}

QModelIndex UICustomFileSystemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const UICustomFileSystemItem* parentItem = rootItem();

    if (parent.isValid())
        parentItem = static_cast<UICustomFileSystemItem*>(parent.internalPointer());

    if (!parentItem)
        return QModelIndex();

    UICustomFileSystemItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}


QModelIndex UICustomFileSystemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    UICustomFileSystemItem *childItem = static_cast<UICustomFileSystemItem*>(index.internalPointer());
    UICustomFileSystemItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem())
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int UICustomFileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    const UICustomFileSystemItem *parentItem = rootItem();
    if (parent.isValid())
        parentItem = static_cast<UICustomFileSystemItem*>(parent.internalPointer());
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

void UICustomFileSystemModel::reset()
{
    beginResetModel();
    m_pRootItem->reset();
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

void UICustomFileSystemModel::initializeTree()
{
    QVector<QVariant> headData;
    headData.resize(UICustomFileSystemModelColumn_Max);
    m_pRootItem = new UICustomFileSystemItem(headData, 0, KFsObjType_Directory);
}
