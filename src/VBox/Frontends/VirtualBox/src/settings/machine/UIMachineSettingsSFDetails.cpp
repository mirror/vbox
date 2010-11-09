/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSFDetails class implementation
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

#include "UIMachineSettingsSFDetails.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>
#include <QPushButton>

UIMachineSettingsSFDetails::UIMachineSettingsSFDetails (DialogType aType,
                                                  bool aEnableSelector, /* for "permanent" checkbox */
                                                  const SFoldersNameList &aUsedNames,
                                                  QWidget *aParent /* = NULL */)
   : QIWithRetranslateUI2<QIDialog> (aParent)
   , mType (aType)
   , mUsePermanent (aEnableSelector)
   , mUsedNames (aUsedNames)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsSFDetails::setupUi (this);

    mCbPermanent->setHidden (!aEnableSelector);

    /* No reset button */
    mPsPath->setResetEnabled (false);

    /* Setup connections: */
    connect (mPsPath, SIGNAL (currentIndexChanged (int)),
             this, SLOT (onSelectPath()));
    connect (mPsPath, SIGNAL (pathChanged (const QString &)),
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

void UIMachineSettingsSFDetails::setPath (const QString &aPath)
{
    mPsPath->setPath (aPath);
}

QString UIMachineSettingsSFDetails::path() const
{
    return mPsPath->path();
}

void UIMachineSettingsSFDetails::setName (const QString &aName)
{
    mLeName->setText (aName);
}

QString UIMachineSettingsSFDetails::name() const
{
    return mLeName->text();
}

void UIMachineSettingsSFDetails::setWriteable (bool aWritable)
{
    mCbReadonly->setChecked (!aWritable);
}

bool UIMachineSettingsSFDetails::isWriteable() const
{
    return !mCbReadonly->isChecked();
}

void UIMachineSettingsSFDetails::setAutoMount (bool aAutoMount)
{
    mCbAutoMount->setChecked (aAutoMount);
}

bool UIMachineSettingsSFDetails::isAutoMounted() const
{
    return mCbAutoMount->isChecked();
}

void UIMachineSettingsSFDetails::setPermanent (bool aPermanent)
{
    mCbPermanent->setChecked (aPermanent);
}

bool UIMachineSettingsSFDetails::isPermanent() const
{
    return mUsePermanent ? mCbPermanent->isChecked() : true;
}

void UIMachineSettingsSFDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsSFDetails::retranslateUi (this);

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

void UIMachineSettingsSFDetails::validate()
{
    UISharedFolderType resultType =
        mUsePermanent && !mCbPermanent->isChecked() ? ConsoleType : MachineType;
    SFolderName pair = qMakePair (mLeName->text(), resultType);

    mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (!mPsPath->path().isEmpty() &&
                                                           QDir(mPsPath->path()).exists() &&
                                                           !mLeName->text().trimmed().isEmpty() &&
                                                           !mLeName->text().contains(" ") &&
                                                           !mUsedNames.contains (pair));
}

void UIMachineSettingsSFDetails::onSelectPath()
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
        mLeName->setText (folder.dirName().replace(' ', '_'));
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

