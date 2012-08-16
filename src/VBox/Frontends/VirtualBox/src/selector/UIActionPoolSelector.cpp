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
        setShortcut(gSS->keySequence(UISelectorShortcuts::VirtualMediaManagerShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Virtual Media Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Virtual Media Manager dialog"));
    }
};

class UIActionSimpleImportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleImportApplianceWizard(QObject *pParent)
        : UIActionSimple(pParent, ":/import_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::ImportApplianceShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Import Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Import an appliance into VirtualBox"));
    }
};

class UIActionSimpleExportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleExportApplianceWizard(QObject *pParent)
        : UIActionSimple(pParent, ":/export_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::ExportApplianceShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Export Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Export one or more VirtualBox virtual machines as an appliance"));
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
        setShortcut(gSS->keySequence(UISelectorShortcuts::PreferencesShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Preferences...", "global settings"));
        setStatusTip(QApplication::translate("UIActionPool", "Display the global settings dialog"));
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
        setShortcut(gSS->keySequence(UISelectorShortcuts::ExitShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "E&xit"));
        setStatusTip(QApplication::translate("UIActionPool", "Close application"));
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

class UIActionSimpleGroupNew : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupNew(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16), ":/vm_new_32px.png", ":/new_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::NewVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&New Machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create a new virtual machine"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleGroupAdd : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupAdd(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Add Machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add an existing virtual machine"));
    }
};

class UIActionSimpleGroupRename : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupRename(QObject *pParent)
        : UIActionSimple(pParent, ":/name_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::RenameVMGroupShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Rena&me Group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Rename the selected virtual machine group"));
    }
};

class UIActionSimpleGroupRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupRemove(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_delete_32px.png", ":/delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::BreakVMGroupShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Break Group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Break the selected virtual machine group"));
    }
};

class UIActionSimpleGroupSort : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleGroupSort(QObject *pParent)
        : UIActionSimple(pParent/*, ":/settings_16px.png", ":/settings_dis_16px.png"*/)
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::SortGroup));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Sort"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort the items of the selected virtual machine group alphabetically"));
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

class UIActionSimpleMachineNew : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineNew(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16), ":/vm_new_32px.png", ":/new_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::NewVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&New..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create a new virtual machine"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleMachineAdd : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineAdd(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Add..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add an existing virtual machine"));
    }
};

class UIActionSimpleMachineAddGroup : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineAddGroup(QObject *pParent)
        : UIActionSimple(pParent, ":/add_shared_folder_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMGroupShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Gro&up"));
        setStatusTip(QApplication::translate("UIActionPool", "Add a new group based on the items selected"));
    }
};

class UIActionSimpleMachineSettings : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineSettings(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_settings_32px.png", ":/settings_16px.png",
                         ":/vm_settings_disabled_32px.png", ":/settings_dis_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::SettingsVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Manage the virtual machine settings"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleMachineClone : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineClone(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_clone_16px.png", ":/vm_clone_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::CloneVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Cl&one..."));
        setStatusTip(QApplication::translate("UIActionPool", "Clone the selected virtual machine"));
    }
};

class UIActionSimpleMachineRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineRemove(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_delete_32px.png", ":/delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::RemoveVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Remove..."));
        setStatusTip(QApplication::translate("UIActionPool", "Remove the selected virtual machines"));
    }
};


class UIActionStateCommonStartOrShow : public UIActionState
{
    Q_OBJECT;

public:

    UIActionStateCommonStartOrShow(QObject *pParent)
        : UIActionState(pParent, QSize(32, 32), QSize(16, 16),
                        ":/vm_start_32px.png", ":/start_16px.png",
                        ":/vm_start_disabled_32px.png", ":/start_dis_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
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
                setStatusTip(QApplication::translate("UIActionPool", "Start the selected virtual machines"));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            case 2:
            {
                setText(QApplication::translate("UIActionPool", "S&how"));
                setStatusTip(QApplication::translate("UIActionPool", "Switch to the windows of the selected virtual machines"));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            default:
                break;
        }
    }
};

class UIActionToggleCommonPauseAndResume : public UIActionToggle
{
    Q_OBJECT;

public:

    UIActionToggleCommonPauseAndResume(QObject *pParent)
        : UIActionToggle(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_pause_32px.png", ":/pause_16px.png",
                         ":/vm_pause_disabled_32px.png", ":/pause_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::PauseVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Pause"));
        setStatusTip(QApplication::translate("UIActionPool", "Suspend the execution of the selected virtual machines"));
    }
};

class UIActionSimpleCommonReset : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonReset(QObject *pParent)
        : UIActionSimple(pParent, ":/reset_16px.png", ":/reset_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::ResetVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Reset"));
        setStatusTip(QApplication::translate("UIActionPool", "Reset the selected virtual machines"));
    }
};

class UIActionSimpleCommonDiscard : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonDiscard(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_discard_32px.png", ":/discard_16px.png",
                         ":/vm_discard_disabled_32px.png", ":/discard_dis_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::DiscardVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setIconText(QApplication::translate("UIActionPool", "Discard"));
        setText(QApplication::translate("UIActionPool", "D&iscard saved state..."));
        setStatusTip(QApplication::translate("UIActionPool", "Discard the saved state of the selected virtual machines"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIActionSimpleCommonRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonRefresh(QObject *pParent)
        : UIActionSimple(pParent, QSize(32, 32), QSize(16, 16),
                         ":/refresh_32px.png", ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::RefreshVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Re&fresh..."));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the accessibility state of the selected virtual machine"));
    }
};

class UIActionSimpleCommonShowInFileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonShowInFileManager(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::ShowVMInFileManagerShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Show in Finder"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Finder"));
#elif defined(Q_WS_WIN)
        setText(QApplication::translate("UIActionPool", "Show in Explorer"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in Explorer"));
#else
        setText(QApplication::translate("UIActionPool", "Show in File Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition file in the File Manager"));
#endif
    }
};

class UIActionSimpleCommonCreateShortcut : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleCommonCreateShortcut(QObject *pParent)
        : UIActionSimple(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::CreateVMAliasShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
#if defined(Q_WS_MAC)
        setText(QApplication::translate("UIActionPool", "Create Alias on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an alias file to the VirtualBox Machine Definition file on your desktop"));
#else
        setText(QApplication::translate("UIActionPool", "Create Shortcut on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Creates an shortcut file to the VirtualBox Machine Definition file on your desktop"));
#endif
    }
};

class UIActionSimpleMachineSortParent : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleMachineSortParent(QObject *pParent)
        : UIActionSimple(pParent/*, ":/settings_16px.png", ":/settings_dis_16px.png"*/)
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::SortParentGroup));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Sort"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort the group of the first selected machine alphabetically"));
    }
};


class UIActionMenuClose : public UIActionMenu
{
    Q_OBJECT;

public:

    UIActionMenuClose(QObject *pParent)
        : UIActionMenu(pParent, ":/exit_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionPool", "&Close"));
    }
};

class UIActionSimpleSave : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleSave(QObject *pParent)
        : UIActionSimple(pParent, ":/state_saved_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::SaveVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Save State"));
        setStatusTip(QApplication::translate("UIActionPool", "Save the machine state of the selected virtual machines"));
    }
};

class UIActionSimpleACPIShutdown : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimpleACPIShutdown(QObject *pParent)
        : UIActionSimple(pParent, ":/acpi_16px.png", ":/acpi_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::ACPIShutdownVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "ACPI Sh&utdown"));
        setStatusTip(QApplication::translate("UIActionPool", "Send the ACPI Power Button press event to the selected virtual machines"));
    }
};

class UIActionSimplePowerOff : public UIActionSimple
{
    Q_OBJECT;

public:

    UIActionSimplePowerOff(QObject *pParent)
        : UIActionSimple(pParent, ":/poweroff_16px.png", ":/poweroff_disabled_16px.png")
    {
        setShortcut(gSS->keySequence(UISelectorShortcuts::PowerOffVMShortcut));
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Po&wer Off"));
        setStatusTip(QApplication::translate("UIActionPool", "Power off the selected virtual machines"));
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
    m_pool[UIActionIndexSelector_Simple_Group_New] = new UIActionSimpleGroupNew(this);
    m_pool[UIActionIndexSelector_Simple_Group_Add] = new UIActionSimpleGroupAdd(this);
    m_pool[UIActionIndexSelector_Simple_Group_Rename] = new UIActionSimpleGroupRename(this);
    m_pool[UIActionIndexSelector_Simple_Group_Remove] = new UIActionSimpleGroupRemove(this);
    m_pool[UIActionIndexSelector_Simple_Group_Sort] = new UIActionSimpleGroupSort(this);
    m_pool[UIActionIndexSelector_Simple_Group_Close_Save] = new UIActionSimpleSave(this);
    m_pool[UIActionIndexSelector_Simple_Group_Close_ACPIShutdown] = new UIActionSimpleACPIShutdown(this);
    m_pool[UIActionIndexSelector_Simple_Group_Close_PowerOff] = new UIActionSimplePowerOff(this);

    /* 'Machine' actions: */
    m_pool[UIActionIndexSelector_Simple_Machine_New] = new UIActionSimpleMachineNew(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Add] = new UIActionSimpleMachineAdd(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Settings] = new UIActionSimpleMachineSettings(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Clone] = new UIActionSimpleMachineClone(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Remove] = new UIActionSimpleMachineRemove(this);
    m_pool[UIActionIndexSelector_Simple_Machine_AddGroup] = new UIActionSimpleMachineAddGroup(this);
    m_pool[UIActionIndexSelector_Simple_Machine_SortParent] = new UIActionSimpleMachineSortParent(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_Save] = new UIActionSimpleSave(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown] = new UIActionSimpleACPIShutdown(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_PowerOff] = new UIActionSimplePowerOff(this);

    /* Common actions: */
    m_pool[UIActionIndexSelector_State_Common_StartOrShow] = new UIActionStateCommonStartOrShow(this);
    m_pool[UIActionIndexSelector_Toggle_Common_PauseAndResume] = new UIActionToggleCommonPauseAndResume(this);
    m_pool[UIActionIndexSelector_Simple_Common_Reset] = new UIActionSimpleCommonReset(this);
    m_pool[UIActionIndexSelector_Simple_Common_Discard] = new UIActionSimpleCommonDiscard(this);
    m_pool[UIActionIndexSelector_Simple_Common_Refresh] = new UIActionSimpleCommonRefresh(this);
    m_pool[UIActionIndexSelector_Simple_Common_ShowInFileManager] = new UIActionSimpleCommonShowInFileManager(this);
    m_pool[UIActionIndexSelector_Simple_Common_CreateShortcut] = new UIActionSimpleCommonCreateShortcut(this);
}

void UIActionPoolSelector::createMenus()
{
    /* Global menus creation: */
    UIActionPool::createMenus();

    /* 'File' menu: */
    m_pool[UIActionIndexSelector_Menu_File] = new UIActionMenuFile(this);

    /* 'Group' menu: */
    m_pool[UIActionIndexSelector_Menu_Group] = new UIActionMenuGroup(this);
    m_pool[UIActionIndexSelector_Menu_Group_Close] = new UIActionMenuClose(this);

    /* 'Machine' menu: */
    m_pool[UIActionIndexSelector_Menu_Machine] = new UIActionMenuMachine(this);
    m_pool[UIActionIndexSelector_Menu_Machine_Close] = new UIActionMenuClose(this);
}

#include "UIActionPoolSelector.moc"


