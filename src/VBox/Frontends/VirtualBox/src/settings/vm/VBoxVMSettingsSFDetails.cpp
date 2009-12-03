/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSFDetails class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "VBoxVMSettingsSFDetails.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>
#include <QPushButton>

VBoxVMSettingsSFDetails::VBoxVMSettingsSFDetails (DialogType aType,
                                                  bool aEnableSelector, /* for "permanent" checkbox */
                                                  const SFoldersNameList &aUsedNames,
                                                  QWidget *aParent /* = NULL */)
   : QIWithRetranslateUI2<QIDialog> (aParent)
   , mType (aType)
   , mUsePermanent (aEnableSelector)
   , mUsedNames (aUsedNames)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSFDetails::setupUi (this);

    mCbPermanent->setHidden (!aEnableSelector);

    /* No reset button */
    mPsPath->setResetEnabled (false);
    connect (mPsPath, SIGNAL (currentIndexChanged (int)),
             this, SLOT (onSelectPath()));

    connect (mLeName, SIGNAL (textChanged (const QString &)),
             this, SLOT (validate()));

    if (aEnableSelector)
    {
        connect (mCbPermanent, SIGNAL (toggled (bool)),
                 this, SLOT (validate()));
    }

     /* Applying language settings */
    retranslateUi();

    /* Validate the initial fields */
    validate();

    adjustSize();

#ifdef Q_WS_MAC
    setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize (minimumSize());
#endif /* Q_WS_MAC */
}

void VBoxVMSettingsSFDetails::setPath (const QString &aPath)
{
    mPsPath->setPath (aPath);
}

QString VBoxVMSettingsSFDetails::path() const
{
    return mPsPath->path();
}

void VBoxVMSettingsSFDetails::setName (const QString &aName)
{
    mLeName->setText (aName);
}

QString VBoxVMSettingsSFDetails::name() const
{
    return mLeName->text();
}

void VBoxVMSettingsSFDetails::setWriteable (bool aWritable)
{
    mCbReadonly->setChecked (!aWritable);
}

bool VBoxVMSettingsSFDetails::isWriteable() const
{
    return !mCbReadonly->isChecked();
}

void VBoxVMSettingsSFDetails::setPermanent (bool aPermanent)
{
    mCbPermanent->setChecked (aPermanent);
}

bool VBoxVMSettingsSFDetails::isPermanent() const
{
    return mUsePermanent ? mCbPermanent->isChecked() : true;
}

void VBoxVMSettingsSFDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsSFDetails::retranslateUi (this);

    switch (mType)
    {
        case AddType:
            setWindowTitle (tr ("Add Share"));
            break;
        case EditType:
            setWindowTitle (tr ("Edit Share"));
            break;
        default:
            AssertMsgFailed (("Incorrect SF Dialog type\n"));
    }
}

void VBoxVMSettingsSFDetails::validate()
{
    SFDialogType resultType =
        mCbPermanent && !mCbPermanent->isChecked() ? ConsoleType :
        mType & MachineType ? MachineType : GlobalType;
    SFolderName pair = qMakePair (mLeName->text(), resultType);

    mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (!mPsPath->path().isEmpty() &&
                                                           !mLeName->text().trimmed().isEmpty() &&
                                                           !mLeName->text().contains(" ") &&
                                                           !mUsedNames.contains (pair));
}

void VBoxVMSettingsSFDetails::onSelectPath()
{
    if (!mPsPath->isPathSelected())
        return;

    QString folderName (mPsPath->path());
#if defined (Q_OS_WIN) || defined (Q_OS_OS2)
    if (folderName[0].isLetter() && folderName[1] == ':' && folderName[2] == 0)
    {
        /* VBoxFilePathSelectorWidget returns root path as 'X:', which is invalid path.
         * Append the trailing backslash to get a valid root path 'X:\'.
         */
        folderName += "\\";
        mPsPath->setPath(folderName);
    }
#endif
    QDir folder (folderName);
    if (!folder.isRoot())
        /* Processing non-root folder */
        mLeName->setText (folder.dirName());
    else
    {
        /* Processing root folder */
#if defined (Q_OS_WIN) || defined (Q_OS_OS2)
        mLeName->setText (folderName.toUpper()[0] + "_DRIVE");
#elif defined (Q_OS_UNIX)
        mLeName->setText ("ROOT");
#endif
    }
    /* Validate the fields */
    validate();
}

