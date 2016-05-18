/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationModel class implementation.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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
# include <QFont>
# include <QHash>
# include <QBrush>
# include <QGraphicsView>
# include <QGraphicsScene>
# include <QGraphicsSceneContextMenuEvent>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIExtraDataManager.h"
# include "UIInformationModel.h"
# include "UIInformationDataItem.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationModel::UIInformationModel(QObject *pParent, const CMachine &machine, const CConsole &console)
    : QAbstractListModel(pParent)
    , m_machine(machine)
    , m_console(console)
{
    /* Prepare information-model: */
    prepare();
}

UIInformationModel::~UIInformationModel()
{
    /* Destroy all data-items: */
    qDeleteAll(m_list);
    m_list.clear();
}

int UIInformationModel::rowCount(const QModelIndex& /*parent */) const
{
    /* Return row-count: */
    return m_list.count();
}

QVariant UIInformationModel::data(const QModelIndex &index, int role) const
{
    /* Get row: */
    int row = index.row();
    /* Get item at the row: */
    UIInformationDataItem *pItem = m_list.at(row);
    /* Return the data for the corresponding role: */
    return pItem->data(index, role);
}

void UIInformationModel::prepare()
{
    /* Prepare role-names for model: */
    QHash<int, QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "";
    roleNames[Qt::DecorationRole] = "";
    roleNames[Qt::UserRole + 1] = "";
    roleNames[Qt::UserRole + 2] = "";
    # if QT_VERSION < 0x050000
    setRoleNames(roleNames);
    # endif /* QT_VERSION < 0x050000 */

    /* Register meta-type: */
    qRegisterMetaType<InformationElementType>();
}

QHash<int, QByteArray> UIInformationModel::roleNames() const
{
    /* Add supported roles and return: */
    QHash<int, QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "";
    roleNames[Qt::DecorationRole] = "";
    roleNames[Qt::UserRole + 1] = "";
    roleNames[Qt::UserRole + 2] = "";
    return roleNames;
}

void UIInformationModel::addItem(UIInformationDataItem *pItem)
{
    /* Make sure item is valid: */
    AssertPtrReturnVoid(pItem);
    /* Add item: */
    m_list.append(pItem);
}

void UIInformationModel::updateData(const QModelIndex &idx)
{
    /* Emit data-changed signal: */
    emit dataChanged(idx, idx);
}

void UIInformationModel::updateData(UIInformationDataItem *pItem)
{
    /* Updates data: */
    AssertPtrReturnVoid(pItem);
    int iRow = m_list.indexOf(pItem);
    QModelIndex index = createIndex(iRow, 0);
    emit dataChanged(index, index);
}

