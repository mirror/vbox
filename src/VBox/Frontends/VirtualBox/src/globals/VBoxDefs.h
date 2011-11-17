/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Header with common definitions and global functions
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxDefs_h__
#define __VBoxDefs_h__

/* Qt includes */
#include <QEvent>
#include <QUuid>
#include <QMetaType>

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#ifdef DEBUG

#define AssertWrapperOk(w)      \
    AssertMsg (w.isOk(), (#w " is not okay (RC=0x%08X)", w.lastRC()))
#define AssertWrapperOkMsg(w, m)      \
    AssertMsg (w.isOk(), (#w ": " m " (RC=0x%08X)", w.lastRC()))

#else /* #ifdef DEBUG */

#define AssertWrapperOk(w)          do {} while (0)
#define AssertWrapperOkMsg(w, m)    do {} while (0)

#endif /* #ifdef DEBUG */

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#endif

/**
 *  Common global VBox namespace.
 *  Later it will replace VBoxDefs struct used as global VBox namespace.
 */
namespace VBoxGlobalDefs
{
    extern const char *UI_ExtPackName;
}

/** Common namespace for all enums */
struct VBoxDefs
{
    /** Medium type. */
    enum MediumType
    {
        MediumType_Invalid,
        MediumType_HardDisk,
        MediumType_DVD,
        MediumType_Floppy,
        MediumType_All
    };

    /** VM display rendering mode. */
    enum RenderMode
    {
        InvalidRenderMode, TimerMode, QImageMode, SDLMode, DDRAWMode, Quartz2DMode
#ifdef VBOX_GUI_USE_QGLFB
        , QGLMode
#endif
    };

    /** Additional Qt event types. */
    enum
    {
          ResizeEventType = QEvent::User + 101
        , RepaintEventType
        , SetRegionEventType
        , ModifierKeyChangeEventType
        , MediaEnumEventType
#if defined (Q_WS_WIN)
        , ShellExecuteEventType
#endif
        , ActivateActionEventType
#if defined (Q_WS_MAC)
        , ShowWindowEventType
#endif
        , AddVDMUrlsEventType
#ifdef VBOX_GUI_USE_QGL
        , VHWACommandProcessType
#endif
    };

    /** Size formatting types. */
    enum FormatSize
    {
        FormatSize_Round,
        FormatSize_RoundDown,
        FormatSize_RoundUp
    };

    static const char* GUI_LastWindowPosition;
    static const char* GUI_LastNormalWindowPosition;
    static const char* GUI_LastScaleWindowPosition;
    static const char* GUI_LastWindowState_Max;
    static const char* GUI_SplitterSizes;
    static const char* GUI_Toolbar;
    static const char* GUI_Statusbar;
    static const char* GUI_LastGuestSizeHint;
    static const char* GUI_LastGuestSizeHintWasFullscreen;
    static const char* GUI_Fullscreen;
    static const char* GUI_Seamless;
    static const char* GUI_Scale;
    static const char* GUI_VirtualScreenToHostScreen;
    static const char* GUI_AutoresizeGuest;
    static const char* GUI_FirstRun;
    static const char* GUI_SaveMountedAtRuntime;
    static const char* GUI_ShowMiniToolBar;
    static const char* GUI_MiniToolBarAlignment;
    static const char* GUI_MiniToolBarAutoHide;
    static const char* GUI_LastCloseAction;
    static const char* GUI_RestrictedCloseActions;
    static const char* GUI_CloseActionHook;
    static const char* GUI_SuppressMessages;
    static const char* GUI_PermanentSharedFoldersAtRuntime;
    static const char* GUI_LanguageId;
    static const char* GUI_PreviewUpdate;
    static const char* GUI_DetailsPageBoxes;
    static const char* GUI_SelectorVMPositions;
    static const char* GUI_Input_MachineShortcuts;
    static const char* GUI_Input_SelectorShortcuts;
#ifdef Q_WS_X11
    static const char* GUI_LicenseKey;
#endif
    static const char* GUI_RegistrationDlgWinID;
    static const char* GUI_RegistrationData;
    static const char* GUI_UpdateDate;
    static const char* GUI_UpdateCheckCount;
    static const char* GUI_LastVMSelected;
    static const char* GUI_InfoDlgState;
    static const char* GUI_RenderMode;
#ifdef VBOX_GUI_WITH_SYSTRAY
    static const char* GUI_TrayIconWinID;
    static const char* GUI_TrayIconEnabled;
    static const char* GUI_MainWindowCount;
#endif
#ifdef Q_WS_MAC
    static const char* GUI_RealtimeDockIconUpdateEnabled;
    static const char* GUI_RealtimeDockIconUpdateMonitor;
    static const char* GUI_PresentationModeEnabled;
#endif /* Q_WS_MAC */
    static const char* GUI_PassCAD;
    static const char* GUI_Export_StorageType;
    static const char* GUI_Export_Username;
    static const char* GUI_Export_Hostname;
    static const char* GUI_Export_Bucket;
    static const char* GUI_PreventBetaWarning;

    static const char* GUI_RecentFolderHD;
    static const char* GUI_RecentFolderCD;
    static const char* GUI_RecentFolderFD;
    static const char* GUI_RecentListHD;
    static const char* GUI_RecentListCD;
    static const char* GUI_RecentListFD;

#ifdef VBOX_WITH_VIDEOHWACCEL
    static const char* GUI_Accelerate2D_StretchLinear;
    static const char* GUI_Accelerate2D_PixformatYV12;
    static const char* GUI_Accelerate2D_PixformatUYVY;
    static const char* GUI_Accelerate2D_PixformatYUY2;
    static const char* GUI_Accelerate2D_PixformatAYUV;
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
    static const char* GUI_DbgEnabled;
    static const char* GUI_DbgAutoShow;
#endif

    static QStringList VBoxFileExts;
    static QStringList VBoxExtPackFileExts;
    static QStringList OVFFileExts;
};

Q_DECLARE_METATYPE(VBoxDefs::MediumType);

#define MAC_LEOPARD_STYLE defined(Q_WS_MAC) && (QT_VERSION >= 0x040300)

#endif /* __VBoxDefs_h__ */

