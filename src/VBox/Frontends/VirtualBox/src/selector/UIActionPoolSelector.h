/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolSelector class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIActionPoolSelector_h__
#define __UIActionPoolSelector_h__

/* Local includes: */
#include "UIActionPool.h"

/* Action keys: */
enum UIActionIndexSelector
{
    /* 'File' menu actions: */
    UIActionIndexSelector_Menu_File = UIActionIndex_Max + 1,
    UIActionIndexSelector_Simple_File_MediumManagerDialog,
    UIActionIndexSelector_Simple_File_ImportApplianceWizard,
    UIActionIndexSelector_Simple_File_ExportApplianceWizard,
    UIActionIndexSelector_Simple_File_PreferencesDialog,
    UIActionIndexSelector_Simple_File_Exit,

    /* 'Group' menu actions: */
    UIActionIndexSelector_Menu_Group,
    UIActionIndexSelector_Simple_Group_Rename,
    UIActionIndexSelector_Simple_Group_Remove,
    UIActionIndexSelector_Simple_Group_Sort,

    /* 'Machine' menu actions: */
    UIActionIndexSelector_Menu_Machine,
    UIActionIndexSelector_Simple_Machine_Settings,
    UIActionIndexSelector_Simple_Machine_Clone,
    UIActionIndexSelector_Simple_Machine_Remove,
    UIActionIndexSelector_Simple_Machine_AddGroup,

    /* Common 'Group' / 'Machine' menu actions: */
    UIActionIndexSelector_Simple_Common_New,
    UIActionIndexSelector_Simple_Common_Add,
    UIActionIndexSelector_State_Common_StartOrShow,
    UIActionIndexSelector_Toggle_Common_PauseAndResume,
    UIActionIndexSelector_Simple_Common_Reset,
    UIActionIndexSelector_Simple_Common_Discard,
    UIActionIndexSelector_Simple_Common_Refresh,
    UIActionIndexSelector_Simple_Common_ShowInFileManager,
    UIActionIndexSelector_Simple_Common_CreateShortcut,
    UIActionIndexSelector_Simple_Common_SortParent,

    /* 'Machine/Close' menu actions: */
    UIActionIndexSelector_Menu_Machine_Close,
    UIActionIndexSelector_Simple_Machine_Close_Save,
    UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown,
    UIActionIndexSelector_Simple_Machine_Close_PowerOff,

    /* Maximum index: */
    UIActionIndexSelector_Max
};

/* Singleton runtime action pool: */
class UIActionPoolSelector : public UIActionPool
{
    Q_OBJECT;

public:

    /* Singleton methods: */
    static void create();
    static void destroy();

private:

    /* Constructor: */
    UIActionPoolSelector() : UIActionPool(UIActionPoolType_Selector) {}

    /* Virtual helping stuff: */
    void createActions();
    void createMenus();
};

#endif // __UIActionPoolSelector_h__

