/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGlobalSettingsGeneral class implementation
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

#include "VBoxGlobalSettingsGeneral.h"
#include "VBoxGlobalSettingsDlg.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>

VBoxGlobalSettingsGeneral* VBoxGlobalSettingsGeneral::mSettings = 0;

void VBoxGlobalSettingsGeneral::getFrom (const CSystemProperties &aProps,
                                         const VBoxGlobalSettings &aGs,
                                         QWidget *aPage,
                                         VBoxGlobalSettingsDlg *aDlg)
{
    /* Create and load General page */
    mSettings = new VBoxGlobalSettingsGeneral (aPage);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    mSettings->load (aProps, aGs);

    /* Fix tab order */
    setTabOrder (aDlg->mTwSelector, mSettings->mLeVdi);
    setTabOrder (mSettings->mLeVdi, mSettings->mTbVdiSelect);
    setTabOrder (mSettings->mTbVdiSelect, mSettings->mTbVdiReset);
    setTabOrder (mSettings->mTbVdiReset, mSettings->mLeMach);
    setTabOrder (mSettings->mLeMach, mSettings->mTbMachSelect);
    setTabOrder (mSettings->mTbMachSelect, mSettings->mTbMachReset);
    setTabOrder (mSettings->mTbMachReset, mSettings->mLeVRDP);
    setTabOrder (mSettings->mLeVRDP, mSettings->mTbVRDPSelect);
    setTabOrder (mSettings->mTbVRDPSelect, mSettings->mTbVRDPReset);
    setTabOrder (mSettings->mTbVRDPReset, mSettings->mCbVirt);
}

void VBoxGlobalSettingsGeneral::putBackTo (CSystemProperties &aProps,
                                           VBoxGlobalSettings &aGs)
{
    mSettings->save (aProps, aGs);
}


VBoxGlobalSettingsGeneral::VBoxGlobalSettingsGeneral (QWidget *aParent)
    : QIWithRetranslateUI<QWidget> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxGlobalSettingsGeneral::setupUi (this);

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

void VBoxGlobalSettingsGeneral::load (const CSystemProperties &aProps,
                                      const VBoxGlobalSettings &)
{
    mLeVdi->setText (aProps.GetDefaultVDIFolder());
    mLeMach->setText (aProps.GetDefaultMachineFolder());
    mLeVRDP->setText (aProps.GetRemoteDisplayAuthLibrary());
    mCbVirt->setChecked (aProps.GetHWVirtExEnabled());
}

void VBoxGlobalSettingsGeneral::save (CSystemProperties &aProps,
                                      VBoxGlobalSettings &)
{
    if (mLeVdi->isModified())
        aProps.SetDefaultVDIFolder (mLeVdi->text());
    if (aProps.isOk() && mLeMach->isModified())
        aProps.SetDefaultMachineFolder (mLeMach->text());
    if (mLeVRDP->isModified())
        aProps.SetRemoteDisplayAuthLibrary (mLeVRDP->text());
    aProps.SetHWVirtExEnabled (mCbVirt->isChecked());
}

void VBoxGlobalSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGlobalSettingsGeneral::retranslateUi (this);
}

void VBoxGlobalSettingsGeneral::onResetFolderClicked()
{
    QToolButton *tb = qobject_cast<QToolButton*> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == mTbVdiReset) le = mLeVdi;
    else if (tb == mTbMachReset) le = mLeMach;
    else if (tb == mTbVRDPReset) le = mLeVRDP;
    Assert (le);

    /*
     *  do this instead of le->setText (QString::null) to cause
     *  isModified() return true
     */
    le->selectAll();
    le->del();
}

void VBoxGlobalSettingsGeneral::onSelectFolderClicked()
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

    /*
     *  do this instead of le->setText (path) to cause
     *  isModified() return true
     */
    le->selectAll();
    le->insert (path);
}

