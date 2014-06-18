/** @file
 * VBox Qt GUI - Extra-data related definitions.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIExtraDataDefs_h___
#define ___UIExtraDataDefs_h___

/* Qt includes: */
#include <QMetaType>

/* Other VBox includes: */
#include <iprt/cdefs.h>

/** Extra-data namespace. */
namespace UIExtraDataDefs
{
    /* Global declarations: */
    extern const char* GUI_LanguageId;
    extern const char* GUI_PreventBetaWarning;
    extern const char* GUI_RecentFolderHD;
    extern const char* GUI_RecentFolderCD;
    extern const char* GUI_RecentFolderFD;
    extern const char* GUI_RecentListHD;
    extern const char* GUI_RecentListCD;
    extern const char* GUI_RecentListFD;

    /* Message-center declarations: */
    extern const char* GUI_SuppressMessages;
    extern const char* GUI_InvertMessageOption;

    /* Settings-dialog declarations: */
    extern const char* GUI_RestrictedGlobalSettingsPages;
    extern const char* GUI_RestrictedMachineSettingsPages;

    /* Wizard declarations: */
    extern const char* GUI_FirstRun;
    extern const char* GUI_HideDescriptionForWizards;

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Update-manager declarations: */
    extern const char* GUI_PreventApplicationUpdate;
    extern const char* GUI_UpdateDate;
    extern const char* GUI_UpdateCheckCount;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Selector UI declarations: */
    extern const char* GUI_Input_SelectorShortcuts;
    extern const char* GUI_LastSelectorWindowPosition;
    extern const char* GUI_SplitterSizes;
    extern const char* GUI_Toolbar;
    extern const char* GUI_Statusbar;
    extern const char* GUI_PreviewUpdate;
    extern const char* GUI_DetailsPageBoxes;
    extern const char* GUI_LastItemSelected;
    extern const char* GUI_GroupDefinitions;
    extern const char* GUI_HideFromManager;
    extern const char* GUI_PreventReconfiguration;
    extern const char* GUI_PreventSnapshotOperations;
    extern const char* GUI_HideDetails;

    /* Runtime UI declarations: */
#ifndef Q_WS_MAC
    extern const char* GUI_MachineWindowIcons;
    extern const char* GUI_MachineWindowNamePostfix;
#endif /* !Q_WS_MAC */
    extern const char* GUI_RestrictedRuntimeMenus;
#ifdef Q_WS_MAC
    extern const char* GUI_RestrictedRuntimeApplicationMenuActions;
#endif /* Q_WS_MAC */
    extern const char* GUI_RestrictedRuntimeMachineMenuActions;
    extern const char* GUI_RestrictedRuntimeViewMenuActions;
    extern const char* GUI_RestrictedRuntimeDevicesMenuActions;
#ifdef VBOX_WITH_DEBUGGER_GUI
    extern const char* GUI_RestrictedRuntimeDebuggerMenuActions;
#endif /* VBOX_WITH_DEBUGGER_GUI */
    extern const char* GUI_RestrictedRuntimeHelpMenuActions;
    extern const char* GUI_RestrictedVisualStates;
    extern const char* GUI_Input_MachineShortcuts;
    extern const char* GUI_LastNormalWindowPosition;
    extern const char* GUI_LastScaleWindowPosition;
    extern const char* GUI_LastWindowState_Max;
    extern const char* GUI_LastGuestSizeHint;
    extern const char* GUI_LastGuestSizeHintWasFullscreen;
    extern const char* GUI_Fullscreen;
    extern const char* GUI_Seamless;
    extern const char* GUI_Scale;
    extern const char* GUI_VirtualScreenToHostScreen;
    extern const char* GUI_AutoresizeGuest;
    extern const char* GUI_AutomountGuestScreens;
    extern const char* GUI_PassCAD;
    extern const char* GUI_DefaultCloseAction;
    extern const char* GUI_RestrictedStatusBarIndicators;
    extern const char* GUI_HidLedsSync;
    extern const char* GUI_GuruMeditationHandler;
    extern const char* GUI_HiDPI_Optimization;

    /* Runtime UI: Mini-toolbar declarations: */
    extern const char* GUI_ShowMiniToolBar;
    extern const char* GUI_MiniToolBarAlignment;
    extern const char* GUI_MiniToolBarAutoHide;

#ifdef Q_WS_MAC
    /* Runtime UI: Mac-dock declarations: */
    extern const char* GUI_RealtimeDockIconUpdateEnabled;
    extern const char* GUI_RealtimeDockIconUpdateMonitor;
    extern const char* GUI_PresentationModeEnabled;
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Runtime UI: Video-acceleration declarations: */
    extern const char* GUI_Accelerate2D_StretchLinear;
    extern const char* GUI_Accelerate2D_PixformatYV12;
    extern const char* GUI_Accelerate2D_PixformatUYVY;
    extern const char* GUI_Accelerate2D_PixformatYUY2;
    extern const char* GUI_Accelerate2D_PixformatAYUV;
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* Runtime UI: Close-dialog declarations: */
    extern const char* GUI_RestrictedCloseActions;
    extern const char* GUI_LastCloseAction;
    extern const char* GUI_CloseActionHook;

    /* Runtime UI: Information-dialog declarations: */
    extern const char* GUI_InfoDlgState;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Runtime UI: Debugger GUI declarations: */
    extern const char* GUI_Dbg_Enabled;
    extern const char* GUI_Dbg_AutoShow;
#endif /* VBOX_WITH_DEBUGGER_GUI */
}
using namespace UIExtraDataDefs /* if header included */;


/** Common UI: Global settings page types. */
enum GlobalSettingsPageType
{
    GlobalSettingsPageType_Invalid,
    GlobalSettingsPageType_General,
    GlobalSettingsPageType_Input,
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    GlobalSettingsPageType_Update,
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    GlobalSettingsPageType_Language,
    GlobalSettingsPageType_Display,
    GlobalSettingsPageType_Network,
    GlobalSettingsPageType_Extensions,
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    GlobalSettingsPageType_Proxy,
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    GlobalSettingsPageType_Max
};
Q_DECLARE_METATYPE(GlobalSettingsPageType);

/** Common UI: Machine settings page types. */
enum MachineSettingsPageType
{
    MachineSettingsPageType_Invalid,
    MachineSettingsPageType_General,
    MachineSettingsPageType_System,
    MachineSettingsPageType_Display,
    MachineSettingsPageType_Storage,
    MachineSettingsPageType_Audio,
    MachineSettingsPageType_Network,
    MachineSettingsPageType_Ports,
    MachineSettingsPageType_Serial,
    MachineSettingsPageType_Parallel,
    MachineSettingsPageType_USB,
    MachineSettingsPageType_SF,
    MachineSettingsPageType_Max
};
Q_DECLARE_METATYPE(MachineSettingsPageType);

/** Common UI: Wizard types. */
enum WizardType
{
    WizardType_Invalid,
    WizardType_NewVM,
    WizardType_CloneVM,
    WizardType_ExportAppliance,
    WizardType_ImportAppliance,
    WizardType_FirstRun,
    WizardType_NewVD,
    WizardType_CloneVD
};

/** Common UI: Wizard modes. */
enum WizardMode
{
    WizardMode_Auto,
    WizardMode_Basic,
    WizardMode_Expert
};


/** Selector UI: Details-element types. */
enum DetailsElementType
{
    DetailsElementType_Invalid,
    DetailsElementType_General,
    DetailsElementType_System,
    DetailsElementType_Preview,
    DetailsElementType_Display,
    DetailsElementType_Storage,
    DetailsElementType_Audio,
    DetailsElementType_Network,
    DetailsElementType_Serial,
#ifdef VBOX_WITH_PARALLEL_PORTS
    DetailsElementType_Parallel,
#endif /* VBOX_WITH_PARALLEL_PORTS */
    DetailsElementType_USB,
    DetailsElementType_SF,
    DetailsElementType_Description
};
Q_DECLARE_METATYPE(DetailsElementType);

/** Selector UI: Preview update interval types. */
enum PreviewUpdateIntervalType
{
    PreviewUpdateIntervalType_Disabled,
    PreviewUpdateIntervalType_500ms,
    PreviewUpdateIntervalType_1000ms,
    PreviewUpdateIntervalType_2000ms,
    PreviewUpdateIntervalType_5000ms,
    PreviewUpdateIntervalType_10000ms,
    PreviewUpdateIntervalType_Max
};


/** Runtime UI: Menu types. */
enum RuntimeMenuType
{
    RuntimeMenuType_Invalid = 0,
    RuntimeMenuType_Machine = RT_BIT(0),
    RuntimeMenuType_View    = RT_BIT(1),
    RuntimeMenuType_Devices = RT_BIT(2),
    RuntimeMenuType_Debug   = RT_BIT(3),
    RuntimeMenuType_Help    = RT_BIT(4),
    RuntimeMenuType_All     = 0xFF
};

#ifdef Q_WS_MAC
/** Runtime UI: Menu "Application": Action types. */
enum RuntimeMenuApplicationActionType
{
    RuntimeMenuApplicationActionType_Invalid     = 0,
    RuntimeMenuApplicationActionType_About       = RT_BIT(0),
    RuntimeMenuApplicationActionType_Preferences = RT_BIT(1),
    RuntimeMenuApplicationActionType_All         = 0xFFFF
};
#endif /* Q_WS_MAC */

/** Runtime UI: Menu "Machine": Action types. */
enum RuntimeMenuMachineActionType
{
    RuntimeMenuMachineActionType_Invalid           = 0,
    RuntimeMenuMachineActionType_SettingsDialog    = RT_BIT(0),
    RuntimeMenuMachineActionType_TakeSnapshot      = RT_BIT(1),
    RuntimeMenuMachineActionType_TakeScreenshot    = RT_BIT(2),
    RuntimeMenuMachineActionType_InformationDialog = RT_BIT(3),
    RuntimeMenuMachineActionType_KeyboardSettings  = RT_BIT(4),
    RuntimeMenuMachineActionType_MouseIntegration  = RT_BIT(5),
    RuntimeMenuMachineActionType_TypeCAD           = RT_BIT(6),
#ifdef Q_WS_X11
    RuntimeMenuMachineActionType_TypeCABS          = RT_BIT(7),
#endif /* Q_WS_X11 */
    RuntimeMenuMachineActionType_Pause             = RT_BIT(8),
    RuntimeMenuMachineActionType_Reset             = RT_BIT(9),
    RuntimeMenuMachineActionType_SaveState         = RT_BIT(10),
    RuntimeMenuMachineActionType_Shutdown          = RT_BIT(11),
    RuntimeMenuMachineActionType_PowerOff          = RT_BIT(12),
#ifndef Q_WS_MAC
    RuntimeMenuMachineActionType_Close             = RT_BIT(13),
#endif /* !Q_WS_MAC */
    RuntimeMenuMachineActionType_All               = 0xFFFF
};

/** Runtime UI: Menu "View": Action types. */
enum RuntimeMenuViewActionType
{
    RuntimeMenuViewActionType_Invalid         = 0,
    RuntimeMenuViewActionType_Fullscreen      = RT_BIT(0),
    RuntimeMenuViewActionType_Seamless        = RT_BIT(1),
    RuntimeMenuViewActionType_Scale           = RT_BIT(2),
    RuntimeMenuViewActionType_GuestAutoresize = RT_BIT(3),
    RuntimeMenuViewActionType_AdjustWindow    = RT_BIT(4),
    RuntimeMenuViewActionType_Multiscreen     = RT_BIT(5),
    RuntimeMenuViewActionType_All             = 0xFFFF
};

/** Runtime UI: Menu "Devices": Action types. */
enum RuntimeMenuDevicesActionType
{
    RuntimeMenuDevicesActionType_Invalid               = 0,
    RuntimeMenuDevicesActionType_OpticalDevices        = RT_BIT(0),
    RuntimeMenuDevicesActionType_FloppyDevices         = RT_BIT(1),
    RuntimeMenuDevicesActionType_USBDevices            = RT_BIT(2),
    RuntimeMenuDevicesActionType_WebCams               = RT_BIT(3),
    RuntimeMenuDevicesActionType_SharedClipboard       = RT_BIT(4),
    RuntimeMenuDevicesActionType_DragAndDrop           = RT_BIT(5),
    RuntimeMenuDevicesActionType_NetworkSettings       = RT_BIT(6),
    RuntimeMenuDevicesActionType_SharedFoldersSettings = RT_BIT(7),
    RuntimeMenuDevicesActionType_VRDEServer            = RT_BIT(8),
    RuntimeMenuDevicesActionType_VideoCapture          = RT_BIT(9),
    RuntimeMenuDevicesActionType_InstallGuestTools     = RT_BIT(10),
    RuntimeMenuDevicesActionType_All                   = 0xFFFF
};

#ifdef VBOX_WITH_DEBUGGER_GUI
/** Runtime UI: Menu "Debugger": Action types. */
enum RuntimeMenuDebuggerActionType
{
    RuntimeMenuDebuggerActionType_Invalid     = 0,
    RuntimeMenuDebuggerActionType_Statistics  = RT_BIT(0),
    RuntimeMenuDebuggerActionType_CommandLine = RT_BIT(1),
    RuntimeMenuDebuggerActionType_Logging     = RT_BIT(2),
    RuntimeMenuDebuggerActionType_LogDialog   = RT_BIT(3),
    RuntimeMenuDebuggerActionType_All         = 0xFFFF
};
#endif /* VBOX_WITH_DEBUGGER_GUI */

/** Runtime UI: Menu "Help": Action types. */
enum RuntimeMenuHelpActionType
{
    RuntimeMenuHelpActionType_Invalid              = 0,
    RuntimeMenuHelpActionType_Contents             = RT_BIT(0),
    RuntimeMenuHelpActionType_WebSite              = RT_BIT(1),
    RuntimeMenuHelpActionType_ResetWarnings        = RT_BIT(2),
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    RuntimeMenuHelpActionType_NetworkAccessManager = RT_BIT(3),
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef Q_WS_MAC
    RuntimeMenuHelpActionType_About                = RT_BIT(4),
    RuntimeMenuHelpActionType_Preferences          = RT_BIT(5),
#endif /* !Q_WS_MAC */
    RuntimeMenuHelpActionType_All                  = 0xFFFF
};

/** Runtime UI: Visual-state types. */
enum UIVisualStateType
{
    UIVisualStateType_Invalid    = 0,
    UIVisualStateType_Normal     = RT_BIT(0),
    UIVisualStateType_Fullscreen = RT_BIT(1),
    UIVisualStateType_Seamless   = RT_BIT(2),
    UIVisualStateType_Scale      = RT_BIT(3),
    UIVisualStateType_All        = 0xFF
};
Q_DECLARE_METATYPE(UIVisualStateType);

/** Runtime UI: Indicator types. */
enum IndicatorType
{
    IndicatorType_Invalid,
    IndicatorType_HardDisks,
    IndicatorType_OpticalDisks,
    IndicatorType_FloppyDisks,
    IndicatorType_Network,
    IndicatorType_USB,
    IndicatorType_SharedFolders,
    IndicatorType_VideoCapture,
    IndicatorType_Features,
    IndicatorType_Mouse,
    IndicatorType_Keyboard,
    IndicatorType_Max
};
Q_DECLARE_METATYPE(IndicatorType);

/** Runtime UI: Machine close actions. */
enum MachineCloseAction
{
    MachineCloseAction_Invalid                    = 0,
    MachineCloseAction_SaveState                  = RT_BIT(0),
    MachineCloseAction_Shutdown                   = RT_BIT(1),
    MachineCloseAction_PowerOff                   = RT_BIT(2),
    MachineCloseAction_PowerOff_RestoringSnapshot = RT_BIT(3),
    MachineCloseAction_All                        = 0xFF
};
Q_DECLARE_METATYPE(MachineCloseAction);

/** Guru Meditation handler types. */
enum GuruMeditationHandlerType
{
    GuruMeditationHandlerType_Default,
    GuruMeditationHandlerType_PowerOff,
    GuruMeditationHandlerType_Ignore
};

/** Runtime UI: HiDPI optimization types. */
enum HiDPIOptimizationType
{
    HiDPIOptimizationType_None,
    HiDPIOptimizationType_Performance
};

/** Runtime UI: Mini-toolbar alignment. */
enum MiniToolbarAlignment
{
    MiniToolbarAlignment_Bottom,
    MiniToolbarAlignment_Top
};

#endif /* !___UIExtraDataDefs_h___ */
