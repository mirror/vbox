/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPoolSelector class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIActionPoolSelector.h"
# include "UIExtraDataDefs.h"
# include "UIIconPool.h"
# include "UIShortcutPool.h"
# include "UIDefs.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* TEMPORARY! */
#if defined(_MSC_VER) && !defined(RT_ARCH_AMD64)
# pragma optimize("g", off)
#endif

/* Namespaces: */
using namespace UIExtraDataDefs;


/** Menu action extension, used as 'File' menu class. */
class UIActionMenuSelectorFile : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorFile(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
#ifdef VBOX_WS_MAC
        setName(QApplication::translate("UIActionPool", "&File", "Mac OS X version"));
#else /* VBOX_WS_MAC */
        setName(QApplication::translate("UIActionPool", "&File", "Non Mac OS X version"));
#endif /* !VBOX_WS_MAC */
    }
};

/** Simple action extension, used as 'Show Virtual Media Manager' action class. */
class UIActionSimpleSelectorFileShowVirtualMediaManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowVirtualMediaManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/media_manager_16px.png", ":/media_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("VirtualMediaManager");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+D");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Virtual Media Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Virtual Media Manager window"));
    }
};

/** Simple action extension, used as 'Show Host Network Manager' action class. */
class UIActionSimpleSelectorFileShowHostNetworkManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowHostNetworkManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/host_iface_manager_16px.png", ":/host_iface_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("HostNetworkManager");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+W");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Host Network Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Host Network Manager window"));
    }
};

/** Simple action extension, used as 'Show Cloud Profile Manager' action class. */
class UIActionSimpleSelectorFileShowCloudProfileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowCloudProfileManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/cloud_profile_manager_16px.png", ":/cloud_profile_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CloudProfileManager");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+C");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Cloud Profile Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Cloud Profile Manager window"));
    }
};

/** Simple action extension, used as 'Show Import Appliance Wizard' action class. */
class UIActionSimpleSelectorFileShowImportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowImportApplianceWizard(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/import_16px.png", ":/import_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ImportAppliance");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+I");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Import"));
        setName(QApplication::translate("UIActionPool", "&Import Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Import an appliance into VirtualBox"));
    }
};

/** Simple action extension, used as 'Show Export Appliance Wizard' action class. */
class UIActionSimpleSelectorFileShowExportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowExportApplianceWizard(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/export_16px.png", ":/export_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ExportAppliance");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+E");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Export"));
        setName(QApplication::translate("UIActionPool", "&Export Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Export one or more VirtualBox virtual machines as an appliance"));
    }
};

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
/** Simple action extension, used as 'Show Extra-data Manager' action class. */
class UIActionSimpleSelectorFileShowExtraDataManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowExtraDataManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/edata_manager_16px.png", ":/edata_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ExtraDataManager");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+X");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "E&xtra Data Manager..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the Extra Data Manager window"));
    }
};
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

/** Simple action extension, used as 'Perform Exit' action class. */
class UIActionSimpleSelectorFilePerformExit : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFilePerformExit(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png", ":/exit_16px.png")
    {
        setMenuRole(QAction::QuitRole);
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("Exit");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Q");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "E&xit"));
        setStatusTip(QApplication::translate("UIActionPool", "Close application"));
    }
};


/** Menu action extension, used as 'Group' menu class. */
class UIActionMenuSelectorGroup : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorGroup(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Group"));
    }
};

/** Simple action extension, used as 'Perform Create Machine' action class. */
class UIActionSimpleSelectorGroupPerformCreateMachine : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorGroupPerformCreateMachine(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_new_32px.png", ":/vm_new_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("NewVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+N");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&New Machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create new virtual machine"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Perform Add Machine' action class. */
class UIActionSimpleSelectorGroupPerformAddMachine : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorGroupPerformAddMachine(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png", ":/vm_add_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+A");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Add Machine..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add existing virtual machine"));
    }
};

/** Simple action extension, used as 'Perform Rename Group' action class. */
class UIActionSimpleSelectorGroupPerformRename : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorGroupPerformRename(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_group_name_16px.png", ":/vm_group_name_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RenameVMGroup");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+M");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Rena&me Group..."));
        setStatusTip(QApplication::translate("UIActionPool", "Rename selected virtual machine group"));
    }
};

/** Simple action extension, used as 'Perform Remove Group' action class. */
class UIActionSimpleSelectorGroupPerformRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorGroupPerformRemove(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_group_remove_16px.png", ":/vm_group_remove_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddVMGroup");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+U");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Ungroup"));
        setStatusTip(QApplication::translate("UIActionPool", "Ungroup items of selected virtual machine group"));
    }
};

/** Simple action extension, used as 'Perform Sort Group' action class. */
class UIActionSimpleSelectorGroupPerformSort : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorGroupPerformSort(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/sort_16px.png", ":/sort_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SortGroup");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Sort"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort items of selected virtual machine group alphabetically"));
    }
};


/** Menu action extension, used as 'Machine' menu class. */
class UIActionMenuSelectorMachine : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMachine(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Machine"));
    }
};

/** Simple action extension, used as 'Perform Create Machine' action class. */
class UIActionSimpleSelectorMachinePerformCreate : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformCreate(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_new_32px.png", ":/vm_new_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("NewVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+N");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&New..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create new virtual machine"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Perform Add Machine' action class. */
class UIActionSimpleSelectorMachinePerformAdd : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformAdd(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_add_16px.png", ":/vm_add_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+A");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Add..."));
        setStatusTip(QApplication::translate("UIActionPool", "Add existing virtual machine"));
    }
};

/** Simple action extension, used as 'Perform Group Machines' action class. */
class UIActionSimpleSelectorMachinePerformGroup : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformGroup(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_group_create_16px.png", ":/vm_group_create_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddVMGroup");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+U");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Gro&up"));
        setStatusTip(QApplication::translate("UIActionPool", "Add new group based on selected virtual machines"));
    }
};

/** Simple action extension, used as 'Show Machine Settings' action class. */
class UIActionSimpleSelectorMachineShowSettings : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachineShowSettings(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_settings_32px.png", ":/vm_settings_16px.png",
                         ":/vm_settings_disabled_32px.png", ":/vm_settings_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SettingsVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+S");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Settings..."));
        setStatusTip(QApplication::translate("UIActionPool", "Display the virtual machine settings window"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Perform Clone Machine' action class. */
class UIActionSimpleSelectorMachinePerformClone : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformClone(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_clone_16px.png", ":/vm_clone_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CloneVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+O");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Cl&one..."));
        setStatusTip(QApplication::translate("UIActionPool", "Clone selected virtual machine"));
    }
};

/** Simple action extension, used as 'Perform Move Machine' action class. */
class UIActionSimpleSelectorMachinePerformMove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformMove(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_move_16px.png", ":/vm_move_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("MoveVM");
    }

#if 0 /* conflict with RenameVMGroup action to be resolved first */
    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+M");
    }
#endif /* conflict with RenameVMGroup action to be resolved first */

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Move..."));
        setStatusTip(QApplication::translate("UIActionPool", "Move selected virtual machine"));
    }
};

/** Simple action extension, used as 'Perform Remove Machine' action class. */
class UIActionSimpleSelectorMachinePerformRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformRemove(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_delete_32px.png", ":/vm_delete_16px.png",
                         ":/vm_delete_disabled_32px.png", ":/vm_delete_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RemoveVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Remove..."));
        setStatusTip(QApplication::translate("UIActionPool", "Remove selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Sort Parent' action class. */
class UIActionSimpleSelectorMachinePerformSortParent : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformSortParent(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/sort_16px.png", ":/sort_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SortGroup");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Sort"));
        setStatusTip(QApplication::translate("UIActionPool", "Sort group of first selected virtual machine alphabetically"));
    }
};


/** Menu action extension, used as 'Start or Show' menu class. */
class UIActionStateSelectorCommonStartOrShow : public UIActionPolymorphicMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionStateSelectorCommonStartOrShow(UIActionPool *pParent)
        : UIActionPolymorphicMenu(pParent,
                                  ":/vm_start_32px.png", ":/vm_start_16px.png",
                                  ":/vm_start_disabled_32px.png", ":/vm_start_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("StartVM");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        switch (state())
        {
            case 0:
            {
                showMenu();
                setName(QApplication::translate("UIActionPool", "S&tart"));
                setStatusTip(QApplication::translate("UIActionPool", "Start selected virtual machines"));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            case 1:
            {
                hideMenu();
                setName(QApplication::translate("UIActionPool", "S&how"));
                setStatusTip(QApplication::translate("UIActionPool", "Switch to the windows of selected virtual machines"));
                setToolTip(text().remove('&').remove('.') +
                           (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
                break;
            }
            default:
                break;
        }
    }
};

/** Simple action extension, used as 'Perform Normal Start' action class. */
class UIActionSimpleSelectorCommonPerformStartNormal : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformStartNormal(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_start_16px.png", ":/vm_start_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("StartVMNormal");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Normal Start"));
        setStatusTip(QApplication::translate("UIActionPool", "Start selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Headless Start' action class. */
class UIActionSimpleSelectorCommonPerformStartHeadless : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformStartHeadless(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_start_headless_16px.png", ":/vm_start_headless_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("StartVMHeadless");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Headless Start"));
        setStatusTip(QApplication::translate("UIActionPool", "Start selected virtual machines in the background"));
    }
};

/** Simple action extension, used as 'Perform Detachable Start' action class. */
class UIActionSimpleSelectorCommonPerformStartDetachable : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformStartDetachable(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_start_separate_16px.png", ":/vm_start_separate_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("StartVMDetachable");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Detachable Start"));
        setStatusTip(QApplication::translate("UIActionPool", "Start selected virtual machines with option of continuing in background"));
    }
};

/** Toggle action extension, used as 'Pause and Resume' action class. */
class UIActionToggleSelectorCommonPauseAndResume : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorCommonPauseAndResume(UIActionPool *pParent)
        : UIActionToggle(pParent,
                         ":/vm_pause_on_16px.png", ":/vm_pause_16px.png",
                         ":/vm_pause_on_disabled_16px.png", ":/vm_pause_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("PauseVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Pause"));
        setStatusTip(QApplication::translate("UIActionPool", "Suspend execution of selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Reset' action class. */
class UIActionSimpleSelectorCommonPerformReset : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformReset(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_reset_16px.png", ":/vm_reset_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ResetVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+T");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Reset"));
        setStatusTip(QApplication::translate("UIActionPool", "Reset selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Discard' action class. */
class UIActionSimpleSelectorCommonPerformDiscard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformDiscard(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_discard_32px.png", ":/vm_discard_16px.png",
                         ":/vm_discard_disabled_32px.png", ":/vm_discard_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("DiscardVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+J");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Discard"));
        setName(QApplication::translate("UIActionPool", "D&iscard Saved State..."));
        setStatusTip(QApplication::translate("UIActionPool", "Discard saved state of selected virtual machines"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Show Machine Logs' action class. */
class UIActionSimpleSelectorCommonShowMachineLogs : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonShowMachineLogs(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png",
                         ":/vm_show_logs_disabled_32px.png", ":/vm_show_logs_disabled_16px.png")
    {
        retranslateUi();
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("LogViewer");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+L");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Show &Log..."));
        setStatusTip(QApplication::translate("UIActionPool", "Show log files of selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Refresh' action class. */
class UIActionSimpleSelectorCommonPerformRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformRefresh(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/refresh_32px.png", ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RefreshVM");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Re&fresh"));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh accessibility state of selected virtual machines"));
    }
};

/** Simple action extension, used as 'Show in File Manager' action class. */
class UIActionSimpleSelectorCommonShowInFileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonShowInFileManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ShowVMInFileManager");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
#if defined(VBOX_WS_MAC)
        setName(QApplication::translate("UIActionPool", "S&how in Finder"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition files in Finder"));
#elif defined(VBOX_WS_WIN)
        setName(QApplication::translate("UIActionPool", "S&how in Explorer"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition files in Explorer"));
#else
        setName(QApplication::translate("UIActionPool", "S&how in File Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Show the VirtualBox Machine Definition files in the File Manager"));
#endif
    }
};

/** Simple action extension, used as 'Perform Create Shortcut' action class. */
class UIActionSimpleSelectorCommonPerformCreateShortcut : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorCommonPerformCreateShortcut(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CreateVMAlias");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
#if defined(VBOX_WS_MAC)
        setName(QApplication::translate("UIActionPool", "Cr&eate Alias on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Create alias files to the VirtualBox Machine Definition files on your desktop"));
#else
        setName(QApplication::translate("UIActionPool", "Cr&eate Shortcut on Desktop"));
        setStatusTip(QApplication::translate("UIActionPool", "Create shortcut files to the VirtualBox Machine Definition files on your desktop"));
#endif
    }
};


/** Menu action extension, used as 'Close' menu class. */
class UIActionMenuSelectorClose : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorClose(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/exit_16px.png")
    {}

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Close"));
    }
};

/** Simple action extension, used as 'Perform Detach' action class. */
class UIActionSimpleSelectorClosePerformDetach : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorClosePerformDetach(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("DetachUIVM");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Detach GUI"));
        setStatusTip(QApplication::translate("UIActionPool", "Detach the GUI from headless VM"));
    }
};

/** Simple action extension, used as 'Perform Save' action class. */
class UIActionSimpleSelectorClosePerformSave : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorClosePerformSave(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_save_state_16px.png", ":/vm_save_state_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SaveVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+V");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Save State"));
        setStatusTip(QApplication::translate("UIActionPool", "Save state of selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform Shutdown' action class. */
class UIActionSimpleSelectorClosePerformShutdown : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorClosePerformShutdown(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_shutdown_16px.png", ":/vm_shutdown_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ACPIShutdownVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+H");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "ACPI Sh&utdown"));
        setStatusTip(QApplication::translate("UIActionPool", "Send ACPI Shutdown signal to selected virtual machines"));
    }
};

/** Simple action extension, used as 'Perform PowerOff' action class. */
class UIActionSimpleSelectorClosePerformPowerOff : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorClosePerformPowerOff(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/vm_poweroff_16px.png", ":/vm_poweroff_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("PowerOffVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+F");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Po&wer Off"));
        setStatusTip(QApplication::translate("UIActionPool", "Power off selected virtual machines"));
    }
};


/** Toggle action extension, used as 'Machine Tools' action class. */
class UIActionToggleSelectorToolsMachine : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsMachine(UIActionPool *pParent)
        : UIActionToggle(pParent, ":/tools_machine_32px.png", ":/tools_machine_32px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsMachine");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Machine Tools"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch to machine tools"));
    }
};

/** Menu action extension, used as 'Machine Tools' menu class. */
class UIActionMenuSelectorToolsMachine : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorToolsMachine(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsMachineMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Machine Tools Menu"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the machine tools menu"));
    }
};

/** Simple action extension, used as 'Show Machine Details' action class. */
class UIActionSimpleSelectorToolsMachineShowDetails : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsMachineShowDetails(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/machine_details_manager_24px.png", ":/machine_details_manager_16px.png",
                         ":/machine_details_manager_24px.png", ":/machine_details_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsMachineDetails");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Details"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the machine details pane"));
    }
};

/** Simple action extension, used as 'Show Machine Snapshots' action class. */
class UIActionSimpleSelectorToolsMachineShowSnapshots : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsMachineShowSnapshots(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/snapshot_manager_24px.png", ":/snapshot_manager_16px.png",
                         ":/snapshot_manager_24px.png", ":/snapshot_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsMachineSnapshots");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Snapshots"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the machine snapshots pane"));
    }
};

/** Simple action extension, used as 'Show Machine Logs' action class. */
class UIActionSimpleSelectorToolsMachineShowLogs : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsMachineShowLogs(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_show_logs_32px.png", ":/vm_show_logs_32px.png",
                         ":/vm_show_logs_32px.png", ":/vm_show_logs_32px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsMachineLogViewer");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Logs"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the machine logs pane"));
    }
};


/** Toggle action extension, used as 'Global Tools' action class. */
class UIActionToggleSelectorToolsGlobal : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsGlobal(UIActionPool *pParent)
        : UIActionToggle(pParent, ":/tools_global_32px.png", ":/tools_global_32px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsGlobal");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Global Tools"));
        setStatusTip(QApplication::translate("UIActionPool", "Switch to global tools"));
    }
};

/** Menu action extension, used as 'Global Tools' menu class. */
class UIActionMenuSelectorToolsGlobal : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorToolsGlobal(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsGlobalMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Global Tools Menu"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the global tools menu"));
    }
};

/** Simple action extension, used as 'Show Virtual Media Manager' action class. */
class UIActionSimpleSelectorToolsGlobalShowVirtualMediaManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsGlobalShowVirtualMediaManager(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/media_manager_24px.png", ":/media_manager_16px.png",
                         ":/media_manager_24px.png", ":/media_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsGlobalVirtualMediaManager");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Virtual Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the Virtual Media Manager"));
    }
};

/** Simple action extension, used as 'Show Host Network Manager' action class. */
class UIActionSimpleSelectorToolsGlobalShowHostNetworkManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsGlobalShowHostNetworkManager(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/host_iface_manager_24px.png", ":/host_iface_manager_16px.png",
                         ":/host_iface_manager_24px.png", ":/host_iface_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsGlobalHostNetworkManager");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Host Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the Host Network Manager"));
    }
};

/** Simple action extension, used as 'Show Cloud Profile Manager' action class. */
class UIActionSimpleSelectorToolsGlobalShowCloudProfileManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsGlobalShowCloudProfileManager(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_manager_24px.png", ":/cloud_profile_manager_16px.png",
                         ":/cloud_profile_manager_24px.png", ":/cloud_profile_manager_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToolsGlobalCloudProfileManager");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the Cloud Profile Manager"));
    }
};


/** Menu action extension, used as 'Snapshot' menu class. */
class UIActionMenuSelectorSnapshot : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshot(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SnapshotMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Snapshot"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the snapshot menu"));
    }
};

/** Simple action extension, used as 'Perform Take' action class. */
class UIActionMenuSelectorSnapshotPerformTake : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshotPerformTake(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/snapshot_take_32px.png", ":/snapshot_take_16px.png",
                         ":/snapshot_take_disabled_32px.png", ":/snapshot_take_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("TakeSnapshot");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+T");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Take..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Take a snapshot of the current virtual machine state"));
        setToolTip(tr("Take Snapshot (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Delete' action class. */
class UIActionMenuSelectorSnapshotPerformDelete : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshotPerformDelete(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/snapshot_delete_32px.png", ":/snapshot_delete_16px.png",
                         ":/snapshot_delete_disabled_32px.png", ":/snapshot_delete_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("DeleteSnapshot");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+D");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Delete..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Delete selected snapshot of the virtual machine"));
        setToolTip(tr("Delete Snapshot (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Restore' action class. */
class UIActionMenuSelectorSnapshotPerformRestore : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshotPerformRestore(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/snapshot_restore_32px.png", ":/snapshot_restore_16px.png",
                         ":/snapshot_restore_disabled_32px.png", ":/snapshot_restore_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RestoreSnapshot");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Restore..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Restore selected snapshot of the virtual machine"));
        setToolTip(tr("Restore Snapshot (%1)").arg(shortcut().toString()));
    }
};

/** Toggle action extension, used as 'Toggle Snapshot Properties' action class. */
class UIActionMenuSelectorSnapshotToggleProperties : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshotToggleProperties(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setIcon(UIIconPool::iconSetFull(":/snapshot_show_details_16px.png", ":/snapshot_show_details_16px.png",
                                        ":/snapshot_show_details_disabled_16px.png", ":/snapshot_show_details_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleSnapshotProperties");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Space");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with the selected snapshot properties"));
        setToolTip(tr("Open Snapshot Properties (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Clone' action class. */
class UIActionMenuSelectorSnapshotPerformClone : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorSnapshotPerformClone(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/vm_clone_32px.png", ":/vm_clone_16px.png",
                         ":/vm_clone_disabled_32px.png", ":/vm_clone_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CloneSnapshot");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+C");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Clone..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Clone selected virtual machine"));
        setToolTip(tr("Clone Virtual Machine (%1)").arg(shortcut().toString()));
    }
};


/** Menu action extension, used as 'Medium' menu class. */
class UIActionMenuSelectorMedium : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMedium(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("MediumMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Medium"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the medium menu"));
    }
};

/** Simple action extension, used as 'Perform Add' action class. */
class UIActionMenuSelectorMediumPerformAdd : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformAdd(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_add_32px.png",          ":/hd_add_16px.png",
                                           ":/hd_add_disabled_32px.png", ":/hd_add_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_add_32px.png",          ":/cd_add_16px.png",
                                           ":/cd_add_disabled_32px.png", ":/cd_add_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_add_32px.png",          ":/fd_add_16px.png",
                                           ":/fd_add_disabled_32px.png", ":/fd_add_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+A");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Add..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Add a disk image file"));
        setToolTip(tr("Add a Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Copy' action class. */
class UIActionMenuSelectorMediumPerformCopy : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformCopy(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_copy_32px.png",          ":/hd_copy_16px.png",
                                           ":/hd_copy_disabled_32px.png", ":/hd_copy_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_copy_32px.png",          ":/cd_copy_16px.png",
                                           ":/cd_copy_disabled_32px.png", ":/cd_copy_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_copy_32px.png",          ":/fd_copy_16px.png",
                                           ":/fd_copy_disabled_32px.png", ":/fd_copy_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CopyMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+C");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Copy..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Copy selected disk image file"));
        setToolTip(tr("Copy Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Move' action class. */
class UIActionMenuSelectorMediumPerformMove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformMove(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_move_32px.png",          ":/hd_move_16px.png",
                                           ":/hd_move_disabled_32px.png", ":/hd_move_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_move_32px.png",          ":/cd_move_16px.png",
                                           ":/cd_move_disabled_32px.png", ":/cd_move_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_move_32px.png",          ":/fd_move_16px.png",
                                           ":/fd_move_disabled_32px.png", ":/fd_move_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("MoveMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+M");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Move..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Move selected disk image file"));
        setToolTip(tr("Move Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Remove' action class. */
class UIActionMenuSelectorMediumPerformRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformRemove(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_remove_32px.png",          ":/hd_remove_16px.png",
                                           ":/hd_remove_disabled_32px.png", ":/hd_remove_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_remove_32px.png",          ":/cd_remove_16px.png",
                                           ":/cd_remove_disabled_32px.png", ":/cd_remove_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_remove_32px.png",          ":/fd_remove_16px.png",
                                           ":/fd_remove_disabled_32px.png", ":/fd_remove_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RemoveMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Remove..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Remove selected disk image file"));
        setToolTip(tr("Remove Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Release' action class. */
class UIActionMenuSelectorMediumPerformRelease : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformRelease(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_release_32px.png",          ":/hd_release_16px.png",
                                           ":/hd_release_disabled_32px.png", ":/hd_release_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_release_32px.png",          ":/cd_release_16px.png",
                                           ":/cd_release_disabled_32px.png", ":/cd_release_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_release_32px.png",          ":/fd_release_16px.png",
                                           ":/fd_release_disabled_32px.png", ":/fd_release_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ReleaseMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+L");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Re&lease..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Release selected disk image file"));
        setToolTip(tr("Release Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Toggle action extension, used as 'Toggle Medium Properties' action class. */
class UIActionMenuSelectorMediumToggleProperties : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumToggleProperties(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setIcon(0, UIIconPool::iconSetFull(":/hd_modify_32px.png",          ":/hd_modify_16px.png",
                                           ":/hd_modify_disabled_32px.png", ":/hd_modify_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_modify_32px.png",          ":/cd_modify_16px.png",
                                           ":/cd_modify_disabled_32px.png", ":/cd_modify_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_modify_32px.png",          ":/fd_modify_16px.png",
                                           ":/fd_modify_disabled_32px.png", ":/fd_modify_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleMediumProperties");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Space");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected disk image file properties"));
        setToolTip(tr("Open Disk Image File Properties (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Refresh' action class. */
class UIActionMenuSelectorMediumPerformRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformRefresh(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/refresh_32px.png",          ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RefreshMedia");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+F");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Re&fresh..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the list of disk image files"));
        setToolTip(tr("Refresh Disk Image Files (%1)").arg(shortcut().toString()));
    }
};


/** Menu action extension, used as 'Network' menu class. */
class UIActionMenuSelectorNetwork : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorNetwork(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("NetworkMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Network"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the network menu"));
    }
};

/** Simple action extension, used as 'Perform Create' action class. */
class UIActionMenuSelectorNetworkPerformCreate : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorNetworkPerformCreate(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/host_iface_add_32px.png",          ":/host_iface_add_16px.png",
                         ":/host_iface_add_disabled_32px.png", ":/host_iface_add_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CreateNetwork");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+C");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Create..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Create new host-only network"));
        setToolTip(tr("Create Host-only Network (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Remove' action class. */
class UIActionMenuSelectorNetworkPerformRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorNetworkPerformRemove(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/host_iface_remove_32px.png",          ":/host_iface_remove_16px.png",
                         ":/host_iface_remove_disabled_32px.png", ":/host_iface_remove_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RemoveNetwork");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Remove..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Remove selected host-only network"));
        setToolTip(tr("Remove Host-only Network (%1)").arg(shortcut().toString()));
    }
};

/** Toggle action extension, used as 'Toggle Network Properties' action class. */
class UIActionMenuSelectorNetworkToggleProperties : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorNetworkToggleProperties(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setIcon(UIIconPool::iconSetFull(":/host_iface_edit_32px.png",          ":/host_iface_edit_16px.png",
                                        ":/host_iface_edit_disabled_32px.png", ":/host_iface_edit_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleNetworkProperties");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Space");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected host-only network properties"));
        setToolTip(tr("Open Host-only Network Properties (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Refresh' action class. */
class UIActionMenuSelectorNetworkPerformRefresh : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorNetworkPerformRefresh(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/refresh_32px.png",          ":/refresh_16px.png",
                         ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RefreshNetworks");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+F");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Re&fresh..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Refresh the list of host-only networks"));
        setToolTip(tr("Refresh Host-only Networks (%1)").arg(shortcut().toString()));
    }
};


/** Menu action extension, used as 'Cloud' menu class. */
class UIActionMenuSelectorCloud : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloud(UIActionPool *pParent)
        : UIActionMenu(pParent)
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CloudProfileMenu");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Cloud"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the cloud menu"));
    }
};

/** Simple action extension, used as 'Perform Add' action class. */
class UIActionMenuSelectorCloudPerformAdd : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudPerformAdd(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_add_32px.png",          ":/cloud_profile_add_16px.png",
                         ":/cloud_profile_add_disabled_32px.png", ":/cloud_profile_add_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("AddCloudProfile");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+A");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Add"));
        setName(QApplication::translate("UIActionPool", "&Add Profile..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Add new cloud profile"));
        setToolTip(tr("Add Cloud Profile (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Remove' action class. */
class UIActionMenuSelectorCloudPerformRemove : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudPerformRemove(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_remove_32px.png",          ":/cloud_profile_remove_16px.png",
                         ":/cloud_profile_remove_disabled_32px.png", ":/cloud_profile_remove_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("RemoveCloudProfile");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+R");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Remove"));
        setName(QApplication::translate("UIActionPool", "&Remove Profile..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Remove selected cloud profile"));
        setToolTip(tr("Remove Cloud Profile (%1)").arg(shortcut().toString()));
    }
};

/** Toggle action extension, used as 'Toggle Properties' action class. */
class UIActionMenuSelectorCloudToggleProperties : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudToggleProperties(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setIcon(UIIconPool::iconSetFull(":/cloud_profile_edit_32px.png",          ":/cloud_profile_edit_16px.png",
                                        ":/cloud_profile_edit_disabled_32px.png", ":/cloud_profile_edit_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleCloudProfileProperties");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Space");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Properties"));
        setName(QApplication::translate("UIActionPool", "Profile &Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected cloud profile properties"));
        setToolTip(tr("Open Cloud Profile Properties (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Refresh' action class. */
class UIActionMenuSelectorCloudPerformImport : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudPerformImport(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_restore_32px.png",          ":/cloud_profile_restore_16px.png",
                         ":/cloud_profile_restore_disabled_32px.png", ":/cloud_profile_restore_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ImportCloudProfiles");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+I");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Import"));
        setName(QApplication::translate("UIActionPool", "&Import Profiles..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Import the list of cloud profiles from external files"));
        setToolTip(tr("Import Cloud Profiles (%1)").arg(shortcut().toString()));
    }
};


/*********************************************************************************************************************************
*   Class UIActionPoolSelector implementation.                                                                                   *
*********************************************************************************************************************************/

UIActionPoolSelector::UIActionPoolSelector(bool fTemporary /* = false */)
    : UIActionPool(UIActionPoolType_Selector, fTemporary)
{
}

void UIActionPoolSelector::preparePool()
{
    /* 'File' actions: */
    m_pool[UIActionIndexST_M_File] = new UIActionMenuSelectorFile(this);
    m_pool[UIActionIndexST_M_File_S_ShowVirtualMediumManager] = new UIActionSimpleSelectorFileShowVirtualMediaManager(this);
    m_pool[UIActionIndexST_M_File_S_ShowHostNetworkManager] = new UIActionSimpleSelectorFileShowHostNetworkManager(this);
    m_pool[UIActionIndexST_M_File_S_ShowCloudProfileManager] = new UIActionSimpleSelectorFileShowCloudProfileManager(this);
    m_pool[UIActionIndexST_M_File_S_ImportAppliance] = new UIActionSimpleSelectorFileShowImportApplianceWizard(this);
    m_pool[UIActionIndexST_M_File_S_ExportAppliance] = new UIActionSimpleSelectorFileShowExportApplianceWizard(this);
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    m_pool[UIActionIndexST_M_File_S_ShowExtraDataManager] = new UIActionSimpleSelectorFileShowExtraDataManager(this);
#endif
    m_pool[UIActionIndexST_M_File_S_Close] = new UIActionSimpleSelectorFilePerformExit(this);

    /* 'Welcome' actions: */
    m_pool[UIActionIndexST_M_Welcome] = new UIActionMenuSelectorMachine(this);
    m_pool[UIActionIndexST_M_Welcome_S_New] = new UIActionSimpleSelectorMachinePerformCreate(this);
    m_pool[UIActionIndexST_M_Welcome_S_Add] = new UIActionSimpleSelectorMachinePerformAdd(this);

    /* 'Group' actions: */
    m_pool[UIActionIndexST_M_Group] = new UIActionMenuSelectorGroup(this);
    m_pool[UIActionIndexST_M_Group_S_New] = new UIActionSimpleSelectorGroupPerformCreateMachine(this);
    m_pool[UIActionIndexST_M_Group_S_Add] = new UIActionSimpleSelectorGroupPerformAddMachine(this);
    m_pool[UIActionIndexST_M_Group_S_Rename] = new UIActionSimpleSelectorGroupPerformRename(this);
    m_pool[UIActionIndexST_M_Group_S_Remove] = new UIActionSimpleSelectorGroupPerformRemove(this);
    m_pool[UIActionIndexST_M_Group_M_StartOrShow] = new UIActionStateSelectorCommonStartOrShow(this);
    m_pool[UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal] = new UIActionSimpleSelectorCommonPerformStartNormal(this);
    m_pool[UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless] = new UIActionSimpleSelectorCommonPerformStartHeadless(this);
    m_pool[UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable] = new UIActionSimpleSelectorCommonPerformStartDetachable(this);
    m_pool[UIActionIndexST_M_Group_T_Pause] = new UIActionToggleSelectorCommonPauseAndResume(this);
    m_pool[UIActionIndexST_M_Group_S_Reset] = new UIActionSimpleSelectorCommonPerformReset(this);
    m_pool[UIActionIndexST_M_Group_M_Close] = new UIActionMenuSelectorClose(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_Detach] = new UIActionSimpleSelectorClosePerformDetach(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_SaveState] = new UIActionSimpleSelectorClosePerformSave(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_Shutdown] = new UIActionSimpleSelectorClosePerformShutdown(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_PowerOff] = new UIActionSimpleSelectorClosePerformPowerOff(this);
    m_pool[UIActionIndexST_M_Group_S_Discard] = new UIActionSimpleSelectorCommonPerformDiscard(this);
    m_pool[UIActionIndexST_M_Group_S_ShowLogDialog] = new UIActionSimpleSelectorCommonShowMachineLogs(this);
    m_pool[UIActionIndexST_M_Group_S_Refresh] = new UIActionSimpleSelectorCommonPerformRefresh(this);
    m_pool[UIActionIndexST_M_Group_S_ShowInFileManager] = new UIActionSimpleSelectorCommonShowInFileManager(this);
    m_pool[UIActionIndexST_M_Group_S_CreateShortcut] = new UIActionSimpleSelectorCommonPerformCreateShortcut(this);
    m_pool[UIActionIndexST_M_Group_S_Sort] = new UIActionSimpleSelectorGroupPerformSort(this);

    /* 'Machine' actions: */
    m_pool[UIActionIndexST_M_Machine] = new UIActionMenuSelectorMachine(this);
    m_pool[UIActionIndexST_M_Machine_S_New] = new UIActionSimpleSelectorMachinePerformCreate(this);
    m_pool[UIActionIndexST_M_Machine_S_Add] = new UIActionSimpleSelectorMachinePerformAdd(this);
    m_pool[UIActionIndexST_M_Machine_S_Settings] = new UIActionSimpleSelectorMachineShowSettings(this);
    m_pool[UIActionIndexST_M_Machine_S_Clone] = new UIActionSimpleSelectorMachinePerformClone(this);
    m_pool[UIActionIndexST_M_Machine_S_Move] = new UIActionSimpleSelectorMachinePerformMove(this);
    m_pool[UIActionIndexST_M_Machine_S_Remove] = new UIActionSimpleSelectorMachinePerformRemove(this);
    m_pool[UIActionIndexST_M_Machine_S_AddGroup] = new UIActionSimpleSelectorMachinePerformGroup(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow] = new UIActionStateSelectorCommonStartOrShow(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal] = new UIActionSimpleSelectorCommonPerformStartNormal(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless] = new UIActionSimpleSelectorCommonPerformStartHeadless(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable] = new UIActionSimpleSelectorCommonPerformStartDetachable(this);
    m_pool[UIActionIndexST_M_Machine_T_Pause] = new UIActionToggleSelectorCommonPauseAndResume(this);
    m_pool[UIActionIndexST_M_Machine_S_Reset] = new UIActionSimpleSelectorCommonPerformReset(this);
    m_pool[UIActionIndexST_M_Machine_M_Close] = new UIActionMenuSelectorClose(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_Detach] = new UIActionSimpleSelectorClosePerformDetach(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_SaveState] = new UIActionSimpleSelectorClosePerformSave(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_Shutdown] = new UIActionSimpleSelectorClosePerformShutdown(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_PowerOff] = new UIActionSimpleSelectorClosePerformPowerOff(this);
    m_pool[UIActionIndexST_M_Machine_S_Discard] = new UIActionSimpleSelectorCommonPerformDiscard(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowLogDialog] = new UIActionSimpleSelectorCommonShowMachineLogs(this);
    m_pool[UIActionIndexST_M_Machine_S_Refresh] = new UIActionSimpleSelectorCommonPerformRefresh(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowInFileManager] = new UIActionSimpleSelectorCommonShowInFileManager(this);
    m_pool[UIActionIndexST_M_Machine_S_CreateShortcut] = new UIActionSimpleSelectorCommonPerformCreateShortcut(this);
    m_pool[UIActionIndexST_M_Machine_S_SortParent] = new UIActionSimpleSelectorMachinePerformSortParent(this);

    /* Machine Tools actions: */
    m_pool[UIActionIndexST_M_Tools_T_Machine] = new UIActionToggleSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine] = new UIActionMenuSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_Details] = new UIActionSimpleSelectorToolsMachineShowDetails(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_Snapshots] = new UIActionSimpleSelectorToolsMachineShowSnapshots(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_LogViewer] = new UIActionSimpleSelectorToolsMachineShowLogs(this);

    /* Global Tools actions: */
    m_pool[UIActionIndexST_M_Tools_T_Global] = new UIActionToggleSelectorToolsGlobal(this);
    m_pool[UIActionIndexST_M_Tools_M_Global] = new UIActionMenuSelectorToolsGlobal(this);
    m_pool[UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager] = new UIActionSimpleSelectorToolsGlobalShowVirtualMediaManager(this);
    m_pool[UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager] = new UIActionSimpleSelectorToolsGlobalShowHostNetworkManager(this);
    m_pool[UIActionIndexST_M_Tools_M_Global_S_CloudProfileManager] = new UIActionSimpleSelectorToolsGlobalShowCloudProfileManager(this);

    /* Snapshot Pane actions: */
    m_pool[UIActionIndexST_M_Snapshot] = new UIActionMenuSelectorSnapshot(this);
    m_pool[UIActionIndexST_M_Snapshot_S_Take] = new UIActionMenuSelectorSnapshotPerformTake(this);
    m_pool[UIActionIndexST_M_Snapshot_S_Delete] = new UIActionMenuSelectorSnapshotPerformDelete(this);
    m_pool[UIActionIndexST_M_Snapshot_S_Restore] = new UIActionMenuSelectorSnapshotPerformRestore(this);
    m_pool[UIActionIndexST_M_Snapshot_T_Properties] = new UIActionMenuSelectorSnapshotToggleProperties(this);
    m_pool[UIActionIndexST_M_Snapshot_S_Clone] = new UIActionMenuSelectorSnapshotPerformClone(this);

    /* Virtual Medium Manager actions: */
    m_pool[UIActionIndexST_M_MediumWindow] = new UIActionMenuSelectorMedium(this);
    m_pool[UIActionIndexST_M_Medium] = new UIActionMenuSelectorMedium(this);
    m_pool[UIActionIndexST_M_Medium_S_Add] = new UIActionMenuSelectorMediumPerformAdd(this);
    m_pool[UIActionIndexST_M_Medium_S_Copy] = new UIActionMenuSelectorMediumPerformCopy(this);
    m_pool[UIActionIndexST_M_Medium_S_Move] = new UIActionMenuSelectorMediumPerformMove(this);
    m_pool[UIActionIndexST_M_Medium_S_Remove] = new UIActionMenuSelectorMediumPerformRemove(this);
    m_pool[UIActionIndexST_M_Medium_S_Release] = new UIActionMenuSelectorMediumPerformRelease(this);
    m_pool[UIActionIndexST_M_Medium_T_Details] = new UIActionMenuSelectorMediumToggleProperties(this);
    m_pool[UIActionIndexST_M_Medium_S_Refresh] = new UIActionMenuSelectorMediumPerformRefresh(this);

    /* Host Network Manager actions: */
    m_pool[UIActionIndexST_M_NetworkWindow] = new UIActionMenuSelectorNetwork(this);
    m_pool[UIActionIndexST_M_Network] = new UIActionMenuSelectorNetwork(this);
    m_pool[UIActionIndexST_M_Network_S_Create] = new UIActionMenuSelectorNetworkPerformCreate(this);
    m_pool[UIActionIndexST_M_Network_S_Remove] = new UIActionMenuSelectorNetworkPerformRemove(this);
    m_pool[UIActionIndexST_M_Network_T_Details] = new UIActionMenuSelectorNetworkToggleProperties(this);
    m_pool[UIActionIndexST_M_Network_S_Refresh] = new UIActionMenuSelectorNetworkPerformRefresh(this);

    /* Cloud Profile Manager actions: */
    m_pool[UIActionIndexST_M_CloudWindow] = new UIActionMenuSelectorCloud(this);
    m_pool[UIActionIndexST_M_Cloud] = new UIActionMenuSelectorCloud(this);
    m_pool[UIActionIndexST_M_Cloud_S_Add] = new UIActionMenuSelectorCloudPerformAdd(this);
    m_pool[UIActionIndexST_M_Cloud_S_Remove] = new UIActionMenuSelectorCloudPerformRemove(this);
    m_pool[UIActionIndexST_M_Cloud_T_Details] = new UIActionMenuSelectorCloudToggleProperties(this);
    m_pool[UIActionIndexST_M_Cloud_S_Import] = new UIActionMenuSelectorCloudPerformImport(this);

    /* Prepare update-handlers for known menus: */
    m_menuUpdateHandlers[UIActionIndexST_M_MediumWindow].ptfs = &UIActionPoolSelector::updateMenuMediumWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Medium].ptfs = &UIActionPoolSelector::updateMenuMedium;
    m_menuUpdateHandlers[UIActionIndexST_M_NetworkWindow].ptfs = &UIActionPoolSelector::updateMenuNetworkWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Network].ptfs = &UIActionPoolSelector::updateMenuNetwork;
    m_menuUpdateHandlers[UIActionIndexST_M_CloudWindow].ptfs = &UIActionPoolSelector::updateMenuCloudWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Cloud].ptfs = &UIActionPoolSelector::updateMenuCloud;

    /* Call to base-class: */
    UIActionPool::preparePool();
}

void UIActionPoolSelector::prepareConnections()
{
    /* Prepare connections: */
    connect(gShortcutPool, SIGNAL(sigSelectorShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));
    connect(gShortcutPool, SIGNAL(sigMachineShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));

    /* Call to base-class: */
    UIActionPool::prepareConnections();
}

void UIActionPoolSelector::updateMenus()
{
    /* 'Help' menu: */
    updateMenuHelp();

    /* 'Log Viewer' menu: */
    updateMenuLogViewerWindow();
    updateMenuLogViewer();

    /* 'Virtual Media Manager' menu: */
    updateMenuMediumWindow();
    updateMenuMedium();
    /* 'Host Network Manager' menu: */
    updateMenuNetworkWindow();
    updateMenuNetwork();
}

void UIActionPoolSelector::updateMenuMediumWindow()
{
    /* Update corresponding menu: */
    updateMenuMediumWrapper(action(UIActionIndexST_M_MediumWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_MediumWindow);
}

void UIActionPoolSelector::updateMenuMedium()
{
    /* Update corresponding menu: */
    updateMenuMediumWrapper(action(UIActionIndexST_M_Medium)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Medium);
}

void UIActionPoolSelector::updateMenuMediumWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Add' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Add)) || fSeparator;
    /* 'Copy' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Copy)) || fSeparator;
    /* 'Move' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Move)) || fSeparator;
    /* 'Remove' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Remove)) || fSeparator;
    /* 'Release' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Release)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Properties' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_T_Details)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Refresh' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Refresh)) || fSeparator;;
}

void UIActionPoolSelector::updateMenuNetworkWindow()
{
    /* Update corresponding menu: */
    updateMenuNetworkWrapper(action(UIActionIndexST_M_NetworkWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_NetworkWindow);
}

void UIActionPoolSelector::updateMenuNetwork()
{
    /* Update corresponding menu: */
    updateMenuNetworkWrapper(action(UIActionIndexST_M_Network)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Network);
}

void UIActionPoolSelector::updateMenuNetworkWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Create' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_S_Create)) || fSeparator;
    /* 'Remove' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_S_Remove)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Properties' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_T_Details)) || fSeparator;

//    /* Separator? */
//    if (fSeparator)
//    {
//        pMenu->addSeparator();
//        fSeparator = false;
//    }

//    /* 'Refresh' action: */
//    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_S_Refresh)) || fSeparator;;
}

void UIActionPoolSelector::updateMenuCloudWindow()
{
    /* Update corresponding menu: */
    updateMenuCloudWrapper(action(UIActionIndexST_M_CloudWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_CloudWindow);
}

void UIActionPoolSelector::updateMenuCloud()
{
    /* Update corresponding menu: */
    updateMenuCloudWrapper(action(UIActionIndexST_M_Cloud)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Cloud);
}

void UIActionPoolSelector::updateMenuCloudWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Add' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Add)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Remove' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Remove)) || fSeparator;
    /* 'Properties' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_T_Details)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Import' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Import)) || fSeparator;
}

void UIActionPoolSelector::updateShortcuts()
{
    /* Call to base-class: */
    UIActionPool::updateShortcuts();
    /* Create temporary Runtime UI pool to do the same: */
    if (!m_fTemporary)
        UIActionPool::createTemporary(UIActionPoolType_Runtime);
}

QString UIActionPoolSelector::shortcutsExtraDataID() const
{
    return GUI_Input_SelectorShortcuts;
}


#include "UIActionPoolSelector.moc"
