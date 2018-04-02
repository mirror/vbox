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
class UIActionSimpleSelectorShowVirtualMediaManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorShowVirtualMediaManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/diskimage_16px.png")
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
class UIActionSimpleSelectorShowHostNetworkManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorShowHostNetworkManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/host_iface_manager_16px.png")
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

/** Simple action extension, used as 'Show Import Appliance Wizard' action class. */
class UIActionSimpleSelectorShowImportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorShowImportApplianceWizard(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/import_16px.png")
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
        setName(QApplication::translate("UIActionPool", "&Import Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Import an appliance into VirtualBox"));
    }
};

/** Simple action extension, used as 'Show Export Appliance Wizard' action class. */
class UIActionSimpleSelectorShowExportApplianceWizard : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorShowExportApplianceWizard(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/export_16px.png")
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
        setName(QApplication::translate("UIActionPool", "&Export Appliance..."));
        setStatusTip(QApplication::translate("UIActionPool", "Export one or more VirtualBox virtual machines as an appliance"));
    }
};

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
/** Simple action extension, used as 'Show Extra-data Manager' action class. */
class UIActionSimpleSelectorShowExtraDataManager : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorShowExtraDataManager(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/edataman_16px.png")
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
class UIActionSimpleSelectorPerformExit : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorPerformExit(UIActionPool *pParent)
        : UIActionSimple(pParent, ":/exit_16px.png")
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
        : UIActionSimple(pParent, ":/vm_add_16px.png")
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
        : UIActionSimple(pParent, ":/vm_add_16px.png")
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
        : UIActionSimple(pParent, ":/vm_start_16px.png")
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
        : UIActionSimple(pParent, ":/vm_start_headless_16px.png")
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
        : UIActionSimple(pParent, ":/vm_start_separate_16px.png")
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


/** Toggle action extension, used as 'Machine Tools' action class. */
class UIActionToggleSelectorToolsMachine : public UIActionToggle
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionToggleSelectorToolsMachine(UIActionPool *pParent)
        : UIActionToggle(pParent, ":/tools_machine_32px.png")
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
class UIActionSimpleSelectorToolsShowMachineDetails : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsShowMachineDetails(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/machine_details_manager_22px.png", ":/machine_details_manager_16px.png",
                         ":/machine_details_manager_22px.png", ":/machine_details_manager_16px.png")
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
class UIActionSimpleSelectorToolsShowMachineSnapshots : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsShowMachineSnapshots(UIActionPool *pParent)
        : UIActionSimple(pParent,
                         ":/snapshot_manager_22px.png", ":/snapshot_manager_16px.png",
                         ":/snapshot_manager_22px.png", ":/snapshot_manager_16px.png")
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
class UIActionSimpleSelectorToolsShowMachineLogs : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorToolsShowMachineLogs(UIActionPool *pParent)
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
        : UIActionToggle(pParent, ":/tools_global_32px.png")
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
                         ":/diskimage_22px.png", ":/diskimage_16px.png",
                         ":/diskimage_22px.png", ":/diskimage_16px.png")
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
                         ":/host_iface_manager_22px.png", ":/host_iface_manager_16px.png",
                         ":/host_iface_manager_22px.png", ":/host_iface_manager_16px.png")
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
class UIActionSimpleSelectorPerformDetach : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorPerformDetach(UIActionPool *pParent)
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
class UIActionSimpleSelectorPerformSave : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorPerformSave(UIActionPool *pParent)
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
class UIActionSimpleSelectorPerformShutdown : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorPerformShutdown(UIActionPool *pParent)
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
class UIActionSimpleSelectorPerformPowerOff : public UIActionSimple
{
    Q_OBJECT;

public:

    /** Constructs action passing @a pParent to the base-class. */
    UIActionSimpleSelectorPerformPowerOff(UIActionPool *pParent)
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
    m_pool[UIActionIndexST_M_File_S_ShowVirtualMediumManager] = new UIActionSimpleSelectorShowVirtualMediaManager(this);
    m_pool[UIActionIndexST_M_File_S_ShowHostNetworkManager] = new UIActionSimpleSelectorShowHostNetworkManager(this);
    m_pool[UIActionIndexST_M_File_S_ImportAppliance] = new UIActionSimpleSelectorShowImportApplianceWizard(this);
    m_pool[UIActionIndexST_M_File_S_ExportAppliance] = new UIActionSimpleSelectorShowExportApplianceWizard(this);
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    m_pool[UIActionIndexST_M_File_S_ShowExtraDataManager] = new UIActionSimpleSelectorShowExtraDataManager(this);
#endif
    m_pool[UIActionIndexST_M_File_S_Close] = new UIActionSimpleSelectorPerformExit(this);

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
    m_pool[UIActionIndexST_M_Group_M_Close_S_Detach] = new UIActionSimpleSelectorPerformDetach(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_SaveState] = new UIActionSimpleSelectorPerformSave(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_Shutdown] = new UIActionSimpleSelectorPerformShutdown(this);
    m_pool[UIActionIndexST_M_Group_M_Close_S_PowerOff] = new UIActionSimpleSelectorPerformPowerOff(this);
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
    m_pool[UIActionIndexST_M_Machine_S_Remove] = new UIActionSimpleSelectorMachinePerformRemove(this);
    m_pool[UIActionIndexST_M_Machine_S_AddGroup] = new UIActionSimpleSelectorMachinePerformGroup(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow] = new UIActionStateSelectorCommonStartOrShow(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal] = new UIActionSimpleSelectorCommonPerformStartNormal(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless] = new UIActionSimpleSelectorCommonPerformStartHeadless(this);
    m_pool[UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable] = new UIActionSimpleSelectorCommonPerformStartDetachable(this);
    m_pool[UIActionIndexST_M_Machine_T_Pause] = new UIActionToggleSelectorCommonPauseAndResume(this);
    m_pool[UIActionIndexST_M_Machine_S_Reset] = new UIActionSimpleSelectorCommonPerformReset(this);
    m_pool[UIActionIndexST_M_Machine_M_Close] = new UIActionMenuSelectorClose(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_Detach] = new UIActionSimpleSelectorPerformDetach(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_SaveState] = new UIActionSimpleSelectorPerformSave(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_Shutdown] = new UIActionSimpleSelectorPerformShutdown(this);
    m_pool[UIActionIndexST_M_Machine_M_Close_S_PowerOff] = new UIActionSimpleSelectorPerformPowerOff(this);
    m_pool[UIActionIndexST_M_Machine_S_Discard] = new UIActionSimpleSelectorCommonPerformDiscard(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowLogDialog] = new UIActionSimpleSelectorCommonShowMachineLogs(this);
    m_pool[UIActionIndexST_M_Machine_S_Refresh] = new UIActionSimpleSelectorCommonPerformRefresh(this);
    m_pool[UIActionIndexST_M_Machine_S_ShowInFileManager] = new UIActionSimpleSelectorCommonShowInFileManager(this);
    m_pool[UIActionIndexST_M_Machine_S_CreateShortcut] = new UIActionSimpleSelectorCommonPerformCreateShortcut(this);
    m_pool[UIActionIndexST_M_Machine_S_SortParent] = new UIActionSimpleSelectorMachinePerformSortParent(this);

    /* Machine Tools actions: */
    m_pool[UIActionIndexST_M_Tools_T_Machine] = new UIActionToggleSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine] = new UIActionMenuSelectorToolsMachine(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_Details] = new UIActionSimpleSelectorToolsShowMachineDetails(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_Snapshots] = new UIActionSimpleSelectorToolsShowMachineSnapshots(this);
    m_pool[UIActionIndexST_M_Tools_M_Machine_S_LogViewer] = new UIActionSimpleSelectorToolsShowMachineLogs(this);

    /* Global Tools actions: */
    m_pool[UIActionIndexST_M_Tools_T_Global] = new UIActionToggleSelectorToolsGlobal(this);
    m_pool[UIActionIndexST_M_Tools_M_Global] = new UIActionMenuSelectorToolsGlobal(this);
    m_pool[UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager] = new UIActionSimpleSelectorToolsGlobalShowVirtualMediaManager(this);
    m_pool[UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager] = new UIActionSimpleSelectorToolsGlobalShowHostNetworkManager(this);

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

