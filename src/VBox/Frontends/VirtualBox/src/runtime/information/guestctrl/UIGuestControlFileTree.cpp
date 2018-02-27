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

class UIGuestControlFileTreeItem : public QITreeWidgetItem
{
    Q_OBJECT;

public:
    UIGuestControlFileTreeItem(UIGuestControlFileTree *pTreeWidget, const QStringList &strings = QStringList());
    UIGuestControlFileTreeItem(UIGuestControlFileTreeItem *pTreeWidgetItem, const QStringList &strings = QStringList());
    virtual ~UIGuestControlFileTreeItem();

};

UIGuestControlFileTreeItem::UIGuestControlFileTreeItem(UIGuestControlFileTree *pTreeWidget, const QStringList &strings /* = QStringList() */)
    :QITreeWidgetItem(pTreeWidget, strings)

{
}
UIGuestControlFileTreeItem::UIGuestControlFileTreeItem(UIGuestControlFileTreeItem *pTreeWidgetItem, const QStringList &strings /*= QStringList() */)
    :QITreeWidgetItem(pTreeWidgetItem, strings)
{
}

UIGuestControlFileTreeItem::~UIGuestControlFileTreeItem(){}

#include "UIGuestControlFileTree.moc"
