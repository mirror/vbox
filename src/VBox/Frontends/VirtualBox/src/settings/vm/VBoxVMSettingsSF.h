/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSF class declaration
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxVMSettingsSF_h__
#define __VBoxVMSettingsSF_h__

/* Local includes */
#include "UISettingsPage.h"
#include "VBoxVMSettingsSF.gen.h"

/* Local forwards */
class SFTreeViewItem;

enum UISharedFolderType
{
    WrongType   = 0x00,
    MachineType = 0x01,
    ConsoleType = 0x02
};
typedef QPair <QString, UISharedFolderType> SFolderName;
typedef QList <SFolderName> SFoldersNameList;

/* Machine settings / Shared Folders page / Folder data: */
struct UISharedFolderData
{
    UISharedFolderType m_type;
    QString m_strName;
    QString m_strHostPath;
    bool m_fAutoMount;
    bool m_fWritable;
    bool m_fEdited;
};

/* Machine settings / Shared Folders page / Cache: */
struct UISettingsCacheMachineSFolders
{
    QList<UISharedFolderData> m_items;
};

class VBoxVMSettingsSF : public UISettingsPageMachine,
                         public Ui::VBoxVMSettingsSF
{
    Q_OBJECT;

public:

    VBoxVMSettingsSF();

    void loadDirectlyFrom(const CConsole &console);
    void saveDirectlyTo(CConsole &console);

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    void loadToCacheFromMachine(const CMachine &machine);
    void loadToCacheFromConsole(const CConsole &console);
    void loadToCacheFromVector(const CSharedFolderVector &vector, UISharedFolderType type);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);
    void saveFromCacheToMachine(CMachine &machine);
    void saveFromCacheToConsole(CConsole &console);

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

    UISharedFolderType m_type;

    QAction  *mNewAction;
    QAction  *mEdtAction;
    QAction  *mDelAction;
    bool      mIsListViewChanged;
    QString   mTrFull;
    QString   mTrReadOnly;
    QString   mTrYes;

    /* Cache: */
    UISettingsCacheMachineSFolders m_cache;
};

#endif // __VBoxVMSettingsSF_h__

