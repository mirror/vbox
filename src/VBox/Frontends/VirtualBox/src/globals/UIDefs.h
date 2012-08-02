/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Global declarations and functions
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIDefs_h__
#define __UIDefs_h__

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
        , TimerMode
        , QImageMode
        , SDLMode
        , DDRAWMode
        , Quartz2DMode
#ifdef VBOX_GUI_USE_QGLFB
        , QGLMode
#endif /* VBOX_GUI_USE_QGLFB */
    };

    /* Additional Qt event types: */
    enum UIEventType
    {
          ResizeEventType = QEvent::User + 101
        , RepaintEventType
        , SetRegionEventType
        , ModifierKeyChangeEventType
        , MediaEnumEventType
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

    /* Machine-window declarations: */
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
    extern const char* GUI_SaveMountedAtRuntime;
    extern const char* GUI_PassCAD;

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

    /* Registration dialog declarations: */
    extern const char* GUI_RegistrationDlgWinID;
    extern const char* GUI_RegistrationData;

    /* Update manager declarations: */
    extern const char* GUI_UpdateDate;
    extern const char* GUI_UpdateCheckCount;

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

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Tray icon declarations: */
    extern const char* GUI_TrayIconWinID;
    extern const char* GUI_TrayIconEnabled;
    extern const char* GUI_MainWindowCount;
#endif /* VBOX_GUI_WITH_SYSTRAY */

    /* File extensions declarations: */
    extern QStringList VBoxFileExts;
    extern QStringList VBoxExtPackFileExts;
    extern QStringList OVFFileExts;
}
using namespace UIDefs /* globally */;

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

#endif // __UIDefs_h__

