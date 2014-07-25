/** @file
 * VBox Qt GUI - UIActionPoolRuntime class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIActionPoolRuntime_h___
#define ___UIActionPoolRuntime_h___

/* Qt includes: */
#include <QMap>
#include <QList>

/* GUI includes: */
#include "UIActionPool.h"
#include "UIExtraDataDefs.h"

/** Runtime action-pool index enum.
  * Naming convention is following:
  * 1. Every menu index prepended with 'M',
  * 2. Every simple-action index prepended with 'S',
  * 3. Every toggle-action index presended with 'T',
  * 4. Every polymorphic-action index presended with 'P',
  * 5. Every sub-index contains full parent-index name. */
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
    UIActionIndexRT_M_Devices_M_USBDevices_S_Settings,
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

/** UIActionPool extension
  * representing action-pool singleton for Runtime UI. */
class UIActionPoolRuntime : public UIActionPool
{
    Q_OBJECT;

signals:

    /** Notifies about 'View' : 'Resize' menu action trigger. */
    void sigNotifyAboutTriggeringViewResize(int iScreenIndex, const QSize &size);

public:

    /** Returns whether the menu with passed @a type is allowed in menu-bar. */
    bool isAllowedInMenuBar(RuntimeMenuType type) const;
    /** Defines menu-bar @a restriction for passed @a level. */
    void setRestrictionForMenuBar(UIActionRestrictionLevel level, RuntimeMenuType restriction);

#ifdef Q_WS_MAC
    /** Returns whether the action with passed @a type is allowed in the 'Application' menu. */
    bool isAllowedInMenuApplication(RuntimeMenuApplicationActionType type) const;
    /** Defines 'Application' menu @a restriction for passed @a level. */
    void setRestrictionForMenuApplication(UIActionRestrictionLevel level, RuntimeMenuApplicationActionType restriction);
#endif /* Q_WS_MAC */

    /** Returns whether the action with passed @a type is allowed in the 'Machine' menu. */
    bool isAllowedInMenuMachine(RuntimeMenuMachineActionType type) const;
    /** Defines 'Machine' menu @a restriction for passed @a level. */
    void setRestrictionForMenuMachine(UIActionRestrictionLevel level, RuntimeMenuMachineActionType restriction);

    /** Returns whether the action with passed @a type is allowed in the 'View' menu. */
    bool isAllowedInMenuView(RuntimeMenuViewActionType type) const;
    /** Defines 'View' menu @a restriction for passed @a level. */
    void setRestrictionForMenuView(UIActionRestrictionLevel level, RuntimeMenuViewActionType restriction);

    /** Returns whether the action with passed @a type is allowed in the 'Devices' menu. */
    bool isAllowedInMenuDevices(RuntimeMenuDevicesActionType type) const;
    /** Defines 'Devices' menu @a restriction for passed @a level. */
    void setRestrictionForMenuDevices(UIActionRestrictionLevel level, RuntimeMenuDevicesActionType restriction);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Returns whether the action with passed @a type is allowed in the 'Debug' menu. */
    bool isAllowedInMenuDebug(RuntimeMenuDebuggerActionType type) const;
    /** Defines 'Debug' menu @a restriction for passed @a level. */
    void setRestrictionForMenuDebugger(UIActionRestrictionLevel level, RuntimeMenuDebuggerActionType restriction);
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /** Returns whether the action with passed @a type is allowed in the 'Help' menu. */
    bool isAllowedInMenuHelp(RuntimeMenuHelpActionType type) const;
    /** Defines 'Help' menu @a restriction for passed @a level. */
    void setRestrictionForMenuHelp(UIActionRestrictionLevel level, RuntimeMenuHelpActionType restriction);

    /** Defines current frame-buffer sizes
      * for menus which uses such arguments to build content. */
    void setCurrentFrameBufferSizes(const QList<QSize> &sizes, bool fUpdateMenu = false);

protected slots:

    /** Prepare 'View' : 'Resize' menu routine. */
    void sltPrepareMenuViewResize();
    /** Handles 'View' : 'Resize' menu @a pAction trigger. */
    void sltHandleActionTriggerViewResize(QAction *pAction);

protected:

    /** Constructor,
      * @param fTemporary is used to determine whether this action-pool is temporary,
      *                   which can be created to re-initialize shortcuts-pool. */
    UIActionPoolRuntime(bool fTemporary = false);

    /** Prepare pool routine. */
    virtual void preparePool();
    /** Prepare connections routine. */
    virtual void prepareConnections();

    /** Update configuration routine. */
    virtual void updateConfiguration();

    /** Update menus routine. */
    void updateMenus();
    /** Update 'Machine' menu routine. */
    void updateMenuMachine();
    /** Update 'View' menu routine. */
    void updateMenuView();
    /** Update 'View' : 'Popup' menu routine. */
    void updateMenuViewPopup();
    /** Update 'View' : 'Status Bar' menu routine. */
    void updateMenuViewStatusBar();
    /** Update 'View' : 'Resize' @a pMenu routine. */
    void updateMenuViewResize(QMenu *pMenu);
    /** Update 'Devices' menu routine. */
    void updateMenuDevices();
    /** Update 'Devices' : 'Hard Drives' menu routine. */
    void updateMenuDevicesHardDrives();
    /** Update 'Devices' : 'USB' menu routine. */
    void updateMenuDevicesUSBDevices();
    /** Update 'Devices' : 'Network' menu routine. */
    void updateMenuDevicesNetwork();
    /** Update 'Devices' : 'Shared Folders' menu routine. */
    void updateMenuDevicesSharedFolders();
    /** Update 'Devices' : 'Video Capture' menu routine. */
    void updateMenuDevicesVideoCapture();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Update 'Debug' menu routine. */
    void updateMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Update 'Help' menu routine. */
    void updateMenuHelp();

    /** Translation handler. */
    virtual void retranslateUi();

    /** Returns extra-data ID to save keyboard shortcuts under. */
    virtual QString shortcutsExtraDataID() const;

    /** Returns the list of Runtime UI main menus. */
    virtual QList<QMenu*> menus() const { return m_mainMenus; }

private:

    /** Holds the list of main-menus. */
    QList<QMenu*> m_mainMenus;

    /** Holds restricted menus. */
    QMap<UIActionRestrictionLevel, RuntimeMenuType> m_restrictedMenus;
#ifdef Q_WS_MAC
    /** Holds restricted actions of the Application menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuApplicationActionType> m_restrictedActionsMenuApplication;
#endif /* Q_WS_MAC */
    /** Holds restricted actions of the Machine menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuMachineActionType> m_restrictedActionsMenuMachine;
    /** Holds restricted actions of the View menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuViewActionType> m_restrictedActionsMenuView;
    /** Holds restricted actions of the Devices menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuDevicesActionType> m_restrictedActionsMenuDevices;
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Holds restricted actions of the Debugger menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuDebuggerActionType> m_restrictedActionsMenuDebug;
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Holds restricted actions of the Help menu. */
    QMap<UIActionRestrictionLevel, RuntimeMenuHelpActionType> m_restrictedActionsMenuHelp;

    /** Defines current frame-buffer sizes
      * for menus which uses such arguments to build content. */
    QList<QSize> m_sizes;

    /* Enable factory in base-class: */
    friend class UIActionPool;
};

#endif /* !___UIActionPoolRuntime_h___ */
