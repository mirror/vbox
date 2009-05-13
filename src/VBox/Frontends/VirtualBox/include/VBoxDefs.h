/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Header with common definitions and global functions
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxDefs_h__
#define __VBoxDefs_h__

/* Qt includes */
#include <QEvent>
#include <QUuid>

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

#if defined (VBOX_GUI_USE_QIMAGE) || \
    defined (VBOX_GUI_USE_SDL) || \
    defined (VBOX_GUI_USE_DDRAW)
  #if !defined (VBOX_GUI_USE_EXT_FRAMEBUFFER)
    #define VBOX_GUI_USE_EXT_FRAMEBUFFER
  #endif
#else
  #if defined (VBOX_GUI_USE_EXT_FRAMEBUFFER)
    #undef VBOX_GUI_USE_EXT_FRAMEBUFFER
  #endif
#endif

/** Null UUID constant to be used as a default value for reference parameters */
extern const QUuid QUuid_null;

/** Common namespace for all enums */
struct VBoxDefs
{
    /** Media type. */
    enum MediaType
    {
        MediaType_Invalid,
        MediaType_HardDisk,
        MediaType_DVD,
        MediaType_Floppy,
        MediaType_All
    };

    /** VM display rendering mode. */
    enum RenderMode
    {
        InvalidRenderMode, TimerMode, QImageMode, SDLMode, DDRAWMode, Quartz2DMode
    };

    /** Additional Qt event types. */
    enum
    {
        AsyncEventType = QEvent::User + 100,
        ResizeEventType,
        RepaintEventType,
        SetRegionEventType,
        MouseCapabilityEventType,
        MousePointerChangeEventType,
        MachineStateChangeEventType,
        AdditionsStateChangeEventType,
        MediaDriveChangeEventType,
        MachineDataChangeEventType,
        MachineRegisteredEventType,
        SessionStateChangeEventType,
        SnapshotEventType,
        CanShowRegDlgEventType,
        CanShowUpdDlgEventType,
        NetworkAdapterChangeEventType,
        USBCtlStateChangeEventType,
        USBDeviceStateChangeEventType,
        SharedFolderChangeEventType,
        RuntimeErrorEventType,
        ModifierKeyChangeEventType,
        MediaEnumEventType,
#if defined (Q_WS_WIN)
        ShellExecuteEventType,
#endif
        ActivateMenuEventType,
#if defined (Q_WS_MAC)
        ShowWindowEventType,
#endif
        ChangeGUILanguageEventType,
#if defined (VBOX_GUI_WITH_SYSTRAY)
        CanShowTrayIconEventType,
        ShowTrayIconEventType,
        TrayIconChangeEventType,
        MainWindowCountChangeEventType,
#endif
        AddVDMUrlsEventType,
        ChangeDockIconUpdateEventType
    };

    /** Size formatting types. */
    enum FormatSize
    {
        FormatSize_Round,
        FormatSize_RoundDown,
        FormatSize_RoundUp
    };

    static const char* GUI_LastWindowPosition;
    static const char* GUI_LastWindowPosition_Max;
    static const char* GUI_Fullscreen;
    static const char* GUI_Seamless;
    static const char* GUI_AutoresizeGuest;
    static const char* GUI_FirstRun;
    static const char* GUI_SaveMountedAtRuntime;
    static const char* GUI_LastCloseAction;
    static const char* GUI_SuppressMessages;
    static const char* GUI_PermanentSharedFoldersAtRuntime;
#ifdef Q_WS_X11
    static const char* GUI_LicenseKey;
#endif
    static const char* GUI_RegistrationDlgWinID;
    static const char* GUI_RegistrationData;
    static const char* GUI_UpdateDlgWinID;
    static const char* GUI_UpdateDate;
    static const char* GUI_UpdateCheckCount;
    static const char* GUI_LastVMSelected;
    static const char* GUI_InfoDlgState;
#ifdef VBOX_GUI_WITH_SYSTRAY
    static const char* GUI_TrayIconWinID;
    static const char* GUI_MainWindowCount;
#endif
#ifdef Q_WS_MAC
    static const char* GUI_RealtimeDockIconUpdateEnabled;
#endif /* Q_WS_MAC */
    static const char* GUI_PassCAD;
};

#define MAC_LEOPARD_STYLE defined(Q_WS_MAC) && (QT_VERSION >= 0x040300)

#endif // __VBoxDefs_h__

