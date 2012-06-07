/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsUSBFilterDetails class implementation
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

/* GUI includes: */
#include "UIMachineSettingsUSBFilterDetails.h"
#include "COMEnumsWrapper.h"

UIMachineSettingsUSBFilterDetails::UIMachineSettingsUSBFilterDetails(UISettingsPageType type, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QIDialog>(pParent, Qt::Sheet)
    , m_type(type)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsUSBFilterDetails::setupUi (this);

    mCbRemote->insertItem (UIMachineSettingsUSB::ModeAny, ""); /* Any */
    mCbRemote->insertItem (UIMachineSettingsUSB::ModeOn,  ""); /* Yes */
    mCbRemote->insertItem (UIMachineSettingsUSB::ModeOff, ""); /* No */
    mLbRemote->setHidden (m_type != UISettingsPageType_Machine);
    mCbRemote->setHidden (m_type != UISettingsPageType_Machine);

    mCbAction->insertItem (0, ""); /* KUSBDeviceFilterAction_Ignore */
    mCbAction->insertItem (1, ""); /* KUSBDeviceFilterAction_Hold */
    mLbAction->setHidden (m_type != UISettingsPageType_Global);
    mCbAction->setHidden (m_type != UISettingsPageType_Global);

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

void UIMachineSettingsUSBFilterDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsUSBFilterDetails::retranslateUi (this);

    mCbRemote->setItemText (UIMachineSettingsUSB::ModeAny, tr ("Any", "remote"));
    mCbRemote->setItemText (UIMachineSettingsUSB::ModeOn,  tr ("Yes", "remote"));
    mCbRemote->setItemText (UIMachineSettingsUSB::ModeOff, tr ("No",  "remote"));

    mCbAction->setItemText (0,
        gCOMenum->toString (KUSBDeviceFilterAction_Ignore));
    mCbAction->setItemText (1,
        gCOMenum->toString (KUSBDeviceFilterAction_Hold));
}

