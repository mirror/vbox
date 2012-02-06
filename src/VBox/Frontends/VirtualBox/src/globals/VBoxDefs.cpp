/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDefs implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include <VBoxDefs.h>

/* Global includes */
#include <QStringList>

/* VBoxGlobalDefs stuff: */
const char* VBoxGlobalDefs::UI_ExtPackName = "Oracle VM VirtualBox Extension Pack";

/* VBoxDefs stuff: */
const char* VBoxDefs::GUI_LastWindowPosition = "GUI/LastWindowPosition";
const char* VBoxDefs::GUI_LastNormalWindowPosition = "GUI/LastNormalWindowPosition";
const char* VBoxDefs::GUI_LastScaleWindowPosition = "GUI/LastScaleWindowPosition";
const char* VBoxDefs::GUI_LastWindowState_Max = "max";
const char* VBoxDefs::GUI_LastGuestSizeHint = "GUI/LastGuestSizeHint";
const char* VBoxDefs::GUI_LastGuestSizeHintWasFullscreen = "GUI/LastGuestSizeHintWasFullscreen";
const char* VBoxDefs::GUI_Toolbar = "GUI/Toolbar";
const char* VBoxDefs::GUI_Statusbar = "GUI/Statusbar";
const char* VBoxDefs::GUI_SplitterSizes = "GUI/SplitterSizes";
const char* VBoxDefs::GUI_Fullscreen = "GUI/Fullscreen";
const char* VBoxDefs::GUI_Seamless = "GUI/Seamless";
const char* VBoxDefs::GUI_Scale = "GUI/Scale";
const char* VBoxDefs::GUI_VirtualScreenToHostScreen = "GUI/VirtualScreenToHostScreen";
const char* VBoxDefs::GUI_AutoresizeGuest = "GUI/AutoresizeGuest";
const char* VBoxDefs::GUI_FirstRun = "GUI/FirstRun";
const char* VBoxDefs::GUI_SaveMountedAtRuntime = "GUI/SaveMountedAtRuntime";
const char* VBoxDefs::GUI_ShowMiniToolBar = "GUI/ShowMiniToolBar";
const char* VBoxDefs::GUI_MiniToolBarAlignment = "GUI/MiniToolBarAlignment";
const char* VBoxDefs::GUI_MiniToolBarAutoHide = "GUI/MiniToolBarAutoHide";
const char* VBoxDefs::GUI_LastCloseAction = "GUI/LastCloseAction";
const char* VBoxDefs::GUI_RestrictedCloseActions = "GUI/RestrictedCloseActions";
const char* VBoxDefs::GUI_CloseActionHook = "GUI/CloseActionHook";
const char* VBoxDefs::GUI_SuppressMessages = "GUI/SuppressMessages";
const char* VBoxDefs::GUI_InvertMessageOption = "GUI/InvertMessageOption";
const char* VBoxDefs::GUI_PermanentSharedFoldersAtRuntime = "GUI/PermanentSharedFoldersAtRuntime";
const char* VBoxDefs::GUI_LanguageId = "GUI/LanguageID";
const char* VBoxDefs::GUI_PreviewUpdate = "GUI/PreviewUpdate";
const char* VBoxDefs::GUI_DetailsPageBoxes = "GUI/DetailsPageBoxes";
const char* VBoxDefs::GUI_SelectorVMPositions = "GUI/SelectorVMPositions";
const char* VBoxDefs::GUI_Input_MachineShortcuts = "GUI/Input/MachineShortcuts";
const char* VBoxDefs::GUI_Input_SelectorShortcuts = "GUI/Input/SelectorShortcuts";
#ifdef Q_WS_X11
const char* VBoxDefs::GUI_LicenseKey = "GUI/LicenseAgreed";
#endif
const char* VBoxDefs::GUI_RegistrationDlgWinID = "GUI/RegistrationDlgWinID";
const char* VBoxDefs::GUI_RegistrationData = "GUI/SUNOnlineData";
const char* VBoxDefs::GUI_UpdateDate = "GUI/UpdateDate";
const char* VBoxDefs::GUI_UpdateCheckCount = "GUI/UpdateCheckCount";
const char* VBoxDefs::GUI_LastVMSelected = "GUI/LastVMSelected";
const char* VBoxDefs::GUI_InfoDlgState = "GUI/InfoDlgState";
const char* VBoxDefs::GUI_RenderMode = "GUI/RenderMode";
#ifdef VBOX_GUI_WITH_SYSTRAY
const char* VBoxDefs::GUI_TrayIconWinID = "GUI/TrayIcon/WinID";
const char* VBoxDefs::GUI_TrayIconEnabled = "GUI/TrayIcon/Enabled";
const char* VBoxDefs::GUI_MainWindowCount = "GUI/MainWindowCount";
#endif
#ifdef Q_WS_MAC
const char* VBoxDefs::GUI_RealtimeDockIconUpdateEnabled = "GUI/RealtimeDockIconUpdateEnabled";
const char* VBoxDefs::GUI_RealtimeDockIconUpdateMonitor = "GUI/RealtimeDockIconUpdateMonitor";
const char* VBoxDefs::GUI_PresentationModeEnabled = "GUI/PresentationModeEnabled";
#endif /* Q_WS_MAC */
const char* VBoxDefs::GUI_PassCAD = "GUI/PassCAD";
const char* VBoxDefs::GUI_Export_StorageType = "GUI/Export/StorageType";
const char* VBoxDefs::GUI_Export_Username = "GUI/Export/Username";
const char* VBoxDefs::GUI_Export_Hostname = "GUI/Export/Hostname";
const char* VBoxDefs::GUI_Export_Bucket = "GUI/Export/Bucket";
const char* VBoxDefs::GUI_PreventBetaWarning = "GUI/PreventBetaWarning";
const char* VBoxDefs::GUI_RecentFolderHD = "GUI/RecentFolderHD";
const char* VBoxDefs::GUI_RecentFolderCD = "GUI/RecentFolderCD";
const char* VBoxDefs::GUI_RecentFolderFD = "GUI/RecentFolderFD";
const char* VBoxDefs::GUI_RecentListHD = "GUI/RecentListHD";
const char* VBoxDefs::GUI_RecentListCD = "GUI/RecentListCD";
const char* VBoxDefs::GUI_RecentListFD = "GUI/RecentListFD";
#ifdef VBOX_WITH_VIDEOHWACCEL
const char* VBoxDefs::GUI_Accelerate2D_StretchLinear = "GUI/Accelerate2D/StretchLinear";
const char* VBoxDefs::GUI_Accelerate2D_PixformatYV12 = "GUI/Accelerate2D/PixformatYV12";
const char* VBoxDefs::GUI_Accelerate2D_PixformatUYVY = "GUI/Accelerate2D/PixformatUYVY";
const char* VBoxDefs::GUI_Accelerate2D_PixformatYUY2 = "GUI/Accelerate2D/PixformatYUY2";
const char* VBoxDefs::GUI_Accelerate2D_PixformatAYUV = "GUI/Accelerate2D/PixformatAYUV";
#endif
#ifdef VBOX_WITH_DEBUGGER_GUI
const char* VBoxDefs::GUI_DbgEnabled = "GUI/Dbg/Enabled";
const char* VBoxDefs::GUI_DbgAutoShow = "GUI/Dbg/AutoShow";
#endif

QStringList VBoxDefs::VBoxFileExts = QStringList() << "xml" << "vbox";
QStringList VBoxDefs::VBoxExtPackFileExts = QStringList() << "vbox-extpack";
QStringList VBoxDefs::OVFFileExts = QStringList() << "ovf" << "ova";

