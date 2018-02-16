/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestSessionTreeItem class implementation.
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

/* GUI includes: */
# include "UIGuestSessionTreeItem.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGuestSessionTreeItem::UIGuestSessionTreeItem(QITreeWidget *pTreeWidget,
                                               GuestSessionTreeItemType itemType)
    :QITreeWidgetItem(pTreeWidget)
    , m_eItemType(itemType)
{
}

UIGuestSessionTreeItem::UIGuestSessionTreeItem(UIGuestSessionTreeItem *pTreeWidgetItem,
                                               GuestSessionTreeItemType itemType)
    :QITreeWidgetItem(pTreeWidgetItem)
    , m_eItemType(itemType)
{
}

UIGuestSessionTreeItem::UIGuestSessionTreeItem(QITreeWidget *pTreeWidget,
                                               const QStringList &strings, GuestSessionTreeItemType itemType)
    :QITreeWidgetItem(pTreeWidget, strings)
    , m_eItemType(itemType)
{
}

UIGuestSessionTreeItem::UIGuestSessionTreeItem(UIGuestSessionTreeItem *pTreeWidgetItem,
                                               const QStringList &strings, GuestSessionTreeItemType itemType)
    :QITreeWidgetItem(pTreeWidgetItem, strings)
    , m_eItemType(itemType)
{
}
