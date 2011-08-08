/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolRuntime class declaration
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

#ifndef __UIActionPoolRuntime_h__
#define __UIActionPoolRuntime_h__

/* Local includes: */
#include "UIActionPool.h"

/* Action keys: */
enum UIActionIndexRuntime
{
    /* 'Machine' menu actions: */
    UIActionIndexRuntime_Menu_Machine = UIActionIndex_Max + 1,
    UIActionIndexRuntime_Simple_SettingsDialog,
    UIActionIndexRuntime_Simple_TakeSnapshot,
    UIActionIndexRuntime_Simple_InformationDialog,
    UIActionIndexRuntime_Menu_MouseIntegration,
    UIActionIndexRuntime_Toggle_MouseIntegration,
    UIActionIndexRuntime_Simple_TypeCAD,
#ifdef Q_WS_X11
    UIActionIndexRuntime_Simple_TypeCABS,
#endif /* Q_WS_X11 */
    UIActionIndexRuntime_Toggle_Pause,
    UIActionIndexRuntime_Simple_Reset,
    UIActionIndexRuntime_Simple_Shutdown,
    UIActionIndexRuntime_Simple_Close,

    /* 'View' menu actions: */
    UIActionIndexRuntime_Menu_View,
    UIActionIndexRuntime_Toggle_Fullscreen,
    UIActionIndexRuntime_Toggle_Seamless,
    UIActionIndexRuntime_Toggle_Scale,
    UIActionIndexRuntime_Toggle_GuestAutoresize,
    UIActionIndexRuntime_Simple_AdjustWindow,

    /* 'Devices' menu actions: */
    UIActionIndexRuntime_Menu_Devices,
    UIActionIndexRuntime_Menu_OpticalDevices,
    UIActionIndexRuntime_Menu_FloppyDevices,
    UIActionIndexRuntime_Menu_USBDevices,
    UIActionIndexRuntime_Menu_NetworkAdapters,
    UIActionIndexRuntime_Simple_NetworkAdaptersDialog,
    UIActionIndexRuntime_Menu_SharedFolders,
    UIActionIndexRuntime_Simple_SharedFoldersDialog,
    UIActionIndexRuntime_Toggle_VRDEServer,
    UIActionIndexRuntime_Simple_InstallGuestTools,

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debugger' menu actions: */
    UIActionIndexRuntime_Menu_Debug,
    UIActionIndexRuntime_Simple_Statistics,
    UIActionIndexRuntime_Simple_CommandLine,
    UIActionIndexRuntime_Toggle_Logging,
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' menu actions: */
    UIActionIndexRuntime_Menu_Dock,
    UIActionIndexRuntime_Menu_DockSettings,
    UIActionIndexRuntime_Toggle_DockPreviewMonitor,
    UIActionIndexRuntime_Toggle_DockDisableMonitor,
#endif /* Q_WS_MAC */

    /* Maximum index: */
    UIActionIndexRuntime_Max
};

/* Singleton runtime action pool: */
class UIActionPoolRuntime : public UIActionPool
{
    Q_OBJECT;

public:

    /* Singleton methods: */
    static void create();
    static void destroy();

private:

    /* Constructor: */
    UIActionPoolRuntime() : UIActionPool(UIActionPoolType_Runtime) {}

    /* Virtual helping stuff: */
    void createActions();
    void createMenus();
};

#endif // __UIActionPoolRuntime_h__

