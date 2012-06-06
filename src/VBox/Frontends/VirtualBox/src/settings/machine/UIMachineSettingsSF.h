/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSF class declaration
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
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
typedef UISettingsCache<UIDataSettingsSharedFolder> UICacheSettingsSharedFolder;

/* Machine settings / Shared Folders page / Shared Folders data: */
struct UIDataSettingsSharedFolders
{
    /* Default constructor: */
    UIDataSettingsSharedFolders() {}
    /* Operators: */
    bool operator==(const UIDataSettingsSharedFolders& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsSharedFolders& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsSharedFolders, UICacheSettingsSharedFolder> UICacheSettingsSharedFolders;

class UIMachineSettingsSF : public UISettingsPageMachine,
                         public Ui::UIMachineSettingsSF
{
    Q_OBJECT;

public:

    UIMachineSettingsSF();

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    void loadToCacheFrom(UISharedFolderType sharedFoldersType);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);
    void saveFromCacheTo(UISharedFolderType sharedFoldersType);

    /* Page changed: */
    bool changed() const { return m_cache.wasChanged(); }

    void setOrderAfter (QWidget *aWidget);

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

    bool removeSharedFolder(const UICacheSettingsSharedFolder &folderCache);
    bool createSharedFolder(const UICacheSettingsSharedFolder &folderCache);

    QAction  *mNewAction;
    QAction  *mEdtAction;
    QAction  *mDelAction;
    QString   mTrFull;
    QString   mTrReadOnly;
    QString   mTrYes;

    /* Cache: */
    UICacheSettingsSharedFolders m_cache;
};

#endif // __UIMachineSettingsSF_h__

