/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsInput class implementation
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

#include "VBoxGLSettingsInput.h"
#include "VBoxGlobalSettings.h"

VBoxGLSettingsInput::VBoxGLSettingsInput()
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsInput::setupUi (this);

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsInput::getFrom (const CSystemProperties &,
                                   const VBoxGlobalSettings &aGs)
{
    mHeHostKey->setKey (aGs.hostKey());
    mCbAutoGrab->setChecked (aGs.autoCapture());
}

void VBoxGLSettingsInput::putBackTo (CSystemProperties &,
                                     VBoxGlobalSettings &aGs)
{
    aGs.setHostKey (mHeHostKey->key());
    aGs.setAutoCapture (mCbAutoGrab->isChecked());
}

void VBoxGLSettingsInput::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mHeHostKey);
    setTabOrder (mHeHostKey, mCbAutoGrab);
}

void VBoxGLSettingsInput::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsInput::retranslateUi (this);
}

