/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionsPool class implementation
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

/* Local includes */
#include "UIActionsPool.h"
#include "VBoxGlobal.h"

UIAction::UIAction(QObject *pParent, UIActionType type)
    : QIWithRetranslateUI3<QAction>(pParent)
    , m_type(type)
{
}

UIActionType UIAction::type() const
{
    return m_type;
}

class UISeparatorAction : public UIAction
{
    Q_OBJECT;

public:

    UISeparatorAction(QObject *pParent)
        : UIAction(pParent, UIActionType_Separator)
    {
        setSeparator(true);
    }
};

class UISimpleAction : public UIAction
{
    Q_OBJECT;

public:

    UISimpleAction(QObject *pParent,
                   const QString &strIcon = QString(), const QString &strIconDis = QString())
        : UIAction(pParent, UIActionType_Simple)
    {
        if (!strIcon.isNull())
            setIcon(VBoxGlobal::iconSet(strIcon.toLatin1().data(), strIconDis.toLatin1().data()));
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
            setIcon(VBoxGlobal::iconSet(strIcon.toLatin1().data(), strIconDis.toLatin1().data()));
        init();
    }

    UIToggleAction(QObject *pParent,
                   const QString &strIconOn, const QString &strIconOff,
                   const QString &strIconOnDis, const QString &strIconOffDis)
        : UIAction(pParent, UIActionType_Toggle)
    {
        setIcon(VBoxGlobal::iconSetOnOff(strIconOn.toLatin1().data(), strIconOff.toLatin1().data(),
                                         strIconOnDis.toLatin1().data(), strIconOffDis.toLatin1().data()));
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
        connect(this, SIGNAL(triggered(bool)), this, SLOT(sltUpdateAppearance()));
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
            setIcon(VBoxGlobal::iconSet(strIcon, strIconDis));
        setMenu(new QMenu);
    }
};

class SeparatorAction : public UISeparatorAction
{
    Q_OBJECT;

public:

    SeparatorAction(QObject *pParent)
        : UISeparatorAction(pParent)
    {
    }

protected:

    void retranslateUi()
    {
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
        menu()->setTitle(tr("&Machine"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Fullscreen Mode"), "F"));
        setStatusTip(tr("Switch to fullscreen mode"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("Seam&less Mode"), "L"));
        setStatusTip(tr("Switch to seamless desktop integration mode"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("Auto-resize &Guest Display"), "G"));
        setStatusTip(tr("Automatically resize the guest display when the window is resized (requires Guest Additions)"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Adjust Window Size"), "A"));
        setStatusTip(tr("Adjust window size and position to best fit the guest display"));
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
        if (!isChecked())
        {
            setText(VBoxGlobal::insertKeyToActionText(tr("Disable &Mouse Integration"), "I"));
            setStatusTip(tr("Temporarily disable host mouse pointer integration"));
        }
        else
        {
            setText(VBoxGlobal::insertKeyToActionText(tr("Enable &Mouse Integration"), "I"));
            setStatusTip(tr("Enable temporarily disabled host mouse pointer integration"));
        }
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Insert Ctrl-Alt-Del"), "Del"));
        setStatusTip(tr("Send the Ctrl-Alt-Del sequence to the virtual machine"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Insert Ctrl-Alt-Backspace"), "Backspace"));
        setStatusTip(tr("Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
    }
};
#endif

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
        setText(VBoxGlobal::insertKeyToActionText(tr("Take &Snapshot..."), "S"));
        setStatusTip(tr("Take a snapshot of the virtual machine"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("Session I&nformation Dialog"), "N"));
        setStatusTip(tr("Show Session Information Dialog"));
    }
};

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
        if (!isChecked())
        {
            setText(VBoxGlobal::insertKeyToActionText(tr ("&Pause"), "P"));
            setStatusTip(tr("Suspend the execution of the virtual machine"));
        }
        else
        {
            setText(VBoxGlobal::insertKeyToActionText(tr ("R&esume"), "P"));
            setStatusTip(tr("Resume the execution of the virtual machine"));
        }
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
        setText(VBoxGlobal::insertKeyToActionText(tr ("&Reset"), "R"));
        setStatusTip(tr("Reset the virtual machine"));
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
#ifdef Q_WS_MAC
        /* Host+H is Hide on the mac */
        setText(VBoxGlobal::insertKeyToActionText(tr ("ACPI Sh&utdown"), "U"));
#else /* Q_WS_MAC */
        setText(VBoxGlobal::insertKeyToActionText(tr ("ACPI S&hutdown"), "H"));
#endif /* !Q_WS_MAC */
        setStatusTip(tr("Send the ACPI Power Button press event to the virtual machine"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Close..." ), "Q"));
        setStatusTip(tr("Close the virtual machine"));
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
        menu()->setTitle(tr("&Devices"));
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
        menu()->setTitle(tr("&CD/DVD Devices"));
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
        menu()->setTitle(tr("&Floppy Devices"));
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
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(tr("&USB Devices"));
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
        setText(tr("&Network Adapters..."));
        setStatusTip(tr("Change the settings of network adapters"));
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
        setText(tr("&Shared Folders..."));
        setStatusTip(tr("Create or modify shared folders"));
    }
};

class ToggleVRDPAction : public UIToggleAction
{
    Q_OBJECT;

public:

    ToggleVRDPAction(QObject *pParent)
        : UIToggleAction(pParent,
                         ":/vrdp_on_16px.png", ":/vrdp_16px.png",
                         ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(tr("&Remote Display"));
        setStatusTip(tr("Enable or disable remote desktop (RDP) connections to this machine"));
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
        setText(VBoxGlobal::insertKeyToActionText(tr("&Install Guest Additions..."), "D"));
        setStatusTip(tr("Mount the Guest Additions installation image"));
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
        menu()->setTitle(tr("De&bug"));
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
        setText(tr("&Statistics...", "debug action"));
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
        setText(tr("&Command Line...", "debug action"));
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
        setText(tr("&Logging...", "debug action"));
    }
};
#endif

UIActionsPool::UIActionsPool(QObject *pParent)
    : QObject(pParent)
    , m_actionsPool(UIActionIndex_End, 0)
{
    /* Common actions: */
    m_actionsPool[UIActionIndex_Separator] = new SeparatorAction(this);

    /* "Machine" menu actions: */
    m_actionsPool[UIActionIndex_Menu_Machine] = new MenuMachineAction(this);
    m_actionsPool[UIActionIndex_Toggle_Fullscreen] = new ToggleFullscreenModeAction(this);
    m_actionsPool[UIActionIndex_Toggle_Seamless] = new ToggleSeamlessModeAction(this);
    m_actionsPool[UIActionIndex_Toggle_GuestAutoresize] = new ToggleGuestAutoresizeAction(this);
    m_actionsPool[UIActionIndex_Simple_AdjustWindow] = new PerformWindowAdjustAction(this);
    m_actionsPool[UIActionIndex_Menu_MouseIntegration] = new MenuMouseIntegrationAction(this);
    m_actionsPool[UIActionIndex_Toggle_MouseIntegration] = new ToggleMouseIntegrationAction(this);
    m_actionsPool[UIActionIndex_Simple_TypeCAD] = new PerformTypeCADAction(this);
#ifdef Q_WS_X11
    m_actionsPool[UIActionIndex_Simple_TypeCABS] = new PerformTypeCABSAction(this);
#endif
    m_actionsPool[UIActionIndex_Simple_TakeSnapshot] = new PerformTakeSnapshotAction(this);
    m_actionsPool[UIActionIndex_Simple_InformationDialog] = new ShowInformationDialogAction(this);
    m_actionsPool[UIActionIndex_Toggle_Pause] = new TogglePauseAction(this);
    m_actionsPool[UIActionIndex_Simple_Reset] = new PerformResetAction(this);
    m_actionsPool[UIActionIndex_Simple_Shutdown] = new PerformShutdownAction(this);
    m_actionsPool[UIActionIndex_Simple_Close] = new PerformCloseAction(this);

    /* "Devices" menu actions: */
    m_actionsPool[UIActionIndex_Menu_Devices] = new MenuDevicesAction(this);
    m_actionsPool[UIActionIndex_Menu_OpticalDevices] = new MenuOpticalDevicesAction(this);
    m_actionsPool[UIActionIndex_Menu_FloppyDevices] = new MenuFloppyDevicesAction(this);
    m_actionsPool[UIActionIndex_Menu_USBDevices] = new MenuUSBDevicesAction(this);
    m_actionsPool[UIActionIndex_Menu_NetworkAdapters] = new MenuNetworkAdaptersAction(this);
    m_actionsPool[UIActionIndex_Simple_NetworkAdaptersDialog] = new ShowNetworkAdaptersDialogAction(this);
    m_actionsPool[UIActionIndex_Menu_SharedFolders] = new MenuSharedFoldersAction(this);
    m_actionsPool[UIActionIndex_Simple_SharedFoldersDialog] = new ShowSharedFoldersDialogAction(this);
    m_actionsPool[UIActionIndex_Toggle_VRDP] = new ToggleVRDPAction(this);
    m_actionsPool[UIActionIndex_Simple_InstallGuestTools] = new PerformInstallGuestToolsAction(this);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* "Debugger" menu actions: */
    m_actionsPool[UIActionIndex_Menu_Debug] = new MenuDebugAction(this);
    m_actionsPool[UIActionIndex_Simple_Statistics] = new ShowStatisticsAction(this);
    m_actionsPool[UIActionIndex_Simple_CommandLine] = new ShowCommandLineAction(this);
    m_actionsPool[UIActionIndex_Toggle_Logging] = new ToggleLoggingAction(this);
#endif

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

#include "UIActionsPool.moc"

