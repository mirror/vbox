/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGlobalSettingsInput class implementation
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

#include "VBoxGlobalSettingsInput.h"
#include "VBoxGlobalSettingsDlg.h"
#include "VBoxGlobalSettings.h"

VBoxGlobalSettingsInput* VBoxGlobalSettingsInput::mSettings = 0;

void VBoxGlobalSettingsInput::getFrom (const CSystemProperties &aProps,
                                       const VBoxGlobalSettings &aGs,
                                       QWidget *aPage,
                                       VBoxGlobalSettingsDlg *aDlg)
{
    /* Create and load Input page */
    mSettings = new VBoxGlobalSettingsInput (aPage);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    mSettings->load (aProps, aGs);

    /* Fix tab order */
    setTabOrder (aDlg->mTwSelector, mSettings->mHeHostKey);
    setTabOrder (mSettings->mHeHostKey, mSettings->mCbAutoGrab);
}

void VBoxGlobalSettingsInput::putBackTo (CSystemProperties &aProps,
                                         VBoxGlobalSettings &aGs)
{
    mSettings->save (aProps, aGs);
}


VBoxGlobalSettingsInput::VBoxGlobalSettingsInput (QWidget *aParent)
    : QIWithRetranslateUI<QWidget> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxGlobalSettingsInput::setupUi (this);

    /* Applying language settings */
    retranslateUi();
}

void VBoxGlobalSettingsInput::load (const CSystemProperties &,
                                    const VBoxGlobalSettings &aGs)
{
    mHeHostKey->setKey (aGs.hostKey());
    mCbAutoGrab->setChecked (aGs.autoCapture());
}

void VBoxGlobalSettingsInput::save (CSystemProperties &,
                                    VBoxGlobalSettings &aGs)
{
    aGs.setHostKey (mHeHostKey->key());
    aGs.setAutoCapture (mCbAutoGrab->isChecked());
}

void VBoxGlobalSettingsInput::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGlobalSettingsInput::retranslateUi (this);
}

