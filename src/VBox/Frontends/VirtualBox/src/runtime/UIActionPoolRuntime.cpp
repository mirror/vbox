/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPoolRuntime class implementation.
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

/* GUI includes: */
#include "UIActionPoolRuntime.h"
#include "UIMultiScreenLayout.h"
#include "UIExtraDataManager.h"
#include "UIShortcutPool.h"
#include "UIFrameBuffer.h"
#include "UIConverter.h"
#include "UISession.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"

/* Namespaces: */
using namespace UIExtraDataDefs;


class UIActionMenuMachineRuntime : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMachineRuntime(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_Machine; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_Machine); }

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
        : UIActionSimple(pParent, ":/vm_settings_16px.png", ":/vm_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SettingsDialog; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SettingsDialog); }

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
        : UIActionSimple(pParent, ":/snapshot_take_16px.png", ":/snapshot_take_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeSnapshot; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeSnapshot); }

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
        : UIActionSimple(pParent, ":/screenshot_take_16px.png", ":/screenshot_take_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeScreenshot; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeScreenshot); }

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
        : UIActionSimple(pParent, ":/session_info_16px.png", ":/session_info_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_InformationDialog; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_InformationDialog); }

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
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Keyboard; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Keyboard); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Keyboard"));
    }
};

class UIActionSimpleKeyboardSettings : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleKeyboardSettings(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/keyboard_settings_16px.png", ":/keyboard_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_KeyboardSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_KeyboardSettings); }

    QString shortcutExtraDataID() const
    {
        return QString("KeyboardSettings");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Keyboard Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global settings window to configure shortcuts"));
    }
};

class UIActionMenuMouse : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMouse(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Mouse; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Mouse); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Mouse"));
    }
};

class UIActionToggleMouseIntegration : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleMouseIntegration(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/mouse_can_seamless_on_16px.png", ":/mouse_can_seamless_16px.png",
                         ":/mouse_can_seamless_on_disabled_16px.png", ":/mouse_can_seamless_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_MouseIntegration; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_MouseIntegration); }

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
        : UIActionSimple(pParent, ":/hostkey_16px.png", ":/hostkey_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCAD; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCAD); }

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
        : UIActionSimple(pParent, ":/hostkey_16px.png", ":/hostkey_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCABS; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCABS); }

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
                         ":/vm_pause_on_disabled_16px.png", ":/vm_pause_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Pause; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Pause); }

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
        : UIActionSimple(pParent, ":/vm_reset_16px.png", ":/vm_reset_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Reset; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Reset); }

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
        : UIActionSimple(pParent, ":/vm_save_state_16px.png", ":/vm_save_state_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SaveState; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SaveState); }

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
        : UIActionSimple(pParent, ":/vm_shutdown_16px.png", ":/vm_shutdown_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Shutdown; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Shutdown); }

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
        : UIActionSimple(pParent, ":/vm_poweroff_16px.png", ":/vm_poweroff_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_PowerOff; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_PowerOff); }

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

#ifndef RT_OS_DARWIN
class UIActionSimplePerformClose : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePerformClose(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Close;}
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Close);}

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
#endif /* !RT_OS_DARWIN */

class UIActionMenuView : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuView(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_View; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_View); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&View"));
    }
};

class UIActionMenuViewPopup : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuViewPopup(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_View; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_View); }

    void retranslateUi() {}
};

class UIActionToggleFullscreenMode : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleFullscreenMode(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/fullscreen_on_16px.png", ":/fullscreen_16px.png",
                         ":/fullscreen_on_disabled_16px.png", ":/fullscreen_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_Fullscreen; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Fullscreen); }

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
                         ":/seamless_on_disabled_16px.png", ":/seamless_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_Seamless; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Seamless); }

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
                         ":/scale_on_disabled_16px.png", ":/scale_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_Scale; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Scale); }

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
                         ":/auto_resize_on_on_disabled_16px.png", ":/auto_resize_on_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_GuestAutoresize; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_GuestAutoresize); }

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
        : UIActionSimple(pParent, ":/adjust_win_size_16px.png", ":/adjust_win_size_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_AdjustWindow; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_AdjustWindow); }

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

class UIActionMenuMenuBar : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMenuBar(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/menubar_16px.png", ":/menubar_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBar; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBar); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Menu Bar"));
    }
};

class UIActionSimpleShowMenuBarSettingsWindow : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowMenuBarSettingsWindow(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/menubar_settings_16px.png", ":/menubar_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBarSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBarSettings); }

    QString shortcutExtraDataID() const
    {
        return QString("MenuBarSettings");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Menu Bar Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Opens window to configure menu-bar"));
    }
};

class UIActionMenuStatusBar : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuStatusBar(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/statusbar_16px.png", ":/statusbar_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBar; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBar); }

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
        : UIActionSimple(pParent, ":/statusbar_settings_16px.png", ":/statusbar_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBarSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBarSettings); }

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

class UIActionToggleStatusBar : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleStatusBar(UIActionPool *pParent)
        : UIActionToggle(pParent, ":/statusbar_on_16px.png", ":/statusbar_16px.png",
                                  ":/statusbar_on_disabled_16px.png", ":/statusbar_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuViewActionType_ToggleStatusBar; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuViewActionType_ToggleStatusBar); }

    QString shortcutExtraDataID() const
    {
        return QString("ToggleStatusBar");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Show Status &Bar"));
        setStatusTip(QApplication::translate("UIActionPool", "Toggle status-bar visibility for this machine"));
    }
};

class UIActionMenuDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDevices(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_Devices; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_Devices); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Devices"));
    }
};

class UIActionMenuHardDrives : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuHardDrives(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/hd_16px.png", ":/hd_disabled_16px.png")
    {
        setShowToolTip(true);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrives; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrives); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Hard Drives"));
    }
};

class UIActionSimpleShowHardDrivesSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowHardDrivesSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/hd_settings_16px.png", ":/hd_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrivesSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrivesSettings); }

    QString shortcutExtraDataID() const
    {
        return QString("HardDriveSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Hard Drive Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Change the settings of hard drives"));
    }
};

class UIActionMenuOpticalDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuOpticalDevices(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/cd_16px.png", ":/cd_disabled_16px.png")
    {
        setShowToolTip(true);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_OpticalDevices; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_OpticalDevices); }

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
        setShowToolTip(true);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_FloppyDevices; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_FloppyDevices); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Floppy Devices"));
    }
};

class UIActionMenuNetworkAdapters : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuNetworkAdapters(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/nw_16px.png", ":/nw_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Network; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Network); }

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
        : UIActionSimple(pParent, ":/nw_settings_16px.png", ":/nw_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_NetworkSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_NetworkSettings); }

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

class UIActionMenuUSBDevices : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuUSBDevices(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/usb_16px.png", ":/usb_disabled_16px.png")
    {
        setShowToolTip(true);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevices; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevices); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&USB Devices"));
    }
};

class UIActionSimpleShowUSBDevicesSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowUSBDevicesSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/usb_settings_16px.png", ":/usb_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevicesSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevicesSettings); }

    QString shortcutExtraDataID() const
    {
        return QString("USBDevicesSettingsDialog");
    }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&USB Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Change the settings of USB devices"));
    }
};

class UIActionMenuWebCams : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuWebCams(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/web_camera_16px.png", ":/web_camera_disabled_16px.png")
    {
        setShowToolTip(true);
    }

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_WebCams; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_WebCams); }

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
        : UIActionMenu(pParent, ":/shared_clipboard_16px.png", ":/shared_clipboard_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedClipboard; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedClipboard); }

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
        : UIActionMenu(pParent, ":/drag_drop_16px.png", ":/drag_drop_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_DragAndDrop; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_DragAndDrop); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "Drag'n'Drop"));
    }
};

class UIActionMenuSharedFolders : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuSharedFolders(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/sf_16px.png", ":/sf_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFolders; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFolders); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Shared Folders"));
    }
};

class UIActionSimpleShowSharedFoldersSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleShowSharedFoldersSettingsDialog(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/sf_settings_16px.png", ":/sf_settings_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFoldersSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFoldersSettings); }

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
                         ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VRDEServer; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VRDEServer); }

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
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCapture; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCapture); }

    void retranslateUi()
    {
        setName(QApplication::translate("UIActionPool", "&Video Capture"));
    }
};

class UIActionToggleVideoCapture : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleVideoCapture(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/video_capture_on_16px.png", ":/video_capture_16px.png",
                         ":/video_capture_on_disabled_16px.png", ":/video_capture_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_StartVideoCapture; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_StartVideoCapture); }

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
        : UIActionSimple(pParent, ":/video_capture_settings_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCaptureSettings; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCaptureSettings); }

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
        : UIActionSimple(pParent, ":/guesttools_16px.png", ":/guesttools_disabled_16px.png") {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_InstallGuestTools; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_InstallGuestTools); }

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
        : UIActionMenu(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuType_Debug; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuType_Debug); }

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
        : UIActionSimple(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Statistics; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Statistics); }

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
        : UIActionSimple(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_CommandLine; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_CommandLine); }

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
        : UIActionToggle(pParent) {}

protected:

    /** Returns action extra-data ID. */
    virtual int extraDataID() const { return UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Logging; }
    /** Returns action extra-data key. */
    virtual QString extraDataKey() const { return gpConverter->toInternalString(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Logging); }

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
        : UIActionMenu(pParent) {}

protected:

    void retranslateUi() {}
};

class UIActionMenuDockSettings : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuDockSettings(UIActionPool *pParent)
        : UIActionMenu(pParent) {}

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
        : UIActionToggle(pParent) {}

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
        : UIActionToggle(pParent) {}

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


UIActionPoolRuntime::UIActionPoolRuntime(bool fTemporary /* = false */)
    : UIActionPool(UIActionPoolType_Runtime, fTemporary)
    , m_pSession(0)
    , m_pMultiScreenLayout(0)
{
}

void UIActionPoolRuntime::setSession(UISession *pSession)
{
    m_pSession = pSession;
    m_invalidations << UIActionIndexRT_M_View << UIActionIndexRT_M_ViewPopup;
}

void UIActionPoolRuntime::setMultiScreenLayout(UIMultiScreenLayout *pMultiScreenLayout)
{
    /* Disconnect old stuff: */
    if (m_pMultiScreenLayout)
    {
        disconnect(this, SIGNAL(sigNotifyAboutTriggeringViewScreenRemap(int, int)),
                   m_pMultiScreenLayout, SLOT(sltHandleScreenLayoutChange(int, int)));
        disconnect(m_pMultiScreenLayout, SIGNAL(sigScreenLayoutUpdate()),
                   this, SLOT(sltHandleScreenLayoutUpdate()));
    }

    /* Assign new multi-screen layout: */
    m_pMultiScreenLayout = pMultiScreenLayout;

    /* Connect new stuff: */
    if (m_pMultiScreenLayout)
    {
        connect(this, SIGNAL(sigNotifyAboutTriggeringViewScreenRemap(int, int)),
                m_pMultiScreenLayout, SLOT(sltHandleScreenLayoutChange(int, int)));
        connect(m_pMultiScreenLayout, SIGNAL(sigScreenLayoutUpdate()),
                this, SLOT(sltHandleScreenLayoutUpdate()));
    }

    /* Invalidate View menu: */
    m_invalidations << UIActionIndexRT_M_View;
}

bool UIActionPoolRuntime::isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType type) const
{
    foreach (const UIExtraDataMetaDefs::RuntimeMenuType &restriction, m_restrictedMenus.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPoolRuntime::setRestrictionForMenuBar(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuType restriction)
{
    m_restrictedMenus[level] = restriction;
    updateMenus();
}

bool UIActionPoolRuntime::isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType type) const
{
    foreach (const UIExtraDataMetaDefs::RuntimeMenuMachineActionType &restriction, m_restrictedActionsMenuMachine.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPoolRuntime::setRestrictionForMenuMachine(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuMachineActionType restriction)
{
    m_restrictedActionsMenuMachine[level] = restriction;
    m_invalidations << UIActionIndexRT_M_Machine;
}

bool UIActionPoolRuntime::isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType type) const
{
    foreach (const UIExtraDataMetaDefs::RuntimeMenuViewActionType &restriction, m_restrictedActionsMenuView.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPoolRuntime::setRestrictionForMenuView(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuViewActionType restriction)
{
    m_restrictedActionsMenuView[level] = restriction;
    m_invalidations << UIActionIndexRT_M_View << UIActionIndexRT_M_ViewPopup;
}

bool UIActionPoolRuntime::isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType type) const
{
    foreach (const UIExtraDataMetaDefs::RuntimeMenuDevicesActionType &restriction, m_restrictedActionsMenuDevices.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPoolRuntime::setRestrictionForMenuDevices(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restriction)
{
    m_restrictedActionsMenuDevices[level] = restriction;
    m_invalidations << UIActionIndexRT_M_Devices;
}

#ifdef VBOX_WITH_DEBUGGER_GUI
bool UIActionPoolRuntime::isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType type) const
{
    foreach (const UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType &restriction, m_restrictedActionsMenuDebug.values())
        if (restriction & type)
            return false;
    return true;
}

void UIActionPoolRuntime::setRestrictionForMenuDebugger(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restriction)
{
    m_restrictedActionsMenuDebug[level] = restriction;
    m_invalidations << UIActionIndexRT_M_Debug;
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIActionPoolRuntime::sltHandleConfigurationChange()
{
    /* Update configuration: */
    updateConfiguration();
}

void UIActionPoolRuntime::sltPrepareMenuViewScreen()
{
    /* Make sure sender is valid: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertPtrReturnVoid(pMenu);

    /* Call to corresponding handler: */
    updateMenuViewScreen(pMenu);
}

void UIActionPoolRuntime::sltPrepareMenuViewMultiscreen()
{
    /* Make sure sender is valid: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertPtrReturnVoid(pMenu);

    /* Call to corresponding handler: */
    updateMenuViewMultiscreen(pMenu);
}

void UIActionPoolRuntime::sltHandleActionTriggerViewScreenToggle()
{
    /* Make sure sender is valid: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertPtrReturnVoid(pAction);

    /* Send request to enable/disable guest-screen: */
    const int iGuestScreenIndex = pAction->property("Guest Screen Index").toInt();
    const bool fScreenEnabled = pAction->isChecked();
    emit sigNotifyAboutTriggeringViewScreenToggle(iGuestScreenIndex, fScreenEnabled);
}

void UIActionPoolRuntime::sltHandleActionTriggerViewScreenResize(QAction *pAction)
{
    /* Make sure sender is valid: */
    AssertPtrReturnVoid(pAction);

    /* Send request to resize guest-screen to required size: */
    const int iGuestScreenIndex = pAction->property("Guest Screen Index").toInt();
    const QSize size = pAction->property("Requested Size").toSize();
    emit sigNotifyAboutTriggeringViewScreenResize(iGuestScreenIndex, size);
}

void UIActionPoolRuntime::sltHandleActionTriggerViewScreenRemap(QAction *pAction)
{
    /* Make sure sender is valid: */
    AssertPtrReturnVoid(pAction);

    /* Send request to remap guest-screen to required host-screen: */
    const int iGuestScreenIndex = pAction->property("Guest Screen Index").toInt();
    const int iHostScreenIndex = pAction->property("Host Screen Index").toInt();
    emit sigNotifyAboutTriggeringViewScreenRemap(iGuestScreenIndex, iHostScreenIndex);
}

void UIActionPoolRuntime::sltHandleScreenLayoutUpdate()
{
    /* Invalidate View menu: */
    m_invalidations << UIActionIndexRT_M_View;
}

void UIActionPoolRuntime::preparePool()
{
    /* 'Machine' actions: */
    m_pool[UIActionIndexRT_M_Machine] = new UIActionMenuMachineRuntime(this);
    m_pool[UIActionIndexRT_M_Machine_S_Settings] = new UIActionSimpleShowSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Machine_S_TakeSnapshot] = new UIActionSimplePerformTakeSnapshot(this);
    m_pool[UIActionIndexRT_M_Machine_S_TakeScreenshot] = new UIActionSimplePerformTakeScreenshot(this);
    m_pool[UIActionIndexRT_M_Machine_S_ShowInformation] = new UIActionSimpleShowInformationDialog(this);
    m_pool[UIActionIndexRT_M_Machine_M_Keyboard] = new UIActionMenuKeyboard(this);
    m_pool[UIActionIndexRT_M_Machine_M_Keyboard_S_Settings] = new UIActionSimpleKeyboardSettings(this);
    m_pool[UIActionIndexRT_M_Machine_M_Mouse] = new UIActionMenuMouse(this);
    m_pool[UIActionIndexRT_M_Machine_M_Mouse_T_Integration] = new UIActionToggleMouseIntegration(this);
    m_pool[UIActionIndexRT_M_Machine_S_TypeCAD] = new UIActionSimplePerformTypeCAD(this);
#ifdef Q_WS_X11
    m_pool[UIActionIndexRT_M_Machine_S_TypeCABS] = new UIActionSimplePerformTypeCABS(this);
#endif /* Q_WS_X11 */
    m_pool[UIActionIndexRT_M_Machine_T_Pause] = new UIActionTogglePause(this);
    m_pool[UIActionIndexRT_M_Machine_S_Reset] = new UIActionSimplePerformReset(this);
    m_pool[UIActionIndexRT_M_Machine_S_Save] = new UIActionSimplePerformSave(this);
    m_pool[UIActionIndexRT_M_Machine_S_Shutdown] = new UIActionSimplePerformShutdown(this);
    m_pool[UIActionIndexRT_M_Machine_S_PowerOff] = new UIActionSimplePerformPowerOff(this);
#ifndef RT_OS_DARWIN
    m_pool[UIActionIndexRT_M_Machine_S_Close] = new UIActionSimplePerformClose(this);
#endif /* !RT_OS_DARWIN */

    /* 'View' actions: */
    m_pool[UIActionIndexRT_M_View] = new UIActionMenuView(this);
    m_pool[UIActionIndexRT_M_ViewPopup] = new UIActionMenuViewPopup(this);
    m_pool[UIActionIndexRT_M_View_T_Fullscreen] = new UIActionToggleFullscreenMode(this);
    m_pool[UIActionIndexRT_M_View_T_Seamless] = new UIActionToggleSeamlessMode(this);
    m_pool[UIActionIndexRT_M_View_T_Scale] = new UIActionToggleScaleMode(this);
    m_pool[UIActionIndexRT_M_View_T_GuestAutoresize] = new UIActionToggleGuestAutoresize(this);
    m_pool[UIActionIndexRT_M_View_S_AdjustWindow] = new UIActionSimplePerformWindowAdjust(this);
    m_pool[UIActionIndexRT_M_View_M_MenuBar] = new UIActionMenuMenuBar(this);
    m_pool[UIActionIndexRT_M_View_M_MenuBar_S_Settings] = new UIActionSimpleShowMenuBarSettingsWindow(this);
    m_pool[UIActionIndexRT_M_View_M_StatusBar] = new UIActionMenuStatusBar(this);
    m_pool[UIActionIndexRT_M_View_M_StatusBar_S_Settings] = new UIActionSimpleShowStatusBarSettingsWindow(this);
    m_pool[UIActionIndexRT_M_View_M_StatusBar_T_Visibility] = new UIActionToggleStatusBar(this);

    /* 'Devices' actions: */
    m_pool[UIActionIndexRT_M_Devices] = new UIActionMenuDevices(this);
    m_pool[UIActionIndexRT_M_Devices_M_HardDrives] = new UIActionMenuHardDrives(this);
    m_pool[UIActionIndexRT_M_Devices_M_HardDrives_S_Settings] = new UIActionSimpleShowHardDrivesSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Devices_M_OpticalDevices] = new UIActionMenuOpticalDevices(this);
    m_pool[UIActionIndexRT_M_Devices_M_FloppyDevices] = new UIActionMenuFloppyDevices(this);
    m_pool[UIActionIndexRT_M_Devices_M_Network] = new UIActionMenuNetworkAdapters(this);
    m_pool[UIActionIndexRT_M_Devices_M_Network_S_Settings] = new UIActionSimpleShowNetworkSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Devices_M_USBDevices] = new UIActionMenuUSBDevices(this);
    m_pool[UIActionIndexRT_M_Devices_M_USBDevices_S_Settings] = new UIActionSimpleShowUSBDevicesSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Devices_M_WebCams] = new UIActionMenuWebCams(this);
    m_pool[UIActionIndexRT_M_Devices_M_SharedClipboard] = new UIActionMenuSharedClipboard(this);
    m_pool[UIActionIndexRT_M_Devices_M_DragAndDrop] = new UIActionMenuDragAndDrop(this);
    m_pool[UIActionIndexRT_M_Devices_M_SharedFolders] = new UIActionMenuSharedFolders(this);
    m_pool[UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings] = new UIActionSimpleShowSharedFoldersSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Devices_T_VRDEServer] = new UIActionToggleVRDEServer(this);
    m_pool[UIActionIndexRT_M_Devices_M_VideoCapture] = new UIActionMenuVideoCapture(this);
    m_pool[UIActionIndexRT_M_Devices_M_VideoCapture_T_Start] = new UIActionToggleVideoCapture(this);
    m_pool[UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings] = new UIActionSimpleShowVideoCaptureSettingsDialog(this);
    m_pool[UIActionIndexRT_M_Devices_S_InstallGuestTools] = new UIActionSimplePerformInstallGuestTools(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' actions: */
    m_pool[UIActionIndexRT_M_Debug] = new UIActionMenuDebug(this);
    m_pool[UIActionIndexRT_M_Debug_S_ShowStatistics] = new UIActionSimpleShowStatistics(this);
    m_pool[UIActionIndexRT_M_Debug_S_ShowCommandLine] = new UIActionSimpleShowCommandLine(this);
    m_pool[UIActionIndexRT_M_Debug_T_Logging] = new UIActionToggleLogging(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* 'Dock' actions: */
    m_pool[UIActionIndexRT_M_Dock] = new UIActionMenuDock(this);
    m_pool[UIActionIndexRT_M_Dock_M_DockSettings] = new UIActionMenuDockSettings(this);
    m_pool[UIActionIndexRT_M_Dock_M_DockSettings_T_PreviewMonitor] = new UIActionToggleDockPreviewMonitor(this);
    m_pool[UIActionIndexRT_M_Dock_M_DockSettings_T_DisableMonitor] = new UIActionToggleDockDisableMonitor(this);
#endif /* Q_WS_MAC */

    /* Prepare update-handlers for known menus: */
    m_menuUpdateHandlers[UIActionIndexRT_M_Machine].ptfr =                 &UIActionPoolRuntime::updateMenuMachine;
    m_menuUpdateHandlers[UIActionIndexRT_M_Machine_M_Keyboard].ptfr =      &UIActionPoolRuntime::updateMenuMachineKeyboard;
    m_menuUpdateHandlers[UIActionIndexRT_M_Machine_M_Mouse].ptfr =         &UIActionPoolRuntime::updateMenuMachineMouse;
    m_menuUpdateHandlers[UIActionIndexRT_M_View].ptfr =                    &UIActionPoolRuntime::updateMenuView;
    m_menuUpdateHandlers[UIActionIndexRT_M_ViewPopup].ptfr =               &UIActionPoolRuntime::updateMenuViewPopup;
    m_menuUpdateHandlers[UIActionIndexRT_M_View_M_MenuBar].ptfr =          &UIActionPoolRuntime::updateMenuViewMenuBar;
    m_menuUpdateHandlers[UIActionIndexRT_M_View_M_StatusBar].ptfr =        &UIActionPoolRuntime::updateMenuViewStatusBar;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices].ptfr =                 &UIActionPoolRuntime::updateMenuDevices;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_HardDrives].ptfr =    &UIActionPoolRuntime::updateMenuDevicesHardDrives;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_Network].ptfr =       &UIActionPoolRuntime::updateMenuDevicesNetwork;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_USBDevices].ptfr =    &UIActionPoolRuntime::updateMenuDevicesUSBDevices;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_SharedFolders].ptfr = &UIActionPoolRuntime::updateMenuDevicesSharedFolders;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_VideoCapture].ptfr =  &UIActionPoolRuntime::updateMenuDevicesVideoCapture;
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_menuUpdateHandlers[UIActionIndexRT_M_Debug].ptfr =                   &UIActionPoolRuntime::updateMenuDebug;
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Call to base-class: */
    UIActionPool::preparePool();
}

void UIActionPoolRuntime::prepareConnections()
{
    /* Prepare connections: */
    connect(gShortcutPool, SIGNAL(sigMachineShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));
    connect(gEDataManager, SIGNAL(sigMenuBarConfigurationChange()), this, SLOT(sltHandleConfigurationChange()));
}

void UIActionPoolRuntime::updateConfiguration()
{
    /* Get machine ID: */
    const QString strMachineID = vboxGlobal().managedVMUuid();
    if (strMachineID.isNull())
        return;

    /* Recache common action restrictions: */
    m_restrictedMenus[UIActionRestrictionLevel_Base] =                  gEDataManager->restrictedRuntimeMenuTypes(strMachineID);
#ifdef Q_WS_MAC
    m_restrictedActionsMenuApplication[UIActionRestrictionLevel_Base] = gEDataManager->restrictedRuntimeMenuApplicationActionTypes(strMachineID);
#endif /* Q_WS_MAC */
    m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] =     gEDataManager->restrictedRuntimeMenuMachineActionTypes(strMachineID);
    m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] =        gEDataManager->restrictedRuntimeMenuViewActionTypes(strMachineID);
    m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] =     gEDataManager->restrictedRuntimeMenuDevicesActionTypes(strMachineID);
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_restrictedActionsMenuDebug[UIActionRestrictionLevel_Base] =       gEDataManager->restrictedRuntimeMenuDebuggerActionTypes(strMachineID);
#endif /* VBOX_WITH_DEBUGGER_GUI */
    m_restrictedActionsMenuHelp[UIActionRestrictionLevel_Base] =        gEDataManager->restrictedRuntimeMenuHelpActionTypes(strMachineID);

    /* Recache visual state action restrictions: */
    UIVisualStateType restrictedVisualStates = gEDataManager->restrictedVisualStates(strMachineID);
    {
        if (restrictedVisualStates & UIVisualStateType_Fullscreen)
            m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuViewActionType)
                (m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuViewActionType_Fullscreen);
        if (restrictedVisualStates & UIVisualStateType_Seamless)
            m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuViewActionType)
                (m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuViewActionType_Seamless);
        if (restrictedVisualStates & UIVisualStateType_Scale)
            m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuViewActionType)
                (m_restrictedActionsMenuView[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuViewActionType_Scale);
    }

    /* Recache reconfiguration action restrictions: */
    bool fReconfigurationAllowed = gEDataManager->machineReconfigurationEnabled(strMachineID);
    if (!fReconfigurationAllowed)
    {
        m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)
            (m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SettingsDialog);
        m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)
            (m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuMachineActionType_KeyboardSettings);
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrivesSettings);
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_NetworkSettings);
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevicesSettings);
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFoldersSettings);
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCaptureSettings);
    }

    /* Recache snapshot related action restrictions: */
    bool fSnapshotOperationsAllowed = gEDataManager->machineSnapshotOperationsEnabled(strMachineID);
    if (!fSnapshotOperationsAllowed)
    {
        m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)
            (m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeSnapshot);
    }

    /* Recache extension-pack related action restrictions: */
    CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
    bool fExtensionPackOperationsAllowed = !extPack.isNull() && extPack.GetUsable();
    if (!fExtensionPackOperationsAllowed)
    {
        m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
            (m_restrictedActionsMenuDevices[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VRDEServer);
    }

    /* Recache close related action restrictions: */
    MachineCloseAction restrictedCloseActions = gEDataManager->restrictedMachineCloseActions(strMachineID);
    bool fAllCloseActionsRestricted =    (restrictedCloseActions & MachineCloseAction_SaveState)
                                      && (restrictedCloseActions & MachineCloseAction_Shutdown)
                                      && (restrictedCloseActions & MachineCloseAction_PowerOff);
                                      // Close VM Dialog hides PowerOff_RestoringSnapshot implicitly if PowerOff is hidden..
                                      // && (m_restrictedCloseActions & MachineCloseAction_PowerOff_RestoringSnapshot);
    if (fAllCloseActionsRestricted)
    {
#ifdef Q_WS_MAC
        m_restrictedActionsMenuApplication[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::MenuApplicationActionType)
            (m_restrictedActionsMenuApplication[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::MenuApplicationActionType_Close);
#else /* !Q_WS_MAC */
        m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)
            (m_restrictedActionsMenuMachine[UIActionRestrictionLevel_Base] | UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Close);
#endif /* !Q_WS_MAC */
    }

    /* Call to base-class: */
    UIActionPool::updateConfiguration();
}

void UIActionPoolRuntime::updateMenu(int iIndex)
{
    /* Call to base-class: */
    if (iIndex < UIActionIndex_Max)
        UIActionPool::updateMenu(iIndex);

    /* If menu with such index is invalidated and there is update-handler: */
    if (m_invalidations.contains(iIndex) && m_menuUpdateHandlers.contains(iIndex))
        (this->*(m_menuUpdateHandlers.value(iIndex).ptfr))();
}

void UIActionPoolRuntime::updateMenus()
{
    /* Clear menu list: */
    m_mainMenus.clear();

#ifdef RT_OS_DARWIN
    /* 'Application' menu: */
    action(UIActionIndex_M_Application)->setVisible(false);
    m_mainMenus << action(UIActionIndex_M_Application)->menu();
    updateMenuApplication();
#endif /* RT_OS_DARWIN */

    /* 'Machine' menu: */
    const bool fAllowToShowMenuMachine = isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType_Machine);
    action(UIActionIndexRT_M_Machine)->setVisible(fAllowToShowMenuMachine);
    if (fAllowToShowMenuMachine)
        m_mainMenus << action(UIActionIndexRT_M_Machine)->menu();
    updateMenuMachine();

    /* 'View' menu: */
    const bool fAllowToShowMenuView = isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType_View);
    action(UIActionIndexRT_M_View)->setVisible(fAllowToShowMenuView);
    action(UIActionIndexRT_M_ViewPopup)->setVisible(fAllowToShowMenuView);
    if (fAllowToShowMenuView)
        m_mainMenus << action(UIActionIndexRT_M_View)->menu();
    updateMenuView();
    updateMenuViewPopup();

    /* 'Devices' menu: */
    const bool fAllowToShowMenuDevices = isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType_Devices);
    action(UIActionIndexRT_M_Devices)->setVisible(fAllowToShowMenuDevices);
    if (fAllowToShowMenuDevices)
        m_mainMenus << action(UIActionIndexRT_M_Devices)->menu();
    updateMenuDevices();

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' menu: */
    const bool fAllowToShowMenuDebug = isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType_Debug);
    action(UIActionIndexRT_M_Debug)->setVisible(fAllowToShowMenuDebug);
    if (fAllowToShowMenuDebug)
        m_mainMenus << action(UIActionIndexRT_M_Debug)->menu();
    updateMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* 'Help' menu: */
    const bool fAllowToShowMenuHelp = isAllowedInMenuBar(UIExtraDataMetaDefs::RuntimeMenuType_Help);
    action(UIActionIndex_Menu_Help)->setVisible(fAllowToShowMenuHelp);
    if (fAllowToShowMenuHelp)
        m_mainMenus << action(UIActionIndex_Menu_Help)->menu();
    updateMenuHelp();
}

void UIActionPoolRuntime::updateMenuMachine()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Machine)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();


    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Settings Dialog' action: */
    const bool fAllowToShowActionSettingsDialog = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SettingsDialog);
    action(UIActionIndexRT_M_Machine_S_Settings)->setEnabled(fAllowToShowActionSettingsDialog);
    if (fAllowToShowActionSettingsDialog)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_Settings));
        fSeparator1 = true;
    }

    /* 'Take Snapshot' action: */
    const bool fAllowToShowActionTakeSnapshot = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeSnapshot);
    action(UIActionIndexRT_M_Machine_S_TakeSnapshot)->setEnabled(fAllowToShowActionTakeSnapshot);
    if (fAllowToShowActionTakeSnapshot)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_TakeSnapshot));
        fSeparator1 = true;
    }

    /* 'Take Screenshot' action: */
    const bool fAllowToShowActionTakeScreenshot = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TakeScreenshot);
    action(UIActionIndexRT_M_Machine_S_TakeScreenshot)->setEnabled(fAllowToShowActionTakeScreenshot);
    if (fAllowToShowActionTakeScreenshot)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_TakeScreenshot));
        fSeparator1 = true;
    }

    /* 'Information Dialog' action: */
    const bool fAllowToShowActionInformationDialog = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_InformationDialog);
    action(UIActionIndexRT_M_Machine_S_ShowInformation)->setEnabled(fAllowToShowActionInformationDialog);
    if (fAllowToShowActionInformationDialog)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_ShowInformation));
        fSeparator1 = true;
    }

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* 'Keyboard' submenu: */
    const bool fAllowToShowActionKeyboard = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Keyboard);
    action(UIActionIndexRT_M_Machine_M_Keyboard)->setEnabled(fAllowToShowActionKeyboard);
    if (fAllowToShowActionKeyboard)
    {
//        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Keyboard));
//        fSeparator2 = true;
    }
    updateMenuMachineKeyboard();

    /* 'Keyboard Settings' action: */
    const bool fAllowToShowActionKeyboardSettings = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_KeyboardSettings);
    action(UIActionIndexRT_M_Machine_M_Keyboard_S_Settings)->setEnabled(fAllowToShowActionKeyboardSettings);
    if (fAllowToShowActionKeyboardSettings)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Keyboard_S_Settings));
        fSeparator2 = true;
    }

    /* 'Mouse' submenu: */
    const bool fAllowToShowActionMouse = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Mouse);
    action(UIActionIndexRT_M_Machine_M_Mouse)->setEnabled(fAllowToShowActionMouse);
    if (fAllowToShowActionMouse)
    {
//        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Mouse));
//        fSeparator2 = true;
    }
    updateMenuMachineMouse();

    /* 'Mouse Integration' action: */
    const bool fAllowToShowActionMouseIntegration = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_MouseIntegration);
    action(UIActionIndexRT_M_Machine_M_Mouse_T_Integration)->setEnabled(fAllowToShowActionMouseIntegration);
    if (fAllowToShowActionMouseIntegration)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Mouse_T_Integration));
        fSeparator2 = true;
    }

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Separator #3? */
    bool fSeparator3 = false;

    /* 'Type CAD' action: */
    const bool fAllowToShowActionTypeCAD = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCAD);
    action(UIActionIndexRT_M_Machine_S_TypeCAD)->setEnabled(fAllowToShowActionTypeCAD);
    if (fAllowToShowActionTypeCAD)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_TypeCAD));
        fSeparator3 = true;
    }

#ifdef Q_WS_X11
    /* 'Type CABS' action: */
    const bool fAllowToShowActionTypeCABS = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_TypeCABS);
    action(UIActionIndexRT_M_Machine_S_TypeCABS)->setEnabled(fAllowToShowActionTypeCABS);
    if (fAllowToShowActionTypeCABS)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_TypeCABS));
        fSeparator3 = true;
    }
#endif /* Q_WS_X11 */

    /* Separator #3: */
    if (fSeparator3)
        pMenu->addSeparator();


    /* Separator #4? */
    bool fSeparator4 = false;

    /* 'Pause' action: */
    const bool fAllowToShowActionPause = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Pause);
    action(UIActionIndexRT_M_Machine_T_Pause)->setEnabled(fAllowToShowActionPause);
    if (fAllowToShowActionPause)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_T_Pause));
        fSeparator4 = true;
    }

    /* 'Reset' action: */
    const bool fAllowToShowActionReset = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Reset);
    action(UIActionIndexRT_M_Machine_S_Reset)->setEnabled(fAllowToShowActionReset);
    if (fAllowToShowActionReset)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_Reset));
        fSeparator4 = true;
    }

    /* 'Save' action: */
    const bool fAllowToShowActionSaveState = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_SaveState);
    action(UIActionIndexRT_M_Machine_S_Save)->setEnabled(fAllowToShowActionSaveState);
    if (fAllowToShowActionSaveState)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_Save));
        fSeparator4 = true;
    }

    /* 'Shutdown' action: */
    const bool fAllowToShowActionShutdown = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Shutdown);
    action(UIActionIndexRT_M_Machine_S_Shutdown)->setEnabled(fAllowToShowActionShutdown);
    if (fAllowToShowActionShutdown)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_Shutdown));
        fSeparator4 = true;
    }

    /* 'PowerOff' action: */
    const bool fAllowToShowActionPowerOff = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_PowerOff);
    action(UIActionIndexRT_M_Machine_S_PowerOff)->setEnabled(fAllowToShowActionPowerOff);
    if (fAllowToShowActionPowerOff)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Machine_S_PowerOff));
        fSeparator4 = true;
    }

#ifndef Q_WS_MAC
    /* Separator #4: */
    if (fSeparator4)
        pMenu->addSeparator();
#endif /* !Q_WS_MAC */


#ifndef Q_WS_MAC
    /* Close action: */
    const bool fAllowToShowActionClose = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Close);
    pMenu->addAction(action(UIActionIndexRT_M_Machine_S_Close));
    action(UIActionIndexRT_M_Machine_S_Close)->setEnabled(fAllowToShowActionClose);
#endif /* !Q_WS_MAC */


    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Machine);
}

void UIActionPoolRuntime::updateMenuMachineKeyboard()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Machine_M_Keyboard)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Keyboard Settings' action: */
    const bool fAllowToShowActionKeyboardSettings = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_KeyboardSettings);
    action(UIActionIndexRT_M_Machine_M_Keyboard_S_Settings)->setEnabled(fAllowToShowActionKeyboardSettings);
    if (fAllowToShowActionKeyboardSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Keyboard_S_Settings));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Machine_M_Keyboard);
}

void UIActionPoolRuntime::updateMenuMachineMouse()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Machine_M_Mouse)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Machine Integration' action: */
    const bool fAllowToShowActionMouseIntegration = isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_MouseIntegration);
    action(UIActionIndexRT_M_Machine_M_Mouse_T_Integration)->setEnabled(fAllowToShowActionMouseIntegration);
    if (fAllowToShowActionMouseIntegration)
        pMenu->addAction(action(UIActionIndexRT_M_Machine_M_Mouse_T_Integration));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Machine_M_Mouse);
}

void UIActionPoolRuntime::updateMenuView()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_View)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();


    /* Visual representation mode flags: */
    const bool fIsAllowToShowActionFullscreen = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Fullscreen);
    const bool fIsAllowToShowActionSeamless   = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Seamless);
    const bool fIsAllowToShowActionScale      = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Scale);

    /* 'Fullscreen' action: */
    action(UIActionIndexRT_M_View_T_Fullscreen)->setEnabled(fIsAllowToShowActionFullscreen);
    if (fIsAllowToShowActionFullscreen)
        pMenu->addAction(action(UIActionIndexRT_M_View_T_Fullscreen));

    /* 'Seamless' action: */
    action(UIActionIndexRT_M_View_T_Seamless)->setEnabled(fIsAllowToShowActionSeamless);
    if (fIsAllowToShowActionSeamless)
        pMenu->addAction(action(UIActionIndexRT_M_View_T_Seamless));

    /* 'Scale' action: */
    action(UIActionIndexRT_M_View_T_Scale)->setEnabled(fIsAllowToShowActionScale);
    if (fIsAllowToShowActionScale)
        pMenu->addAction(action(UIActionIndexRT_M_View_T_Scale));

    /* Visual representation mode separator: */
    if (fIsAllowToShowActionFullscreen || fIsAllowToShowActionSeamless || fIsAllowToShowActionScale)
        pMenu->addSeparator();


    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Adjust Window' action: */
    const bool fAllowToShowActionAdjustWindow = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_AdjustWindow);
    action(UIActionIndexRT_M_View_S_AdjustWindow)->setEnabled(fAllowToShowActionAdjustWindow);
    if (fAllowToShowActionAdjustWindow)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_S_AdjustWindow));
        fSeparator1 = true;
    }

    /* 'Guest Autoresize' action: */
    const bool fAllowToShowActionGuestAutoresize = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_GuestAutoresize);
    action(UIActionIndexRT_M_View_T_GuestAutoresize)->setEnabled(fAllowToShowActionGuestAutoresize);
    if (fAllowToShowActionGuestAutoresize)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_T_GuestAutoresize));
        fSeparator1 = true;
    }

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* 'Menu Bar' submenu: */
    const bool fAllowToShowActionMenuBar = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBar);
    action(UIActionIndexRT_M_View_M_MenuBar)->setEnabled(fAllowToShowActionMenuBar);
    if (fAllowToShowActionMenuBar)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_M_MenuBar));
        fSeparator2 = true;
    }
    updateMenuViewMenuBar();

    /* 'Status Bar' submenu: */
    const bool fAllowToShowActionStatusBar = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBar);
    action(UIActionIndexRT_M_View_M_StatusBar)->setEnabled(fAllowToShowActionStatusBar);
    if (fAllowToShowActionStatusBar)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_M_StatusBar));
        fSeparator2 = true;
    }
    updateMenuViewStatusBar();

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Do we have to show resize or multiscreen menu? */
    const bool fAllowToShowActionResize = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Resize);
    const bool fAllowToShowActionMultiscreen = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Multiscreen);
    if (fAllowToShowActionResize && session())
    {
        for (int iGuestScreenIndex = 0; iGuestScreenIndex < session()->frameBuffers().size(); ++iGuestScreenIndex)
        {
            /* Add 'Virtual Screen %1' menu: */
            QMenu *pSubMenu = pMenu->addMenu(QApplication::translate("UIMultiScreenLayout",
                                                                     "Virtual Screen %1").arg(iGuestScreenIndex + 1));
            pSubMenu->setProperty("Guest Screen Index", iGuestScreenIndex);
            connect(pSubMenu, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuViewScreen()));
        }
    }
    else if (fAllowToShowActionMultiscreen && multiScreenLayout())
    {
        /* Only for multi-screen host case: */
        if (session()->hostScreens().size() > 1)
        {
            for (int iGuestScreenIndex = 0; iGuestScreenIndex < session()->frameBuffers().size(); ++iGuestScreenIndex)
            {
                /* Add 'Virtual Screen %1' menu: */
                QMenu *pSubMenu = pMenu->addMenu(QApplication::translate("UIMultiScreenLayout",
                                                                         "Virtual Screen %1").arg(iGuestScreenIndex + 1));
                pSubMenu->setProperty("Guest Screen Index", iGuestScreenIndex);
                connect(pSubMenu, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuViewMultiscreen()));
            }
        }
    }


    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_View);
}

void UIActionPoolRuntime::updateMenuViewPopup()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_ViewPopup)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();


    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Adjust Window' action: */
    const bool fAllowToShowActionAdjustWindow = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_AdjustWindow);
    action(UIActionIndexRT_M_View_S_AdjustWindow)->setEnabled(fAllowToShowActionAdjustWindow);
    if (fAllowToShowActionAdjustWindow)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_S_AdjustWindow));
        fSeparator1 = true;
    }

    /* 'Guest Autoresize' action: */
    const bool fAllowToShowActionGuestAutoresize = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_GuestAutoresize);
    action(UIActionIndexRT_M_View_T_GuestAutoresize)->setEnabled(fAllowToShowActionGuestAutoresize);
    if (fAllowToShowActionGuestAutoresize)
    {
        pMenu->addAction(action(UIActionIndexRT_M_View_T_GuestAutoresize));
        fSeparator1 = true;
    }

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Do we have to show resize menu? */
    const bool fAllowToShowActionResize = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Resize);
    if (fAllowToShowActionResize && session())
    {
        for (int iGuestScreenIndex = 0; iGuestScreenIndex < session()->frameBuffers().size(); ++iGuestScreenIndex)
        {
            /* Add 'Virtual Screen %1' menu: */
            QMenu *pSubMenu = pMenu->addMenu(QApplication::translate("UIMultiScreenLayout",
                                                                     "Virtual Screen %1").arg(iGuestScreenIndex + 1));
            pSubMenu->setProperty("Guest Screen Index", iGuestScreenIndex);
            connect(pSubMenu, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuViewScreen()));
        }
    }


    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_ViewPopup);
}

void UIActionPoolRuntime::updateMenuViewMenuBar()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_View_M_MenuBar)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Menu Bar Settings' action: */
    const bool fAllowToShowActionMenuBarSettings = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBarSettings);
    action(UIActionIndexRT_M_View_M_MenuBar_S_Settings)->setEnabled(fAllowToShowActionMenuBarSettings);
    if (fAllowToShowActionMenuBarSettings)
        pMenu->addAction(action(UIActionIndexRT_M_View_M_MenuBar_S_Settings));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_View_M_MenuBar);
}

void UIActionPoolRuntime::updateMenuViewStatusBar()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_View_M_StatusBar)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Status Bar Settings' action: */
    const bool fAllowToShowActionStatusBarSettings = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBarSettings);
    action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(fAllowToShowActionStatusBarSettings);
    if (fAllowToShowActionStatusBarSettings)
        pMenu->addAction(action(UIActionIndexRT_M_View_M_StatusBar_S_Settings));

    /* 'Toggle Status Bar' action: */
    const bool fAllowToShowActionToggleStatusBar = isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_ToggleStatusBar);
    action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setEnabled(fAllowToShowActionToggleStatusBar);
    if (fAllowToShowActionToggleStatusBar)
        pMenu->addAction(action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_View_M_StatusBar);
}

void UIActionPoolRuntime::updateMenuViewScreen(QMenu *pMenu)
{
    /* Make sure UI session defined: */
    AssertPtrReturnVoid(session());

    /* Clear contents: */
    pMenu->clear();

    /* Prepare new contents: */
    const QList<QSize> sizes = QList<QSize>()
                               << QSize(640, 480)
                               << QSize(800, 600)
                               << QSize(1024, 768)
                               << QSize(1280, 720)
                               << QSize(1280, 800)
                               << QSize(1366, 768)
                               << QSize(1440, 900)
                               << QSize(1600, 900)
                               << QSize(1680, 1050)
                               << QSize(1920, 1080)
                               << QSize(1920, 1200);

    /* Get corresponding screen index and frame-buffer size: */
    const int iGuestScreenIndex = pMenu->property("Guest Screen Index").toInt();
    const UIFrameBuffer* pFrameBuffer = session()->frameBuffer(iGuestScreenIndex);
    const QSize screenSize = QSize(pFrameBuffer->width(), pFrameBuffer->height());
    const bool fScreenEnabled = session()->isScreenVisible(iGuestScreenIndex);

    /* For non-primary screens: */
    if (iGuestScreenIndex > 0)
    {
        /* Create 'toggle' action: */
        QAction *pToggleAction = pMenu->addAction(UIActionPoolRuntime::tr("Enable", "Virtual Screen"),
                                                  this, SLOT(sltHandleActionTriggerViewScreenToggle()));
        AssertPtrReturnVoid(pToggleAction);
        {
            /* Configure 'toggle' action: */
            pToggleAction->setEnabled(session()->isGuestSupportsGraphics());
            pToggleAction->setProperty("Guest Screen Index", iGuestScreenIndex);
            pToggleAction->setCheckable(true);
            pToggleAction->setChecked(fScreenEnabled);
            /* Add separator: */
            pMenu->addSeparator();
        }
    }

    /* Create exclusive 'resize' action-group: */
    QActionGroup *pActionGroup = new QActionGroup(pMenu);
    AssertPtrReturnVoid(pActionGroup);
    {
        /* Configure exclusive 'resize' action-group: */
        pActionGroup->setExclusive(true);
        /* For every available size: */
        foreach (const QSize &size, sizes)
        {
            /* Create exclusive 'resize' action: */
            QAction *pAction = pActionGroup->addAction(UIActionPoolRuntime::tr("Resize to %1x%2", "Virtual Screen")
                                                                               .arg(size.width()).arg(size.height()));
            AssertPtrReturnVoid(pAction);
            {
                /* Configure exclusive 'resize' action: */
                pAction->setEnabled(session()->isGuestSupportsGraphics() && fScreenEnabled);
                pAction->setProperty("Guest Screen Index", iGuestScreenIndex);
                pAction->setProperty("Requested Size", size);
                pAction->setCheckable(true);
                if (screenSize.width() == size.width() &&
                    screenSize.height() == size.height())
                {
                    pAction->setChecked(true);
                }
            }
        }
        /* Insert group actions into menu: */
        pMenu->addActions(pActionGroup->actions());
        /* Install listener for exclusive action-group: */
        connect(pActionGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltHandleActionTriggerViewScreenResize(QAction*)));
    }
}

void UIActionPoolRuntime::updateMenuViewMultiscreen(QMenu *pMenu)
{
    /* Make sure UI session defined: */
    AssertPtrReturnVoid(multiScreenLayout());

    /* Clear contents: */
    pMenu->clear();

    /* Get corresponding screen index and size: */
    const int iGuestScreenIndex = pMenu->property("Guest Screen Index").toInt();

    /* Create exclusive action-group: */
    QActionGroup *pActionGroup = new QActionGroup(pMenu);
    AssertPtrReturnVoid(pActionGroup);
    {
        /* Configure exclusive action-group: */
        pActionGroup->setExclusive(true);
        for (int iHostScreenIndex = 0; iHostScreenIndex < session()->hostScreens().size(); ++iHostScreenIndex)
        {
            QAction *pAction = pActionGroup->addAction(UIMultiScreenLayout::tr("Use Host Screen %1")
                                                                               .arg(iHostScreenIndex + 1));
            AssertPtrReturnVoid(pAction);
            {
                pAction->setCheckable(true);
                pAction->setProperty("Guest Screen Index", iGuestScreenIndex);
                pAction->setProperty("Host Screen Index", iHostScreenIndex);
                if (multiScreenLayout()->hasHostScreenForGuestScreen(iGuestScreenIndex) &&
                    multiScreenLayout()->hostScreenForGuestScreen(iGuestScreenIndex) == iHostScreenIndex)
                    pAction->setChecked(true);
            }
        }
        /* Insert group actions into menu: */
        pMenu->addActions(pActionGroup->actions());
        /* Install listener for exclusive action-group: */
        connect(pActionGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltHandleActionTriggerViewScreenRemap(QAction*)));
    }
}

void UIActionPoolRuntime::updateMenuDevices()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();


    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Hard Drives' submenu: */
    const bool fAllowToShowActionHardDrives = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrives);
    action(UIActionIndexRT_M_Devices_M_HardDrives)->setEnabled(fAllowToShowActionHardDrives);
    if (fAllowToShowActionHardDrives)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_HardDrives));
        fSeparator1 = true;
    }
    updateMenuDevicesHardDrives();

    /* 'Optical Devices' submenu: */
    const bool fAllowToShowActionOpticalDevices = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_OpticalDevices);
    action(UIActionIndexRT_M_Devices_M_OpticalDevices)->setEnabled(fAllowToShowActionOpticalDevices);
    if (fAllowToShowActionOpticalDevices)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_OpticalDevices));
        fSeparator1 = true;
    }

    /* 'Floppy Devices' submenu: */
    const bool fAllowToShowActionFloppyDevices = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_FloppyDevices);
    action(UIActionIndexRT_M_Devices_M_FloppyDevices)->setEnabled(fAllowToShowActionFloppyDevices);
    if (fAllowToShowActionFloppyDevices)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_FloppyDevices));
        fSeparator1 = true;
    }

    /* 'Network' submenu: */
    const bool fAllowToShowActionNetwork = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Network);
    action(UIActionIndexRT_M_Devices_M_Network)->setEnabled(fAllowToShowActionNetwork);
    if (fAllowToShowActionNetwork)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_Network));
        fSeparator1 = true;
    }
    updateMenuDevicesNetwork();

    /* 'USB Devices' submenu: */
    const bool fAllowToShowActionUSBDevices = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevices);
    action(UIActionIndexRT_M_Devices_M_USBDevices)->setEnabled(fAllowToShowActionUSBDevices);
    if (fAllowToShowActionUSBDevices)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_USBDevices));
        fSeparator1 = true;
    }
    updateMenuDevicesUSBDevices();

    /* 'Web Cams' submenu: */
    const bool fAllowToShowActionWebCams = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_WebCams);
    action(UIActionIndexRT_M_Devices_M_WebCams)->setEnabled(fAllowToShowActionWebCams);
    if (fAllowToShowActionWebCams)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_WebCams));
        fSeparator1 = true;
    }

    /* 'Shared Clipboard' submenu: */
    const bool fAllowToShowActionSharedClipboard = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedClipboard);
    action(UIActionIndexRT_M_Devices_M_SharedClipboard)->setEnabled(fAllowToShowActionSharedClipboard);
    if (fAllowToShowActionSharedClipboard)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_SharedClipboard));
        fSeparator1 = true;
    }

    /* 'Drag&Drop' submenu: */
    const bool fAllowToShowActionDragAndDrop = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_DragAndDrop);
    action(UIActionIndexRT_M_Devices_M_DragAndDrop)->setEnabled(fAllowToShowActionDragAndDrop);
    if (fAllowToShowActionDragAndDrop)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_DragAndDrop));
        fSeparator1 = true;
    }

    /* 'Shared Folders' submenu: */
    const bool fAllowToShowActionSharedFolders = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFolders);
    action(UIActionIndexRT_M_Devices_M_SharedFolders)->setEnabled(fAllowToShowActionSharedFolders);
    if (fAllowToShowActionSharedFolders)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_SharedFolders));
        fSeparator1 = true;
    }
    updateMenuDevicesSharedFolders();

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* 'VRDE Server' action: */
    const bool fAllowToShowActionVRDEServer = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VRDEServer);
    action(UIActionIndexRT_M_Devices_T_VRDEServer)->setEnabled(fAllowToShowActionVRDEServer);
    if (fAllowToShowActionVRDEServer)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_T_VRDEServer));
        fSeparator2 = true;
    }

    /* 'Video Capture' action: */
    const bool fAllowToShowActionVideoCapture = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_StartVideoCapture);
    action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start)->setEnabled(fAllowToShowActionVideoCapture);
    if (fAllowToShowActionVideoCapture)
    {
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start));
        fSeparator2 = true;
    }
    updateMenuDevicesVideoCapture();

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Install Guest Tools action: */
    const bool fAllowToShowActionInstallGuestTools = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_InstallGuestTools);
    action(UIActionIndexRT_M_Devices_S_InstallGuestTools)->setEnabled(fAllowToShowActionInstallGuestTools);
    if (fAllowToShowActionInstallGuestTools)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_S_InstallGuestTools));


    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Devices);
}

void UIActionPoolRuntime::updateMenuDevicesHardDrives()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices_M_HardDrives)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Hard Drives Settings' action: */
    const bool fAllowToShowActionHardDrivesSettings = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_HardDrivesSettings);
    action(UIActionIndexRT_M_Devices_M_HardDrives_S_Settings)->setEnabled(fAllowToShowActionHardDrivesSettings);
    if (fAllowToShowActionHardDrivesSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_HardDrives_S_Settings));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Devices_M_HardDrives);
}

void UIActionPoolRuntime::updateMenuDevicesNetwork()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices_M_Network)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Network Settings' action: */
    const bool fAllowToShowActionNetworkSettings = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_NetworkSettings);
    action(UIActionIndexRT_M_Devices_M_Network_S_Settings)->setEnabled(fAllowToShowActionNetworkSettings);
    if (fAllowToShowActionNetworkSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_Network_S_Settings));
}

void UIActionPoolRuntime::updateMenuDevicesUSBDevices()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices_M_USBDevices)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'USB Devices Settings' action: */
    const bool fAllowToShowActionUSBDevicesSettings = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevicesSettings);
    action(UIActionIndexRT_M_Devices_M_USBDevices_S_Settings)->setEnabled(fAllowToShowActionUSBDevicesSettings);
    if (fAllowToShowActionUSBDevicesSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_USBDevices_S_Settings));
}

void UIActionPoolRuntime::updateMenuDevicesSharedFolders()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices_M_SharedFolders)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Shared Folders Settings' action: */
    const bool fAllowToShowActionSharedFoldersSettings = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_SharedFoldersSettings);
    action(UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings)->setEnabled(fAllowToShowActionSharedFoldersSettings);
    if (fAllowToShowActionSharedFoldersSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Devices_M_SharedFolders);
}

void UIActionPoolRuntime::updateMenuDevicesVideoCapture()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Devices_M_VideoCapture)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Video Capture Settings' action: */
    const bool fAllowToShowActionVideoCaptureSettings = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_VideoCaptureSettings);
    action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings)->setEnabled(fAllowToShowActionVideoCaptureSettings);
    if (fAllowToShowActionVideoCaptureSettings)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings));

    /* 'Start Video Capture' action: */
    const bool fAllowToShowActionStartVideoCapture = isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_StartVideoCapture);
    action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings)->setEnabled(fAllowToShowActionStartVideoCapture);
    if (fAllowToShowActionStartVideoCapture)
        pMenu->addAction(action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Devices_M_VideoCapture);
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIActionPoolRuntime::updateMenuDebug()
{
    /* Get corresponding menu: */
    QMenu *pMenu = action(UIActionIndexRT_M_Debug)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Statistics' action: */
    const bool fAllowToShowActionStatistics = isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Statistics);
    action(UIActionIndexRT_M_Debug_S_ShowStatistics)->setEnabled(fAllowToShowActionStatistics);
    if (fAllowToShowActionStatistics)
        pMenu->addAction(action(UIActionIndexRT_M_Debug_S_ShowStatistics));

    /* 'Command Line' action: */
    const bool fAllowToShowActionCommandLine = isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_CommandLine);
    action(UIActionIndexRT_M_Debug_S_ShowCommandLine)->setEnabled(fAllowToShowActionCommandLine);
    if (fAllowToShowActionCommandLine)
        pMenu->addAction(action(UIActionIndexRT_M_Debug_S_ShowCommandLine));

    /* 'Logging' action: */
    const bool fAllowToShowActionLogging = isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Logging);
    action(UIActionIndexRT_M_Debug_T_Logging)->setEnabled(fAllowToShowActionLogging);
    if (fAllowToShowActionLogging)
        pMenu->addAction(action(UIActionIndexRT_M_Debug_T_Logging));

    /* 'Log Dialog' action: */
    const bool fAllowToShowActionLogDialog = isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_LogDialog);
    action(UIActionIndex_Simple_LogDialog)->setEnabled(fAllowToShowActionLogDialog);
    if (fAllowToShowActionLogDialog)
        pMenu->addAction(action(UIActionIndex_Simple_LogDialog));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexRT_M_Debug);
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIActionPoolRuntime::retranslateUi()
{
    /* Call to base-class: */
    UIActionPool::retranslateUi();
    /* Create temporary Selector UI pool to do the same: */
    if (!m_fTemporary)
        UIActionPool::createTemporary(UIActionPoolType_Selector);
}

QString UIActionPoolRuntime::shortcutsExtraDataID() const
{
    return GUI_Input_MachineShortcuts;
}

#include "UIActionPoolRuntime.moc"

