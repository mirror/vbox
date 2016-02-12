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
}

int UIInformationModel::rowCount(const QModelIndex & /*parent */) const
{
    return m_list.count();
}

QVariant UIInformationModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();
    UIInformationDataItem *pItem = m_list.at(row);
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
    setRoleNames(roleNames);

    /* Register meta-type: */
    qRegisterMetaType<InformationElementType>();
}

QHash<int, QByteArray> UIInformationModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "";
    roleNames[Qt::DecorationRole] = "";
    roleNames[Qt::UserRole + 1] = "";
    roleNames[Qt::UserRole + 2] = "";
    return roleNames;
}

void UIInformationModel::addItem(UIInformationDataItem *pItem)
{
    AssertPtrReturnVoid(pItem);
    m_list.append(pItem);
}

void UIInformationModel::updateData(const QModelIndex &idx)
{
    emit dataChanged(idx, idx);
}

