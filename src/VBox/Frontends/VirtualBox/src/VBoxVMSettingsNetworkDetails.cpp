/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetworkDetails class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

/* VBox Includes */
#include "VBoxGlobal.h"
#include "VBoxVMSettingsNetworkDetails.h"

/* Qt Includes */
#include <QRegExpValidator>

/* VBoxVMSettingsNetwork Stuff */
VBoxVMSettingsNetworkDetails::VBoxVMSettingsNetworkDetails (QWidget *aParent)
    : QIWithRetranslateUI2 <QIDialog> (aParent
#ifdef Q_WS_MAC
    , Qt::Sheet
#endif /* Q_WS_MAC */
    )
    , mMacAddress (QString::null)
    , mCableConnected (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetworkDetails::setupUi (this);

    /* Setup widgets */
    mLeMAC->setValidator (new QRegExpValidator (QRegExp (
                          "[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    mLeMAC->setMinimumWidthByText (QString().fill ('0', 12));
    connect (mTbMAC, SIGNAL (clicked()), this, SLOT (generateMac()));
#if defined (Q_WS_MAC)
    /* Remove tool-button border at MAC */
    mTbMAC->setStyleSheet ("QToolButton {border: 0px none black;}");
#endif /* Q_WS_MAC */

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsNetworkDetails::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;
    mMacAddress = mAdapter.GetMACAddress();
    mCableConnected = mAdapter.GetCableConnected();
}

void VBoxVMSettingsNetworkDetails::putBackToAdapter()
{
    mAdapter.SetMACAddress (mMacAddress);
    mAdapter.SetCableConnected (mCableConnected);
}

void VBoxVMSettingsNetworkDetails::reload()
{
    mLeMAC->setText (mMacAddress);
    mCbCable->setChecked (mCableConnected);
}

void VBoxVMSettingsNetworkDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetworkDetails::retranslateUi (this);
}

void VBoxVMSettingsNetworkDetails::showEvent (QShowEvent *aEvent)
{
    /* Resize to minimum size */
    resize (minimumSizeHint());
    qApp->processEvents();
    setFixedSize (minimumSizeHint());

    /* Centering dialog */
    VBoxGlobal::centerWidget (this, parentWidget(), false);

    QIDialog::showEvent (aEvent);
}

void VBoxVMSettingsNetworkDetails::accept()
{
    /* Save temporary attributes */
    mMacAddress = mLeMAC->text().isEmpty() ? QString::null : mLeMAC->text();
    mCableConnected = mCbCable->isChecked();

    QIDialog::accept();
}

void VBoxVMSettingsNetworkDetails::generateMac()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
}

