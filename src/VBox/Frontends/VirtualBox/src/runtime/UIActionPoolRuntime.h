/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolRuntime class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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

/** Runtime action-pool index enum.
  * Naming convention is following:
  * 1. Every simple-action index prepended with 'S',
  * 2. Every toggle-action index presended with 'T',
  * 3. Every menu index prepended with 'M',
  * 4. Every sub-index contains full parent-index name. */
enum UIActionIndexRT
{
    /* 'Machine' menu actions: */
    UIActionIndexRT_M_Machine = UIActionIndex_Max + 1,
    UIActionIndexRT_M_Machine_S_Settings,
    UIActionIndexRT_M_Machine_S_TakeSnapshot,
    UIActionIndexRT_M_Machine_S_TakeScreenshot,
    UIActionIndexRT_M_Machine_S_ShowInformation,
    UIActionIndexRT_M_Machine_M_Keyboard,
    UIActionIndexRT_M_Machine_M_Keyboard_S_Settings,
    UIActionIndexRT_M_Machine_M_Mouse,
    UIActionIndexRT_M_Machine_M_Mouse_T_Integration,
    UIActionIndexRT_M_Machine_S_TypeCAD,
#ifdef Q_WS_X11
    UIActionIndexRT_M_Machine_S_TypeCABS,
#endif /* Q_WS_X11 */
    UIActionIndexRT_M_Machine_T_Pause,
    UIActionIndexRT_M_Machine_S_Reset,
    UIActionIndexRT_M_Machine_S_Save,
    UIActionIndexRT_M_Machine_S_Shutdown,
    UIActionIndexRT_M_Machine_S_PowerOff,
    UIActionIndexRT_M_Machine_S_Close,

    /* 'View' menu actions: */
    UIActionIndexRT_M_View,
    UIActionIndexRT_M_ViewPopup,
    UIActionIndexRT_M_View_T_Fullscreen,
    UIActionIndexRT_M_View_T_Seamless,
    UIActionIndexRT_M_View_T_Scale,
    UIActionIndexRT_M_View_S_AdjustWindow,
    UIActionIndexRT_M_View_T_GuestAutoresize,
    UIActionIndexRT_M_View_M_StatusBar,
    UIActionIndexRT_M_View_M_StatusBar_S_Settings,
    UIActionIndexRT_M_View_M_StatusBar_T_Visibility,

    /* 'Devices' menu actions: */
    UIActionIndexRT_M_Devices,
    UIActionIndexRT_M_Devices_M_HardDrives,
    UIActionIndexRT_M_Devices_M_HardDrives_S_Settings,
    UIActionIndexRT_M_Devices_M_OpticalDevices,
    UIActionIndexRT_M_Devices_M_FloppyDevices,
    UIActionIndexRT_M_Devices_M_USBDevices,
    UIActionIndexRT_M_Devices_M_WebCams,
    UIActionIndexRT_M_Devices_M_SharedClipboard,
    UIActionIndexRT_M_Devices_M_DragAndDrop,
    UIActionIndexRT_M_Devices_M_Network,
    UIActionIndexRT_M_Devices_M_Network_S_Settings,
    UIActionIndexRT_M_Devices_M_SharedFolders,
    UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings,
    UIActionIndexRT_M_Devices_T_VRDEServer,
    UIActionIndexRT_M_Devices_M_VideoCapture,
    UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings,
    UIActionIndexRT_M_Devices_M_VideoCapture_T_Start,
    UIActionIndexRT_M_Devices_S_InstallGuestTools,

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debugger' menu actions: */
    UIActionIndexRT_M_Debug,
    UIActionIndexRT_M_Debug_S_ShowStatistics,
    UIActionIndexRT_M_Debug_S_ShowCommandLine,
    UIActionIndexRT_M_Debug_T_Logging,
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' menu actions: */
    UIActionIndexRT_M_Dock,
    UIActionIndexRT_M_Dock_M_DockSettings,
    UIActionIndexRT_M_Dock_M_DockSettings_T_PreviewMonitor,
    UIActionIndexRT_M_Dock_M_DockSettings_T_DisableMonitor,
#endif /* Q_WS_MAC */

    /* Maximum index: */
    UIActionIndexRT_Max
};

/* Singleton runtime action pool: */
class UIActionPoolRuntime : public UIActionPool
{
    Q_OBJECT;

private:

    /* Constructor: */
    UIActionPoolRuntime();

    /** Translation handler. */
    void retranslateUi();

    /* Helper: Shortcuts stuff: */
    QString shortcutsExtraDataID() const;

    /* Helpers: Prepare stuff: */
    void createActions();
    void createMenus();

    /* Friend zone: */
    friend class UIActionPool;
};

#endif // __UIActionPoolRuntime_h__

