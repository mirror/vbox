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

    mPsVRDP->setMode (VBoxFilePathSelectorWidget::FileMode);

    /* Setup connections */
    connect (mPsVdi,  SIGNAL (selectPath()), this, SLOT (onSelectFolderClicked()));
    connect (mPsMach, SIGNAL (selectPath()), this, SLOT (onSelectFolderClicked()));
    connect (mPsVRDP, SIGNAL (selectPath()), this, SLOT (onSelectFolderClicked()));
    connect (mPsVdi,  SIGNAL (resetPath()), this, SLOT (onResetFolderClicked()));
    connect (mPsMach, SIGNAL (resetPath()), this, SLOT (onResetFolderClicked()));
    connect (mPsVRDP, SIGNAL (resetPath()), this, SLOT (onResetFolderClicked()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsGeneral::getFrom (const CSystemProperties &aProps,
                                         const VBoxGlobalSettings &)
{
    mPsVdi->setPath (aProps.GetDefaultVDIFolder());
    mPsMach->setPath (aProps.GetDefaultMachineFolder());
    mPsVRDP->setPath (aProps.GetRemoteDisplayAuthLibrary());
}

void VBoxGLSettingsGeneral::putBackTo (CSystemProperties &aProps,
                                           VBoxGlobalSettings &)
{
    if (mPsVdi->isModified())
        aProps.SetDefaultVDIFolder (mPsVdi->path());
    if (aProps.isOk() && mPsMach->isModified())
        aProps.SetDefaultMachineFolder (mPsMach->path());
    if (mPsVdi->isModified())
        aProps.SetRemoteDisplayAuthLibrary (mPsVRDP->path());
}

void VBoxGLSettingsGeneral::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mPsVdi);
    setTabOrder (mPsVdi, mPsMach);
    setTabOrder (mPsMach, mPsVRDP);
}

void VBoxGLSettingsGeneral::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsGeneral::retranslateUi (this);

    mPsVdi->setPathWhatsThis (tr ("Displays the path to the default VDI folder. This folder is used, if not explicitly specified otherwise, when adding existing or creating new virtual hard disks."));
    mPsVdi->setSelectorWhatsThis (tr ("Opens a dialog to select the default VDI folder."));
    mPsVdi->setResetWhatsThis (tr ("Resets the VDI folder path to the default value. The actual default path will be displayed after accepting the changes and opening this dialog again."));

    mPsMach->setPathWhatsThis (tr ("Displays the path to the default virtual machine folder. This folder is used, if not explicitly specified otherwise, when creating new virtual machines."));
    mPsMach->setSelectorWhatsThis (tr ("Opens a dialog to select the default virtual machine folder."));
    mPsMach->setResetWhatsThis (tr ("Resets the virtual machine folder path to the default value. The actual default path will be displayed after accepting the changes and opening this dialog again."));

    mPsVRDP->setPathWhatsThis (tr ("Displays the path to the library that provides authentication for Remote Display (VRDP) clients."));
    mPsVRDP->setSelectorWhatsThis (tr ("Opens a dialog to select the VRDP authentication library file."));
    mPsVRDP->setResetWhatsThis (tr ("Resets the authentication library file to the default value. The actual default library file will be displayed after accepting the changes and opening this dialog again."));
}

void VBoxGLSettingsGeneral::onResetFolderClicked()
{
    VBoxFilePathSelectorWidget *ps = qobject_cast<VBoxFilePathSelectorWidget*> (sender());
    Assert (ps);
    ps->setPath ("");
}

void VBoxGLSettingsGeneral::onSelectFolderClicked()
{
    VBoxFilePathSelectorWidget *ps = qobject_cast<VBoxFilePathSelectorWidget*> (sender());
    Assert (ps);

    QString initDir = VBoxGlobal::getFirstExistingDir (ps->path());
    if (initDir.isNull())
        initDir = vboxGlobal().virtualBox().GetHomeFolder();


    QString path = ps == mPsVRDP ?
        VBoxGlobal::getOpenFileName (initDir, QString::null, this, QString::null) :
        VBoxGlobal::getExistingDirectory (initDir, this);
    if (path.isNull())
        return;

    path = QDir::convertSeparators (path);
    /* remove trailing slash if any */
    path.remove (QRegExp ("[\\\\/]$"));

    ps->setPath (path);
}

