/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolRuntime class implementation
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

/* Local includes: */
#include "UIActionPoolRuntime.h"
#include "UIShortcuts.h"
#include "VBoxGlobal.h"

class MenuMachineAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuMachineAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Machine"));
    }
};

class ShowSettingsDialogAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowSettingsDialogAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/settings_16px.png", ":/settings_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("SettingsDialog");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("S");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Manage the virtual machine settings"));
    }
};

class PerformTakeSnapshotAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformTakeSnapshotAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/take_snapshot_16px.png", ":/take_snapshot_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("TakeSnapshot");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("T");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Take Sn&apshot..."));
        setStatusTip(QApplication::translate("UIActionPool", "Take a snapshot of the virtual machine"));
    }
};

class PerformTakeScreenshotAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformTakeScreenshotAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/take_screenshot_16px.png", ":/take_screenshot_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("TakeScreenshot");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("E");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Take Screensh&ot..."));
        setStatusTip(QApplication::translate("UIActionPool", "Take a screenshot of the virtual machine"));
    }
};

class ShowInformationDialogAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowInformationDialogAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/session_info_16px.png", ":/session_info_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("InformationDialog");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("N");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Session I&nformation..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show Session Information Dialog"));
    }
};

class MenuMouseIntegrationAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuMouseIntegrationAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class ToggleMouseIntegrationAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleMouseIntegrationAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/mouse_can_seamless_on_16px.png", ":/mouse_can_seamless_16px.png",
                         ":/mouse_can_seamless_on_disabled_16px.png", ":/mouse_can_seamless_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("MouseIntegration");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("I");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Disable &Mouse Integration"));
        setStatusTip(QApplication::translate("UIActionPool", "Temporarily disable host mouse pointer integration"));
    }
};

class PerformTypeCADAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformTypeCADAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/hostkey_16px.png", ":/hostkey_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("TypeCAD");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("Del");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Insert Ctrl-Alt-Del"));
        setStatusTip(QApplication::translate("UIActionPool", "Send the Ctrl-Alt-Del sequence to the virtual machine"));
    }
};

#ifdef Q_WS_X11
class PerformTypeCABSAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformTypeCABSAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/hostkey_16px.png", ":/hostkey_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("TypeCABS");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("Backspace");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Ins&ert Ctrl-Alt-Backspace"));
        setStatusTip(QApplication::translate("UIActionPool", "Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
    }
};
#endif /* Q_WS_X11 */

class TogglePauseAction : public UIActionToggle
{
    Q_OBJECT;

public:

    TogglePauseAction(UIActionPool *pParent)
        : UIActionToggle(pParent, ":/pause_16px.png", ":/pause_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Pause");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("P");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Pause"));
        setStatusTip(QApplication::translate("UIActionPool", "Suspend the execution of the virtual machine"));
    }
};

class PerformResetAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformResetAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/reset_16px.png", ":/reset_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Reset");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("R");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Reset"));
        setStatusTip(QApplication::translate("UIActionPool", "Reset the virtual machine"));
    }
};

class PerformShutdownAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformShutdownAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/acpi_16px.png", ":/acpi_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Shutdown");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
#ifdef Q_WS_MAC
        return QKeySequence("U");
#else /* Q_WS_MAC */
        return QKeySequence("H");
#endif /* !Q_WS_MAC */
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "ACPI Sh&utdown"));
        setStatusTip(QApplication::translate("UIActionPool", "Send the ACPI Power Button press event to the virtual machine"));
    }
};

class PerformCloseAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformCloseAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Close");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("Q");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Close..."));
        setStatusTip(QApplication::translate("UIActionPool", "Close the virtual machine"));
    }
};

class MenuViewAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuViewAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&View"));
    }
};

class ToggleFullscreenModeAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleFullscreenModeAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/fullscreen_on_16px.png", ":/fullscreen_16px.png",
                         ":/fullscreen_on_disabled_16px.png", ":/fullscreen_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("FullscreenMode");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("F");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Switch to &Fullscreen"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch between normal and fullscreen mode"));
    }
};

class ToggleSeamlessModeAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleSeamlessModeAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/seamless_on_16px.png", ":/seamless_16px.png",
                         ":/seamless_on_disabled_16px.png", ":/seamless_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("SeamlessMode");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("L");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Switch to Seam&less Mode"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch between normal and seamless desktop integration mode"));
    }
};

class ToggleScaleModeAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleScaleModeAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/scale_on_16px.png", ":/scale_16px.png",
                         ":/scale_on_disabled_16px.png", ":/scale_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("ScaleMode");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("C");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Switch to &Scale Mode"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch between normal and scale mode"));
    }
};

class ToggleGuestAutoresizeAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleGuestAutoresizeAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/auto_resize_on_on_16px.png", ":/auto_resize_on_16px.png",
                         ":/auto_resize_on_on_disabled_16px.png", ":/auto_resize_on_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("GuestAutoresize");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("G");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Auto-resize &Guest Display"));
        setStatusTip(QApplication::translate("UIActionPool", "Automatically resize the guest display when the window is resized (requires Guest Additions)"));
    }
};

class PerformWindowAdjustAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformWindowAdjustAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/adjust_win_size_16px.png", ":/adjust_win_size_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("WindowAdjust");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("A");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Adjust Window Size"));
        setStatusTip(QApplication::translate("UIActionPool", "Adjust window size and position to best fit the guest display"));
    }
};

class MenuDevicesAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuDevicesAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Devices"));
    }
};

class MenuOpticalDevicesAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuOpticalDevicesAction(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/cd_16px.png", ":/cd_disabled_16px.png")
    {
        qobject_cast<UIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&CD/DVD Devices"));
    }
};

class MenuFloppyDevicesAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuFloppyDevicesAction(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/fd_16px.png", ":/fd_disabled_16px.png")
    {
        qobject_cast<UIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Floppy Devices"));
    }
};

class MenuUSBDevicesAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuUSBDevicesAction(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/usb_16px.png", ":/usb_disabled_16px.png")
    {
        qobject_cast<UIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&USB Devices"));
    }
};

class MenuSharedClipboardAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuSharedClipboardAction(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Shared &Clipboard"));
    }
};

class MenuDragAndDropAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuDragAndDropAction(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Drag'n'Drop"));
    }
};

class MenuNetworkAdaptersAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuNetworkAdaptersAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class ShowNetworkAdaptersDialogAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowNetworkAdaptersDialogAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/nw_16px.png", ":/nw_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("NetworkAdaptersDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Network Adapters..."));
        setStatusTip(QApplication::translate("UIActionPool", "Change the settings of network adapters"));
    }
};

class MenuSharedFoldersAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuSharedFoldersAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class ShowSharedFoldersDialogAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowSharedFoldersDialogAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("SharedFoldersDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Shared Folders..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create or modify shared folders"));
    }
};

class ToggleVRDEServerAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleVRDEServerAction(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/vrdp_on_16px.png", ":/vrdp_16px.png",
                         ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("VRDPServer");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Enable R&emote Display"));
        setStatusTip(QApplication::translate("UIActionPool", "Enable remote desktop (RDP) connections to this machine"));
    }
};

class PerformInstallGuestToolsAction : public UIActionSimple
{
    Q_OBJECT;

public:

    PerformInstallGuestToolsAction(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/guesttools_16px.png", ":/guesttools_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("InstallGuestAdditions");
    }

    QKeySequence defaultShortcut(UIActionPoolType) const
    {
        return QKeySequence("D");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Install Guest Additions..."));
        setStatusTip(QApplication::translate("UIActionPool", "Mount the Guest Additions installation image"));
    }
};

#ifdef VBOX_WITH_DEBUGGER_GUI
class MenuDebugAction : public UIActionMenu
{
    Q_OBJECT;

public:

    MenuDebugAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "De&bug"));
    }
};

class ShowStatisticsAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowStatisticsAction(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("StatisticWindow");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Statistics...", "debug action"));
    }
};

class ShowCommandLineAction : public UIActionSimple
{
    Q_OBJECT;

public:

    ShowCommandLineAction(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("CommandLineWindow");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Command Line...", "debug action"));
    }
};

class ToggleLoggingAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleLoggingAction(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Logging");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Enable &Logging...", "debug action"));
    }
};
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef RT_OS_DARWIN
class DockMenuAction : public UIActionMenu
{
    Q_OBJECT;

public:

    DockMenuAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class DockSettingsMenuAction : public UIActionMenu
{
    Q_OBJECT;

public:

    DockSettingsMenuAction(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Dock Icon"));
    }
};

class ToggleDockPreviewMonitorAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleDockPreviewMonitorAction(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("DockPreviewMonitor");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Show Monitor Preview"));
    }
};

class ToggleDockDisableMonitorAction : public UIActionToggle
{
    Q_OBJECT;

public:

    ToggleDockDisableMonitorAction(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("DockDisableMonitor");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Show Application Icon"));
    }
};
#endif /* Q_WS_MAC */


QString UIActionPoolRuntime::shortcutsExtraDataID() const
{
    return GUI_Input_MachineShortcuts;
}

void UIActionPoolRuntime::createActions()
{
    /* Global actions creation: */
    UIActionPool::createActions();

    /* 'Machine' actions: */
    m_pool[UIActionIndexRuntime_Simple_SettingsDialog] = new ShowSettingsDialogAction(this);
    m_pool[UIActionIndexRuntime_Simple_TakeSnapshot] = new PerformTakeSnapshotAction(this);
    m_pool[UIActionIndexRuntime_Simple_TakeScreenshot] = new PerformTakeScreenshotAction(this);
    m_pool[UIActionIndexRuntime_Simple_InformationDialog] = new ShowInformationDialogAction(this);
    m_pool[UIActionIndexRuntime_Toggle_MouseIntegration] = new ToggleMouseIntegrationAction(this);
    m_pool[UIActionIndexRuntime_Simple_TypeCAD] = new PerformTypeCADAction(this);
#ifdef Q_WS_X11
    m_pool[UIActionIndexRuntime_Simple_TypeCABS] = new PerformTypeCABSAction(this);
#endif /* Q_WS_X11 */
    m_pool[UIActionIndexRuntime_Toggle_Pause] = new TogglePauseAction(this);
    m_pool[UIActionIndexRuntime_Simple_Reset] = new PerformResetAction(this);
    m_pool[UIActionIndexRuntime_Simple_Shutdown] = new PerformShutdownAction(this);
    m_pool[UIActionIndexRuntime_Simple_Close] = new PerformCloseAction(this);

    /* 'View' actions: */
    m_pool[UIActionIndexRuntime_Toggle_Fullscreen] = new ToggleFullscreenModeAction(this);
    m_pool[UIActionIndexRuntime_Toggle_Seamless] = new ToggleSeamlessModeAction(this);
    m_pool[UIActionIndexRuntime_Toggle_Scale] = new ToggleScaleModeAction(this);
    m_pool[UIActionIndexRuntime_Toggle_GuestAutoresize] = new ToggleGuestAutoresizeAction(this);
    m_pool[UIActionIndexRuntime_Simple_AdjustWindow] = new PerformWindowAdjustAction(this);

    /* 'Devices' actions: */
    m_pool[UIActionIndexRuntime_Simple_NetworkAdaptersDialog] = new ShowNetworkAdaptersDialogAction(this);
    m_pool[UIActionIndexRuntime_Simple_SharedFoldersDialog] = new ShowSharedFoldersDialogAction(this);
    m_pool[UIActionIndexRuntime_Toggle_VRDEServer] = new ToggleVRDEServerAction(this);
    m_pool[UIActionIndexRuntime_Simple_InstallGuestTools] = new PerformInstallGuestToolsAction(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' actions: */
    m_pool[UIActionIndexRuntime_Simple_Statistics] = new ShowStatisticsAction(this);
    m_pool[UIActionIndexRuntime_Simple_CommandLine] = new ShowCommandLineAction(this);
    m_pool[UIActionIndexRuntime_Toggle_Logging] = new ToggleLoggingAction(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' actions: */
    m_pool[UIActionIndexRuntime_Toggle_DockPreviewMonitor] = new ToggleDockPreviewMonitorAction(this);
    m_pool[UIActionIndexRuntime_Toggle_DockDisableMonitor] = new ToggleDockDisableMonitorAction(this);
#endif /* Q_WS_MAC */
}

void UIActionPoolRuntime::createMenus()
{
    /* Global menus creation: */
    UIActionPool::createMenus();

    /* On Mac OS X, all QMenu's are consumed by Qt after they are added to another QMenu or a QMenuBar.
     * This means we have to recreate all QMenus when creating a new QMenuBar.
     * For simplicity we doing this on all platforms right now. */

    /* Recreate 'close' item as well.
     * This makes sure it is removed also from the Application menu: */
    if (m_pool[UIActionIndexRuntime_Simple_Close])
        delete m_pool[UIActionIndexRuntime_Simple_Close];
    m_pool[UIActionIndexRuntime_Simple_Close] = new PerformCloseAction(this);

    /* 'Machine' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Machine])
        delete m_pool[UIActionIndexRuntime_Menu_Machine];
    m_pool[UIActionIndexRuntime_Menu_Machine] = new MenuMachineAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_MouseIntegration])
        delete m_pool[UIActionIndexRuntime_Menu_MouseIntegration];
    m_pool[UIActionIndexRuntime_Menu_MouseIntegration] = new MenuMouseIntegrationAction(this);

    /* 'View' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_View])
        delete m_pool[UIActionIndexRuntime_Menu_View];
    m_pool[UIActionIndexRuntime_Menu_View] = new MenuViewAction(this);

    /* 'Devices' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Devices])
        delete m_pool[UIActionIndexRuntime_Menu_Devices];
    m_pool[UIActionIndexRuntime_Menu_Devices] = new MenuDevicesAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_OpticalDevices])
        delete m_pool[UIActionIndexRuntime_Menu_OpticalDevices];
    m_pool[UIActionIndexRuntime_Menu_OpticalDevices] = new MenuOpticalDevicesAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_FloppyDevices])
        delete m_pool[UIActionIndexRuntime_Menu_FloppyDevices];
    m_pool[UIActionIndexRuntime_Menu_FloppyDevices] = new MenuFloppyDevicesAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_USBDevices])
        delete m_pool[UIActionIndexRuntime_Menu_USBDevices];
    m_pool[UIActionIndexRuntime_Menu_USBDevices] = new MenuUSBDevicesAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_SharedClipboard])
        delete m_pool[UIActionIndexRuntime_Menu_SharedClipboard];
    m_pool[UIActionIndexRuntime_Menu_SharedClipboard] = new MenuSharedClipboardAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_DragAndDrop])
        delete m_pool[UIActionIndexRuntime_Menu_DragAndDrop];
    m_pool[UIActionIndexRuntime_Menu_DragAndDrop] = new MenuDragAndDropAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_NetworkAdapters])
        delete m_pool[UIActionIndexRuntime_Menu_NetworkAdapters];
    m_pool[UIActionIndexRuntime_Menu_NetworkAdapters] = new MenuNetworkAdaptersAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_SharedFolders])
        delete m_pool[UIActionIndexRuntime_Menu_SharedFolders];
    m_pool[UIActionIndexRuntime_Menu_SharedFolders] = new MenuSharedFoldersAction(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Debug])
        delete m_pool[UIActionIndexRuntime_Menu_Debug];
    m_pool[UIActionIndexRuntime_Menu_Debug] = new MenuDebugAction(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Dock])
        delete m_pool[UIActionIndexRuntime_Menu_Dock];
    m_pool[UIActionIndexRuntime_Menu_Dock] = new DockMenuAction(this);
    if (m_pool[UIActionIndexRuntime_Menu_DockSettings])
        delete m_pool[UIActionIndexRuntime_Menu_DockSettings];
    m_pool[UIActionIndexRuntime_Menu_DockSettings] = new DockSettingsMenuAction(this);
#endif /* Q_WS_MAC */
}

#include "UIActionPoolRuntime.moc"

