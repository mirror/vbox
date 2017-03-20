/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSF class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsSF_h__
#define __UIMachineSettingsSF_h__

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsSF.gen.h"

/* COM includes: */
#include "CSharedFolder.h"

/* Forward declarations: */
class SFTreeViewItem;

enum UISharedFolderType { MachineType, ConsoleType };
typedef QPair <QString, UISharedFolderType> SFolderName;
typedef QList <SFolderName> SFoldersNameList;

/* Machine settings / Shared Folders page / Shared Folder data: */
struct UIDataSettingsSharedFolder
{
    /* Default constructor: */
    UIDataSettingsSharedFolder()
        : m_type(MachineType)
        , m_strName(QString())
        , m_strHostPath(QString())
        , m_fAutoMount(false)
        , m_fWritable(false) {}
    /* Functions: */
    bool equal(const UIDataSettingsSharedFolder &other) const
    {
        return (m_type == other.m_type) &&
               (m_strName == other.m_strName) &&
               (m_strHostPath == other.m_strHostPath) &&
               (m_fAutoMount == other.m_fAutoMount) &&
               (m_fWritable == other.m_fWritable);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsSharedFolder &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsSharedFolder &other) const { return !equal(other); }
    /* Variables: */
    UISharedFolderType m_type;
    QString m_strName;
    QString m_strHostPath;
    bool m_fAutoMount;
    bool m_fWritable;
};
typedef UISettingsCache<UIDataSettingsSharedFolder> UISettingsCacheSharedFolder;

/* Machine settings / Shared Folders page / Shared Folders data: */
struct UIDataSettingsSharedFolders
{
    /* Default constructor: */
    UIDataSettingsSharedFolders() {}
    /* Operators: */
    bool operator==(const UIDataSettingsSharedFolders& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsSharedFolders& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsSharedFolders, UISettingsCacheSharedFolder> UISettingsCacheSharedFolders;

class UIMachineSettingsSF : public UISettingsPageMachine,
                         public Ui::UIMachineSettingsSF
{
    Q_OBJECT;

public:

    UIMachineSettingsSF();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    void loadToCacheFrom(UISharedFolderType sharedFoldersType);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);
    /** Saves data of @a sharedFoldersType from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(UISharedFolderType sharedFoldersType);

    /** Returns whether the page content was changed. */
    bool changed() const { return m_cache.wasChanged(); }

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
    void retranslateUi();

private slots:

    void addTriggered();
    void edtTriggered();
    void delTriggered();

    void processCurrentChanged (QTreeWidgetItem *aCurrentItem);
    void processDoubleClick (QTreeWidgetItem *aItem);
    void showContextMenu (const QPoint &aPos);

    void adjustList();
    void adjustFields();

private:

    void resizeEvent (QResizeEvent *aEvent);

    void showEvent (QShowEvent *aEvent);

    SFTreeViewItem* root(UISharedFolderType type);
    SFoldersNameList usedList (bool aIncludeSelected);

    bool isSharedFolderTypeSupported(UISharedFolderType sharedFolderType) const;
    void updateRootItemsVisibility();
    void setRootItemVisible(UISharedFolderType sharedFolderType, bool fVisible);

    void polishPage();

    CSharedFolderVector getSharedFolders(UISharedFolderType sharedFoldersType);

    bool removeSharedFolder(const UISettingsCacheSharedFolder &folderCache);
    bool createSharedFolder(const UISettingsCacheSharedFolder &folderCache);

    QAction  *mNewAction;
    QAction  *mEdtAction;
    QAction  *mDelAction;
    QString   mTrFull;
    QString   mTrReadOnly;
    QString   mTrYes;

    /* Cache: */
    UISettingsCacheSharedFolders m_cache;
};

#endif // __UIMachineSettingsSF_h__

