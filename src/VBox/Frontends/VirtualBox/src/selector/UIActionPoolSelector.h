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

    /* 'Machine' menu actions: */
    UIActionIndexSelector_Menu_Machine,
    UIActionIndexSelector_Simple_Machine_NewWizard,
    UIActionIndexSelector_Simple_Machine_AddDialog,
    UIActionIndexSelector_Simple_Machine_SettingsDialog,
    UIActionIndexSelector_Simple_Machine_CloneWizard,
    UIActionIndexSelector_Simple_Machine_RemoveDialog,
    UIActionIndexSelector_State_Machine_StartOrShow,
    UIActionIndexSelector_Simple_Machine_Discard,
    UIActionIndexSelector_Toggle_Machine_PauseAndResume,
    UIActionIndexSelector_Simple_Machine_Reset,
    UIActionIndexSelector_Simple_Machine_Refresh,
    UIActionIndexSelector_Simple_Machine_LogDialog,
    UIActionIndexSelector_Simple_Machine_ShowInFileManager,
    UIActionIndexSelector_Simple_Machine_CreateShortcut,
    UIActionIndexSelector_Simple_Machine_Sort,

    /* 'Machine/Close' menu actions: */
    UIActionIndexSelector_Menu_Machine_Close,
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

