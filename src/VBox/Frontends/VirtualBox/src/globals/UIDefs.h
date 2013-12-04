/** @file
 * VBox Qt GUI - UIDefs namespace and other global declarations.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDefs_h___
#define ___UIDefs_h___

/* Qt includes: */
#include <QEvent>
#include <QStringList>

/* COM includes: */
#include "COMEnums.h"

/* Other VBox defines: */
#define LOG_GROUP LOG_GROUP_GUI

/* Other VBox includes: */
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

/* Defines: */
#define MAC_LEOPARD_STYLE defined(Q_WS_MAC) && (QT_VERSION >= 0x040300)

#ifdef DEBUG
# define AssertWrapperOk(w)      \
    AssertMsg (w.isOk(), (#w " is not okay (RC=0x%08X)", w.lastRC()))
# define AssertWrapperOkMsg(w, m)      \
    AssertMsg (w.isOk(), (#w ": " m " (RC=0x%08X)", w.lastRC()))
#else /* !DEBUG */
# define AssertWrapperOk(w)          do {} while (0)
# define AssertWrapperOkMsg(w, m)    do {} while (0)
#endif /* DEBUG */

#ifndef SIZEOF_ARRAY
# define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#endif /* SIZEOF_ARRAY */

/* Global UI namespace: */
namespace UIDefs
{
    /* VM display rendering mode: */
    enum RenderMode
    {
          InvalidRenderMode
        , QImageMode
#ifdef VBOX_GUI_USE_QUARTZ2D
        , Quartz2DMode
#endif /* VBOX_GUI_USE_QUARTZ2D */
    };

    /* Additional Qt event types: */
    enum UIEventType
    {
          ResizeEventType = QEvent::User + 101
        , SetRegionEventType
        , ModifierKeyChangeEventType
#ifdef Q_WS_WIN
        , ShellExecuteEventType
#endif /* Q_WS_WIN */
        , ActivateActionEventType
#ifdef Q_WS_MAC
        , ShowWindowEventType
#endif /* Q_WS_MAC */
        , AddVDMUrlsEventType
#ifdef VBOX_GUI_USE_QGL
        , VHWACommandProcessType
#endif /* VBOX_GUI_USE_QGL */
    };

    /* Size formatting types: */
    enum FormatSize
    {
        FormatSize_Round,
        FormatSize_RoundDown,
        FormatSize_RoundUp
    };

    /* Global declarations: */
    extern const char* GUI_RenderMode;
    extern const char* GUI_LanguageId;
    extern const char* GUI_ExtPackName;
    extern const char* GUI_PreventBetaWarning;
    extern const char* GUI_RecentFolderHD;
    extern const char* GUI_RecentFolderCD;
    extern const char* GUI_RecentFolderFD;
    extern const char* GUI_RecentListHD;
    extern const char* GUI_RecentListCD;
    extern const char* GUI_RecentListFD;

    /* Selector-window declarations: */
    extern const char* GUI_Input_SelectorShortcuts;
    extern const char* GUI_LastSelectorWindowPosition;
    extern const char* GUI_SplitterSizes;
    extern const char* GUI_Toolbar;
    extern const char* GUI_Statusbar;
    extern const char* GUI_PreviewUpdate;
    extern const char* GUI_DetailsPageBoxes;
    extern const char* GUI_SelectorVMPositions;
    extern const char* GUI_LastVMSelected;
    extern const char* GUI_LastItemSelected;
    extern const char* GUI_GroupDefinitions;
    extern const char* GUI_HideFromManager;
    extern const char* GUI_PreventReconfiguration;
    extern const char* GUI_PreventSnapshotOperations;
    extern const char* GUI_HideDetails;

    /* Machine-window declarations: */
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
    extern const char* GUI_SaveMountedAtRuntime;
    extern const char* GUI_PassCAD;
    extern const char* GUI_DefaultCloseAction;
    extern const char* GUI_RestrictedStatusBarIndicators;
    extern const char* GUI_HidLedsSync;

    /* Settings dialogs stuff: */
    extern const char* GUI_RestrictedGlobalSettingsPages;
    extern const char* GUI_RestrictedMachineSettingsPages;

    /* Mini tool-bar declarations: */
    extern const char* GUI_ShowMiniToolBar;
    extern const char* GUI_MiniToolBarAlignment;
    extern const char* GUI_MiniToolBarAutoHide;

    /* Close-dialog declarations: */
    extern const char* GUI_RestrictedCloseActions;
    extern const char* GUI_LastCloseAction;
    extern const char* GUI_CloseActionHook;

    /* Wizards declarations: */
    extern const char* GUI_FirstRun;
    extern const char* GUI_HideDescriptionForWizards;
    extern const char* GUI_Export_StorageType;
    extern const char* GUI_Export_Username;
    extern const char* GUI_Export_Hostname;
    extern const char* GUI_Export_Bucket;

    /* Message-center declarations: */
    extern const char* GUI_SuppressMessages;
    extern const char* GUI_InvertMessageOption;

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Update manager declarations: */
    extern const char* GUI_PreventApplicationUpdate;
    extern const char* GUI_UpdateDate;
    extern const char* GUI_UpdateCheckCount;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Information dialog declarations: */
    extern const char* GUI_InfoDlgState;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debugger GUI declarations: */
    extern const char* GUI_DbgEnabled;
    extern const char* GUI_DbgAutoShow;
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_X11
    /* License GUI declarations: */
    extern const char* GUI_LicenseKey;
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    /* Mac dock declarations: */
    extern const char* GUI_RealtimeDockIconUpdateEnabled;
    extern const char* GUI_RealtimeDockIconUpdateMonitor;
    extern const char* GUI_PresentationModeEnabled;
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Video-acceleration declarations: */
    extern const char* GUI_Accelerate2D_StretchLinear;
    extern const char* GUI_Accelerate2D_PixformatYV12;
    extern const char* GUI_Accelerate2D_PixformatUYVY;
    extern const char* GUI_Accelerate2D_PixformatYUY2;
    extern const char* GUI_Accelerate2D_PixformatAYUV;
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* File extensions declarations: */
    extern QStringList VBoxFileExts;
    extern QStringList VBoxExtPackFileExts;
    extern QStringList OVFFileExts;
}
using namespace UIDefs /* globally */;

#ifdef Q_WS_MAC
/** Known MacOS X releases. */
enum MacOSXRelease
{
    MacOSXRelease_Unknown,
    MacOSXRelease_SnowLeopard,
    MacOSXRelease_Lion,
    MacOSXRelease_MountainLion,
    MacOSXRelease_Mavericks
};
#endif /* Q_WS_MAC */

struct StorageSlot
{
    StorageSlot() : bus(KStorageBus_Null), port(0), device(0) {}
    StorageSlot(const StorageSlot &other) : bus(other.bus), port(other.port), device(other.device) {}
    StorageSlot(KStorageBus otherBus, LONG iPort, LONG iDevice) : bus(otherBus), port(iPort), device(iDevice) {}
    StorageSlot& operator=(const StorageSlot &other) { bus = other.bus; port = other.port; device = other.device; return *this; }
    bool operator==(const StorageSlot &other) const { return bus == other.bus && port == other.port && device == other.device; }
    bool operator!=(const StorageSlot &other) const { return bus != other.bus || port != other.port || device != other.device; }
    bool operator<(const StorageSlot &other) const { return (bus <  other.bus) ||
                                                            (bus == other.bus && port <  other.port) ||
                                                            (bus == other.bus && port == other.port && device < other.device); }
    bool operator>(const StorageSlot &other) const { return (bus >  other.bus) ||
                                                            (bus == other.bus && port >  other.port) ||
                                                            (bus == other.bus && port == other.port && device > other.device); }
    bool isNull() const { return bus == KStorageBus_Null; }
    KStorageBus bus; LONG port; LONG device;
};
Q_DECLARE_METATYPE(StorageSlot);

/* Common UI size suffixes: */
enum SizeSuffix
{
    SizeSuffix_Byte = 0,
    SizeSuffix_KiloByte,
    SizeSuffix_MegaByte,
    SizeSuffix_GigaByte,
    SizeSuffix_TeraByte,
    SizeSuffix_PetaByte,
    SizeSuffix_Max
};

/* Runtime UI menu types: */
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
/** Runtime UI: Application menu: Action types. */
enum RuntimeMenuApplicationActionType
{
    RuntimeMenuApplicationActionType_Invalid = 0,
    RuntimeMenuApplicationActionType_About   = RT_BIT(0),
    RuntimeMenuApplicationActionType_All     = 0xFFFF
};
#endif /* Q_WS_MAC */

/** Runtime UI: Machine menu: Action types. */
enum RuntimeMenuMachineActionType
{
    RuntimeMenuMachineActionType_Invalid           = 0,
    RuntimeMenuMachineActionType_SettingsDialog    = RT_BIT(0),
    RuntimeMenuMachineActionType_TakeSnapshot      = RT_BIT(1),
    RuntimeMenuMachineActionType_TakeScreenshot    = RT_BIT(2),
    RuntimeMenuMachineActionType_InformationDialog = RT_BIT(3),
    RuntimeMenuMachineActionType_MouseIntegration  = RT_BIT(4),
    RuntimeMenuMachineActionType_TypeCAD           = RT_BIT(5),
#ifdef Q_WS_X11
    RuntimeMenuMachineActionType_TypeCABS          = RT_BIT(6),
#endif /* Q_WS_X11 */
    RuntimeMenuMachineActionType_Pause             = RT_BIT(7),
    RuntimeMenuMachineActionType_Reset             = RT_BIT(8),
    RuntimeMenuMachineActionType_SaveState         = RT_BIT(9),
    RuntimeMenuMachineActionType_Shutdown          = RT_BIT(10),
    RuntimeMenuMachineActionType_PowerOff          = RT_BIT(11),
#ifndef Q_WS_MAC
    RuntimeMenuMachineActionType_Close             = RT_BIT(12),
#endif /* !Q_WS_MAC */
    RuntimeMenuMachineActionType_All               = 0xFFFF
};

/** Runtime UI: View menu: Action types. */
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

/** Runtime UI: Devices menu: Action types. */
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
/** Runtime UI: Debugger menu: Action types. */
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

/** Runtime UI: Help menu: Action types. */
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
#endif /* !Q_WS_MAC */
    RuntimeMenuHelpActionType_All                  = 0xFFFF
};

/* Runtime UI visual-state types: */
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

/* Details element type: */
enum DetailsElementType
{
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

/* Global settings page type: */
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

/* Machine settings page type: */
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

/* Indicator type: */
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

/* Machine close action: */
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

#endif /* !___UIDefs_h___ */

