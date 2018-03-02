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
// # include <QAbstractItemModel>
// # include <QHBoxLayout>
// # include <QPlainTextEdit>
// # include <QPushButton>
// # include <QSplitter>
# include <QHeaderView>
# include <QVBoxLayout>

/* GUI includes: */
// # include "QILabel.h"
// # include "QILineEdit.h"
// # include "QIWithRetranslateUI.h"
// # include "UIExtraDataManager.h"
# include "UIGuestControlFileTable.h"
// # include "UIVMInformationDialog.h"
// # include "VBoxGlobal.h"

/* COM includes: */
// # include "CGuest.h"
# include "CFsObjInfo.h"
# include "CGuestDirectory.h"
// # include "CGuestSessionStateChangedEvent.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */



class UIFileTableItem
{
public:
    explicit UIFileTableItem(const QList<QVariant> &data, UIFileTableItem *parentItem = 0);
    ~UIFileTableItem();

    void appendChild(UIFileTableItem *child);

    UIFileTableItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    UIFileTableItem *parentItem();

private:
    QList<UIFileTableItem*> m_childItems;
    QList<QVariant> m_itemData;
    UIFileTableItem *m_parentItem;
};


UIFileTableItem::UIFileTableItem(const QList<QVariant> &data, UIFileTableItem *parent)
{
    m_parentItem = parent;
    m_itemData = data;
}

UIFileTableItem::~UIFileTableItem()
{
    qDeleteAll(m_childItems);
}

void UIFileTableItem::appendChild(UIFileTableItem *item)
{
    m_childItems.append(item);
}

UIFileTableItem *UIFileTableItem::child(int row)
{
    return m_childItems.value(row);
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



UIGuestControlFileModel::UIGuestControlFileModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_pParent(qobject_cast<UIGuestControlFileTable*>(parent))
{
    QList<QVariant> rootData;
    // rootData << "Title" << "Summary";
    // rootItem = new UIFileTableItem(rootData);
}

UIFileTableItem* UIGuestControlFileModel::rootItem() const
{
    if (!m_pParent)
        return 0;
    return m_pParent->m_pRootItem;
}

UIGuestControlFileModel::~UIGuestControlFileModel()
{
    //delete rootItem;
}

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

QVariant UIGuestControlFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());

    return item->data(index.column());
}

Qt::ItemFlags UIGuestControlFileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

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

UIGuestControlFileView::UIGuestControlFileView(QWidget *pParent /*= 0*/)
    :QTableView(pParent)
{
    setShowGrid(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    verticalHeader()->setVisible(false);
}

UIGuestControlFileTable::UIGuestControlFileTable(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_pRootItem(0)
    , m_pView(0)
    , m_pModel(0)
    , m_pMainLayout(0)
{
    prepareObjects();
}

UIGuestControlFileTable::~UIGuestControlFileTable()
{
    delete m_pRootItem;
}

void UIGuestControlFileTable::prepareObjects()
{
    m_pMainLayout = new QVBoxLayout();
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_pMainLayout);

    m_pModel = new UIGuestControlFileModel(this);
    if (!m_pModel)
        return;
    m_pView = new UIGuestControlFileView;
    if (m_pView)
    {
        m_pMainLayout->addWidget(m_pView);
        m_pView->setModel(m_pModel);
    }
}

UIGuestFileTable::UIGuestFileTable(QWidget *pParent /*= 0*/)
    :UIGuestControlFileTable(pParent)
{
}

void UIGuestFileTable::initGuestFileTable(const CGuestSession &session)
{
    if (!session.isOk())
        return;
    if (session.GetStatus() != KGuestSessionStatus_Started)
        return;
    m_comGuestSession = session;

    if(m_pRootItem)
        delete m_pRootItem;

    QList<QVariant> rootData;
    rootData << "Name" << "Size";
    m_pRootItem = new UIFileTableItem(rootData);

    // for(int i = 0; i < 10; ++i)
    // {
    //     QList<QVariant> data;
    //     data << "some name" << 23432;
    //     UIFieTableItem *item = new UIFileTableItem(data, m_pRootItem);
    //     m_pRootItem->appendChild(item);
    //     if (i == 4)

    // }

    /* Read the root directory and get the list: */
    readDirectory("/", m_pRootItem);
    m_pModel->signalUpdate();
}

void UIGuestFileTable::readDirectory(const QString& strPath,
                                     UIFileTableItem *parent)

{
    CGuestDirectory directory;
    QVector<KDirectoryOpenFlag> flag;
    flag.push_back(KDirectoryOpenFlag_None);

    directory = m_comGuestSession.DirectoryOpen(strPath, /*aFilter*/ "", flag);
    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        QMap<QString, UIFileTableItem*> directories;
        QMap<QString, UIFileTableItem*> files;

        while (fsInfo.isOk())
        {
            QList<QVariant> data;
            data << fsInfo.GetName() << static_cast<qulonglong>(fsInfo.GetObjectSize());
            UIFileTableItem *item = new UIFileTableItem(data, m_pRootItem);
            //parent->appendChild(item);
            if (fsInfo.GetType() == KFsObjType_Directory)
                directories.insert(fsInfo.GetName(), item);
            else
                files.insert(fsInfo.GetName(), item);
            fsInfo = directory.Read();
        }
        insertToTree(directories, parent);
        insertToTree(files, parent);
    }
}

void UIGuestFileTable::insertToTree(const QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent)
{
    for(QMap<QString,UIFileTableItem*>::const_iterator iterator = map.begin();
        iterator != map.end(); ++iterator)
    {
        if(iterator.key() != ".")
            parent->appendChild(iterator.value());
    }
}
//     if (!treeParent || treeParent->depth() - startDepth >= iMaxDepth || strPath.isEmpty())
//         return;
//     QVector<KDirectoryOpenFlag> flag;
//     flag.push_back(KDirectoryOpenFlag_None);
//     CGuestDirectory directory;
//     directory = m_comGuestSession.DirectoryOpen(strPath, /*aFilter*/ "", flag);
//     treeParent->setIsOpened(true);
//     if (directory.isOk())
//     {
//         CFsObjInfo fsInfo = directory.Read();
//         while (fsInfo.isOk())
//         {
//             if (fsInfo.GetName() != "."
//                 && fsInfo.GetName() != "..")
//             {
//                 QString path(strPath);
//                 if (path.at(path.length() -1 ) != '/')
//                     path.append(QString("/").append(fsInfo.GetName()));
//                 else
//                     path.append(fsInfo.GetName());
//                 UIGuestControlFileTreeItem *treeItem =
//                     new UIGuestControlFileTreeItem(treeParent, treeParent->depth() + 1 /*depth */,
//                                                    path, getFsObjInfoStringList<CFsObjInfo>(fsInfo));
//                 if (fsInfo.GetType() == KFsObjType_Directory)
//                 {
//                     treeItem->setIsDirectory(true);
//                     treeItem->setIcon(0, QIcon(":/sf_32px.png"));
//                     treeItem->setIsOpened(false);
//                     readDirectory(path, treeItem, startDepth, iMaxDepth);
//                 }
//                 else
//                 {
//                     treeItem->setIsDirectory(false);
//                     treeItem->setIsOpened(false);
//                     treeItem->setIcon(0, QIcon(":/vm_open_filemanager_16px"));
//                     treeItem->setHidden(true);
//                 }
//             }

//             fsInfo = directory.Read();
//         }
//         directory.Close();
//     }
// }
