/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPoolManager class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
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
#include "UICommon.h"
#include "UIActionPoolManager.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIShortcutPool.h"
#include "UIDefs.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"

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
        : UIActionSimple(pParent, ":/media_manager_16px.png", ":/media_manager_disabled_16px.png")
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
        : UIActionSimple(pParent, ":/host_iface_manager_16px.png", ":/host_iface_manager_disabled_16px.png")
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
        return QKeySequence("Ctrl+H");
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
        : UIActionSimple(pParent, ":/cloud_profile_manager_16px.png", ":/cloud_profile_manager_disabled_16px.png")
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
        return QKeySequence("Ctrl+P");
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
        : UIActionSimple(pParent,
                         ":/import_32px.png", ":/import_16px.png",
                         ":/import_disabled_32px.png", ":/import_disabled_16px.png")
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
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Show Export Appliance Wizard' action class. */
class UIActionSimpleSelectorFileShowExportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowExportApplianceWizard(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/export_32px.png", ":/export_16px.png",
                         ":/export_disabled_32px.png", ":/export_disabled_16px.png")
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
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
    }
};

/** Simple action extension, used as 'Show New Cloud VM Wizard' action class. */
class UIActionSimpleSelectorFileShowNewCloudVMWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorFileShowNewCloudVMWizard(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_vm_new_32px.png", ":/cloud_vm_new_16px.png",
                         ":/cloud_vm_new_disabled_32px.png", ":/cloud_vm_new_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("NewCloudVM");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&New Cloud VM..."));
        setStatusTip(QApplication::translate("UIActionPool", "Create new cloud virtual machine"));
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
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
        : UIActionSimple(pParent,
                         ":/vm_new_32px.png", ":/vm_new_16px.png",
                         ":/vm_new_disabled_32px.png", ":/vm_new_disabled_16px.png")
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
        : UIActionSimple(pParent,
                         ":/vm_add_32px.png", ":/vm_add_16px.png",
                         ":/vm_add_disabled_32px.png", ":/vm_add_disabled_16px.png")
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
        : UIActionSimple(pParent,
                         ":/vm_new_32px.png", ":/vm_new_16px.png",
                         ":/vm_new_disabled_32px.png", ":/vm_new_disabled_16px.png")
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
        : UIActionSimple(pParent,
                         ":/vm_add_32px.png", ":/vm_add_16px.png",
                         ":/vm_add_disabled_32px.png", ":/vm_add_disabled_16px.png")
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
        setToolTip(text().remove('&').remove('.') +
                   (shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(shortcut().toString())));
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

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Move..."));
        setStatusTip(QApplication::translate("UIActionPool", "Move selected virtual machine"));
    }
};

/** Simple action extension, used as 'Perform Export Machine to OCI' action class. */
class UIActionSimpleSelectorMachinePerformExportToOCI : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformExportToOCI(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/export_16px.png", ":/export_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ExportToOCI");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "E&xport to OCI..."));
        setStatusTip(QApplication::translate("UIActionPool", "Export selected virtual machine to OCI"));
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

/** Simple action extension, used as 'Machine Search' action class. */
class UIActionSimpleSelectorMachinePerformSearch : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorMachinePerformSearch(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/sort_16px.png", ":/sort_disabled_16px.png")
    {}

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("SearchVM");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+F");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "S&earch"));
        setStatusTip(QApplication::translate("UIActionPool", "Search virtual machines with respect to a search term"));
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

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "Po&wer Off"));
        setStatusTip(QApplication::translate("UIActionPool", "Power off selected virtual machines"));
    }
};


/** Menu action extension, used as 'Machine Tools' menu class. */
class UIActionMenuSelectorToolsMachine : public UIActionMenu
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorToolsMachine(UIActionPool *pParent)
        : UIActionMenu(pParent, ":/tools_menu_24px.png") /// @todo replace with 16px icon
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
        setName(QApplication::translate("UIActionPool", "Tools"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the machine tools menu"));
    }
};

/** Simple action extension, used as 'Show Machine Details' action class. */
class UIActionToggleSelectorToolsMachineShowDetails : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsMachineShowDetails(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setProperty("UIToolType", QVariant::fromValue(UIToolType_Details));
        /// @todo use icons with check-boxes
        setIcon(UIIconPool::iconSetFull(":/machine_details_manager_24px.png", ":/machine_details_manager_16px.png",
                                        ":/machine_details_manager_disabled_24px.png", ":/machine_details_manager_disabled_16px.png"));
    }

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
class UIActionToggleSelectorToolsMachineShowSnapshots : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsMachineShowSnapshots(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setProperty("UIToolType", QVariant::fromValue(UIToolType_Snapshots));
        /// @todo use icons with check-boxes
        setIcon(UIIconPool::iconSetFull(":/snapshot_manager_24px.png", ":/snapshot_manager_16px.png",
                                        ":/snapshot_manager_disabled_24px.png", ":/snapshot_manager_disabled_16px.png"));
    }

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
class UIActionToggleSelectorToolsMachineShowLogs : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsMachineShowLogs(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setProperty("UIToolType", QVariant::fromValue(UIToolType_Logs));
        /// @todo use icons with check-boxes
        setIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png",
                                        ":/vm_show_logs_disabled_32px.png", ":/vm_show_logs_disabled_16px.png"));
    }

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
                         ":/media_manager_disabled_24px.png", ":/media_manager_disabled_16px.png")
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
                         ":/host_iface_manager_disabled_24px.png", ":/host_iface_manager_disabled_16px.png")
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
                         ":/cloud_profile_manager_disabled_24px.png", ":/cloud_profile_manager_disabled_16px.png")
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Take Snapshot (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Delete Snapshot (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Restore Snapshot (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
        setIcon(UIIconPool::iconSetFull(":/snapshot_show_details_32px.png", ":/snapshot_show_details_16px.png",
                                        ":/snapshot_show_details_disabled_32px.png", ":/snapshot_show_details_disabled_16px.png"));
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
        return QKeySequence("Ctrl+Shift+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Snapshot Pane"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with the selected snapshot properties"));
        setToolTip(QApplication::translate("UIActionPool", "Open Snapshot Properties (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Clone Virtual Machine (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        setToolTip(QApplication::translate("UIActionPool", "Add a Disk Image File (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Create' action class. */
class UIActionMenuSelectorMediumPerformCreate : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumPerformCreate(UIActionPool *pParent)
        : UIActionSimple(pParent)
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
        setIcon(0, UIIconPool::iconSetFull(":/hd_create_32px.png",          ":/hd_create_16px.png",
                                           ":/hd_create_disabled_32px.png", ":/hd_create_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_create_32px.png",          ":/cd_create_16px.png",
                                           ":/cd_create_disabled_32px.png", ":/cd_create_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_create_32px.png",          ":/fd_create_16px.png",
                                           ":/fd_create_disabled_32px.png", ":/fd_create_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("CreateMedium");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Create..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Create a new disk image"));
        setToolTip(QApplication::translate("UIActionPool", "Create a New Disk Image (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        setToolTip(QApplication::translate("UIActionPool", "Copy Disk Image File (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        setToolTip(QApplication::translate("UIActionPool", "Move Disk Image File (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        setToolTip(QApplication::translate("UIActionPool", "Remove Disk Image File (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        setToolTip(QApplication::translate("UIActionPool", "Release Disk Image File (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        return QKeySequence("Ctrl+Shift+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected disk image file properties"));
        setToolTip(QApplication::translate("UIActionPool", "Open Disk Image File Properties (%1)").arg(shortcut().toString()));
    }
};

/** Toggle action extension, used as 'Toggle Search Pane' action class. */
class UIActionMenuSelectorMediumToggleSearch : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorMediumToggleSearch(UIActionPool *pParent)
        : UIActionToggle(pParent)
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
        setIcon(0, UIIconPool::iconSetFull(":/hd_search_32px.png",          ":/hd_search_16px.png",
                                           ":/hd_search_disabled_32px.png", ":/hd_search_disabled_16px.png"));
        setIcon(1, UIIconPool::iconSetFull(":/cd_search_32px.png",          ":/cd_search_16px.png",
                                           ":/cd_search_disabled_32px.png", ":/cd_search_disabled_16px.png"));
        setIcon(2, UIIconPool::iconSetFull(":/fd_search_32px.png",          ":/fd_search_16px.png",
                                           ":/fd_search_disabled_32px.png", ":/fd_search_disabled_16px.png"));
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ToggleMediumSearch");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Search"));
        setShortcutScope(QApplication::translate("UIActionPool", "Media Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open the medium search pane"));
        setToolTip(QApplication::translate("UIActionPool", "Open Medium Search Pane (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Refresh Disk Image Files (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Create Host-only Network (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Remove Host-only Network (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        return QKeySequence("Ctrl+Shift+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setName(QApplication::translate("UIActionPool", "&Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Network Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected host-only network properties"));
        setToolTip(QApplication::translate("UIActionPool", "Open Host-only Network Properties (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Refresh Host-only Networks (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Add Cloud Profile (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Perform Import' action class. */
class UIActionMenuSelectorCloudPerformImport : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudPerformImport(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_restore_32px.png",          ":/cloud_profile_restore_16px.png",
                         ":/cloud_profile_restore_disabled_32px.png", ":/cloud_profile_restore_disabled_16px.png")
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Import Cloud Profiles (%1)").arg(shortcut().toString()));
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
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

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
        setToolTip(QApplication::translate("UIActionPool", "Remove Cloud Profile (%1)").arg(shortcut().toString()));
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
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
        return QKeySequence("Ctrl+Shift+P");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Properties"));
        setName(QApplication::translate("UIActionPool", "Profile &Properties"));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Open pane with selected cloud profile properties"));
        setToolTip(QApplication::translate("UIActionPool", "Open Cloud Profile Properties (%1)").arg(shortcut().toString()));
    }
};

/** Simple action extension, used as 'Try Page' action class. */
class UIActionMenuSelectorCloudShowTryPage : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudShowTryPage(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_try_32px.png",          ":/cloud_profile_try_16px.png",
                         ":/cloud_profile_try_disabled_32px.png", ":/cloud_profile_try_disabled_16px.png")
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ShowCloudProfileTryPage");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+T");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Try"));
        setName(QApplication::translate("UIActionPool", "&Try Oracle Cloud for Free..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Try Oracle cloud for free"));
        const QString strToolTip = QApplication::translate("UIActionPool", "Try Oracle Cloud for Free");
        setToolTip(shortcut().isEmpty() ? strToolTip : QString("%1 (%2)").arg(strToolTip, shortcut().toString()));
    }
};

/** Simple action extension, used as 'Show Help' action class. */
class UIActionMenuSelectorCloudShowHelp : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionMenuSelectorCloudShowHelp(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/cloud_profile_help_32px.png",          ":/cloud_profile_help_16px.png",
                         ":/cloud_profile_help_disabled_32px.png", ":/cloud_profile_help_disabled_16px.png")
    {
        setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

protected:

    /** Returns shortcut extra-data ID. */
    virtual QString shortcutExtraDataID() const /* override */
    {
        return QString("ShowCloudProfileHelp");
    }

    /** Returns default shortcut. */
    virtual QKeySequence defaultShortcut(UIActionPoolType) const /* override */
    {
        return QKeySequence("Ctrl+Shift+H");
    }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */
    {
        setIconText(QApplication::translate("UIActionPool", "Help"));
        setName(QApplication::translate("UIActionPool", "&Show Help..."));
        setShortcutScope(QApplication::translate("UIActionPool", "Cloud Profile Manager"));
        setStatusTip(QApplication::translate("UIActionPool", "Show cloud profile help"));
        setToolTip(QApplication::translate("UIActionPool", "Show Cloud Profile Help (%1)").arg(shortcut().toString()));
    }
};


/*********************************************************************************************************************************
*   Class UIActionPoolManager implementation.                                                                                    *
*********************************************************************************************************************************/

UIActionPoolManager::UIActionPoolManager(bool fTemporary /* = false */)
    : UIActionPool(UIActionPoolType_Manager, fTemporary)
{
}

void UIActionPoolManager::preparePool()
{
    /* 'File' actions: */
    m_pool[UIActionIndexST_M_File] = new UIActionMenuSelectorFile(this);
    m_pool[UIActionIndexST_M_File_S_ShowVirtualMediumManager] = new UIActionSimpleSelectorFileShowVirtualMediaManager(this);
    m_pool[UIActionIndexST_M_File_S_ShowHostNetworkManager] = new UIActionSimpleSelectorFileShowHostNetworkManager(this);
    m_pool[UIActionIndexST_M_File_S_ShowCloudProfileManager] = new UIActionSimpleSelectorFileShowCloudProfileManager(this);
    m_pool[UIActionIndexST_M_File_S_ImportAppliance] = new UIActionSimpleSelectorFileShowImportApplianceWizard(this);
    m_pool[UIActionIndexST_M_File_S_ExportAppliance] = new UIActionSimpleSelectorFileShowExportApplianceWizard(this);
    m_pool[UIActionIndexST_M_File_S_NewCloudVM] = new UIActionSimpleSelectorFileShowNewCloudVMWizard(this);
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
    m_pool[UIActionIndexST_M_Group_M_Tools] = new UIActionMenuSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Group_M_Tools_T_Details] = new UIActionToggleSelectorToolsMachineShowDetails(this);
    m_pool[UIActionIndexST_M_Group_M_Tools_T_Snapshots] = new UIActionToggleSelectorToolsMachineShowSnapshots(this);
    m_pool[UIActionIndexST_M_Group_M_Tools_T_Logs] = new UIActionToggleSelectorToolsMachineShowLogs(this);
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
    m_pool[UIActionIndexST_M_Machine_S_ExportToOCI] = new UIActionSimpleSelectorMachinePerformExportToOCI(this);
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
    m_pool[UIActionIndexST_M_Machine_M_Tools] = new UIActionMenuSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Machine_M_Tools_T_Details] = new UIActionToggleSelectorToolsMachineShowDetails(this);
    m_pool[UIActionIndexST_M_Machine_M_Tools_T_Snapshots] = new UIActionToggleSelectorToolsMachineShowSnapshots(this);
    m_pool[UIActionIndexST_M_Machine_M_Tools_T_Logs] = new UIActionToggleSelectorToolsMachineShowLogs(this);
    m_pool[UIActionIndexST_M_Machine_S_Discard] = new UIActionSimpleSelectorCommonPerformDiscard(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowLogDialog] = new UIActionSimpleSelectorCommonShowMachineLogs(this);
    m_pool[UIActionIndexST_M_Machine_S_Refresh] = new UIActionSimpleSelectorCommonPerformRefresh(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowInFileManager] = new UIActionSimpleSelectorCommonShowInFileManager(this);
    m_pool[UIActionIndexST_M_Machine_S_CreateShortcut] = new UIActionSimpleSelectorCommonPerformCreateShortcut(this);
    m_pool[UIActionIndexST_M_Machine_S_SortParent] = new UIActionSimpleSelectorMachinePerformSortParent(this);
    m_pool[UIActionIndexST_M_Machine_S_Search] = new UIActionSimpleSelectorMachinePerformSearch(this);

    /* Global Tools actions: */
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
    m_pool[UIActionIndexST_M_Medium_S_Create] = new UIActionMenuSelectorMediumPerformCreate(this);
    m_pool[UIActionIndexST_M_Medium_S_Copy] = new UIActionMenuSelectorMediumPerformCopy(this);
    m_pool[UIActionIndexST_M_Medium_S_Move] = new UIActionMenuSelectorMediumPerformMove(this);
    m_pool[UIActionIndexST_M_Medium_S_Remove] = new UIActionMenuSelectorMediumPerformRemove(this);
    m_pool[UIActionIndexST_M_Medium_S_Release] = new UIActionMenuSelectorMediumPerformRelease(this);
    m_pool[UIActionIndexST_M_Medium_T_Details] = new UIActionMenuSelectorMediumToggleProperties(this);
    m_pool[UIActionIndexST_M_Medium_T_Search] = new UIActionMenuSelectorMediumToggleSearch(this);
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
    m_pool[UIActionIndexST_M_Cloud_S_Import] = new UIActionMenuSelectorCloudPerformImport(this);
    m_pool[UIActionIndexST_M_Cloud_S_Remove] = new UIActionMenuSelectorCloudPerformRemove(this);
    m_pool[UIActionIndexST_M_Cloud_T_Details] = new UIActionMenuSelectorCloudToggleProperties(this);
    m_pool[UIActionIndexST_M_Cloud_S_TryPage] = new UIActionMenuSelectorCloudShowTryPage(this);
    m_pool[UIActionIndexST_M_Cloud_S_Help] = new UIActionMenuSelectorCloudShowHelp(this);

    /* 'Group' action groups: */
    m_groupPool[UIActionIndexST_M_Group_M_Tools] = new QActionGroup(m_pool.value(UIActionIndexST_M_Group_M_Tools));
    m_groupPool[UIActionIndexST_M_Group_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Group_M_Tools_T_Details));
    m_groupPool[UIActionIndexST_M_Group_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Group_M_Tools_T_Snapshots));
    m_groupPool[UIActionIndexST_M_Group_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Group_M_Tools_T_Logs));

    /* 'Machine' action groups: */
    m_groupPool[UIActionIndexST_M_Machine_M_Tools] = new QActionGroup(m_pool.value(UIActionIndexST_M_Machine_M_Tools));
    m_groupPool[UIActionIndexST_M_Machine_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Machine_M_Tools_T_Details));
    m_groupPool[UIActionIndexST_M_Machine_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Machine_M_Tools_T_Snapshots));
    m_groupPool[UIActionIndexST_M_Machine_M_Tools]->addAction(m_pool.value(UIActionIndexST_M_Machine_M_Tools_T_Logs));

    /* Prepare update-handlers for known menus: */
    m_menuUpdateHandlers[UIActionIndexST_M_File].ptfm =                  &UIActionPoolManager::updateMenuFile;
    m_menuUpdateHandlers[UIActionIndexST_M_Welcome].ptfm =               &UIActionPoolManager::updateMenuWelcome;
    m_menuUpdateHandlers[UIActionIndexST_M_Group].ptfm =                 &UIActionPoolManager::updateMenuGroup;
    m_menuUpdateHandlers[UIActionIndexST_M_Machine].ptfm =               &UIActionPoolManager::updateMenuMachine;
    m_menuUpdateHandlers[UIActionIndexST_M_Group_M_StartOrShow].ptfm =   &UIActionPoolManager::updateMenuGroupStartOrShow;
    m_menuUpdateHandlers[UIActionIndexST_M_Machine_M_StartOrShow].ptfm = &UIActionPoolManager::updateMenuMachineStartOrShow;
    m_menuUpdateHandlers[UIActionIndexST_M_Group_M_Close].ptfm =         &UIActionPoolManager::updateMenuGroupClose;
    m_menuUpdateHandlers[UIActionIndexST_M_Machine_M_Close].ptfm =       &UIActionPoolManager::updateMenuMachineClose;
    m_menuUpdateHandlers[UIActionIndexST_M_Group_M_Tools].ptfm =         &UIActionPoolManager::updateMenuGroupTools;
    m_menuUpdateHandlers[UIActionIndexST_M_Machine_M_Tools].ptfm =       &UIActionPoolManager::updateMenuMachineTools;
    m_menuUpdateHandlers[UIActionIndexST_M_MediumWindow].ptfm =          &UIActionPoolManager::updateMenuMediumWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Medium].ptfm =                &UIActionPoolManager::updateMenuMedium;
    m_menuUpdateHandlers[UIActionIndexST_M_NetworkWindow].ptfm =         &UIActionPoolManager::updateMenuNetworkWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Network].ptfm =               &UIActionPoolManager::updateMenuNetwork;
    m_menuUpdateHandlers[UIActionIndexST_M_CloudWindow].ptfm =           &UIActionPoolManager::updateMenuCloudWindow;
    m_menuUpdateHandlers[UIActionIndexST_M_Cloud].ptfm =                 &UIActionPoolManager::updateMenuCloud;
    m_menuUpdateHandlers[UIActionIndexST_M_Snapshot].ptfm =              &UIActionPoolManager::updateMenuSnapshot;

    /* Call to base-class: */
    UIActionPool::preparePool();
}

void UIActionPoolManager::prepareConnections()
{
    /* Prepare connections: */
    connect(gShortcutPool, SIGNAL(sigSelectorShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));
    connect(gShortcutPool, SIGNAL(sigMachineShortcutsReloaded()), this, SLOT(sltApplyShortcuts()));

    /* Call to base-class: */
    UIActionPool::prepareConnections();
}

void UIActionPoolManager::updateMenu(int iIndex)
{
    /* If index belongs to base-class => delegate to base-class: */
    if (iIndex < UIActionIndex_Max)
        UIActionPool::updateMenu(iIndex);
    /* Otherwise,
     * if menu with such index is invalidated
     * and there is update-handler => handle it here: */
    else if (   iIndex > UIActionIndex_Max
             && m_invalidations.contains(iIndex)
             && m_menuUpdateHandlers.contains(iIndex))
             (this->*(m_menuUpdateHandlers.value(iIndex).ptfm))();
}

void UIActionPoolManager::updateMenus()
{
    /* Clear menu list: */
    m_mainMenus.clear();

    /* 'File' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_File));
    updateMenuFile();

    /* 'Welcome' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Welcome));
    updateMenuWelcome();
    /* 'Group' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Group));
    updateMenuGroup();
    /* 'Machine' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Machine));
    updateMenuMachine();

    /* 'Group' / 'Start or Show' menu: */
    updateMenuGroupStartOrShow();
    /* 'Machine' / 'Start or Show' menu: */
    updateMenuMachineStartOrShow();
    /* 'Group' / 'Close' menu: */
    updateMenuGroupClose();
    /* 'Machine' / 'Close' menu: */
    updateMenuMachineClose();
    /* 'Group' / 'Tools' menu: */
    updateMenuGroupTools();
    /* 'Machine' / 'Tools' menu: */
    updateMenuMachineTools();

    /* 'Virtual Media Manager' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Medium));
    updateMenuMediumWindow();
    updateMenuMedium();
    /* 'Host Network Manager' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Network));
    updateMenuNetworkWindow();
    updateMenuNetwork();
    /* 'Cloud Profile Manager' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Cloud));
    updateMenuCloudWindow();
    updateMenuCloud();

    /* 'Snapshot' menu: */
    addMenu(m_mainMenus, action(UIActionIndexST_M_Snapshot));
    updateMenuSnapshot();
    /* 'Log' menu: */
    addMenu(m_mainMenus, action(UIActionIndex_M_Log));
    updateMenuLogViewerWindow();
    updateMenuLogViewer();

    /* 'Help' menu: */
    addMenu(m_mainMenus, action(UIActionIndex_Menu_Help));
    updateMenuHelp();
}

void UIActionPoolManager::updateMenuFile()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_File)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Check if Ext Pack is ready, some of actions my depend on it: */
    CExtPack extPack = uiCommon().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
    const bool fExtPackAccessible = !extPack.isNull() && extPack.GetUsable();

    /* The Application / 'File' menu contents is very different depending on host type. */

#ifdef VBOX_WS_MAC

    /* 'About' action goes to Application menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_About));
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* 'Check for Updates' action goes to Application menu: */
    if (gEDataManager->applicationUpdateEnabled())
        pMenu->addAction(action(UIActionIndex_M_Application_S_CheckForUpdates));
    /* 'Network Access Manager' action goes to Application menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_NetworkAccessManager));
# endif
    /* 'Reset Warnings' action goes to Application menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_ResetWarnings));
    /* 'Preferences' action goes to Application menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_Preferences));
    /* 'Close' action goes to Application menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_Close));

    /* 'Import Appliance' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ImportAppliance));
    /* 'Export Appliance' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ExportAppliance));
    /* 'New Cloud VM' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_NewCloudVM));
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /* 'Show Extra-data Manager' action goes to 'File' menu for Debug build: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif
    /* 'Show Virtual Medium Manager' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
    /* 'Show Host Network Manager' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
    /* 'Show Cloud Profile Manager' action goes to 'File' menu: */
    if (fExtPackAccessible)
        pMenu->addAction(action(UIActionIndexST_M_File_S_ShowCloudProfileManager));

#else /* !VBOX_WS_MAC */

    /* 'Preferences' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_Preferences));
    /* Separator after 'Preferences' action of the 'File' menu: */
    pMenu->addSeparator();
    /* 'Import Appliance' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ImportAppliance));
    /* 'Export Appliance' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ExportAppliance));
    /* 'New Cloud VM' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_NewCloudVM));
    /* Separator after 'Export Appliance' action of the 'File' menu: */
    pMenu->addSeparator();
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /* 'Extra-data Manager' action goes to 'File' menu for Debug build: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif
    /* 'Show Virtual Medium Manager' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
    /* 'Show Host Network Manager' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
    /* 'Show Cloud Profile Manager' action goes to 'File' menu: */
    if (fExtPackAccessible)
        pMenu->addAction(action(UIActionIndexST_M_File_S_ShowCloudProfileManager));
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* 'Network Access Manager' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_NetworkAccessManager));
    /* 'Check for Updates' action goes to 'File' menu: */
    if (gEDataManager->applicationUpdateEnabled())
        pMenu->addAction(action(UIActionIndex_M_Application_S_CheckForUpdates));
# endif
    /* Separator after tool actions of the 'File' menu: */
    pMenu->addSeparator();
    /* 'Reset Warnings' action goes 'File' menu: */
    pMenu->addAction(action(UIActionIndex_M_Application_S_ResetWarnings));
    /* Separator after 'Reset Warnings' action of the 'File' menu: */
    pMenu->addSeparator();
    /* 'Close' action goes to 'File' menu: */
    pMenu->addAction(action(UIActionIndexST_M_File_S_Close));

#endif /* !VBOX_WS_MAC */

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_File);
}

void UIActionPoolManager::updateMenuWelcome()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Welcome)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate Welcome menu: */
    pMenu->addAction(action(UIActionIndexST_M_Welcome_S_New));
    pMenu->addAction(action(UIActionIndexST_M_Welcome_S_Add));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Welcome);
}

void UIActionPoolManager::updateMenuGroup()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Group)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate Machine-menu: */
    pMenu->addAction(action(UIActionIndexST_M_Group_S_New));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Add));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Rename));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Remove));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Group_M_StartOrShow));
    pMenu->addAction(action(UIActionIndexST_M_Group_T_Pause));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Reset));
    pMenu->addMenu(action(UIActionIndexST_M_Group_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addMenu(action(UIActionIndexST_M_Group_M_Tools)->menu());
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Discard));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_ShowLogDialog));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Group_S_ShowInFileManager));
    pMenu->addAction(action(UIActionIndexST_M_Group_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Group_S_Sort));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Group);
}

void UIActionPoolManager::updateMenuMachine()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Machine)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate Machine-menu: */
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_New));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Add));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Settings));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Clone));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Move));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_ExportToOCI));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Remove));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_AddGroup));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_StartOrShow));
    pMenu->addAction(action(UIActionIndexST_M_Machine_T_Pause));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Reset));
    pMenu->addMenu(action(UIActionIndexST_M_Machine_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addMenu(action(UIActionIndexST_M_Machine_M_Tools)->menu());
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Discard));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_ShowLogDialog));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_ShowInFileManager));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_SortParent));
    pMenu->addAction(action(UIActionIndexST_M_Machine_S_Search));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Machine);
}

void UIActionPoolManager::updateMenuGroupStartOrShow()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Group_M_StartOrShow)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Group' / 'Start or Show' menu: */
    pMenu->addAction(action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Group_M_StartOrShow);
}

void UIActionPoolManager::updateMenuMachineStartOrShow()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Machine_M_StartOrShow)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Machine' / 'Start or Show' menu: */
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Machine_M_StartOrShow);
}

void UIActionPoolManager::updateMenuGroupClose()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Group_M_Close)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Group' / 'Close' menu: */
    // pMenu->addAction(action(UIActionIndexST_M_Group_M_Close_S_Detach));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Close_S_SaveState));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Close_S_Shutdown));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Close_S_PowerOff));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Group_M_Close);
}

void UIActionPoolManager::updateMenuMachineClose()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Machine_M_Close)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Machine' / 'Close' menu: */
    // pMenu->addAction(action(UIActionIndexST_M_Machine_M_Close_S_Detach));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Close_S_SaveState));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Close_S_Shutdown));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Close_S_PowerOff));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Machine_M_Close);
}

void UIActionPoolManager::updateMenuGroupTools()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Group_M_Tools)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Group' / 'Tools' menu: */
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Tools_T_Details));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Tools_T_Snapshots));
    pMenu->addAction(action(UIActionIndexST_M_Group_M_Tools_T_Logs));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Group_M_Tools);
}

void UIActionPoolManager::updateMenuMachineTools()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Machine_M_Tools)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate 'Machine' / 'Tools' menu: */
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Tools_T_Details));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Tools_T_Snapshots));
    pMenu->addAction(action(UIActionIndexST_M_Machine_M_Tools_T_Logs));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Machine_M_Tools);
}

void UIActionPoolManager::updateMenuMediumWindow()
{
    /* Update corresponding menu: */
    updateMenuMediumWrapper(action(UIActionIndexST_M_MediumWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_MediumWindow);
}

void UIActionPoolManager::updateMenuMedium()
{
    /* Update corresponding menu: */
    updateMenuMediumWrapper(action(UIActionIndexST_M_Medium)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Medium);
}

void UIActionPoolManager::updateMenuMediumWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Add' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Add)) || fSeparator;
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Create)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Copy' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Copy)) || fSeparator;
    /* 'Move' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Move)) || fSeparator;
    /* 'Remove' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Remove)) || fSeparator;
    /* 'Release' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_S_Release)) || fSeparator;
    /* 'Search' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Medium_T_Search)) || fSeparator;
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

void UIActionPoolManager::updateMenuNetworkWindow()
{
    /* Update corresponding menu: */
    updateMenuNetworkWrapper(action(UIActionIndexST_M_NetworkWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_NetworkWindow);
}

void UIActionPoolManager::updateMenuNetwork()
{
    /* Update corresponding menu: */
    updateMenuNetworkWrapper(action(UIActionIndexST_M_Network)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Network);
}

void UIActionPoolManager::updateMenuNetworkWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Create' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_S_Create)) || fSeparator;

    /* Separator? */
    if (fSeparator)
    {
        pMenu->addSeparator();
        fSeparator = false;
    }

    /* 'Remove' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Network_S_Remove)) || fSeparator;
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

void UIActionPoolManager::updateMenuCloudWindow()
{
    /* Update corresponding menu: */
    updateMenuCloudWrapper(action(UIActionIndexST_M_CloudWindow)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_CloudWindow);
}

void UIActionPoolManager::updateMenuCloud()
{
    /* Update corresponding menu: */
    updateMenuCloudWrapper(action(UIActionIndexST_M_Cloud)->menu());

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Cloud);
}

void UIActionPoolManager::updateMenuCloudWrapper(UIMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Separator? */
    bool fSeparator = false;

    /* 'Add' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Add)) || fSeparator;
    /* 'Import' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Import)) || fSeparator;

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

    /* 'Try Page' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_TryPage)) || fSeparator;
    /* 'Help' action: */
    fSeparator = addAction(pMenu, action(UIActionIndexST_M_Cloud_S_Help)) || fSeparator;
}

void UIActionPoolManager::updateMenuSnapshot()
{
    /* Get corresponding menu: */
    UIMenu *pMenu = action(UIActionIndexST_M_Snapshot)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* Populate Snapshot-menu: */
    pMenu->addAction(action(UIActionIndexST_M_Snapshot_S_Take));
    pMenu->addAction(action(UIActionIndexST_M_Snapshot_S_Delete));
    pMenu->addAction(action(UIActionIndexST_M_Snapshot_S_Restore));
    pMenu->addAction(action(UIActionIndexST_M_Snapshot_T_Properties));
    pMenu->addAction(action(UIActionIndexST_M_Snapshot_S_Clone));

    /* Mark menu as valid: */
    m_invalidations.remove(UIActionIndexST_M_Snapshot);
}

void UIActionPoolManager::updateShortcuts()
{
    /* Call to base-class: */
    UIActionPool::updateShortcuts();
    /* Create temporary Runtime UI pool to do the same: */
    if (!m_fTemporary)
        UIActionPool::createTemporary(UIActionPoolType_Runtime);
}

void UIActionPoolManager::setShortcutsVisible(int iIndex, bool fVisible)
{
    /* Prepare a list of actions: */
    QList<UIAction*> actions;

    /* Handle known menus: */
    switch (iIndex)
    {
        case UIActionIndexST_M_Welcome:
        {
            actions << action(UIActionIndexST_M_Welcome_S_New)
                    << action(UIActionIndexST_M_Welcome_S_Add);
            break;
        }
        case UIActionIndexST_M_Group:
        {
            actions << action(UIActionIndexST_M_Group_S_New)
                    << action(UIActionIndexST_M_Group_S_Add)
                    << action(UIActionIndexST_M_Group_S_Rename)
                    << action(UIActionIndexST_M_Group_S_Remove)
                    << action(UIActionIndexST_M_Group_M_StartOrShow)
                    << action(UIActionIndexST_M_Group_T_Pause)
                    << action(UIActionIndexST_M_Group_S_Reset)
                    << action(UIActionIndexST_M_Group_S_Discard)
                    << action(UIActionIndexST_M_Group_S_ShowLogDialog)
                    << action(UIActionIndexST_M_Group_S_Refresh)
                    << action(UIActionIndexST_M_Group_S_ShowInFileManager)
                    << action(UIActionIndexST_M_Group_S_CreateShortcut)
                    << action(UIActionIndexST_M_Group_S_Sort)
                    << action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal)
                    << action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless)
                    << action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable)
                    // << action(UIActionIndexST_M_Group_M_Close_S_Detach)
                    << action(UIActionIndexST_M_Group_M_Close_S_SaveState)
                    << action(UIActionIndexST_M_Group_M_Close_S_Shutdown)
                    << action(UIActionIndexST_M_Group_M_Close_S_PowerOff)
                    << action(UIActionIndexST_M_Group_M_Tools_T_Details)
                    << action(UIActionIndexST_M_Group_M_Tools_T_Snapshots)
                    << action(UIActionIndexST_M_Group_M_Tools_T_Logs);
            break;
        }
        case UIActionIndexST_M_Machine:
        {
            actions << action(UIActionIndexST_M_Machine_S_New)
                    << action(UIActionIndexST_M_Machine_S_Add)
                    << action(UIActionIndexST_M_Machine_S_Settings)
                    << action(UIActionIndexST_M_Machine_S_Clone)
                    << action(UIActionIndexST_M_Machine_S_Move)
                    << action(UIActionIndexST_M_Machine_S_ExportToOCI)
                    << action(UIActionIndexST_M_Machine_S_Remove)
                    << action(UIActionIndexST_M_Machine_S_AddGroup)
                    << action(UIActionIndexST_M_Machine_M_StartOrShow)
                    << action(UIActionIndexST_M_Machine_T_Pause)
                    << action(UIActionIndexST_M_Machine_S_Reset)
                    << action(UIActionIndexST_M_Machine_S_Discard)
                    << action(UIActionIndexST_M_Machine_S_ShowLogDialog)
                    << action(UIActionIndexST_M_Machine_S_Refresh)
                    << action(UIActionIndexST_M_Machine_S_ShowInFileManager)
                    << action(UIActionIndexST_M_Machine_S_CreateShortcut)
                    << action(UIActionIndexST_M_Machine_S_SortParent)
                    << action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal)
                    << action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless)
                    << action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable)
                    // << action(UIActionIndexST_M_Machine_M_Close_S_Detach)
                    << action(UIActionIndexST_M_Machine_M_Close_S_SaveState)
                    << action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)
                    << action(UIActionIndexST_M_Machine_M_Close_S_PowerOff)
                    << action(UIActionIndexST_M_Machine_M_Tools_T_Details)
                    << action(UIActionIndexST_M_Machine_M_Tools_T_Snapshots)
                    << action(UIActionIndexST_M_Machine_M_Tools_T_Logs);
            break;
        }
        default:
            break;
    }

    /* Update shortcut visibility: */
    foreach (UIAction *pAction, actions)
        fVisible ? pAction->showShortcut() : pAction->hideShortcut();
}

QString UIActionPoolManager::shortcutsExtraDataID() const
{
    return GUI_Input_SelectorShortcuts;
}


#include "UIActionPoolManager.moc"
