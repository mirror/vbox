/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolSelector class implementation
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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

class UIMenuActionFile : public UIMenuAction
{
    Q_OBJECT;

public:

    UIMenuActionFile(QObject *pParent)
        : UIMenuAction(pParent)
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

class UISimpleActionMediumManagerDialog : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionMediumManagerDialog(QObject *pParent)
        : UISimpleAction(pParent, ":/diskimage_16px.png")
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

class UISimpleActionImportApplianceWizard : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionImportApplianceWizard(QObject *pParent)
        : UISimpleAction(pParent, ":/import_16px.png")
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

class UISimpleActionExportApplianceWizard : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionExportApplianceWizard(QObject *pParent)
        : UISimpleAction(pParent, ":/export_16px.png")
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

class UISimpleActionPreferencesDialog : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionPreferencesDialog(QObject *pParent)
        : UISimpleAction(pParent, ":/global_settings_16px.png")
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

class UISimpleActionExit : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionExit(QObject *pParent)
        : UISimpleAction(pParent, ":/exit_16px.png")
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

class UIMenuActionMachine : public UIMenuAction
{
    Q_OBJECT;

public:

    UIMenuActionMachine(QObject *pParent)
        : UIMenuAction(pParent)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        menu()->setTitle(QApplication::translate("UIActionPool", "&Machine"));
    }
};

class UISimpleActionNewWizard : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionNewWizard(QObject *pParent)
        : UISimpleAction(pParent, QSize(32, 32), QSize(16, 16), ":/vm_new_32px.png", ":/new_16px.png")
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

class UISimpleActionAddDialog : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionAddDialog(QObject *pParent)
        : UISimpleAction(pParent, ":/vm_add_16px.png")
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

class UISimpleActionSettingsDialog : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionSettingsDialog(QObject *pParent)
        : UISimpleAction(pParent, QSize(32, 32), QSize(16, 16),
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

class UISimpleActionCloneWizard : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionCloneWizard(QObject *pParent)
        : UISimpleAction(pParent, ":/vm_clone_16px.png", ":/vm_clone_disabled_16px.png")
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

class UISimpleActionRemoveDialog : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionRemoveDialog(QObject *pParent)
        : UISimpleAction(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_delete_32px.png", ":/delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "&Remove"));
        setStatusTip(QApplication::translate("UIActionPool", "Remove the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RemoveVMShortcut));
    }
};

class UIStateActionStartOrShow : public UIStateAction
{
    Q_OBJECT;

public:

    UIStateActionStartOrShow(QObject *pParent)
        : UIStateAction(pParent, QSize(32, 32), QSize(16, 16),
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

class UISimpleActionDiscard : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionDiscard(QObject *pParent)
        : UISimpleAction(pParent, QSize(32, 32), QSize(16, 16),
                         ":/vm_discard_32px.png", ":/discard_16px.png",
                         ":/vm_discard_disabled_32px.png", ":/discard_dis_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setIconText(QApplication::translate("UIActionPool", "Discard"));
        setText(QApplication::translate("UIActionPool", "D&iscard Saved State"));
        setStatusTip(QApplication::translate("UIActionPool", "Discard the saved state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::DiscardVMShortcut));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

class UIToggleActionPauseAndResume : public UIToggleAction
{
    Q_OBJECT;

public:

    UIToggleActionPauseAndResume(QObject *pParent)
        : UIToggleAction(pParent, QSize(32, 32), QSize(16, 16),
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

class UISimpleActionReset : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionReset(QObject *pParent)
        : UISimpleAction(pParent, ":/reset_16px.png", ":/reset_disabled_16px.png")
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

class UISimpleActionRefresh : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionRefresh(QObject *pParent)
        : UISimpleAction(pParent, QSize(32, 32), QSize(16, 16),
                         ":/refresh_32px.png", ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Re&fresh"));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the accessibility state of the selected virtual machine"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::RefreshVMShortcut));
    }
};

class UISimpleActionShowInFileManager : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionShowInFileManager(QObject *pParent)
        : UISimpleAction(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
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

class UISimpleActionCreateShortcut : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionCreateShortcut(QObject *pParent)
        : UISimpleAction(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
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

class UISimpleActionSort : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionSort(QObject *pParent)
        : UISimpleAction(pParent/*, ":/settings_16px.png", ":/settings_dis_16px.png"*/)
    {
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setText(QApplication::translate("UIActionPool", "Sort List"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort the VM list alphabetically (Shift for descending order)"));
        setShortcut(gSS->keySequence(UISelectorShortcuts::ResortVMList));
    }
};

class UIMenuActionMachineClose : public UIMenuAction
{
    Q_OBJECT;

public:

    UIMenuActionMachineClose(QObject *pParent)
        : UIMenuAction(pParent, ":/exit_16px.png")
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

class UISimpleActionACPIShutdown : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionACPIShutdown(QObject *pParent)
        : UISimpleAction(pParent, ":/acpi_16px.png", ":/acpi_disabled_16px.png")
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

class UISimpleActionPowerOff : public UISimpleAction
{
    Q_OBJECT;

public:

    UISimpleActionPowerOff(QObject *pParent)
        : UISimpleAction(pParent, ":/poweroff_16px.png", ":/poweroff_disabled_16px.png")
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
    m_pool[UIActionIndexSelector_Simple_File_MediumManagerDialog] = new UISimpleActionMediumManagerDialog(this);
    m_pool[UIActionIndexSelector_Simple_File_ImportApplianceWizard] = new UISimpleActionImportApplianceWizard(this);
    m_pool[UIActionIndexSelector_Simple_File_ExportApplianceWizard] = new UISimpleActionExportApplianceWizard(this);
    m_pool[UIActionIndexSelector_Simple_File_PreferencesDialog] = new UISimpleActionPreferencesDialog(this);
    m_pool[UIActionIndexSelector_Simple_File_Exit] = new UISimpleActionExit(this);

    /* 'Machine' actions: */
    m_pool[UIActionIndexSelector_Simple_Machine_NewWizard] = new UISimpleActionNewWizard(this);
    m_pool[UIActionIndexSelector_Simple_Machine_AddDialog] = new UISimpleActionAddDialog(this);
    m_pool[UIActionIndexSelector_Simple_Machine_SettingsDialog] = new UISimpleActionSettingsDialog(this);
    m_pool[UIActionIndexSelector_Simple_Machine_CloneWizard] = new UISimpleActionCloneWizard(this);
    m_pool[UIActionIndexSelector_Simple_Machine_RemoveDialog] = new UISimpleActionRemoveDialog(this);
    m_pool[UIActionIndexSelector_State_Machine_StartOrShow] = new UIStateActionStartOrShow(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Discard] = new UISimpleActionDiscard(this);
    m_pool[UIActionIndexSelector_Toggle_Machine_PauseAndResume] = new UIToggleActionPauseAndResume(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Reset] = new UISimpleActionReset(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Refresh] = new UISimpleActionRefresh(this);
    m_pool[UIActionIndexSelector_Simple_Machine_ShowInFileManager] = new UISimpleActionShowInFileManager(this);
    m_pool[UIActionIndexSelector_Simple_Machine_CreateShortcut] = new UISimpleActionCreateShortcut(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Sort] = new UISimpleActionSort(this);

    /* 'Machine/Close' actions: */
    m_pool[UIActionIndexSelector_Simple_Machine_Close_ACPIShutdown] = new UISimpleActionACPIShutdown(this);
    m_pool[UIActionIndexSelector_Simple_Machine_Close_PowerOff] = new UISimpleActionPowerOff(this);
}

void UIActionPoolSelector::createMenus()
{
    /* Global menus creation: */
    UIActionPool::createMenus();

    /* 'File' menu: */
    m_pool[UIActionIndexSelector_Menu_File] = new UIMenuActionFile(this);

    /* 'Machine' menu: */
    m_pool[UIActionIndexSelector_Menu_Machine] = new UIMenuActionMachine(this);

    /* 'Machine/Close' menu: */
    m_pool[UIActionIndexSelector_Menu_Machine_Close] = new UIMenuActionMachineClose(this);
}

#include "UIActionPoolSelector.moc"


