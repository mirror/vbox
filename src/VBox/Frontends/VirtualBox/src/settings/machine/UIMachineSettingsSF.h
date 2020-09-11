/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSF class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSF_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSF_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* COM includes: */
#include "CSharedFolder.h"

/* Forward declarations: */
class QHBoxLayout;
class QTreeWidgetItem;
class QITreeWidget;
class QILabelSeparator;
class SFTreeViewItem;
class UIToolBar;

struct UIDataSettingsSharedFolder;
struct UIDataSettingsSharedFolders;
enum UISharedFolderType { MachineType, ConsoleType };
typedef UISettingsCache<UIDataSettingsSharedFolder> UISettingsCacheSharedFolder;
typedef UISettingsCachePool<UIDataSettingsSharedFolders, UISettingsCacheSharedFolder> UISettingsCacheSharedFolders;


/** Machine settings: Shared Folders page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsSF : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Shared Folders settings page. */
    UIMachineSettingsSF();
    /** Destructs Shared Folders settings page. */
    ~UIMachineSettingsSF();

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */;

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *aEvent) /* override */;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

private slots:

    /** Handles command to add shared folder. */
    void sltAddFolder();
    /** Handles command to edit shared folder. */
    void sltEditFolder();
    /** Handles command to remove shared folder. */
    void sltRemoveFolder();

    /** Handles @a pCurrentItem change. */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);
    /** Handles @a pItem double-click. */
    void sltHandleDoubleClick(QTreeWidgetItem *pItem);
    /** Handles context menu request for @a position. */
    void sltHandleContextMenuRequest(const QPoint &position);

    /** Performs request to adjust tree. */
    void sltAdjustTree();
    /** Performs request to adjust tree fields. */
    void sltAdjustTreeFields();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares Widgets. */
    void prepareWidgets();
    /** Prepares shared folders tree-wdget. */
    void prepareTreeWidget();
    /** Prepares shared folders toolbar. */
    void prepareToolbar();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Returns the tree-view root item for corresponding shared folder @a type. */
    SFTreeViewItem *root(UISharedFolderType type);
    /** Returns a list of used shared folder names. */
    QStringList usedList(bool fIncludeSelected);

    /** Returns whether the corresponding @a enmFoldersType supported. */
    bool isSharedFolderTypeSupported(UISharedFolderType enmFoldersType) const;
    /** Updates root item visibility. */
    void updateRootItemsVisibility();
    /** Defines whether the root item of @a enmFoldersType is @a fVisible. */
    void setRootItemVisible(UISharedFolderType enmFoldersType, bool fVisible);

    /** Creates shared folder item based on passed @a data. */
    void addSharedFolderItem(const UIDataSettingsSharedFolder &sharedFolderData, bool fChoose);

    /** Gathers a vector of shared folders of the passed @a enmFoldersType. */
    CSharedFolderVector getSharedFolders(UISharedFolderType enmFoldersType);
    /** Gathers a vector of shared folders of the passed @a enmFoldersType. */
    bool getSharedFolders(UISharedFolderType enmFoldersType, CSharedFolderVector &folders);
    /** Look for a folder with the the passed @a strFolderName. */
    bool getSharedFolder(const QString &strFolderName, const CSharedFolderVector &folders, CSharedFolder &comFolder);

    /** Saves existing folder data from the cache. */
    bool saveFoldersData();
    /** Removes shared folder defined by a @a folderCache. */
    bool removeSharedFolder(const UISettingsCacheSharedFolder &folderCache);
    /** Creates shared folder defined by a @a folderCache. */
    bool createSharedFolder(const UISettingsCacheSharedFolder &folderCache);

    /** Holds the page data cache instance. */
    UISettingsCacheSharedFolders *m_pCache;

    /** @name Widgets
      * @{ */
        /** Holds the widget separator instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the tree layout instance. */
        QHBoxLayout      *m_pLayoutTree;
        /** Holds the tree-widget instance. */
        QITreeWidget     *m_pTreeWidget;
        /** Holds the toolbar instance. */
        UIToolBar        *m_pToolbar;
        /** Holds the 'add shared folder' action instance. */
        QAction          *m_pActionAdd;
        /** Holds the 'edit shared folder' action instance. */
        QAction          *m_pActionEdit;
        /** Holds the 'remove shared folder' action instance. */
        QAction          *m_pActionRemove;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSF_h */
