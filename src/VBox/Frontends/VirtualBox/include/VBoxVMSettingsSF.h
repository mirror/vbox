/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSF class declaration
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxVMSettingsSF_h__
#define __VBoxVMSettingsSF_h__

/* Local includes */
#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsSF.gen.h"

/* Local forwards */
class SFTreeViewItem;

enum SFDialogType
{
    WrongType   = 0x00,
    GlobalType  = 0x01,
    MachineType = 0x02,
    ConsoleType = 0x04
};
typedef QPair <QString, SFDialogType> SFolderName;
typedef QList <SFolderName> SFoldersNameList;

class VBoxVMSettingsSF : public VBoxSettingsPage, public Ui::VBoxVMSettingsSF
{
    Q_OBJECT;

public:

    VBoxVMSettingsSF (int aType = WrongType, QWidget *aParent = 0);

    void getFromGlobal();
    void getFromMachine (const CMachine &aMachine);
    void getFromConsole (const CConsole &aConsole);

    void putBackToGlobal();
    void putBackToMachine();
    void putBackToConsole();

    int dialogType() const;

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

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

    void showEvent (QShowEvent *aEvent);

    void createSharedFolder (const QString &aName, const QString &aPath, bool aWritable, SFDialogType aType);
    void removeSharedFolder (const QString &aName, const QString &aPath, SFDialogType aType);

    void getFrom (const CSharedFolderVector &aVec, SFTreeViewItem *aItem);
    void putBackTo (CSharedFolderVector &aVec, SFTreeViewItem *aItem);

    SFTreeViewItem* searchRoot (bool aIsPermanent, SFDialogType aType = WrongType);
    bool isEditable (const QString &aKey);
    SFoldersNameList usedList (bool aIncludeSelected);

    int       mDialogType;
    QAction  *mNewAction;
    QAction  *mEdtAction;
    QAction  *mDelAction;
    bool      mIsListViewChanged;
    CMachine  mMachine;
    CConsole  mConsole;
    QString   mTrFull;
    QString   mTrReadOnly;
};

#endif // __VBoxVMSettingsSF_h__
