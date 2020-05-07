/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItem class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"


/*********************************************************************************************************************************
*   Class UIVirtualMachineItem implementation.                                                                                   *
*********************************************************************************************************************************/

UIVirtualMachineItem::UIVirtualMachineItem(UIVirtualMachineItemType enmType)
    : m_enmType(enmType)
    , m_fAccessible(false)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_fHasDetails(false)
{
}

UIVirtualMachineItem::~UIVirtualMachineItem()
{
}

UIVirtualMachineItemLocal *UIVirtualMachineItem::toLocal()
{
    return   itemType() == UIVirtualMachineItemType_Local
           ? static_cast<UIVirtualMachineItemLocal*>(this)
           : 0;
}

UIVirtualMachineItemCloud *UIVirtualMachineItem::toCloud()
{
    return   (   itemType() == UIVirtualMachineItemType_CloudFake
              || itemType() == UIVirtualMachineItemType_CloudReal)
           ? static_cast<UIVirtualMachineItemCloud*>(this)
           : 0;
}

QPixmap UIVirtualMachineItem::osPixmap(QSize *pLogicalSize /* = 0 */) const
{
    if (pLogicalSize)
        *pLogicalSize = m_logicalPixmapSize;
    return m_pixmap;
}


/*********************************************************************************************************************************
*   Class UIVirtualMachineItemMimeData implementation.                                                                           *
*********************************************************************************************************************************/

QString UIVirtualMachineItemMimeData::m_type = "application/org.virtualbox.gui.vmselector.UIVirtualMachineItem";

UIVirtualMachineItemMimeData::UIVirtualMachineItemMimeData(UIVirtualMachineItem *pItem)
    : m_pItem(pItem)
{
}

QStringList UIVirtualMachineItemMimeData::formats() const
{
    QStringList types;
    types << type();
    return types;
}
