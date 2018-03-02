/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTree class implementation.
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
// # include <QVBoxLayout>

/* GUI includes: */
// # include "QILabel.h"
// # include "QILineEdit.h"
// # include "QIWithRetranslateUI.h"
// # include "UIExtraDataManager.h"
# include "UIGuestControlFileTree.h"
// # include "UIVMInformationDialog.h"
// # include "VBoxGlobal.h"

/* COM includes: */
// # include "CGuest.h"
// # include "CGuestSessionStateChangedEvent.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGuestControlFileTreeItem::UIGuestControlFileTreeItem(UIGuestControlFileTree *pTreeWidget,
                                                       int depth, const QString &path,
                                                       const QStringList &strings /* = QStringList() */)
    :QITreeWidgetItem(pTreeWidget, strings)
    , m_iDepth(depth)
    , m_strPath(path)
    , m_bIsOpened(false)
    , m_bIsDirectory(false)
{
    prepare();
}

UIGuestControlFileTreeItem::UIGuestControlFileTreeItem(UIGuestControlFileTreeItem *pTreeWidgetItem,
                                                       int depth, const QString &path,
                                                       const QStringList &strings /*= QStringList() */)
    :QITreeWidgetItem(pTreeWidgetItem, strings)
    , m_iDepth(depth)
    , m_strPath(path)
    , m_bIsOpened(false)
    , m_bIsDirectory(false)
{
    prepare();
}

UIGuestControlFileTreeItem::~UIGuestControlFileTreeItem(){}

void UIGuestControlFileTreeItem::prepare()
{
    /* For debugging: */
    setBackground(1, Qt::red);
    //QBrush::QBrush(Qt::GlobalColor
    //Qt::red
}

QList<UIGuestControlFileTreeItem*>  UIGuestControlFileTreeItem::children()
{
    QList<UIGuestControlFileTreeItem*> children;
    for(int i = 0; i < childCount(); ++i)
    {
        UIGuestControlFileTreeItem* ch = dynamic_cast<UIGuestControlFileTreeItem*>(child(i));
        if(ch)
            children.push_back(ch);
    }
    return children;
}

int UIGuestControlFileTreeItem::depth() const
{
    return m_iDepth;
}

const QString& UIGuestControlFileTreeItem::path() const
{
    return m_strPath;
}

bool UIGuestControlFileTreeItem::isOpened() const
{
    return m_bIsOpened;
}
void UIGuestControlFileTreeItem::setIsOpened(bool flag)
{
    m_bIsOpened = flag;
    setBackground(1, Qt::green);
}

bool UIGuestControlFileTreeItem::isDirectory() const
{
    return m_bIsDirectory;
}
void UIGuestControlFileTreeItem::setIsDirectory(bool flag)
{
    m_bIsDirectory = flag;
}
