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
#include "UIExtraDataDefs.h"
#include "UIShortcutPool.h"
#include "VBoxGlobal.h"

/* Namespaces: */
using namespace UIExtraDataDefs;


class UIActionMenuMachineRuntime : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMachineRuntime(UIActionPool *pParent)
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

class UIActionSimpleShowSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_settings_16px.png", ":/vm_settings_disabled_16px.png")
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

class UIActionSimplePerformTakeSnapshot : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformTakeSnapshot(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/snapshot_take_16px.png", ":/snapshot_take_disabled_16px.png")
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

class UIActionSimplePerformTakeScreenshot : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformTakeScreenshot(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/screenshot_take_16px.png", ":/screenshot_take_disabled_16px.png")
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

class UIActionSimpleShowInformationDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowInformationDialog(UIActionPool *pParent)
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
        setStatusTip(QApplication::translate("UIActionPool", "Show Session Information Window"));
    }
};

class UIActionMenuKeyboard : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuKeyboard(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionSimpleKeyboardSettings : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleKeyboardSettings(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/keyboard_settings_16px.png", ":/keyboard_settings_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("KeyboardSettings");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Configure &Shortcuts..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global settings window to configure shortcuts"));
    }
};

class UIActionMenuMouseIntegration : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMouseIntegration(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionToggleMouseIntegration : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleMouseIntegration(UIActionPool *pParent)
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

class UIActionSimplePerformTypeCAD : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformTypeCAD(UIActionPool *pParent)
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
class UIActionSimplePerformTypeCABS : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformTypeCABS(UIActionPool *pParent)
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

class UIActionTogglePause : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionTogglePause(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/vm_pause_on_16px.png", ":/vm_pause_16px.png",
                         ":/vm_pause_on_disabled_16px.png", ":/vm_pause_disabled_16px.png")
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

class UIActionSimplePerformReset : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformReset(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_reset_16px.png", ":/vm_reset_disabled_16px.png")
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

class UIActionSimplePerformSave : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformSave(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_save_state_16px.png", ":/vm_save_state_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("Save");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Save State"));
        setStatusTip(QApplication::translate("UIActionPool", "Save the machine state of the virtual machine"));
    }
};

class UIActionSimplePerformShutdown : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformShutdown(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_shutdown_16px.png", ":/vm_shutdown_disabled_16px.png")
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

class UIActionSimplePerformPowerOff : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformPowerOff(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_poweroff_16px.png", ":/vm_poweroff_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("PowerOff");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Po&wer Off"));
        setStatusTip(QApplication::translate("UIActionPool", "Power off the virtual machine"));
    }
};

class UIActionSimplePerformClose : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformClose(UIActionPool *pParent)
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

class UIActionMenuView : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuView(UIActionPool *pParent)
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

class UIActionToggleFullscreenMode : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleFullscreenMode(UIActionPool *pParent)
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

class UIActionToggleSeamlessMode : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleSeamlessMode(UIActionPool *pParent)
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

class UIActionToggleScaleMode : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleScaleMode(UIActionPool *pParent)
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
        setName(QApplication::translate("UIActionPool", "Switch to &Scaled Mode"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch between normal and scaled mode"));
    }
};

class UIActionToggleGuestAutoresize : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleGuestAutoresize(UIActionPool *pParent)
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

class UIActionSimplePerformWindowAdjust : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformWindowAdjust(UIActionPool *pParent)
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

class UIActionMenuStatusBar : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuStatusBar(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Status Bar"));
    }
};

class UIActionSimpleShowStatusBarSettingsWindow : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowStatusBarSettingsWindow(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("StatusBarSettings");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Status Bar Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Opens window to configure status-bar"));
    }
};

class UIActionMenuDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDevices(UIActionPool *pParent)
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

class UIActionMenuHardDisks : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuHardDisks(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/hd_16px.png", ":/hd_disabled_16px.png")
    {
        qobject_cast<UIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionSimpleShowStorageSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowStorageSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/hd_settings_16px.png", ":/hd_settings_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("StorageSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Storage Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Change the settings of storage devices"));
    }
};

class UIActionMenuOpticalDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuOpticalDevices(UIActionPool *pParent)
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

class UIActionMenuFloppyDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuFloppyDevices(UIActionPool *pParent)
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

class UIActionMenuUSBDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuUSBDevices(UIActionPool *pParent)
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

class UIActionMenuWebCams : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuWebCams(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/web_camera_16px.png", ":/web_camera_disabled_16px.png")
    {
        qobject_cast<UIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Webcams"));
    }
};

class UIActionMenuSharedClipboard : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuSharedClipboard(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/shared_clipboard_16px.png", ":/shared_clipboard_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Shared &Clipboard"));
    }
};

class UIActionMenuDragAndDrop : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDragAndDrop(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/drag_drop_16px.png", ":/drag_drop_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Drag'n'Drop"));
    }
};

class UIActionMenuNetworkAdapters : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuNetworkAdapters(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/nw_16px.png", ":/nw_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Network"));
    }
};

class UIActionSimpleShowNetworkSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowNetworkSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/nw_settings_16px.png", ":/nw_settings_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("NetworkSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Network Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Change the settings of network adapters"));
    }
};

class UIActionMenuSharedFolders : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuSharedFolders(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionSimpleShowSharedFoldersSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowSharedFoldersSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/sf_settings_16px.png", ":/sf_settings_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("SharedFoldersSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Shared Folders Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create or modify shared folders"));
    }
};

class UIActionToggleVRDEServer : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleVRDEServer(UIActionPool *pParent)
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
        setName(QApplication::translate("UIActionPool", "R&emote Display"));
        setStatusTip(QApplication::translate("UIActionPool", "Toggle remote desktop (RDP) connections to this machine"));
    }
};

class UIActionMenuVideoCapture : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuVideoCapture(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionToggleVideoCapture : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleVideoCapture(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/video_capture_on_16px.png", ":/video_capture_16px.png",
                         ":/video_capture_on_disabled_16px.png", ":/video_capture_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("VideoCapture");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Video Capture"));
        setStatusTip(QApplication::translate("UIActionPool", "Toggle video capture"));
    }
};

class UIActionSimpleShowVideoCaptureSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowVideoCaptureSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/video_capture_settings_16px.png")
    {
        retranslateUi();
    }

protected:

    QString shortcutExtraDataID() const
    {
        return QString("VideoCaptureSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Video Capture Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Configure video capture settings"));
    }
};

class UIActionSimplePerformInstallGuestTools : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformInstallGuestTools(UIActionPool *pParent)
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
        setName(QApplication::translate("UIActionPool", "&Insert Guest Additions CD image..."));
        setStatusTip(QApplication::translate("UIActionPool", "Insert the Guest Additions disk file into the virtual drive"));
    }
};

#ifdef VBOX_WITH_DEBUGGER_GUI
class UIActionMenuDebug : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDebug(UIActionPool *pParent)
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

class UIActionSimpleShowStatistics : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowStatistics(UIActionPool *pParent)
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

class UIActionSimpleShowCommandLine : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowCommandLine(UIActionPool *pParent)
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

class UIActionToggleLogging : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleLogging(UIActionPool *pParent)
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
        setName(QApplication::translate("UIActionPool", "&Logging...", "debug action"));
    }
};
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef RT_OS_DARWIN
class UIActionMenuDock : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDock(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class UIActionMenuDockSettings : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDockSettings(UIActionPool *pParent)
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

class UIActionToggleDockPreviewMonitor : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleDockPreviewMonitor(UIActionPool *pParent)
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

class UIActionToggleDockDisableMonitor : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleDockDisableMonitor(UIActionPool *pParent)
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


UIActionPoolRuntime::UIActionPoolRuntime()
    : UIActionPool(UIActionPoolType_Runtime)
{
    /* Prepare connections: */
    connect(gShortcutPool, SIGNAL(sigMachineShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));
}

void UIActionPoolRuntime::retranslateUi()
{
    /* Translate all the actions: */
    foreach (const int iActionPoolKey, m_pool.keys())
        m_pool[iActionPoolKey]->retranslateUi();
    /* Re-apply Runtime UI shortcuts: */
    sltApplyShortcuts();
    /* Temporary create Selector UI pool to do the same: */
    UIActionPool::createTemporary(UIActionPoolType_Selector);
}

QString UIActionPoolRuntime::shortcutsExtraDataID() const
{
    return GUI_Input_MachineShortcuts;
}

void UIActionPoolRuntime::createActions()
{
    /* Global actions creation: */
    UIActionPool::createActions();

    /* 'Machine' actions: */
    m_pool[UIActionIndexRuntime_Simple_SettingsDialog] = new UIActionSimpleShowSettingsDialog(this);
    m_pool[UIActionIndexRuntime_Simple_TakeSnapshot] = new UIActionSimplePerformTakeSnapshot(this);
    m_pool[UIActionIndexRuntime_Simple_TakeScreenshot] = new UIActionSimplePerformTakeScreenshot(this);
    m_pool[UIActionIndexRuntime_Simple_InformationDialog] = new UIActionSimpleShowInformationDialog(this);
    m_pool[UIActionIndexRuntime_Simple_KeyboardSettings] = new UIActionSimpleKeyboardSettings(this);
    m_pool[UIActionIndexRuntime_Toggle_MouseIntegration] = new UIActionToggleMouseIntegration(this);
    m_pool[UIActionIndexRuntime_Simple_TypeCAD] = new UIActionSimplePerformTypeCAD(this);
#ifdef Q_WS_X11
    m_pool[UIActionIndexRuntime_Simple_TypeCABS] = new UIActionSimplePerformTypeCABS(this);
#endif /* Q_WS_X11 */
    m_pool[UIActionIndexRuntime_Toggle_Pause] = new UIActionTogglePause(this);
    m_pool[UIActionIndexRuntime_Simple_Reset] = new UIActionSimplePerformReset(this);
    m_pool[UIActionIndexRuntime_Simple_Save] = new UIActionSimplePerformSave(this);
    m_pool[UIActionIndexRuntime_Simple_Shutdown] = new UIActionSimplePerformShutdown(this);
    m_pool[UIActionIndexRuntime_Simple_PowerOff] = new UIActionSimplePerformPowerOff(this);
    m_pool[UIActionIndexRuntime_Simple_Close] = new UIActionSimplePerformClose(this);

    /* 'View' actions: */
    m_pool[UIActionIndexRuntime_Toggle_Fullscreen] = new UIActionToggleFullscreenMode(this);
    m_pool[UIActionIndexRuntime_Toggle_Seamless] = new UIActionToggleSeamlessMode(this);
    m_pool[UIActionIndexRuntime_Toggle_Scale] = new UIActionToggleScaleMode(this);
    m_pool[UIActionIndexRuntime_Toggle_GuestAutoresize] = new UIActionToggleGuestAutoresize(this);
    m_pool[UIActionIndexRuntime_Simple_AdjustWindow] = new UIActionSimplePerformWindowAdjust(this);
    m_pool[UIActionIndexRuntime_Simple_StatusBarSettings] = new UIActionSimpleShowStatusBarSettingsWindow(this);

    /* 'Devices' actions: */
    m_pool[UIActionIndexRuntime_Simple_StorageSettings] = new UIActionSimpleShowStorageSettingsDialog(this);
    m_pool[UIActionIndexRuntime_Simple_NetworkSettings] = new UIActionSimpleShowNetworkSettingsDialog(this);
    m_pool[UIActionIndexRuntime_Simple_SharedFoldersSettings] = new UIActionSimpleShowSharedFoldersSettingsDialog(this);
    m_pool[UIActionIndexRuntime_Toggle_VRDEServer] = new UIActionToggleVRDEServer(this);
    m_pool[UIActionIndexRuntime_Toggle_VideoCapture] = new UIActionToggleVideoCapture(this);
    m_pool[UIActionIndexRuntime_Simple_VideoCaptureSettings] = new UIActionSimpleShowVideoCaptureSettingsDialog(this);
    m_pool[UIActionIndexRuntime_Simple_InstallGuestTools] = new UIActionSimplePerformInstallGuestTools(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' actions: */
    m_pool[UIActionIndexRuntime_Simple_Statistics] = new UIActionSimpleShowStatistics(this);
    m_pool[UIActionIndexRuntime_Simple_CommandLine] = new UIActionSimpleShowCommandLine(this);
    m_pool[UIActionIndexRuntime_Toggle_Logging] = new UIActionToggleLogging(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' actions: */
    m_pool[UIActionIndexRuntime_Toggle_DockPreviewMonitor] = new UIActionToggleDockPreviewMonitor(this);
    m_pool[UIActionIndexRuntime_Toggle_DockDisableMonitor] = new UIActionToggleDockDisableMonitor(this);
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
    m_pool[UIActionIndexRuntime_Simple_Close] = new UIActionSimplePerformClose(this);

    /* 'Machine' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Machine])
        delete m_pool[UIActionIndexRuntime_Menu_Machine];
    m_pool[UIActionIndexRuntime_Menu_Machine] = new UIActionMenuMachineRuntime(this);
    if (m_pool[UIActionIndexRuntime_Menu_Keyboard])
        delete m_pool[UIActionIndexRuntime_Menu_Keyboard];
    m_pool[UIActionIndexRuntime_Menu_Keyboard] = new UIActionMenuKeyboard(this);
    if (m_pool[UIActionIndexRuntime_Menu_MouseIntegration])
        delete m_pool[UIActionIndexRuntime_Menu_MouseIntegration];
    m_pool[UIActionIndexRuntime_Menu_MouseIntegration] = new UIActionMenuMouseIntegration(this);

    /* 'View' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_View])
        delete m_pool[UIActionIndexRuntime_Menu_View];
    m_pool[UIActionIndexRuntime_Menu_View] = new UIActionMenuView(this);
    if (m_pool[UIActionIndexRuntime_Menu_StatusBar])
        delete m_pool[UIActionIndexRuntime_Menu_StatusBar];
    m_pool[UIActionIndexRuntime_Menu_StatusBar] = new UIActionMenuStatusBar(this);

    /* 'Devices' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Devices])
        delete m_pool[UIActionIndexRuntime_Menu_Devices];
    m_pool[UIActionIndexRuntime_Menu_Devices] = new UIActionMenuDevices(this);
    if (m_pool[UIActionIndexRuntime_Menu_HardDisks])
        delete m_pool[UIActionIndexRuntime_Menu_HardDisks];
    m_pool[UIActionIndexRuntime_Menu_HardDisks] = new UIActionMenuHardDisks(this);
    if (m_pool[UIActionIndexRuntime_Menu_OpticalDevices])
        delete m_pool[UIActionIndexRuntime_Menu_OpticalDevices];
    m_pool[UIActionIndexRuntime_Menu_OpticalDevices] = new UIActionMenuOpticalDevices(this);
    if (m_pool[UIActionIndexRuntime_Menu_FloppyDevices])
        delete m_pool[UIActionIndexRuntime_Menu_FloppyDevices];
    m_pool[UIActionIndexRuntime_Menu_FloppyDevices] = new UIActionMenuFloppyDevices(this);
    if (m_pool[UIActionIndexRuntime_Menu_USBDevices])
        delete m_pool[UIActionIndexRuntime_Menu_USBDevices];
    m_pool[UIActionIndexRuntime_Menu_USBDevices] = new UIActionMenuUSBDevices(this);
    if (m_pool[UIActionIndexRuntime_Menu_WebCams])
        delete m_pool[UIActionIndexRuntime_Menu_WebCams];
    m_pool[UIActionIndexRuntime_Menu_WebCams] = new UIActionMenuWebCams(this);
    if (m_pool[UIActionIndexRuntime_Menu_SharedClipboard])
        delete m_pool[UIActionIndexRuntime_Menu_SharedClipboard];
    m_pool[UIActionIndexRuntime_Menu_SharedClipboard] = new UIActionMenuSharedClipboard(this);
    if (m_pool[UIActionIndexRuntime_Menu_DragAndDrop])
        delete m_pool[UIActionIndexRuntime_Menu_DragAndDrop];
    m_pool[UIActionIndexRuntime_Menu_DragAndDrop] = new UIActionMenuDragAndDrop(this);
    if (m_pool[UIActionIndexRuntime_Menu_Network])
        delete m_pool[UIActionIndexRuntime_Menu_Network];
    m_pool[UIActionIndexRuntime_Menu_Network] = new UIActionMenuNetworkAdapters(this);
    if (m_pool[UIActionIndexRuntime_Menu_SharedFolders])
        delete m_pool[UIActionIndexRuntime_Menu_SharedFolders];
    m_pool[UIActionIndexRuntime_Menu_SharedFolders] = new UIActionMenuSharedFolders(this);
    if (m_pool[UIActionIndexRuntime_Menu_VideoCapture])
        delete m_pool[UIActionIndexRuntime_Menu_VideoCapture];
    m_pool[UIActionIndexRuntime_Menu_VideoCapture] = new UIActionMenuVideoCapture(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Debug])
        delete m_pool[UIActionIndexRuntime_Menu_Debug];
    m_pool[UIActionIndexRuntime_Menu_Debug] = new UIActionMenuDebug(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' menu: */
    if (m_pool[UIActionIndexRuntime_Menu_Dock])
        delete m_pool[UIActionIndexRuntime_Menu_Dock];
    m_pool[UIActionIndexRuntime_Menu_Dock] = new UIActionMenuDock(this);
    if (m_pool[UIActionIndexRuntime_Menu_DockSettings])
        delete m_pool[UIActionIndexRuntime_Menu_DockSettings];
    m_pool[UIActionIndexRuntime_Menu_DockSettings] = new UIActionMenuDockSettings(this);
#endif /* Q_WS_MAC */
}

#include "UIActionPoolRuntime.moc"

