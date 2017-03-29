/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSF class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineSettingsSF_h___
#define ___UIMachineSettingsSF_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsSF.gen.h"

/* COM includes: */
#include "CSharedFolder.h"

/* Forward declarations: */
class SFTreeViewItem;
struct UIDataSettingsSharedFolder;
struct UIDataSettingsSharedFolders;
enum UISharedFolderType { MachineType, ConsoleType };
typedef UISettingsCache<UIDataSettingsSharedFolder> UISettingsCacheSharedFolder;
typedef UISettingsCachePool<UIDataSettingsSharedFolders, UISettingsCacheSharedFolder> UISettingsCacheSharedFolders;
typedef QPair<QString, UISharedFolderType> SFolderName;
typedef QList<SFolderName> SFoldersNameList;


/** Machine settings: Shared Folders page. */
class UIMachineSettingsSF : public UISettingsPageMachine,
                            public Ui::UIMachineSettingsSF
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

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    void loadToCacheFrom(UISharedFolderType sharedFoldersType);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() /* override */;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;
    /** Saves data of @a sharedFoldersType from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(UISharedFolderType sharedFoldersType);

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
    void sltAddSharedFolder();
    /** Handles command to edit shared folder. */
    void sltEditSharedFolder();
    /** Handles command to remove shared folder. */
    void sltRemoveSharedFolder();

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
    /** Prepares shared folders tree. */
    void prepareFoldersTree();
    /** Prepares shared folders toolbar. */
    void prepareFoldersToolbar();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Returns the tree-view root item for corresponding shared folder @a type. */
    SFTreeViewItem *root(UISharedFolderType type);
    /** Returns a list of used shared folder names paired with their types. */
    SFoldersNameList usedList(bool aIncludeSelected);

    /** Returns whether the corresponding @a sharedFolderType supported. */
    bool isSharedFolderTypeSupported(UISharedFolderType sharedFolderType) const;
    /** Updates root item visibility. */
    void updateRootItemsVisibility();
    /** Defines whether the root item of @a sharedFolderType is @a fVisible. */
    void setRootItemVisible(UISharedFolderType sharedFolderType, bool fVisible);

    /** Gathers a vector of shared folders of the passed @a sharedFoldersType. */
    CSharedFolderVector getSharedFolders(UISharedFolderType sharedFoldersType);

    /** Creates shared folder defined by a @a folderCache. */
    bool createSharedFolder(const UISettingsCacheSharedFolder &folderCache);
    /** Removes shared folder defined by a @a folderCache. */
    bool removeSharedFolder(const UISettingsCacheSharedFolder &folderCache);

    /** Holds the Add action instance. */
    QAction *m_pActionAdd;
    /** Holds the Edit action instance. */
    QAction *m_pActionEdit;
    /** Holds the Remove action instance. */
    QAction *m_pActionRemove;

    /** Holds the "Full" access translation tag. */
    QString  m_strTrFull;
    /** Holds the "Read-only" access translation tag. */
    QString  m_strTrReadOnly;
    /** Holds the "Yes" for auto-mount translation tag. */
    QString  m_strTrYes;

    /** Holds the page data cache instance. */
    UISettingsCacheSharedFolders *m_pCache;
};

#endif /* !___UIMachineSettingsSF_h___ */

