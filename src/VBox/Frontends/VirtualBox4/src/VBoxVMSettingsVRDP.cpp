/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsVRDP class implementation
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

#include "VBoxVMSettingsVRDP.h"
#include "VBoxGlobal.h"
#include "QIWidgetValidator.h"

VBoxVMSettingsVRDP::VBoxVMSettingsVRDP()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsVRDP::setupUi (this);

    /* Setup validation */
    mLeVRDPPort->setValidator (new QIntValidator (0, 0xFFFF, this));
    mLeVRDPTimeout->setValidator (new QIntValidator (this));

    /* Setup dialog */
    mCbVRDPMethod->insertItem (0, ""); /* KVRDPAuthType_Null */
    mCbVRDPMethod->insertItem (1, ""); /* KVRDPAuthType_External */
    mCbVRDPMethod->insertItem (2, ""); /* KVRDPAuthType_Guest */

    /* Initial disabled */
    mGbVRDP->setChecked (false);

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsVRDP::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    CVRDPServer vrdp = aMachine.GetVRDPServer();
    if (!vrdp.isNull())
    {
        mGbVRDP->setChecked (vrdp.GetEnabled());
        mLeVRDPPort->setText (QString::number (vrdp.GetPort()));
        mCbVRDPMethod->setCurrentIndex (mCbVRDPMethod->
                                        findText (vboxGlobal().toString (vrdp.GetAuthType())));
        mLeVRDPTimeout->setText (QString::number (vrdp.GetAuthTimeout()));
    }
}

void VBoxVMSettingsVRDP::putBackTo()
{
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    if (!vrdp.isNull())
    {
        vrdp.SetEnabled (mGbVRDP->isChecked());
        vrdp.SetPort (mLeVRDPPort->text().toULong());
        vrdp.SetAuthType (vboxGlobal().toVRDPAuthType (mCbVRDPMethod->currentText()));
        vrdp.SetAuthTimeout (mLeVRDPTimeout->text().toULong());
    }
}

void VBoxVMSettingsVRDP::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mGbVRDP, SIGNAL (toggled (bool)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPPort, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPTimeout, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
}

void VBoxVMSettingsVRDP::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbVRDP);
    setTabOrder (mGbVRDP, mLeVRDPPort);
    setTabOrder (mLeVRDPPort, mCbVRDPMethod);
    setTabOrder (mCbVRDPMethod, mLeVRDPTimeout);
}

void VBoxVMSettingsVRDP::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsVRDP::retranslateUi (this);

    mCbVRDPMethod->setItemText (0,
        vboxGlobal().toString (KVRDPAuthType_Null));
    mCbVRDPMethod->setItemText (1,
        vboxGlobal().toString (KVRDPAuthType_External));
    mCbVRDPMethod->setItemText (2,
        vboxGlobal().toString (KVRDPAuthType_Guest));
}

