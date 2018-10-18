/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPoolSelector class declaration.
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

#ifndef ___UIActionPoolSelector_h___
#define ___UIActionPoolSelector_h___

/* GUI includes: */
#include "UIActionPool.h"
#include "UILibraryDefs.h"


/** Runtime action-pool index enum.
  * Naming convention is following:
  * 1. Every menu index prepended with 'M',
  * 2. Every simple-action index prepended with 'S',
  * 3. Every toggle-action index presended with 'T',
  * 4. Every polymorphic-action index presended with 'P',
  * 5. Every sub-index contains full parent-index name. */
enum UIActionIndexST
{
    /* 'File' menu actions: */
    UIActionIndexST_M_File = UIActionIndex_Max + 1,
    UIActionIndexST_M_File_S_ShowVirtualMediumManager,
    UIActionIndexST_M_File_S_ShowHostNetworkManager,
    UIActionIndexST_M_File_S_ShowCloudProfileManager,
    UIActionIndexST_M_File_S_ImportAppliance,
    UIActionIndexST_M_File_S_ExportAppliance,
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    UIActionIndexST_M_File_S_ShowExtraDataManager,
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    UIActionIndexST_M_File_S_Close,

    /* 'Group' menu actions: */
    UIActionIndexST_M_Group,
    UIActionIndexST_M_Group_S_New,
    UIActionIndexST_M_Group_S_Add,
    UIActionIndexST_M_Group_S_Rename,
    UIActionIndexST_M_Group_S_Remove,
    UIActionIndexST_M_Group_M_StartOrShow,
    UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal,
    UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless,
    UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable,
    UIActionIndexST_M_Group_T_Pause,
    UIActionIndexST_M_Group_S_Reset,
    UIActionIndexST_M_Group_M_Close,
    UIActionIndexST_M_Group_M_Close_S_Detach,
    UIActionIndexST_M_Group_M_Close_S_SaveState,
    UIActionIndexST_M_Group_M_Close_S_Shutdown,
    UIActionIndexST_M_Group_M_Close_S_PowerOff,
    UIActionIndexST_M_Group_S_Discard,
    UIActionIndexST_M_Group_S_ShowLogDialog,
    UIActionIndexST_M_Group_S_Refresh,
    UIActionIndexST_M_Group_S_ShowInFileManager,
    UIActionIndexST_M_Group_S_CreateShortcut,
    UIActionIndexST_M_Group_S_Sort,

    /* 'Machine' menu actions: */
    UIActionIndexST_M_Machine,
    UIActionIndexST_M_Machine_S_New,
    UIActionIndexST_M_Machine_S_Add,
    UIActionIndexST_M_Machine_S_Settings,
    UIActionIndexST_M_Machine_S_Clone,
    UIActionIndexST_M_Machine_S_Move,
    UIActionIndexST_M_Machine_S_Remove,
    UIActionIndexST_M_Machine_S_AddGroup,
    UIActionIndexST_M_Machine_M_StartOrShow,
    UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal,
    UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless,
    UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable,
    UIActionIndexST_M_Machine_T_Pause,
    UIActionIndexST_M_Machine_S_Reset,
    UIActionIndexST_M_Machine_M_Close,
    UIActionIndexST_M_Machine_M_Close_S_Detach,
    UIActionIndexST_M_Machine_M_Close_S_SaveState,
    UIActionIndexST_M_Machine_M_Close_S_Shutdown,
    UIActionIndexST_M_Machine_M_Close_S_PowerOff,
    UIActionIndexST_M_Machine_S_Discard,
    UIActionIndexST_M_Machine_S_ShowLogDialog,
    UIActionIndexST_M_Machine_S_Refresh,
    UIActionIndexST_M_Machine_S_ShowInFileManager,
    UIActionIndexST_M_Machine_S_CreateShortcut,
    UIActionIndexST_M_Machine_S_SortParent,

    /* Machine Tools actions: */
    UIActionIndexST_M_Tools_T_Machine,
    UIActionIndexST_M_Tools_M_Machine,
    UIActionIndexST_M_Tools_M_Machine_S_Details,
    UIActionIndexST_M_Tools_M_Machine_S_Snapshots,
    UIActionIndexST_M_Tools_M_Machine_S_LogViewer,

    /* Global Tools actions: */
    UIActionIndexST_M_Tools_T_Global,
    UIActionIndexST_M_Tools_M_Global,
    UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager,
    UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager,
    UIActionIndexST_M_Tools_M_Global_S_CloudProfileManager,

    /* Snapshot Pane actions: */
    UIActionIndexST_M_Snapshot,
    UIActionIndexST_M_Snapshot_S_Take,
    UIActionIndexST_M_Snapshot_S_Delete,
    UIActionIndexST_M_Snapshot_S_Restore,
    UIActionIndexST_M_Snapshot_T_Properties,
    UIActionIndexST_M_Snapshot_S_Clone,

    /* Virtual Media Manager actions: */
    UIActionIndexST_M_MediumWindow,
    UIActionIndexST_M_Medium,
    UIActionIndexST_M_Medium_S_Add,
    UIActionIndexST_M_Medium_S_Copy,
    UIActionIndexST_M_Medium_S_Move,
    UIActionIndexST_M_Medium_S_Remove,
    UIActionIndexST_M_Medium_S_Release,
    UIActionIndexST_M_Medium_T_Details,
    UIActionIndexST_M_Medium_S_Refresh,

    /* Host Network Manager actions: */
    UIActionIndexST_M_NetworkWindow,
    UIActionIndexST_M_Network,
    UIActionIndexST_M_Network_S_Create,
    UIActionIndexST_M_Network_S_Remove,
    UIActionIndexST_M_Network_T_Details,
    UIActionIndexST_M_Network_S_Refresh,

    /* Host Cloud Profile actions: */
    UIActionIndexST_M_CloudWindow,
    UIActionIndexST_M_Cloud,
    UIActionIndexST_M_Cloud_S_Add,
    UIActionIndexST_M_Cloud_S_Remove,
    UIActionIndexST_M_Cloud_T_Details,
    UIActionIndexST_M_Cloud_S_Refresh,

    /* Maximum index: */
    UIActionIndexST_Max
};


/** UIActionPool extension
  * representing action-pool singleton for Selector UI. */
class SHARED_LIBRARY_STUFF UIActionPoolSelector : public UIActionPool
{
    Q_OBJECT;

protected:

    /** Constructs action-pool.
      * @param  fTemporary  Brings whether this action-pool is temporary,
      *                     used to (re-)initialize shortcuts-pool. */
    UIActionPoolSelector(bool fTemporary = false);

    /** Prepares pool. */
    virtual void preparePool() /* override */;
    /** Prepares connections. */
    virtual void prepareConnections() /* override */;

    /** Updates menus. */
    virtual void updateMenus() /* override */;

    /** Updates 'Medium' window menu. */
    void updateMenuMediumWindow();
    /** Updates 'Medium' menu. */
    void updateMenuMedium();
    /** Updates 'Medium' @a pMenu. */
    void updateMenuMediumWrapper(UIMenu *pMenu);

    /** Updates 'Network' window menu. */
    void updateMenuNetworkWindow();
    /** Updates 'Network' menu. */
    void updateMenuNetwork();
    /** Updates 'Network' @a pMenu. */
    void updateMenuNetworkWrapper(UIMenu *pMenu);

    /** Updates 'Cloud' window menu. */
    void updateMenuCloudWindow();
    /** Updates 'Cloud' menu. */
    void updateMenuCloud();
    /** Updates 'Cloud' @a pMenu. */
    void updateMenuCloudWrapper(UIMenu *pMenu);

    /** Updates shortcuts. */
    virtual void updateShortcuts() /* override */;

    /** Returns extra-data ID to save keyboard shortcuts under. */
    virtual QString shortcutsExtraDataID() const /* override */;

    /** Returns the list of Selector UI main menus. */
    virtual QList<QMenu*> menus() const /* override */ { return QList<QMenu*>(); }

private:

    /** Enables factory in base-class. */
    friend class UIActionPool;
};


#endif /* !___UIActionPoolSelector_h___ */

