/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionsPool class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include "UIActionsPool.h"
#include "UIIconPool.h"
#include "UIMachineShortcuts.h"
#include "VBoxGlobal.h"

/* Global includes */
#include <QHelpEvent>
#include <QToolTip>
#include <QtGlobal>

/* Extended QMenu class used in UIActions */
class QIMenu : public QMenu
{
    Q_OBJECT;

public:

    /* QIMenu constructor: */
    QIMenu(QWidget *pParent = 0) : QMenu(pParent), m_fShowToolTips(false) {}

    /* Setter/getter for 'show-tool-tips' feature: */
    void setShowToolTips(bool fShowToolTips) { m_fShowToolTips = fShowToolTips; }
    bool isToolTipsShown() const { return m_fShowToolTips; }

private:

    /* Event handler reimplementation: */
    bool event(QEvent *pEvent)
    {
        /* Handle particular event-types: */
        switch (pEvent->type())
        {
            /* Tool-tip request handler: */
            case QEvent::ToolTip:
            {
                /* Get current help-event: */
                QHelpEvent *pHelpEvent = static_cast<QHelpEvent*>(pEvent);
                /* Get action which caused help-event: */
                QAction *pAction = actionAt(pHelpEvent->pos());
                /* If action present => show action's tool-tip if needed: */
                if (pAction && m_fShowToolTips)
                    QToolTip::showText(pHelpEvent->globalPos(), pAction->toolTip());
                break;
            }
            default:
                break;
        }
        /* Base-class event-handler: */
        return QMenu::event(pEvent);
    }

    /* Reflects 'show-tool-tip' feature activity: */
    bool m_fShowToolTips;
};

/* Action activation event */
class ActivateActionEvent : public QEvent
{
public:

    ActivateActionEvent(QAction *pAction)
        : QEvent((QEvent::Type)VBoxDefs::ActivateActionEventType)
        , m_pAction(pAction) {}
    QAction* action() const { return m_pAction; }

private:

    QAction *m_pAction;
};

UIAction::UIAction(QObject *pParent, UIActionType type)
    : QIWithRetranslateUI3<QAction>(pParent)
    , m_type(type)
{
    /* Default is no specific menu role. We will set them explicit later. */
    setMenuRole(QAction::NoRole);
}

UIActionType UIAction::type() const
{
    return m_type;
}

class UISimpleAction : public UIAction
{
    Q_OBJECT;

public:

    UISimpleAction(QObject *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString())
        : UIAction(pParent, UIActionType_Simple)
    {
        if (!strIcon.isNull())
            setIcon(UIIconPool::iconSet(strIcon,
                                        strIconDis));
    }

    UISimpleAction(QObject *pParent,
                   const QIcon& icon)
        : UIAction(pParent, UIActionType_Simple)
    {
        if (!icon.isNull())
            setIcon(icon);
    }
};

class UIToggleAction : public UIAction
{
    Q_OBJECT;

public:

    UIToggleAction(QObject *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString())
        : UIAction(pParent, UIActionType_Toggle)
    {
        if (!strIcon.isNull())
            setIcon(UIIconPool::iconSet(strIcon,
                                        strIconDis));
        init();
    }

    UIToggleAction(QObject *pParent,
                   const QString &strIconOn, const QString &strIconOff,
                   const QString &strIconOnDis, const QString &strIconOffDis)
        : UIAction(pParent, UIActionType_Toggle)
    {
        setIcon(UIIconPool::iconSetOnOff(strIconOn, strIconOff,
                                         strIconOnDis, strIconOffDis));
        init();
    }

    UIToggleAction(QObject *pParent,
                   const QIcon &icon)
        : UIAction(pParent, UIActionType_Toggle)
    {
        if (!icon.isNull())
            setIcon(icon);
        init();
    }

private slots:

    void sltUpdateAppearance()
    {
        retranslateUi();
    }

private:

    void init()
    {
        setCheckable(true);
        connect(this, SIGNAL(toggled(bool)), this, SLOT(sltUpdateAppearance()));
    }
};

class UIMenuAction : public UIAction
{
    Q_OBJECT;

public:

    UIMenuAction(QObject *pParent,
                 const QString &strIcon = QString(), const QString &strIconDis = QString())
        : UIAction(pParent, UIActionType_Menu)
    {
        if (!strIcon.isNull())
            setIcon(UIIconPool::iconSet(strIcon,
                                        strIconDis));
        setMenu(new QIMenu);
    }

    UIMenuAction(QObject *pParent,
                 const QIcon &icon)
        : UIAction(pParent, UIActionType_Menu)
    {
        if (!icon.isNull())
            setIcon(icon);
        setMenu(new QIMenu);
    }
};

class MenuMachineAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuMachineAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&Machine"));
    }
};

class ShowSettingsDialogAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowSettingsDialogAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/settings_16px.png", ":/settings_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Settings..."), gMS->shortcut(UIMachineShortcuts::SettingsDialogShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Manage the virtual machine settings"));
    }
};

class PerformTakeSnapshotAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformTakeSnapshotAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/take_snapshot_16px.png", ":/take_snapshot_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Take &Snapshot..."), gMS->shortcut(UIMachineShortcuts::TakeSnapshotShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Take a snapshot of the virtual machine"));
    }
};

class ShowInformationDialogAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowInformationDialogAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/session_info_16px.png", ":/session_info_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Session I&nformation..."), gMS->shortcut(UIMachineShortcuts::InformationDialogShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Show Session Information Dialog"));
    }
};

class MenuMouseIntegrationAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuMouseIntegrationAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
    }
};

class ToggleMouseIntegrationAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleMouseIntegrationAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/mouse_can_seamless_on_16px.png", ":/mouse_can_seamless_16px.png",
                         ":/mouse_can_seamless_on_disabled_16px.png", ":/mouse_can_seamless_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Disable &Mouse Integration"), gMS->shortcut(UIMachineShortcuts::MouseIntegrationShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Temporarily disable host mouse pointer integration"));
    }
};

class PerformTypeCADAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformTypeCADAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/hostkey_16px.png", ":/hostkey_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Insert Ctrl-Alt-Del"), gMS->shortcut(UIMachineShortcuts::TypeCADShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Send the Ctrl-Alt-Del sequence to the virtual machine"));
    }
};

#ifdef Q_WS_X11
class PerformTypeCABSAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformTypeCABSAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/hostkey_16px.png", ":/hostkey_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Insert Ctrl-Alt-Backspace"), gMS->shortcut(UIMachineShortcuts::TypeCABSShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
    }
};
#endif

class TogglePauseAction : public UIToggleAction
{
    Q_OBJECT;

public:

    TogglePauseAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/pause_16px.png", ":/pause_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Pause"), gMS->shortcut(UIMachineShortcuts::PauseShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Suspend the execution of the virtual machine"));
    }
};

class PerformResetAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformResetAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/reset_16px.png", ":/reset_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Reset"), gMS->shortcut(UIMachineShortcuts::ResetShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Reset the virtual machine"));
    }
};

class PerformShutdownAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformShutdownAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/acpi_16px.png", ":/acpi_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "ACPI Sh&utdown"), gMS->shortcut(UIMachineShortcuts::ShutdownShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Send the ACPI Power Button press event to the virtual machine"));
    }
};

class PerformCloseAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformCloseAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Close..."), gMS->shortcut(UIMachineShortcuts::CloseShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Close the virtual machine"));
    }
};

class MenuViewAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuViewAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&View"));
    }
};

class ToggleFullscreenModeAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleFullscreenModeAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/fullscreen_on_16px.png", ":/fullscreen_16px.png",
                         ":/fullscreen_on_disabled_16px.png", ":/fullscreen_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Switch to &Fullscreen"), gMS->shortcut(UIMachineShortcuts::FullscreenModeShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Switch between normal and fullscreen mode"));
    }
};

class ToggleSeamlessModeAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleSeamlessModeAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/seamless_on_16px.png", ":/seamless_16px.png",
                         ":/seamless_on_disabled_16px.png", ":/seamless_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Switch to Seam&less Mode"), gMS->shortcut(UIMachineShortcuts::SeamlessModeShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Switch between normal and seamless desktop integration mode"));
    }
};

class ToggleScaleModeAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleScaleModeAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/scale_on_16px.png", ":/scale_16px.png",
                         ":/scale_on_disabled_16px.png", ":/scale_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Switch to &Scale Mode"), gMS->shortcut(UIMachineShortcuts::ScaleModeShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Switch between normal and scale mode"));
    }
};

class ToggleGuestAutoresizeAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleGuestAutoresizeAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/auto_resize_on_on_16px.png", ":/auto_resize_on_16px.png",
                         ":/auto_resize_on_on_disabled_16px.png", ":/auto_resize_on_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Auto-resize &Guest Display"), gMS->shortcut(UIMachineShortcuts::GuestAutoresizeShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Automatically resize the guest display when the window is resized (requires Guest Additions)"));
    }
};

class PerformWindowAdjustAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformWindowAdjustAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/adjust_win_size_16px.png", ":/adjust_win_size_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Adjust Window Size"), gMS->shortcut(UIMachineShortcuts::WindowAdjustShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Adjust window size and position to best fit the guest display"));
    }
};

class MenuDevicesAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuDevicesAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&Devices"));
    }
};

class MenuOpticalDevicesAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuOpticalDevicesAction(QObject *pParent)
        : UIMenuAction(pParent,
                       ":/cd_16px.png", ":/cd_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&CD/DVD Devices"));
    }
};

class MenuFloppyDevicesAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuFloppyDevicesAction(QObject *pParent)
        : UIMenuAction(pParent,
                       ":/fd_16px.png", ":/fd_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&Floppy Devices"));
    }
};

class MenuUSBDevicesAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuUSBDevicesAction(QObject *pParent)
        : UIMenuAction(pParent,
                       ":/usb_16px.png", ":/usb_disabled_16px.png")
    {
        qobject_cast<QIMenu*>(menu())->setShowToolTips(true);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "&USB Devices"));
    }
};

class MenuNetworkAdaptersAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuNetworkAdaptersAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
    }
};

class ShowNetworkAdaptersDialogAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowNetworkAdaptersDialogAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/nw_16px.png", ":/nw_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Network Adapters..."), gMS->shortcut(UIMachineShortcuts::NetworkAdaptersDialogShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Change the settings of network adapters"));
    }
};

class MenuSharedFoldersAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuSharedFoldersAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
    }
};

class ShowSharedFoldersDialogAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowSharedFoldersDialogAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Shared Folders..."), gMS->shortcut(UIMachineShortcuts::SharedFoldersDialogShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Create or modify shared folders"));
    }
};

class ToggleVRDEServerAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleVRDEServerAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/vrdp_on_16px.png", ":/vrdp_16px.png",
                         ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Enable R&emote Display"), gMS->shortcut(UIMachineShortcuts::VRDPServerShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Enable remote desktop (RDP) connections to this machine"));
    }
};

class PerformInstallGuestToolsAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformInstallGuestToolsAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/guesttools_16px.png", ":/guesttools_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Install Guest Additions..."), gMS->shortcut(UIMachineShortcuts::InstallGuestAdditionsShortcut)));
        setStatusTip(QApplication::translate("UIActionsPool", "Mount the Guest Additions installation image"));
    }
};

#ifdef VBOX_WITH_DEBUGGER_GUI
class MenuDebugAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuDebugAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionsPool", "De&bug"));
    }
};

class ShowStatisticsAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowStatisticsAction(QObject *pParent)
        : UISimpleAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Statistics...", "debug action"), gMS->shortcut(UIMachineShortcuts::StatisticWindowShortcut)));
    }
};

class ShowCommandLineAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowCommandLineAction(QObject *pParent)
        : UISimpleAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "&Command Line...", "debug action"), gMS->shortcut(UIMachineShortcuts::CommandLineWindowShortcut)));
    }
};

class ToggleLoggingAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleLoggingAction(QObject *pParent)
        : UIToggleAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIActionsPool", "Enable &Logging...", "debug action"), gMS->shortcut(UIMachineShortcuts::LoggingShortcut)));
    }
};
#endif

class MenuHelpAction : public UIMenuAction
{
    Q_OBJECT;

public:

    MenuHelpAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionsPool", "&Help"));
    }
};

class ShowHelpAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowHelpAction(QObject *pParent)
        : UISimpleAction(pParent,
                         UIIconPool::defaultIcon(UIIconPool::DialogHelpIcon))
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setShortcut(gMS->shortcut(UIMachineShortcuts::HelpShortcut));
        setText(QApplication::translate("UIMessageCenter", "&Contents..."));
        setStatusTip(QApplication::translate("UIMessageCenter", "Show the online help contents"));
    }
};

class ShowWebAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowWebAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/site_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIMessageCenter", "&VirtualBox Web Site..."), gMS->shortcut(UIMachineShortcuts::WebShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Open the browser and go to the VirtualBox product web site"));
    }
};

class PerformResetWarningsAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformResetWarningsAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/reset_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIMessageCenter", "&Reset All Warnings"), gMS->shortcut(UIMachineShortcuts::ResetWarningsShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Go back to showing all suppressed warnings and messages"));
    }
};

#ifdef VBOX_WITH_REGISTRATION
class PerformRegisterAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformRegisterAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/register_16px.png", ":/register_disabled_16px.png")
    {
        setEnabled(vboxGlobal().virtualBox().
                   GetExtraData(VBoxDefs::GUI_RegistrationDlgWinID).isEmpty());
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIMessageCenter", "R&egister VirtualBox..."), gMS->shortcut(UIMachineShortcuts::RegisterShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Open VirtualBox registration form"));
    }
};
#endif /* VBOX_WITH_REGISTRATION */

class PerformUpdateAction : public UISimpleAction
{
    Q_OBJECT;

public:

    PerformUpdateAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/refresh_16px.png", ":/refresh_disabled_16px.png")
    {
        setMenuRole(QAction::ApplicationSpecificRole);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIMessageCenter", "C&heck for Updates..."), gMS->shortcut(UIMachineShortcuts::UpdateShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Check for a new VirtualBox version"));
    }
};

class ShowAboutAction : public UISimpleAction
{
    Q_OBJECT;

public:

    ShowAboutAction(QObject *pParent)
        : UISimpleAction(pParent,
                         ":/about_16px.png")
    {
        setMenuRole(QAction::AboutRole);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(vboxGlobal().insertKeyToActionText(QApplication::translate("UIMessageCenter", "&About VirtualBox..."), gMS->shortcut(UIMachineShortcuts::AboutShortcut)));
        setStatusTip(QApplication::translate("UIMessageCenter", "Show a dialog with product information"));
    }
};

#ifdef Q_WS_MAC
class DockMenuAction : public UIMenuAction
{
    Q_OBJECT;

public:

    DockMenuAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi() {}
};

class DockSettingsMenuAction : public UIMenuAction
{
    Q_OBJECT;

public:

    DockSettingsMenuAction(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionsPool", "Dock Icon"));
    }
};

class ToggleDockPreviewMonitorAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleDockPreviewMonitorAction(QObject *pParent)
        : UIToggleAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionsPool", "Show Monitor Preview"));
    }
};

class ToggleDockDisableMonitorAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleDockDisableMonitorAction(QObject *pParent)
        : UIToggleAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionsPool", "Show Application Icon"));
    }
};
#endif /* Q_WS_MAC */

UIActionsPool::UIActionsPool(QObject *pParent)
    : QObject(pParent)
    , m_actionsPool(UIActionIndex_End, 0)
{
    /* "Machine" menu actions: */
    m_actionsPool[UIActionIndex_Simple_SettingsDialog] = new ShowSettingsDialogAction(this);
    m_actionsPool[UIActionIndex_Simple_TakeSnapshot] = new PerformTakeSnapshotAction(this);
    m_actionsPool[UIActionIndex_Simple_InformationDialog] = new ShowInformationDialogAction(this);
    m_actionsPool[UIActionIndex_Toggle_MouseIntegration] = new ToggleMouseIntegrationAction(this);
    m_actionsPool[UIActionIndex_Simple_TypeCAD] = new PerformTypeCADAction(this);
#ifdef Q_WS_X11
    m_actionsPool[UIActionIndex_Simple_TypeCABS] = new PerformTypeCABSAction(this);
#endif
    m_actionsPool[UIActionIndex_Toggle_Pause] = new TogglePauseAction(this);
    m_actionsPool[UIActionIndex_Simple_Reset] = new PerformResetAction(this);
    m_actionsPool[UIActionIndex_Simple_Shutdown] = new PerformShutdownAction(this);
    m_actionsPool[UIActionIndex_Simple_Close] = new PerformCloseAction(this);

    /* "View" menu actions: */
    m_actionsPool[UIActionIndex_Toggle_Fullscreen] = new ToggleFullscreenModeAction(this);
    m_actionsPool[UIActionIndex_Toggle_Seamless] = new ToggleSeamlessModeAction(this);
    m_actionsPool[UIActionIndex_Toggle_Scale] = new ToggleScaleModeAction(this);
    m_actionsPool[UIActionIndex_Toggle_GuestAutoresize] = new ToggleGuestAutoresizeAction(this);
    m_actionsPool[UIActionIndex_Simple_AdjustWindow] = new PerformWindowAdjustAction(this);

    /* "Devices" menu actions: */
    m_actionsPool[UIActionIndex_Simple_NetworkAdaptersDialog] = new ShowNetworkAdaptersDialogAction(this);
    m_actionsPool[UIActionIndex_Simple_SharedFoldersDialog] = new ShowSharedFoldersDialogAction(this);
    m_actionsPool[UIActionIndex_Toggle_VRDEServer] = new ToggleVRDEServerAction(this);
    m_actionsPool[UIActionIndex_Simple_InstallGuestTools] = new PerformInstallGuestToolsAction(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* "Debugger" menu actions: */
    m_actionsPool[UIActionIndex_Simple_Statistics] = new ShowStatisticsAction(this);
    m_actionsPool[UIActionIndex_Simple_CommandLine] = new ShowCommandLineAction(this);
    m_actionsPool[UIActionIndex_Toggle_Logging] = new ToggleLoggingAction(this);
#endif

    /* "Help" menu actions: */
    m_actionsPool[UIActionIndex_Simple_Help] = new ShowHelpAction(this);
    m_actionsPool[UIActionIndex_Simple_Web] = new ShowWebAction(this);
    m_actionsPool[UIActionIndex_Simple_ResetWarnings] = new PerformResetWarningsAction(this);
#ifdef VBOX_WITH_REGISTRATION
    m_actionsPool[UIActionIndex_Simple_Register] = new PerformRegisterAction(this);
#endif /* VBOX_WITH_REGISTRATION */
    m_actionsPool[UIActionIndex_Simple_Update] = new PerformUpdateAction(this);
    m_actionsPool[UIActionIndex_Simple_About] = new ShowAboutAction(this);

#ifdef Q_WS_MAC
    /* "Dock" menu actions: */
    m_actionsPool[UIActionIndex_Toggle_DockPreviewMonitor] = new ToggleDockPreviewMonitorAction(this);
    m_actionsPool[UIActionIndex_Toggle_DockDisableMonitor] = new ToggleDockDisableMonitorAction(this);
#endif /* Q_WS_MAC */

    /* Create all menus */
    createMenus();

    /* Test all actions were initialized */
    for (int i = 0; i < m_actionsPool.size(); ++i)
        if (!m_actionsPool.at(i))
            AssertMsgFailed(("Action #%d is not created!\n", i));
}

UIActionsPool::~UIActionsPool()
{
    for (int i = 0; i < m_actionsPool.size(); ++i)
        delete m_actionsPool.at(i);
    m_actionsPool.clear();
}

UIAction* UIActionsPool::action(UIActionIndex index) const
{
    return m_actionsPool.at(index);
}

void UIActionsPool::createMenus()
{
    /* On Mac OS X, all QMenu's are consumed by Qt after they are added to
     * another QMenu or a QMenuBar. This means we have to recreate all QMenus
     * when creating a new QMenuBar. For simplicity we doing this on all
     * platforms right now. */
    if (m_actionsPool[UIActionIndex_Simple_Help])
        delete m_actionsPool[UIActionIndex_Simple_Help];
    m_actionsPool[UIActionIndex_Simple_Help] = new ShowHelpAction(this);
    if (m_actionsPool[UIActionIndex_Simple_Web])
        delete m_actionsPool[UIActionIndex_Simple_Web];
    m_actionsPool[UIActionIndex_Simple_Web] = new ShowWebAction(this);
    if (m_actionsPool[UIActionIndex_Simple_ResetWarnings])
        delete m_actionsPool[UIActionIndex_Simple_ResetWarnings];
    m_actionsPool[UIActionIndex_Simple_ResetWarnings] = new PerformResetWarningsAction(this);
#ifdef VBOX_WITH_REGISTRATION
    if (m_actionsPool[UIActionIndex_Simple_Register])
        delete m_actionsPool[UIActionIndex_Simple_Register]
    m_actionsPool[UIActionIndex_Simple_Register] = new PerformRegisterAction(this);
#endif /* VBOX_WITH_REGISTRATION */
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
    /* For whatever reason, Qt doesn't fully remove items with a
     * ApplicationSpecificRole from the application menu. Although the QAction
     * itself is deleted, a dummy entry is leaved back in the menu. Hiding
     * before deletion helps. */
    m_actionsPool[UIActionIndex_Simple_Update]->setVisible(false);
#endif /* Q_WS_MAC */
    /* Delete the help items as well. This makes sure they are removed also
     * from the Application menu. */
#if !(defined(Q_WS_MAC) && (QT_VERSION < 0x040700))
    if (m_actionsPool[UIActionIndex_Simple_About])
        delete m_actionsPool[UIActionIndex_Simple_About];
    m_actionsPool[UIActionIndex_Simple_About] = new ShowAboutAction(this);
    if (m_actionsPool[UIActionIndex_Simple_Update])
        delete m_actionsPool[UIActionIndex_Simple_Update];
    m_actionsPool[UIActionIndex_Simple_Update] = new PerformUpdateAction(this);
#endif
    if (m_actionsPool[UIActionIndex_Simple_Close])
        delete m_actionsPool[UIActionIndex_Simple_Close];
    m_actionsPool[UIActionIndex_Simple_Close] = new PerformCloseAction(this);

    /* Menus */
    if (m_actionsPool[UIActionIndex_Menu_Machine])
        delete m_actionsPool[UIActionIndex_Menu_Machine];
    m_actionsPool[UIActionIndex_Menu_Machine] = new MenuMachineAction(this);
    if (m_actionsPool[UIActionIndex_Menu_MouseIntegration])
        delete m_actionsPool[UIActionIndex_Menu_MouseIntegration];
    m_actionsPool[UIActionIndex_Menu_MouseIntegration] = new MenuMouseIntegrationAction(this);

    if (m_actionsPool[UIActionIndex_Menu_View])
        delete m_actionsPool[UIActionIndex_Menu_View];
    m_actionsPool[UIActionIndex_Menu_View] = new MenuViewAction(this);

    if (m_actionsPool[UIActionIndex_Menu_Devices])
        delete m_actionsPool[UIActionIndex_Menu_Devices];
    m_actionsPool[UIActionIndex_Menu_Devices] = new MenuDevicesAction(this);
    if (m_actionsPool[UIActionIndex_Menu_OpticalDevices])
        delete m_actionsPool[UIActionIndex_Menu_OpticalDevices];
    m_actionsPool[UIActionIndex_Menu_OpticalDevices] = new MenuOpticalDevicesAction(this);
    if (m_actionsPool[UIActionIndex_Menu_FloppyDevices])
        delete m_actionsPool[UIActionIndex_Menu_FloppyDevices];
    m_actionsPool[UIActionIndex_Menu_FloppyDevices] = new MenuFloppyDevicesAction(this);
    if (m_actionsPool[UIActionIndex_Menu_USBDevices])
        delete m_actionsPool[UIActionIndex_Menu_USBDevices];
    m_actionsPool[UIActionIndex_Menu_USBDevices] = new MenuUSBDevicesAction(this);
    if (m_actionsPool[UIActionIndex_Menu_NetworkAdapters])
        delete m_actionsPool[UIActionIndex_Menu_NetworkAdapters];
    m_actionsPool[UIActionIndex_Menu_NetworkAdapters] = new MenuNetworkAdaptersAction(this);
    if (m_actionsPool[UIActionIndex_Menu_SharedFolders])
        delete m_actionsPool[UIActionIndex_Menu_SharedFolders];
    m_actionsPool[UIActionIndex_Menu_SharedFolders] = new MenuSharedFoldersAction(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (m_actionsPool[UIActionIndex_Menu_Debug])
        delete m_actionsPool[UIActionIndex_Menu_Debug];
    m_actionsPool[UIActionIndex_Menu_Debug] = new MenuDebugAction(this);
#endif /* VBOX_WITH_DEBUGGER_GUI */

    if (m_actionsPool[UIActionIndex_Menu_Help])
        delete m_actionsPool[UIActionIndex_Menu_Help];
    m_actionsPool[UIActionIndex_Menu_Help] = new MenuHelpAction(this);

#ifdef Q_WS_MAC
    if (m_actionsPool[UIActionIndex_Menu_Dock])
        delete m_actionsPool[UIActionIndex_Menu_Dock];
    m_actionsPool[UIActionIndex_Menu_Dock] = new DockMenuAction(this);
    if (m_actionsPool[UIActionIndex_Menu_DockSettings])
        delete m_actionsPool[UIActionIndex_Menu_DockSettings];
    m_actionsPool[UIActionIndex_Menu_DockSettings] = new DockSettingsMenuAction(this);
#endif /* Q_WS_MAC */
}

bool UIActionsPool::processHotKey(const QKeySequence &key)
{
    for (int i = 0; i < m_actionsPool.size(); ++i)
    {
        UIAction *pAction = m_actionsPool.at(i);
        /* Skip menus/separators */
        if (pAction->type() == UIActionType_Menu)
            continue;
        QString hotkey = VBoxGlobal::extractKeyFromActionText(pAction->text());
        if (pAction->isEnabled() && pAction->isVisible() && !hotkey.isEmpty())
        {
            if (key.matches(QKeySequence(hotkey)) == QKeySequence::ExactMatch)
            {
                /* We asynchronously post a special event instead of calling
                 * pAction->trigger() directly, to let key presses and
                 * releases be processed correctly by Qt first.
                 * Note: we assume that nobody will delete the menu item
                 * corresponding to the key sequence, so that the pointer to
                 * menu data posted along with the event will remain valid in
                 * the event handler, at least until the main window is closed. */
                QApplication::postEvent(this, new ActivateActionEvent(pAction));
                return true;
            }
        }
    }

    return false;
}

bool UIActionsPool::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::ActivateActionEventType:
        {
            ActivateActionEvent *pActionEvent = static_cast<ActivateActionEvent*>(pEvent);
            pActionEvent->action()->trigger();

            // TODO_NEW_CORE
            /* The main window and its children can be destroyed at this point (if, for example, the activated menu
             * item closes the main window). Detect this situation to prevent calls to destroyed widgets: */
//            QWidgetList list = QApplication::topLevelWidgets();
//            bool destroyed = list.indexOf(machineWindowWrapper()->machineWindow()) < 0;
//            if (!destroyed && machineWindowWrapper()->machineWindow()->statusBar())
//                machineWindowWrapper()->machineWindow()->statusBar()->clearMessage();

            pEvent->accept();
            return true;
        }
        default:
            break;
    }

    return QObject::event(pEvent);
}

#include "UIActionsPool.moc"

