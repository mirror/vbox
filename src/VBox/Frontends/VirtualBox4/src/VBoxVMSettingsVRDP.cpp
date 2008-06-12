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
#include "VBoxVMSettingsDlg.h"
#include "VBoxVMSettingsUtils.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

VBoxVMSettingsVRDP* VBoxVMSettingsVRDP::mSettings = 0;

VBoxVMSettingsVRDP::VBoxVMSettingsVRDP (QWidget *aParent,
                                        VBoxVMSettingsDlg *aDlg,
                                        const QString &aPath)
    : QWidget (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsVRDP::setupUi (this);

    /* Setup validation */
    mLeVRDPPort->setValidator (new QIntValidator (0, 0xFFFF, aDlg));
    mLeVRDPTimeout->setValidator (new QIntValidator (aDlg));

    mValidator = new QIWidgetValidator (aPath, aParent, aDlg);
    connect (mValidator, SIGNAL (validityChanged (const QIWidgetValidator*)),
             aDlg, SLOT (enableOk (const QIWidgetValidator*)));
    connect (mValidator, SIGNAL (isValidRequested (QIWidgetValidator*)),
             aDlg, SLOT (revalidate (QIWidgetValidator*)));

    /* Setup connections */
    connect (mGbVRDP, SIGNAL (toggled (bool)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPPort, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPTimeout, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));

    /* Setup dialog */
    mCbVRDPMethod->insertItem (0,
        vboxGlobal().toString (KVRDPAuthType_Null));
    mCbVRDPMethod->insertItem (1,
        vboxGlobal().toString (KVRDPAuthType_External));
    mCbVRDPMethod->insertItem (2,
        vboxGlobal().toString (KVRDPAuthType_Guest));
}

void VBoxVMSettingsVRDP::getFromMachine (const CMachine &aMachine,
                                         QWidget *aPage,
                                         VBoxVMSettingsDlg *aDlg,
                                         const QString &aPath)
{
    CVRDPServer vrdp = aMachine.GetVRDPServer();

    if (vrdp.isNull())
    {
        /* Disable the VRDP category if VRDP is
         * not available (i.e. in VirtualBox OSE) */
        QList<QTreeWidgetItem*> items = aDlg->mTwSelector->findItems (
            "#vrdp", Qt::MatchExactly, listView_Link);
        QTreeWidgetItem *vrdpItem = items.count() ? items [0] : 0;
        Assert (vrdpItem);
        if (vrdpItem)
            vrdpItem->setHidden (true);
        return;

        /* If aMachine has something to say, show the message */
        vboxProblem().cannotLoadMachineSettings (aMachine, false /* strict */);
    }

    mSettings = new VBoxVMSettingsVRDP (aPage, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);

    mSettings->getFrom (aMachine);

    /* Fixing Tab Order */
    setTabOrder (aDlg->mTwSelector, mSettings->mGbVRDP);
    setTabOrder (mSettings->mGbVRDP, mSettings->mLeVRDPPort);
    setTabOrder (mSettings->mLeVRDPPort, mSettings->mCbVRDPMethod);
    setTabOrder (mSettings->mCbVRDPMethod, mSettings->mLeVRDPTimeout);
}

void VBoxVMSettingsVRDP::putBackToMachine()
{
    mSettings->putBackTo();
}

void VBoxVMSettingsVRDP::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    CVRDPServer vrdp = aMachine.GetVRDPServer();
    mGbVRDP->setChecked (vrdp.GetEnabled());
    mLeVRDPPort->setText (QString::number (vrdp.GetPort()));
    mCbVRDPMethod->setCurrentIndex (mCbVRDPMethod->
        findText (vboxGlobal().toString (vrdp.GetAuthType())));
    mLeVRDPTimeout->setText (QString::number (vrdp.GetAuthTimeout()));
}

void VBoxVMSettingsVRDP::putBackTo()
{
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    vrdp.SetEnabled (mGbVRDP->isChecked());
    vrdp.SetPort (mLeVRDPPort->text().toULong());
    vrdp.SetAuthType (vboxGlobal().toVRDPAuthType (mCbVRDPMethod->currentText()));
    vrdp.SetAuthTimeout (mLeVRDPTimeout->text().toULong());
}

