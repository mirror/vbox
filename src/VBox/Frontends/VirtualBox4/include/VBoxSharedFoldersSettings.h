/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSharedFoldersSettings class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __VBoxSharedFoldersSettings_h__
#define __VBoxSharedFoldersSettings_h__

#include <VBoxSharedFoldersSettings.gen.h>

/* Qt includes */
#include <QDialog>

class QLineEdit;
class QPushButton;
class QCheckBox;
class SFTreeViewItem;
class QDialogButtonBox;

enum SFDialogType
{
    WrongType   = 0x00,
    GlobalType  = 0x01,
    MachineType = 0x02,
    ConsoleType = 0x04
};
typedef QPair<QString, SFDialogType> SFolderName;
typedef QList<SFolderName> SFoldersNameList;

class VBoxSharedFoldersSettings : public QWidget, public Ui::VBoxSharedFoldersSettings
{
    Q_OBJECT;

public:

    VBoxSharedFoldersSettings (QWidget *aParent = 0, int aType = WrongType);

    int dialogType() { return mDialogType; }

    void getFromGlobal();
    void getFromMachine (const CMachine &aMachine);
    void getFromConsole (const CConsole &aConsole);

    void putBackToGlobal();
    void putBackToMachine();
    void putBackToConsole();

private slots:

    void tbAddPressed();
    void tbEditPressed();
    void tbRemovePressed();

    void processCurrentChanged (QTreeWidgetItem *aCurrentItem,
                                QTreeWidgetItem *aPreviousItem = 0);
    void processDoubleClick (QTreeWidgetItem *aItem, int aColumn);

    void adjustList();
    void adjustFields();

private:

    void showEvent (QShowEvent *aEvent);

    void removeSharedFolder (const QString &aName, const QString &aPath,
                             SFDialogType aType);
    void createSharedFolder (const QString &aName, const QString &aPath,
                             bool aWritable,
                             SFDialogType aType);

    void getFrom (const CSharedFolderEnumerator &aEn, SFTreeViewItem *aItem);
    void putBackTo (CSharedFolderEnumerator &aEn, SFTreeViewItem *aItem);

    SFTreeViewItem* searchRoot (bool aIsPermanent,
                                SFDialogType aType = WrongType);
    bool isEditable (const QString &);
    SFoldersNameList usedList (bool aIncludeSelected);

    int      mDialogType;
    bool     mIsListViewChanged;
    CMachine mMachine;
    CConsole mConsole;
    QString  mTrFull;
    QString  mTrReadOnly;
};

class VBoxAddSFDialog : public QDialog
{
    Q_OBJECT;

public:

    enum DialogType
    {
        AddDialogType,
        EditDialogType
    };

    VBoxAddSFDialog (VBoxSharedFoldersSettings *aParent,
                     VBoxAddSFDialog::DialogType aType,
                     bool aEnableSelector /* for "permanent" checkbox */,
                     const SFoldersNameList &aUsedNames);
    ~VBoxAddSFDialog() {}

    QString getPath();
    QString getName();
    bool getPermanent();
    bool getWritable();

    void setPath (const QString &aPath);
    void setName (const QString &aName);
    void setPermanent (bool aPermanent);
    void setWritable (bool aWritable);

private slots:

    void validate();
    void showFileDialog();

private:

    void showEvent (QShowEvent *aEvent);

    QDialogButtonBox *mButtonBox;
    QLineEdit        *mLePath;
    QLineEdit        *mLeName;
    QCheckBox        *mCbPermanent;
    QCheckBox        *mCbReadonly;
    SFoldersNameList  mUsedNames;
};

#endif // __VBoxSharedFoldersSettings_h__

