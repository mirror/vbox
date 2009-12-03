/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsUSBFilterDetails class implementation
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

#include "VBoxVMSettingsUSBFilterDetails.h"
#include "VBoxGlobal.h"

VBoxVMSettingsUSBFilterDetails::VBoxVMSettingsUSBFilterDetails (VBoxVMSettingsUSB::FilterType aType, QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI2<QIDialog> (aParent, Qt::Sheet)
    , mType (aType)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsUSBFilterDetails::setupUi (this);

    mCbRemote->insertItem (VBoxVMSettingsUSB::ModeAny, ""); /* Any */
    mCbRemote->insertItem (VBoxVMSettingsUSB::ModeOn,  ""); /* Yes */
    mCbRemote->insertItem (VBoxVMSettingsUSB::ModeOff, ""); /* No */
    mLbRemote->setHidden (mType != VBoxVMSettingsUSB::MachineType);
    mCbRemote->setHidden (mType != VBoxVMSettingsUSB::MachineType);

    mCbAction->insertItem (0, ""); /* KUSBDeviceFilterAction_Ignore */
    mCbAction->insertItem (1, ""); /* KUSBDeviceFilterAction_Hold */
    mLbAction->setHidden (mType != VBoxVMSettingsUSB::HostType);
    mCbAction->setHidden (mType != VBoxVMSettingsUSB::HostType);

    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));
    mLeVendorID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
    mLeProductID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
    mLeRevision->setValidator (new QRegExpValidator (QRegExp ("[0-9]{0,4}"), this));
    mLePort->setValidator (new QRegExpValidator (QRegExp ("[0-9]*"), this));

    /* Applying language settings */
    retranslateUi();

    resize (minimumSize());
    setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void VBoxVMSettingsUSBFilterDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsUSBFilterDetails::retranslateUi (this);

    mCbRemote->setItemText (VBoxVMSettingsUSB::ModeAny, tr ("Any", "remote"));
    mCbRemote->setItemText (VBoxVMSettingsUSB::ModeOn,  tr ("Yes", "remote"));
    mCbRemote->setItemText (VBoxVMSettingsUSB::ModeOff, tr ("No",  "remote"));

    mCbAction->setItemText (0,
        vboxGlobal().toString (KUSBDeviceFilterAction_Ignore));
    mCbAction->setItemText (1,
        vboxGlobal().toString (KUSBDeviceFilterAction_Hold));
}

