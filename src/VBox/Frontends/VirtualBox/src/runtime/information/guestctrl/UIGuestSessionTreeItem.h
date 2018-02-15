/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestSessionTreeItem class declaration.
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

#ifndef ___UIGuestSessionTreeItem_h___
#define ___UIGuestSessionTreeItem_h___

/* GUI includes: */
# include "QITreeWidget.h"


/* Forward declarations: */


class UIGuestSessionTreeItem : public QITreeWidgetItem
{

    Q_OBJECT;

signals:

public:

    enum GuestSessionTreeItemType{
        SessionTreeItem = 0,
        ProcessTreeItem,
        GuestSessionItemTypeMax
    };


    /** Constructs item passing @a pTreeWidget into the base-class. */
    UIGuestSessionTreeItem(QITreeWidget *pTreeWidget, GuestSessionTreeItemType itemType);
    /** Constructs item passing @a pTreeWidgetItem into the base-class. */
    UIGuestSessionTreeItem(UIGuestSessionTreeItem *pTreeWidgetItem, GuestSessionTreeItemType itemType);
    /** Constructs item passing @a pTreeWidget and @a strings into the base-class. */
    UIGuestSessionTreeItem(QITreeWidget *pTreeWidget, const QStringList &strings, GuestSessionTreeItemType itemType);
    /** Constructs item passing @a pTreeWidgetItem and @a strings into the base-class. */
    UIGuestSessionTreeItem(UIGuestSessionTreeItem *pTreeWidgetItem,
                           const QStringList &strings, GuestSessionTreeItemType itemType);

private slots:


private:
    GuestSessionTreeItemType m_eItemType;

};

#endif /* !___UIGuestSessionTreeItem_h___ */
