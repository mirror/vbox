/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolSelector class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include "UIActionPoolSelector.h"
#include "UISelectorShortcuts.h"

class UIActionMenuFile : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuFile(QObject *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#ifdef Q_WS_MAC
        menu()->setTitle(QApplication::translate("UIActionPool", "&File", "Mac OS X version"));
#else /* Q_WS_MAC */
        menu()->setTitle(QApplication::translate("UIActionPool", "&File", "Non Mac OS X version"));
#endif /* !Q_WS_MAC */
    }
};

class UIActionSimpleMediumManagerDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMediumManagerDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/diskimage_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Virtual Media Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Virtual Media Manager dialog"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::VirtualMediaManagerShortcut));
    }
};

class UIActionSimpleImportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleImportApplianceWizard(QObject *pParent)
        : UIActionSimple(pParent, ":/import_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Import Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Import an appliance into VirtualBox"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ImportApplianceShortcut));
    }
};

class UIActionSimpleExportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleExportApplianceWizard(QObject *pParent)
        : UIActionSimple(pParent, ":/export_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Export Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Export one or more VirtualBox virtual machines as an appliance"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ExportApplianceShortcut));
    }
};

class UIActionSimplePreferencesDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePreferencesDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/global_settings_16px.png")
    {
        setMenuRole(QAction::PreferencesRole);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Preferences...", "global settings"));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global settings dialog"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::PreferencesShortcut));
    }
};

class UIActionSimpleExit : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleExit(QObject *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "E&xit"));
        setStatusTip(QApplication::translate("UIActionPool", "Close application"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ExitShortcut));
    }
};


class UIActionMenuGroup : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuGroup(QObject *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionPool", "&Group"));
    }
};

class UIActionSimpleGroupNewWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupNewWizard(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16), ":/vm_new_32px.png", ":/new_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&New machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create a new virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::NewVMShortcut));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleGroupAddDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupAddDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Add machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add an existing virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMShortcut));
    }
};

class UIActionSimpleGroupRenameDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupRenameDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/name_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Rena&me group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Rename the selected virtual machine group"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RenameVMGroupShortcut));
    }
};

class UIActionSimpleGroupRemoveDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupRemoveDialog(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_delete_32px.png", ":/delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Break group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Break the selected virtual machine group"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::BreakVMGroupShortcut));
    }
};

class UIActionStateGroupStartOrShow : public UIActionState
{
    Q_OBJECT;

public:

    UIActionStateGroupStartOrShow(QObject *pParent)
        : UIActionState(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_start_32px.png", ":/start_16px.png",
                         ":/vm_start_disabled_32px.png", ":/start_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        switch (m_iState)
        {
            case 1:
            {
                setText(QApplication::translate("UIActionPool", "S&tart"));
                setStatusTip(QApplication::translate("UIActionPool", "Start the selected virtual machine"));
                setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            case 2:
            {
                setText(QApplication::translate("UIActionPool", "S&how"));
                setStatusTip(QApplication::translate("UIActionPool", "Switch to the window of the selected virtual machine"));
                setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            default:
                break;
        }
    }
};

class UIActionToggleGroupPauseAndResume : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleGroupPauseAndResume(QObject *pParent)
        : UIActionToggle(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_pause_32px.png", ":/pause_16px.png",
                         ":/vm_pause_disabled_32px.png", ":/pause_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Pause"));
        setStatusTip(QApplication::translate("UIActionPool", "Suspend the execution of the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::PauseVMShortcut));
    }
};

class UIActionSimpleGroupReset : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupReset(QObject *pParent)
        : UIActionSimple(pParent, ":/reset_16px.png", ":/reset_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Reset"));
        setStatusTip(QApplication::translate("UIActionPool", "Reset the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ResetVMShortcut));
    }
};

class UIActionSimpleGroupRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupRefresh(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/refresh_32px.png", ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Re&fresh..."));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the accessibility state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RefreshVMShortcut));
    }
};

class UIActionSimpleGroupShowInFileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupShowInFileManager(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Show in Finder"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Finder."));
#elif defined(Q_WS_WIN)
        setText(QApplication::translate("UIActionPool", "Show in Explorer"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Explorer."));
#else
        setText(QApplication::translate("UIActionPool", "Show in File Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in the File Manager"));
#endif
        setShortcut(gSS->keySequence(UISelectorShortcuts::ShowVMInFileManagerShortcut));
    }
};

class UIActionSimpleGroupCreateShortcut : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupCreateShortcut(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Create alias on desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an alias file to the VirtualBox Machine Definition file on your desktop."));
#else
        setText(QApplication::translate("UIActionPool", "Create shortcut on desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an shortcut file to the VirtualBox Machine Definition file on your desktop."));
#endif
        setShortcut(gSS->keySequence(UISelectorShortcuts::CreateVMAliasShortcut));
    }
};

class UIActionSimpleGroupSort : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupSort(QObject *pParent)
        : UIActionSimple(pParent/*, ":/settings_16px.png", ":/settings_dis_16px.png"*/)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Sort group"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort the items of the selected group alphabetically"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::SortGroup));
    }
};


class UIActionMenuMachine : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMachine(QObject *pParent)
        : UIActionMenu(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionPool", "&Machine"));
    }
};

class UIActionSimpleMachineNewWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineNewWizard(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16), ":/vm_new_32px.png", ":/new_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&New..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create a new virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::NewVMShortcut));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleMachineAddDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineAddDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Add..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add an existing virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMShortcut));
    }
};

class UIActionSimpleMachineAddGroupDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineAddGroupDialog(QObject *pParent)
        : UIActionSimple(pParent, ":/add_shared_folder_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Add group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add a new group based on the items selected"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMGroupShortcut));
    }
};

class UIActionSimpleMachineSettingsDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineSettingsDialog(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_settings_32px.png", ":/settings_16px.png",
                         ":/vm_settings_disabled_32px.png", ":/settings_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Manage the virtual machine settings"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::SettingsVMShortcut));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleMachineCloneWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineCloneWizard(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_clone_16px.png", ":/vm_clone_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Cl&one..."));
        setStatusTip(QApplication::translate("UIActionPool", "Clone the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::CloneVMShortcut));
    }
};

class UIActionSimpleMachineRemoveDialog : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineRemoveDialog(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_delete_32px.png", ":/delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Remove..."));
        setStatusTip(QApplication::translate("UIActionPool", "Remove the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RemoveVMShortcut));
    }
};

class UIActionStateMachineStartOrShow : public UIActionState
{
    Q_OBJECT;

public:

    UIActionStateMachineStartOrShow(QObject *pParent)
        : UIActionState(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_start_32px.png", ":/start_16px.png",
                         ":/vm_start_disabled_32px.png", ":/start_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        switch (m_iState)
        {
            case 1:
            {
                setText(QApplication::translate("UIActionPool", "S&tart"));
                setStatusTip(QApplication::translate("UIActionPool", "Start the selected virtual machine"));
                setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            case 2:
            {
                setText(QApplication::translate("UIActionPool", "S&how"));
                setStatusTip(QApplication::translate("UIActionPool", "Switch to the window of the selected virtual machine"));
                setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            default:
                break;
        }
    }
};

class UIActionSimpleMachineDiscard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineDiscard(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_discard_32px.png", ":/discard_16px.png",
                         ":/vm_discard_disabled_32px.png", ":/discard_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setIconText(QApplication::translate("UIActionPool", "Discard"));
        setText(QApplication::translate("UIActionPool", "D&iscard saved state..."));
        setStatusTip(QApplication::translate("UIActionPool", "Discard the saved state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::DiscardVMShortcut));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionToggleMachinePauseAndResume : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleMachinePauseAndResume(QObject *pParent)
        : UIActionToggle(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_pause_32px.png", ":/pause_16px.png",
                         ":/vm_pause_disabled_32px.png", ":/pause_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Pause"));
        setStatusTip(QApplication::translate("UIActionPool", "Suspend the execution of the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::PauseVMShortcut));
    }
};

class UIActionSimpleMachineReset : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineReset(QObject *pParent)
        : UIActionSimple(pParent, ":/reset_16px.png", ":/reset_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Reset"));
        setStatusTip(QApplication::translate("UIActionPool", "Reset the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ResetVMShortcut));
    }
};

class UIActionSimpleMachineRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineRefresh(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/refresh_32px.png", ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Re&fresh..."));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the accessibility state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RefreshVMShortcut));
    }
};

class UIActionSimpleMachineShowInFileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineShowInFileManager(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Show in Finder"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Finder."));
#elif defined(Q_WS_WIN)
        setText(QApplication::translate("UIActionPool", "Show in Explorer"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Explorer."));
#else
        setText(QApplication::translate("UIActionPool", "Show in File Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in the File Manager"));
#endif
        setShortcut(gSS->keySequence(UISelectorShortcuts::ShowVMInFileManagerShortcut));
    }
};

class UIActionSimpleMachineCreateShortcut : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineCreateShortcut(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Create Alias on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an Alias file to the VirtualBox Machine Definition file on your Desktop."));
#else
        setText(QApplication::translate("UIActionPool", "Create Shortcut on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an Shortcut file to the VirtualBox Machine Definition file on your Desktop."));
#endif
        setShortcut(gSS->keySequence(UISelectorShortcuts::CreateVMAliasShortcut));
    }
};

class UIActionSimpleCommonSortParent : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonSortParent(QObject *pParent)
        : UIActionSimple(pParent/*, ":/settings_16px.png", ":/settings_dis_16px.png"*/)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Sort parent group"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort the parent group of the first selected item alphabetically"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::SortParentGroup));
    }
};


class UIActionMenuMachineClose : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuMachineClose(QObject *pParent)
        : UIActionMenu(pParent, ":/exit_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionPool", "&Close"));
        menu()->setStatusTip(QApplication::translate("UIActionPool", "Close the virtual machine"));
    }
};

class UIActionSimpleSave : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleSave(QObject *pParent)
        : UIActionSimple(pParent, ":/state_saved_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Save state"));
        setStatusTip(QApplication::translate("UIActionPool", "Save the machine state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::SaveVMShortcut));
    }
};

class UIActionSimpleACPIShutdown : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleACPIShutdown(QObject *pParent)
        : UIActionSimple(pParent, ":/acpi_16px.png", ":/acpi_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "ACPI Sh&utdown"));
        setStatusTip(QApplication::translate("UIActionPool", "Send the ACPI Power Button press event to the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ACPIShutdownVMShortcut));
    }
};

class UIActionSimplePowerOff : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePowerOff(QObject *pParent)
        : UIActionSimple(pParent, ":/poweroff_16px.png", ":/poweroff_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Po&wer Off"));
        setStatusTip(QApplication::translate("UIActionPool", "Power off the virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::PowerOffVMShortcut));
    }
};


/* static */
void UIActionPoolSelector::create()
{
    /* Check that instance do NOT exists: */
    if (m_pInstance)
        return;

    /* Create instance: */
    UIActionPoolSelector *pPool = new UIActionPoolSelector;
    /* Prepare instance: */
    pPool->prepare();
}

/* static */
void UIActionPoolSelector::destroy()
{
    /* Check that instance exists: */
    if (!m_pInstance)
        return;

    /* Cleanup instance: */
    m_pInstance->cleanup();
    /* Delete instance: */
    delete m_pInstance;
}

void UIActionPoolSelector::createActions()
{
    /* Global actions creation: */
    UIActionPool::createActions();

    /* 'File' actions: */
    m_pool[UIActionIndexSelector_Simple_File_MediumManagerDialog] = new UIActionSimpleMediumManagerDialog(this);
    m_pool[UIActionIndexSelector_Simple_File_ImportApplianceWizard] = new UIActionSimpleImportApplianceWizard(this);
    m_pool[UIActionIndexSelector_Simple_File_ExportApplianceWizard] = new UIActionSimpleExportApplianceWizard(this);
    m_pool[UIActionIndexSelector_Simple_File_PreferencesDialog] = new UIActionSimplePreferencesDialog(this);
    m_pool[UIActionIndexSelector_Simple_File_Exit] = new UIActionSimpleExit(this);

    /* 'Group' actions: */
    m_pool[UIActionIndexSelector_Simple_Group_NewWizard] = new UIActionSimpleGroupNewWizard(this);
    m_pool[UIActionIndexSelector_Simple_Group_AddDialog] = new UIActionSimpleGroupAddDialog(this);
    m_pool[UIActionIndexSelector_Simple_Group_RenameDialog] = new UIActionSimpleGroupRenameDialog(this);
    m_pool[UIActionIndexSelector_Simple_Group_RemoveDialog] = new UIActionSimpleGroupRemoveDialog(this);
    m_pool[UIActionIndexSelector_State_Group_StartOrShow] = new UIActionStateGroupStartOrShow(this);
    m_pool[UIActionIndexSelector_Toggle_Group_PauseAndResume] = new UIActionToggleGroupPauseAndResume(this);
    m_pool[UIActionIndexSelector_Simple_Group_Reset] = new UIActionSimpleGroupReset(this);
    m_pool[UIActionIndexSelector_Simple_Group_Refresh] = new UIActionSimpleGroupRefresh(this);
    m_pool[UIActionIndexSelector_Simple_Group_ShowInFileManager] = new UIActionSimpleGroupShowInFileManager(this);
    m_pool[UIActionIndexSelector_Simple_Group_CreateShortcut] = new UIActionSimpleGroupCreateShortcut(this);
    m_pool[UIActionIndexSelector_Simple_Group_Sort] = new UIActionSimpleGroupSort(this);

    /* 'Machine' actions: */
    m_pool[UIActionIndexSelector_Simple_Machine_NewWizard] = new UIActionSimpleMachineNewWizard(this);
    m_pool[UIActionIndexSelector_Simple_Machine_AddDialog] = new UIActionSimpleMachineAddDialog(this);
    m_pool[UIActionIndexSelector_Simple_Machine_AddGroupDialog] = new UIActionSimpleMachineAddGroupDialog(this);
    m_pool[UIActionIndexSelector_Simple_Machine_SettingsDialog] = new UIActionSimpleMachineSettingsDialog(this);
    m_pool[UIActionIndexSelector_Simple_Machine_CloneWizard] = new UIActionSimpleMachineCloneWizard(this);
    m_pool[UIActionIndexSelector_Simple_Machine_RemoveDialog] = new UIActionSimpleMachineRemoveDialog(this);
    m_pool[UIActionIndexSelector_State_Machine_StartOrShow] = new UIActionStateMachineStartOrShow(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Discard] = new UIActionSimpleMachineDiscard(this);
    m_pool[UIActionIndexSelector_Toggle_Machine_PauseAndResume] = new UIActionToggleMachinePauseAndResume(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Reset] = new UIActionSimpleMachineReset(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Refresh] = new UIActionSimpleMachineRefresh(this);
    m_pool[UIActionIndexSelector_Simple_Machine_ShowInFileManager] = new UIActionSimpleMachineShowInFileManager(this);
    m_pool[UIActionIndexSelector_Simple_Machine_CreateShortcut] = new UIActionSimpleMachineCreateShortcut(this);

    /* Common actions: */
    m_pool[UIActionIndexSelector_Simple_Common_SortParent] = new UIActionSimpleCommonSortParent(this);

    /* 'Machine/Close' actions: */
    m_pool[UIActionIndexSelector_Simple_Machine_Close_Save] = new UIActionSimpleSave(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown] = new UIActionSimpleACPIShutdown(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_PowerOff] = new UIActionSimplePowerOff(this);
}

void UIActionPoolSelector::createMenus()
{
    /* Global menus creation: */
    UIActionPool::createMenus();

    /* 'File' menu: */
    m_pool[UIActionIndexSelector_Menu_File] = new UIActionMenuFile(this);

    /* 'Group' menu: */
    m_pool[UIActionIndexSelector_Menu_Group] = new UIActionMenuGroup(this);

    /* 'Machine' menu: */
    m_pool[UIActionIndexSelector_Menu_Machine] = new UIActionMenuMachine(this);

    /* 'Machine/Close' menu: */
    m_pool[UIActionIndexSelector_Menu_Machine_Close] = new UIActionMenuMachineClose(this);
}

#include "UIActionPoolSelector.moc"


