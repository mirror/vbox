/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileModel class implementation.
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
# include "UIGuestControlFileModel.h"
# include "UIGuestControlFileTable.h"
# include "UIGuestControlFileManager.h"


#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

const char* UIGuestControlFileModel::strUpDirectoryString = "..";

UIGuestControlFileProxyModel::UIGuestControlFileProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}

bool UIGuestControlFileProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    UIFileTableItem *pLeftItem = static_cast<UIFileTableItem*>(left.internalPointer());
    UIFileTableItem *pRightItem = static_cast<UIFileTableItem*>(right.internalPointer());

    UIGuestControlFileManagerOptions *pOptions = UIGuestControlFileManagerOptions::instance();

    if (pLeftItem && pRightItem)
    {
        /* List the directories before the files if options say so: */
        if (pOptions && pOptions->bListDirectoriesOnTop)
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
        if (sortColumn() == UIGuestControlFileModelColumn_ChangeTime)
        {
            QVariant dataLeft = pLeftItem->data(UIGuestControlFileModelColumn_ChangeTime);
            QVariant dataRight = pRightItem->data(UIGuestControlFileModelColumn_ChangeTime);
            QDateTime leftDateTime = dataLeft.toDateTime();
            QDateTime rightDateTime = dataRight.toDateTime();
            return (leftDateTime < rightDateTime);
        }
        /* When we show human readble sizes in size column sorting gets confused, so do it here: */
        else if(sortColumn() == UIGuestControlFileModelColumn_Size)
        {
            qulonglong leftSize = pLeftItem->data(UIGuestControlFileModelColumn_Size).toULongLong();
            qulonglong rightSize = pRightItem->data(UIGuestControlFileModelColumn_Size).toULongLong();
            return (leftSize < rightSize);

        }
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

UIGuestControlFileModel::UIGuestControlFileModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_pParent(qobject_cast<UIGuestControlFileTable*>(parent))
{
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

bool UIGuestControlFileModel::setData(const QModelIndex &index, const QVariant &value, int role)
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

QVariant UIGuestControlFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        /* dont show anything but the name for up directories: */
        if (item->isUpDirectory() && index.column() != UIGuestControlFileModelColumn_Name)
            return QVariant();
        /* Format date/time column: */
        if (item->data(index.column()).canConvert(QMetaType::QDateTime))
        {
            QDateTime dateTime = item->data(index.column()).toDateTime();
            if (dateTime.isValid())
                return dateTime.toString("dd.MM.yyyy hh:mm:ss");
        }
        /* Decide whether to show human-readable file object sizes: */
        if (index.column() == UIGuestControlFileModelColumn_Size)
        {
            UIGuestControlFileManagerOptions* pOptions =
                UIGuestControlFileManagerOptions::instance();
            if (pOptions && pOptions->bShowHumanReadableSizes)
            {
                qulonglong size = item->data(index.column()).toULongLong();
                return UIGuestControlFileTable::humanReadableSize(size);
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

Qt::ItemFlags UIGuestControlFileModel::flags(const QModelIndex &index) const
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

QModelIndex UIGuestControlFileModel::index(UIFileTableItem* item)
{
    if (!item)
        return QModelIndex();
    return createIndex(item->row(), 0, item);
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
