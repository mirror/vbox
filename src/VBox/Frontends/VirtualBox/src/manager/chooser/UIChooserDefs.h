/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserDefs class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserDefs_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QGraphicsItem>

/* Other VBox includes: */
#include <iprt/cdefs.h>


/** UIChooserNode types. */
enum UIChooserNodeType
{
    UIChooserNodeType_Any     = QGraphicsItem::UserType,
    UIChooserNodeType_Group,
    UIChooserNodeType_Global,
    UIChooserNodeType_Machine
};


/** UIChooserNodeGroup types. */
enum UIChooserNodeGroupType
{
    UIChooserNodeGroupType_Invalid,
    UIChooserNodeGroupType_Local,
    UIChooserNodeGroupType_Provider,
    UIChooserNodeGroupType_Profile
};


/** UIChooserItem search flags. */
enum UIChooserItemSearchFlag
{
    UIChooserItemSearchFlag_Global        = RT_BIT(0),
    UIChooserItemSearchFlag_Machine       = RT_BIT(1),
    UIChooserItemSearchFlag_LocalGroup    = RT_BIT(2),
    UIChooserItemSearchFlag_CloudProvider = RT_BIT(3),
    UIChooserItemSearchFlag_CloudProfile  = RT_BIT(4),
    UIChooserItemSearchFlag_ExactId       = RT_BIT(5),
    UIChooserItemSearchFlag_ExactName     = RT_BIT(6)
};


/** UIChooserItem drag token types. */
enum UIChooserItemDragToken
{
    UIChooserItemDragToken_Off,
    UIChooserItemDragToken_Up,
    UIChooserItemDragToken_Down
};


/** UIChooserItemMachine enumeration flags. */
enum UIChooserItemMachineEnumerationFlag
{
    UIChooserItemMachineEnumerationFlag_Unique       = RT_BIT(0),
    UIChooserItemMachineEnumerationFlag_Inaccessible = RT_BIT(1)
};


#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserDefs_h */
