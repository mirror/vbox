/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsGeneral class implementation
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

#include "VBoxGLSettingsGeneral.h"
#include "VBoxGlobal.h"

#include <QDir>

VBoxGLSettingsGeneral::VBoxGLSettingsGeneral()
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsGeneral::setupUi (this);

    /* Setup connections */
    connect (mTbVdiSelect, SIGNAL (clicked()), this, SLOT (onSelectFolderClicked()));
    connect (mTbMachSelect, SIGNAL (clicked()), this, SLOT (onSelectFolderClicked()));
    connect (mTbVRDPSelect, SIGNAL (clicked()), this, SLOT (onSelectFolderClicked()));
    connect (mTbVdiReset, SIGNAL (clicked()), this, SLOT (onResetFolderClicked()));
    connect (mTbMachReset, SIGNAL (clicked()), this, SLOT (onResetFolderClicked()));
    connect (mTbVRDPReset, SIGNAL (clicked()), this, SLOT (onResetFolderClicked()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsGeneral::getFrom (const CSystemProperties &aProps,
                                         const VBoxGlobalSettings &)
{
    mLeVdi->setText (aProps.GetDefaultVDIFolder());
    mLeMach->setText (aProps.GetDefaultMachineFolder());
    mLeVRDP->setText (aProps.GetRemoteDisplayAuthLibrary());
}

void VBoxGLSettingsGeneral::putBackTo (CSystemProperties &aProps,
                                           VBoxGlobalSettings &)
{
    if (mLeVdi->isModified())
        aProps.SetDefaultVDIFolder (mLeVdi->text());
    if (aProps.isOk() && mLeMach->isModified())
        aProps.SetDefaultMachineFolder (mLeMach->text());
    if (mLeVRDP->isModified())
        aProps.SetRemoteDisplayAuthLibrary (mLeVRDP->text());
}

void VBoxGLSettingsGeneral::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mLeVdi);
    setTabOrder (mLeVdi, mTbVdiSelect);
    setTabOrder (mTbVdiSelect, mTbVdiReset);
    setTabOrder (mTbVdiReset, mLeMach);
    setTabOrder (mLeMach, mTbMachSelect);
    setTabOrder (mTbMachSelect, mTbMachReset);
    setTabOrder (mTbMachReset, mLeVRDP);
    setTabOrder (mLeVRDP, mTbVRDPSelect);
    setTabOrder (mTbVRDPSelect, mTbVRDPReset);
}

void VBoxGLSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsGeneral::retranslateUi (this);
}

void VBoxGLSettingsGeneral::onResetFolderClicked()
{
    QToolButton *tb = qobject_cast<QToolButton*> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == mTbVdiReset) le = mLeVdi;
    else if (tb == mTbMachReset) le = mLeMach;
    else if (tb == mTbVRDPReset) le = mLeVRDP;
    Assert (le);

    /* Do this instead of le->setText (QString::null) to cause
     * isModified() return true */
    le->selectAll();
    le->del();
}

void VBoxGLSettingsGeneral::onSelectFolderClicked()
{
    QToolButton *tb = qobject_cast<QToolButton*> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == mTbVdiSelect) le = mLeVdi;
    else if (tb == mTbMachSelect) le = mLeMach;
    else if (tb == mTbVRDPSelect) le = mLeVRDP;
    Assert (le);

    QString initDir = VBoxGlobal::getFirstExistingDir (le->text());
    if (initDir.isNull())
        initDir = vboxGlobal().virtualBox().GetHomeFolder();

    QString path = le == mLeVRDP ?
        VBoxGlobal::getOpenFileName (initDir, QString::null, this, QString::null) :
        VBoxGlobal::getExistingDirectory (initDir, this);
    if (path.isNull())
        return;

    path = QDir::convertSeparators (path);
    /* remove trailing slash if any */
    path.remove (QRegExp ("[\\\\/]$"));

    /* Do this instead of le->setText (QString::null) to cause
     * isModified() return true */
    le->selectAll();
    le->insert (path);
}

