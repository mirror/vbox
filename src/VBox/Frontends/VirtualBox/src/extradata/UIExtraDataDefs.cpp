/* $Id$ */
/** @file
 * VBox Qt GUI - Extra-data related definitions.
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
#include "UIExtraDataDefs.h"


/* General: */
const char *UIExtraDataDefs::GUI_EventHandlingType = "GUI/EventHandlingType";
const char *UIExtraDataDefs::GUI_RestrictedDialogs = "GUI/RestrictedDialogs";


/* Messaging: */
const char *UIExtraDataDefs::GUI_SuppressMessages = "GUI/SuppressMessages";
const char *UIExtraDataDefs::GUI_InvertMessageOption = "GUI/InvertMessageOption";
#if !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG)
const char *UIExtraDataDefs::GUI_PreventBetaWarning = "GUI/PreventBetaWarning";
#endif


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
/* Application Update: */
const char *UIExtraDataDefs::GUI_PreventApplicationUpdate = "GUI/PreventApplicationUpdate";
const char *UIExtraDataDefs::GUI_UpdateDate = "GUI/UpdateDate";
const char *UIExtraDataDefs::GUI_UpdateCheckCount = "GUI/UpdateCheckCount";
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */


/* Progress: */
const char *UIExtraDataDefs::GUI_Progress_LegacyMode = "GUI/Progress/LegacyMode";


/* Settings: */
const char *UIExtraDataDefs::GUI_Customizations = "GUI/Customizations";
const char *UIExtraDataDefs::GUI_RestrictedGlobalSettingsPages = "GUI/RestrictedGlobalSettingsPages";
const char *UIExtraDataDefs::GUI_RestrictedMachineSettingsPages = "GUI/RestrictedMachineSettingsPages";

/* Settings: General: */
const char *UIExtraDataDefs::GUI_HostScreenSaverDisabled = "GUI/HostScreenSaverDisabled";

/* Settings: Language: */
const char *UIExtraDataDefs::GUI_LanguageID = "GUI/LanguageID";

/* Settings: Display: */
const char *UIExtraDataDefs::GUI_MaxGuestResolution = "GUI/MaxGuestResolution";
const char *UIExtraDataDefs::GUI_ActivateHoveredMachineWindow = "GUI/ActivateHoveredMachineWindow";

/* Settings: Keyboard: */
const char *UIExtraDataDefs::GUI_Input_SelectorShortcuts = "GUI/Input/SelectorShortcuts";
const char *UIExtraDataDefs::GUI_Input_MachineShortcuts = "GUI/Input/MachineShortcuts";
const char *UIExtraDataDefs::GUI_Input_HostKeyCombination = "GUI/Input/HostKeyCombination";
const char *UIExtraDataDefs::GUI_Input_AutoCapture = "GUI/Input/AutoCapture";
const char *UIExtraDataDefs::GUI_RemapScancodes = "GUI/RemapScancodes";

/* Settings: Proxy: */
const char *UIExtraDataDefs::GUI_ProxySettings = "GUI/ProxySettings";

/* Settings: Storage: */
const char *UIExtraDataDefs::GUI_RecentFolderHD = "GUI/RecentFolderHD";
const char *UIExtraDataDefs::GUI_RecentFolderCD = "GUI/RecentFolderCD";
const char *UIExtraDataDefs::GUI_RecentFolderFD = "GUI/RecentFolderFD";
const char *UIExtraDataDefs::GUI_RecentListHD = "GUI/RecentListHD";
const char *UIExtraDataDefs::GUI_RecentListCD = "GUI/RecentListCD";
const char *UIExtraDataDefs::GUI_RecentListFD = "GUI/RecentListFD";

/* Settings: Network: */
const char *UIExtraDataDefs::GUI_RestrictedNetworkAttachmentTypes = "GUI/RestrictedNetworkAttachmentTypes";

/* VISO Creator: */
const char *UIExtraDataDefs::GUI_VISOCreator_RecentFolder   = "GUI/VISOCreator/RecentFolder";

/* VirtualBox Manager: */
const char *UIExtraDataDefs::GUI_LastSelectorWindowPosition = "GUI/LastWindowPosition";
const char *UIExtraDataDefs::GUI_SplitterSizes = "GUI/SplitterSizes";
const char *UIExtraDataDefs::GUI_Toolbar = "GUI/Toolbar";
const char *UIExtraDataDefs::GUI_Toolbar_Text = "GUI/Toolbar/Text";
const char *UIExtraDataDefs::GUI_Toolbar_MachineTools_Order = "GUI/Toolbar/MachineTools/Order";
const char *UIExtraDataDefs::GUI_Toolbar_GlobalTools_Order = "GUI/Toolbar/GlobalTools/Order";
const char *UIExtraDataDefs::GUI_Tools_LastItemsSelected = "GUI/Tools/LastItemsSelected";
const char *UIExtraDataDefs::GUI_Statusbar = "GUI/Statusbar";
const char *UIExtraDataDefs::GUI_GroupDefinitions = "GUI/GroupDefinitions";
const char *UIExtraDataDefs::GUI_LastItemSelected = "GUI/LastItemSelected";
const char *UIExtraDataDefs::GUI_DetailsPageBoxes = "GUI/DetailsPageBoxes";
const char *UIExtraDataDefs::GUI_PreviewUpdate = "GUI/PreviewUpdate";
const char *UIExtraDataDefs::GUI_Details_Elements = "GUI/Details/Elements";
const char *UIExtraDataDefs::GUI_Details_Elements_Preview_UpdateInterval = "GUI/Details/Elements/Preview/UpdateInterval";

/* Snapshot Manager: */
const char *UIExtraDataDefs::GUI_SnapshotManager_Details_Expanded = "GUI/SnapshotManager/Details/Expanded";

/* Virtual Media Manager: */
const char *UIExtraDataDefs::GUI_VirtualMediaManager_Details_Expanded = "GUI/VirtualMediaManager/Details/Expanded";
const char *UIExtraDataDefs::GUI_VirtualMediaManager_Search_Widget_Expanded = "GUI/VirtualMediaManager/SearchWidget/Expanded";

/* Host Network Manager: */
const char *UIExtraDataDefs::GUI_HostNetworkManager_Details_Expanded = "GUI/HostNetworkManager/Details/Expanded";

/* Cloud Profile Manager: */
const char *UIExtraDataDefs::GUI_CloudProfileManager_Details_Expanded = "GUI/CloudProfileManager/Details/Expanded";

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
/* Extra-data Manager: */
const char *UIExtraDataDefs::GUI_ExtraDataManager_Geometry = "GUI/ExtraDataManager/Geometry";
const char *UIExtraDataDefs::GUI_ExtraDataManager_SplitterHints = "GUI/ExtraDataManager/SplitterHints";
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

/* Wizards: */
const char *UIExtraDataDefs::GUI_HideDescriptionForWizards = "GUI/HideDescriptionForWizards";


/* Virtual Machine: */
const char *UIExtraDataDefs::GUI_HideFromManager = "GUI/HideFromManager";
const char *UIExtraDataDefs::GUI_HideDetails = "GUI/HideDetails";
const char *UIExtraDataDefs::GUI_PreventReconfiguration = "GUI/PreventReconfiguration";
const char *UIExtraDataDefs::GUI_PreventSnapshotOperations = "GUI/PreventSnapshotOperations";
const char *UIExtraDataDefs::GUI_FirstRun = "GUI/FirstRun";
const char *UIExtraDataDefs::GUI_MachineWindowIcons = "GUI/MachineWindowIcons";
#ifndef VBOX_WS_MAC
const char *UIExtraDataDefs::GUI_MachineWindowNamePostfix = "GUI/MachineWindowNamePostfix";
#endif
const char *UIExtraDataDefs::GUI_LastNormalWindowPosition = "GUI/LastNormalWindowPosition";
const char *UIExtraDataDefs::GUI_LastScaleWindowPosition = "GUI/LastScaleWindowPosition";
const char *UIExtraDataDefs::GUI_Geometry_State_Max = "max";
#ifndef VBOX_WS_MAC
const char *UIExtraDataDefs::GUI_MenuBar_Enabled = "GUI/MenuBar/Enabled";
#endif
const char *UIExtraDataDefs::GUI_MenuBar_ContextMenu_Enabled = "GUI/MenuBar/ContextMenu/Enabled";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeMenus = "GUI/RestrictedRuntimeMenus";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeApplicationMenuActions = "GUI/RestrictedRuntimeApplicationMenuActions";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeMachineMenuActions = "GUI/RestrictedRuntimeMachineMenuActions";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeViewMenuActions = "GUI/RestrictedRuntimeViewMenuActions";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeInputMenuActions = "GUI/RestrictedRuntimeInputMenuActions";
const char *UIExtraDataDefs::GUI_RestrictedRuntimeDevicesMenuActions = "GUI/RestrictedRuntimeDevicesMenuActions";
#ifdef VBOX_WITH_DEBUGGER_GUI
const char *UIExtraDataDefs::GUI_RestrictedRuntimeDebuggerMenuActions = "GUI/RestrictedRuntimeDebuggerMenuActions";
#endif
#ifdef VBOX_WS_MAC
const char *UIExtraDataDefs::GUI_RestrictedRuntimeWindowMenuActions = "GUI/RestrictedRuntimeWindowMenuActions";
#endif
const char *UIExtraDataDefs::GUI_RestrictedRuntimeHelpMenuActions = "GUI/RestrictedRuntimeHelpMenuActions";
const char *UIExtraDataDefs::GUI_RestrictedVisualStates = "GUI/RestrictedVisualStates";
const char *UIExtraDataDefs::GUI_Fullscreen = "GUI/Fullscreen";
const char *UIExtraDataDefs::GUI_Seamless = "GUI/Seamless";
const char *UIExtraDataDefs::GUI_Scale = "GUI/Scale";
#ifdef VBOX_WS_X11
const char *UIExtraDataDefs::GUI_Fullscreen_LegacyMode = "GUI/Fullscreen/LegacyMode";
const char *UIExtraDataDefs::GUI_DistinguishMachineWindowGroups = "GUI/DistinguishMachineWindowGroups";
#endif /* VBOX_WS_X11 */
const char *UIExtraDataDefs::GUI_AutoresizeGuest = "GUI/AutoresizeGuest";
const char *UIExtraDataDefs::GUI_LastVisibilityStatusForGuestScreen = "GUI/LastVisibilityStatusForGuestScreen";
const char *UIExtraDataDefs::GUI_LastGuestSizeHint = "GUI/LastGuestSizeHint";
const char *UIExtraDataDefs::GUI_VirtualScreenToHostScreen = "GUI/VirtualScreenToHostScreen";
const char *UIExtraDataDefs::GUI_AutomountGuestScreens = "GUI/AutomountGuestScreens";
#ifndef VBOX_WS_MAC
const char *UIExtraDataDefs::GUI_ShowMiniToolBar = "GUI/ShowMiniToolBar";
const char *UIExtraDataDefs::GUI_MiniToolBarAutoHide = "GUI/MiniToolBarAutoHide";
const char *UIExtraDataDefs::GUI_MiniToolBarAlignment = "GUI/MiniToolBarAlignment";
#endif /* !VBOX_WS_MAC */
const char *UIExtraDataDefs::GUI_StatusBar_Enabled = "GUI/StatusBar/Enabled";
const char *UIExtraDataDefs::GUI_StatusBar_ContextMenu_Enabled = "GUI/StatusBar/ContextMenu/Enabled";
const char *UIExtraDataDefs::GUI_RestrictedStatusBarIndicators = "GUI/RestrictedStatusBarIndicators";
const char *UIExtraDataDefs::GUI_StatusBar_IndicatorOrder = "GUI/StatusBar/IndicatorOrder";
#ifdef VBOX_WS_MAC
const char *UIExtraDataDefs::GUI_RealtimeDockIconUpdateEnabled = "GUI/RealtimeDockIconUpdateEnabled";
const char *UIExtraDataDefs::GUI_RealtimeDockIconUpdateMonitor = "GUI/RealtimeDockIconUpdateMonitor";
const char *UIExtraDataDefs::GUI_DockIconDisableOverlay = "GUI/DockIconDisableOverlay";
#endif /* VBOX_WS_MAC */
const char *UIExtraDataDefs::GUI_PassCAD = "GUI/PassCAD";
const char *UIExtraDataDefs::GUI_MouseCapturePolicy = "GUI/MouseCapturePolicy";
const char *UIExtraDataDefs::GUI_GuruMeditationHandler = "GUI/GuruMeditationHandler";
const char *UIExtraDataDefs::GUI_HidLedsSync = "GUI/HidLedsSync";
const char *UIExtraDataDefs::GUI_ScaleFactor = "GUI/ScaleFactor";
const char *UIExtraDataDefs::GUI_Scaling_Optimization = "GUI/Scaling/Optimization";

/* Virtual Machine: Session Information Dialog: */
const char *UIExtraDataDefs::GUI_SessionInformationDialogGeometry = "GUI/SessionInformationDialogGeometry";

/* Guest control UI: */
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerDialogGeometry = "GUI/GuestControl/FileManagerDialogGeometry";
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerVisiblePanels = "GUI/GuestControl/FileManagerVisiblePanels";
const char *UIExtraDataDefs::GUI_GuestControl_ProcessControlSplitterHints = "GUI/GuestControl/ProcessControlSplitterHints";
const char *UIExtraDataDefs::GUI_GuestControl_ProcessControlDialogGeometry = "GUI/GuestControl/ProcessControlDialogGeometry";

/* Soft Keyboard: */
const char *UIExtraDataDefs::GUI_SoftKeyboard_DialogGeometry = "GUI/SoftKeyboardDialogGeometry";
const char *UIExtraDataDefs::GUI_SoftKeyboard_ColorTheme  = "GUI/SoftKeyboardColorTheme";
const char *UIExtraDataDefs::GUI_SoftKeyboard_SelectedColorTheme  = "GUI/SoftKeyboardSelectedColorTheme";
const char *UIExtraDataDefs::GUI_SoftKeyboard_SelectedLayout  = "GUI/SoftKeyboardSelectedLayout";
const char *UIExtraDataDefs::GUI_SoftKeyboard_Options  = "GUI/SoftKeyboardOptions";
const char *UIExtraDataDefs::GUI_SoftKeyboard_HideNumPad = "GUI/SoftKeyboardHideNumPad";
const char *UIExtraDataDefs::GUI_SoftKeyboard_HideOSMenuKeys = "GUI/SoftKeyboardHideOSMenuKeys";
const char *UIExtraDataDefs::GUI_SoftKeyboard_HideMultimediaKeys = "GUI/SoftKeyboardHideMultimediaKeys";

/* File Manager options: */
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerOptions = "GUI/GuestControl/FileManagerOptions";
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerListDirectoriesFirst = "ListDirectoriesFirst";
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerShowDeleteConfirmation = "ShowDeleteConfimation";
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerShowHumanReadableSizes = "ShowHumanReadableSizes";
const char *UIExtraDataDefs::GUI_GuestControl_FileManagerShowHiddenObjects = "ShowHiddenObjects";

/* Virtual Machine: Close dialog: */
const char *UIExtraDataDefs::GUI_DefaultCloseAction = "GUI/DefaultCloseAction";
const char *UIExtraDataDefs::GUI_RestrictedCloseActions = "GUI/RestrictedCloseActions";
const char *UIExtraDataDefs::GUI_LastCloseAction = "GUI/LastCloseAction";
const char *UIExtraDataDefs::GUI_CloseActionHook = "GUI/CloseActionHook";

#ifdef VBOX_WITH_DEBUGGER_GUI
/* Virtual Machine: Debug UI: */
const char *UIExtraDataDefs::GUI_Dbg_Enabled = "GUI/Dbg/Enabled";
const char *UIExtraDataDefs::GUI_Dbg_AutoShow = "GUI/Dbg/AutoShow";
#endif /* VBOX_WITH_DEBUGGER_GUI */

/* Virtual Machine: Log-viewer: */
const char *UIExtraDataDefs::GUI_LogWindowGeometry = "GUI/LogWindowGeometry";
const char *UIExtraDataDefs::GUI_LogViewerOptions = "GUI/LogViewerOptions";
const char *UIExtraDataDefs::GUI_LogViewerWrapLinesEnabled = "WrapLines";
const char *UIExtraDataDefs::GUI_LogViewerShowLineNumbersDisabled = "showLineNumbersDisabled";
const char *UIExtraDataDefs::GUI_LogViewerNoFontStyleName = "noFontStyleName";
const char *UIExtraDataDefs::GUI_GuestControl_LogViewerVisiblePanels = "GUI/LogViewerVisiblePanels";

/* VM Resource Monitor: */
const char *UIExtraDataDefs::GUI_VMResourceManager_HiddenColumns = "GUI/VMResourceManagerHiddenColumns";

/* Obsolete keys: */
QMap<QString, QString> UIExtraDataDefs::prepareObsoleteKeysMap()
{
    QMap<QString, QString> map;
    map.insertMulti(GUI_Details_Elements, GUI_DetailsPageBoxes);
    map.insertMulti(GUI_Details_Elements_Preview_UpdateInterval, GUI_PreviewUpdate);
    return map;
}
QMap<QString, QString> UIExtraDataDefs::g_mapOfObsoleteKeys = UIExtraDataDefs::prepareObsoleteKeysMap();


bool UIToolStuff::isTypeOfClass(UIToolType enmType, UIToolClass enmClass)
{
    switch (enmClass)
    {
        case UIToolClass_Global:
        {
            switch (enmType)
            {
                case UIToolType_Welcome:
                case UIToolType_Media:
                case UIToolType_Network:
                case UIToolType_Cloud:
                case UIToolType_VMResourceMonitor:
                    return true;
                default:
                    break;
            }
            break;
        }
        case UIToolClass_Machine:
        {
            switch (enmType)
            {
                case UIToolType_Details:
                case UIToolType_Snapshots:
                case UIToolType_Logs:
                    return true;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
    return false;
}
